#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "agent_cycle_demo_config.h"

#define DEMO_OUTPUT_ROOT "results/scenarios"

static int ensure_directory(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int copy_file(const char *source_path, const char *destination_path)
{
    FILE *source = fopen(source_path, "rb");
    FILE *destination;
    int character;
    int failed = 0;
    if (source == NULL)
        return 0;
    destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        fclose(source);
        return 0;
    }
    while ((character = fgetc(source)) != EOF)
    {
        if (fputc(character, destination) == EOF)
        {
            failed = 1;
            break;
        }
    }
    if (ferror(source))
        failed = 1;
    if (fclose(source) != 0)
        failed = 1;
    if (fclose(destination) != 0)
        failed = 1;
    return !failed;
}

static int write_report(const char *path, const AgentCycleDemoConfig *config,
                        const MiniSNNAgentIOContext *agent_io,
                        const MiniSNNSensorEncoder *encoder,
                        const MiniSNNActionDecoder *decoder,
                        const MiniSNNAgentCycle *cycle)
{
    FILE *file = fopen(path, "wb");
    int failed = 0;
    if (file == NULL)
        return 0;
    if (fprintf(file,
                "<!doctype html><html><head><meta charset=\"utf-8\"><title>Agent cycle demo</title>"
                "<style>body{font-family:system-ui;background:#121821;color:#e6edf3;max-width:960px;"
                "margin:auto;padding:24px}table{border-collapse:collapse}td,th{border:1px solid #445;"
                "padding:6px;text-align:left}code{color:#8be9fd}</style></head><body>"
                "<h1>Generic brain-agent cycle</h1><p>This local demonstration has no domain semantics. "
                "External code owns targets and rewards.</p><h2>Configuration</h2><table>"
                "<tr><th>model</th><td>%s</td></tr><tr><th>neurons</th><td>%u</td></tr>"
                "<tr><th>brain steps/tick</th><td>%u</td></tr><tr><th>sensor schema</th><td>%llu</td></tr>"
                "<tr><th>action schema</th><td>%llu</td></tr><tr><th>encoder contract</th><td>%llu</td></tr>"
                "<tr><th>decoder contract</th><td>%llu</td></tr></table>"
                "<h2>Temporal order</h2><ol><li>external sensor submission</li><li>encoding</li>"
                "<li>input then one neural step, repeated N times</li><li>activity decoding</li>"
                "<li>atomic action publication</li><li>external action consumption and delayed feedback</li></ol>"
                "<p>Feedback due at tick T is passed to C2 before neural step zero of T. A failure after a "
                "neural step faults the cycle and requires an explicit transient reset; there is no per-tick "
                "network rollback. Reset keeps topology and learned weights, while clearing currents, model "
                "state, STDP traces, R-STDP eligibility and pending rewards.</p>"
                "<h2>Run summary</h2><p>ticks=%llu; neural_steps=%llu; actions=%llu; total_reward=%.17g; resets=%llu.</p>"
                "<p><a href=\"agent_cycle_trace.csv\">trace CSV</a> | "
                "<a href=\"agent_cycle_feedback.csv\">feedback CSV</a> | "
                "<a href=\"agent_cycle_summary.txt\">summary</a> | "
                "<a href=\"config_source.ini\">source configuration</a> | "
                "<a href=\"config_used.ini\">effective configuration</a></p></body></html>\n",
                minisnn_neuron_model_name(config->neuron_model), config->neuron_count,
                config->brain_steps_per_tick,
                (unsigned long long)minisnn_agent_io_sensor_schema_signature(agent_io),
                (unsigned long long)minisnn_agent_io_action_schema_signature(agent_io),
                (unsigned long long)minisnn_sensor_encoder_contract_signature(encoder),
                (unsigned long long)minisnn_action_decoder_contract_signature(decoder),
                (unsigned long long)minisnn_agent_cycle_total_ticks(cycle),
                (unsigned long long)minisnn_agent_cycle_total_neural_steps(cycle),
                (unsigned long long)minisnn_agent_cycle_total_actions(cycle),
                minisnn_agent_cycle_total_reward(cycle),
                (unsigned long long)minisnn_agent_cycle_reset_count(cycle)) < 0)
        failed = 1;
    if (fclose(file) != 0)
        failed = 1;
    return !failed;
}

int main(int argc, char **argv)
{
    const char *config_path = argc == 2 ? argv[1] : "configs/agent_cycle_demo.ini";
    AgentCycleDemoConfig config;
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    const MiniSNNSensorChannelSpec sensor_channels[] =
    {
        {10U, "input_signal", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec action_channels[] =
    {
        {20U, "output_signal", 0.0, 1.0, 0.0}
    };
    MiniSNNSensorEncodingSpec encoding;
    const MiniSNNActionDecodingSpec decoding =
    {
        20U, MINISNN_ACTION_DECODING_POPULATION_RATE,
        0U, 1U, 0U, 0U, 0.0, 1.0,
        0.0, 0.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0
    };
    MiniSNNConfig network_config;
    MiniSNNPlasticityConfig plasticity;
    MiniSNNRewardConfig reward;
    MiniSNNSensorSchema *sensor_schema = NULL;
    MiniSNNActionSchema *action_schema = NULL;
    MiniSNNAgentIOContext *agent_io = NULL;
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNAgentCycle *cycle = NULL;
    MiniSNN *network = NULL;
    MiniSNNActionFrame action = {0};
    MiniSNNAgentCycleDiagnostics diagnostics;
    MiniSNNConnectionInfo connection_before;
    MiniSNNConnectionInfo connection_after;
    MiniSNNAgentFeedback queued_feedback = {0};
    int queued_feedback_active = 0;
    FILE *trace = NULL;
    FILE *feedback_file = NULL;
    FILE *summary = NULL;
    char error_message[256];
    char output_dir[256];
    char config_source_path[320];
    char config_used_path[320];
    char trace_path[320];
    char feedback_path[320];
    char summary_path[320];
    char report_path[320];
    int exit_code = 1;

    if (argc > 2)
    {
        printf("Uso: agent_cycle_demo.exe [configs/agent_cycle_demo.ini]\n");
        goto cleanup;
    }
    if (!agent_cycle_demo_config_load_file(config_path, &config, error_message,
                                           sizeof(error_message)))
    {
        printf("Erro ao carregar configuracao: %s.\n", error_message);
        goto cleanup;
    }
    if (snprintf(output_dir, sizeof(output_dir), "%s/%s", DEMO_OUTPUT_ROOT,
                 config.run_name) < 0 ||
        snprintf(config_source_path, sizeof(config_source_path), "%s/config_source.ini",
                 output_dir) < 0 ||
        snprintf(config_used_path, sizeof(config_used_path), "%s/config_used.ini",
                 output_dir) < 0 ||
        snprintf(trace_path, sizeof(trace_path), "%s/agent_cycle_trace.csv", output_dir) < 0 ||
        snprintf(feedback_path, sizeof(feedback_path), "%s/agent_cycle_feedback.csv", output_dir) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/agent_cycle_summary.txt", output_dir) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/agent_cycle_report.html", output_dir) < 0)
    {
        printf("Erro ao montar caminhos de saida.\n");
        goto cleanup;
    }
    if (!ensure_directory("results") || !ensure_directory(DEMO_OUTPUT_ROOT) ||
        !ensure_directory(output_dir) || !copy_file(config_path, config_source_path) ||
        !agent_cycle_demo_config_write_file(config_used_path, &config, error_message,
                                            sizeof(error_message)))
    {
        printf("Erro ao preparar proveniencia e resultados: %s.\n", error_message);
        goto cleanup;
    }

    network_config = minisnn_default_config();
    network_config.neuron_count = (int)config.neuron_count;
    network_config.neuron_model = config.neuron_model;
    network = minisnn_create_with_config(&network_config);
    if (network == NULL || !minisnn_connect_delayed(network, 0, 1, 0.5, 1))
    {
        printf("Erro ao criar rede do demo.\n");
        goto cleanup;
    }
    plasticity = minisnn_default_plasticity_config();
    plasticity.enabled = 1;
    plasticity.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    reward = minisnn_default_reward_config();
    reward.enabled = 1;
    if (!minisnn_set_plasticity_config(network, &plasticity) ||
        !minisnn_set_reward_config(network, &reward))
    {
        printf("Erro ao configurar R-STDP do demo.\n");
        goto cleanup;
    }
    sensor_schema = minisnn_sensor_schema_create(sensor_channels, 1U, &io_error);
    action_schema = minisnn_action_schema_create(action_channels, 1U, &io_error);
    agent_io = minisnn_agent_io_create(sensor_schema, action_schema, &io_error);
    encoding.sensor_channel_id = 10U;
    encoding.target_neuron_start = 0U;
    encoding.target_neuron_count = 1U;
    encoding.mode = MINISNN_SENSOR_ENCODING_LINEAR_CURRENT;
    encoding.gain = config.input_current;
    encoding.bias = 0.0;
    encoding.pulse_current = 0.0;
    encoding.maximum_rate = 0.0;
    encoding.phase_offset = 0U;
    encoder = minisnn_sensor_encoder_create(sensor_schema, &encoding, 1U,
                                            config.neuron_count,
                                            config.brain_steps_per_tick,
                                            &encoder_error);
    decoder = minisnn_action_decoder_create(action_schema, &decoding, 1U,
                                            config.neuron_count,
                                            config.brain_steps_per_tick,
                                            &decoder_error);
    cycle = minisnn_agent_cycle_create(network, agent_io, encoder, decoder,
                                       &cycle_error);
    if (sensor_schema == NULL || action_schema == NULL || agent_io == NULL ||
        encoder == NULL || decoder == NULL || cycle == NULL ||
        !minisnn_action_frame_init(&action, 1U))
    {
        printf("Erro ao criar o ciclo: %s.\n",
               minisnn_agent_cycle_error_string(cycle_error));
        goto cleanup;
    }
    trace = fopen(trace_path, "wb");
    feedback_file = fopen(feedback_path, "wb");
    summary = fopen(summary_path, "wb");
    if (trace == NULL || feedback_file == NULL || summary == NULL ||
        fprintf(trace, "episode_id,episode_tick,global_tick,input_signal,brain_steps,spike_count,output_signal,cycle_state\n") < 0 ||
        fprintf(feedback_file,
                "source_tick,delivery_tick,reward,episode_terminal,status\n") < 0)
    {
        printf("Erro ao abrir saidas do demo.\n");
        goto cleanup;
    }

    for (uint32_t index = 0U; index < config.ticks; index++)
    {
        MiniSNNSensorFrame sensor = {0};
        MiniSNNAgentFeedback feedback;
        double signal = index % 3U == 0U ? 0.0 : 1.0;
        double target = index % 3U == 2U ? 0.0 : signal;
        double reward_value;
        int terminal = (index + 1U) % config.reset_interval == 0U;

        if (!minisnn_sensor_frame_init(&sensor, 1U) ||
            !minisnn_sensor_frame_set_values(&sensor,
                                              minisnn_agent_cycle_next_global_tick(cycle),
                                              &signal, 1U, &io_error) ||
            !minisnn_agent_io_submit_sensor_frame(agent_io, &sensor) ||
            !minisnn_agent_cycle_run_tick(cycle, &diagnostics) ||
            !minisnn_agent_io_consume_action_frame(agent_io, &action))
        {
            minisnn_sensor_frame_destroy(&sensor);
            printf("Erro no tick %u: %s.\n", index,
                   minisnn_agent_cycle_error_string(minisnn_agent_cycle_last_error(cycle)));
            goto cleanup;
        }
        minisnn_sensor_frame_destroy(&sensor);
        if (queued_feedback_active)
        {
            if (diagnostics.feedback_events_delivered == 0U ||
                fprintf(feedback_file, "%llu,%llu,%.6f,%u,delivered_on_tick\n",
                        (unsigned long long)queued_feedback.source_tick,
                        (unsigned long long)queued_feedback.delivery_tick,
                        queued_feedback.reward,
                        (unsigned int)queued_feedback.episode_terminal) < 0)
            {
                printf("Erro ao registrar entrega de feedback.\n");
                goto cleanup;
            }
            queued_feedback_active = 0;
        }
        reward_value = index % 3U == 0U ? 0.0 :
            ((action.values[0] >= 0.5) == (target >= 0.5) ? 1.0 : -1.0);
        feedback.source_tick = action.tick;
        feedback.delivery_tick = action.tick + 1U;
        feedback.reward = reward_value;
        feedback.episode_terminal = terminal ? 1U : 0U;
        if (!minisnn_agent_cycle_submit_feedback(cycle, &feedback) ||
            fprintf(trace, "%llu,%llu,%llu,%.6f,%u,%llu,%.6f,%d\n",
                    (unsigned long long)diagnostics.episode_id,
                    (unsigned long long)diagnostics.episode_tick,
                    (unsigned long long)diagnostics.global_tick, signal,
                    diagnostics.brain_steps_executed,
                    (unsigned long long)diagnostics.total_spikes, action.values[0],
                    (int)diagnostics.state) < 0 ||
            fprintf(feedback_file, "%llu,%llu,%.6f,%u,%s\n",
                    (unsigned long long)feedback.source_tick,
                    (unsigned long long)feedback.delivery_tick, feedback.reward,
                    (unsigned int)feedback.episode_terminal,
                    terminal ? "delivered_at_episode_boundary" : "queued") < 0)
        {
            printf("Erro ao registrar feedback externo.\n");
            goto cleanup;
        }
        if (!terminal)
        {
            queued_feedback = feedback;
            queued_feedback_active = 1;
        }
        if (terminal)
        {
            if (!minisnn_get_connection(network, 0U, &connection_before) ||
                !minisnn_agent_cycle_reset_episode(cycle) ||
                !minisnn_get_connection(network, 0U, &connection_after) ||
                connection_before.weight != connection_after.weight)
            {
                printf("Erro ao resetar episodio com pesos preservados.\n");
                goto cleanup;
            }
        }
    }
    if (fprintf(summary,
                "agent_cycle_demo\nconfig_source=%s\nmodel=%s\nneurons=%u\n"
                "brain_steps_per_tick=%u\nticks=%llu\nneural_steps=%llu\nactions=%llu\n"
                "total_reward=%.17g\nresets=%llu\nsensor_schema_signature=%llu\n"
                "action_schema_signature=%llu\nencoder_contract_signature=%llu\n"
                "decoder_contract_signature=%llu\ncycle_state=%d\n",
                config_path, minisnn_neuron_model_name(config.neuron_model),
                config.neuron_count, config.brain_steps_per_tick,
                (unsigned long long)minisnn_agent_cycle_total_ticks(cycle),
                (unsigned long long)minisnn_agent_cycle_total_neural_steps(cycle),
                (unsigned long long)minisnn_agent_cycle_total_actions(cycle),
                minisnn_agent_cycle_total_reward(cycle),
                (unsigned long long)minisnn_agent_cycle_reset_count(cycle),
                (unsigned long long)minisnn_agent_io_sensor_schema_signature(agent_io),
                (unsigned long long)minisnn_agent_io_action_schema_signature(agent_io),
                (unsigned long long)minisnn_sensor_encoder_contract_signature(encoder),
                (unsigned long long)minisnn_action_decoder_contract_signature(decoder),
                (int)minisnn_agent_cycle_state(cycle)) < 0 ||
        fclose(trace) != 0 || fclose(feedback_file) != 0 || fclose(summary) != 0)
    {
        trace = NULL;
        feedback_file = NULL;
        summary = NULL;
        printf("Erro ao fechar saidas do demo.\n");
        goto cleanup;
    }
    trace = NULL;
    feedback_file = NULL;
    summary = NULL;
    if (!write_report(report_path, &config, agent_io, encoder, decoder, cycle))
    {
        printf("Erro ao gravar relatorio HTML.\n");
        goto cleanup;
    }
    printf("Agent cycle demo concluido em %s\n", output_dir);
    exit_code = 0;

cleanup:
    if (trace != NULL)
        fclose(trace);
    if (feedback_file != NULL)
        fclose(feedback_file);
    if (summary != NULL)
        fclose(summary);
    minisnn_action_frame_destroy(&action);
    minisnn_agent_cycle_destroy(&cycle);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_agent_io_destroy(&agent_io);
    minisnn_action_schema_destroy(&action_schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    minisnn_destroy(&network);
    return exit_code;
}
