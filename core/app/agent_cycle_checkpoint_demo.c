#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "agent_cycle_checkpoint_demo_config.h"

#define CHECKPOINT_DEMO_OUTPUT_ROOT "results/scenarios"
#define CHECKPOINT_DEMO_PATH_MAX 512U

typedef struct
{
    MiniSNN *network;
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNActionSchema *action_schema;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *encoder;
    MiniSNNActionDecoder *decoder;
    MiniSNNAgentCycle *cycle;
} CheckpointDemo;

static int ensure_directory(const char *path)
{
    return path != NULL &&
        (CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS);
}

static int join_path(char *out_path, size_t out_size, const char *directory,
                     const char *name)
{
    int written;
    if (out_path == NULL || out_size == 0U || directory == NULL || name == NULL)
        return 0;
    written = snprintf(out_path, out_size, "%s/%s", directory, name);
    return written >= 0 && (size_t)written < out_size;
}

static int copy_file_bytes(const char *source_path, const char *destination_path)
{
    FILE *source;
    FILE *destination;
    int value;
    int ok = 1;
    int source_error;
    int source_close_error;
    int destination_close_error;
    if (source_path == NULL || destination_path == NULL ||
        (source = fopen(source_path, "rb")) == NULL)
        return 0;
    destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        fclose(source);
        return 0;
    }
    while ((value = fgetc(source)) != EOF)
        if (fputc(value, destination) == EOF)
        {
            ok = 0;
            break;
        }
    source_error = ferror(source);
    source_close_error = fclose(source);
    destination_close_error = fclose(destination);
    if (source_error || source_close_error != 0 || destination_close_error != 0)
        ok = 0;
    return ok;
}

static void checkpoint_demo_destroy(CheckpointDemo *demo)
{
    if (demo == NULL)
        return;
    minisnn_agent_cycle_destroy(&demo->cycle);
    minisnn_action_decoder_destroy(&demo->decoder);
    minisnn_sensor_encoder_destroy(&demo->encoder);
    minisnn_agent_io_destroy(&demo->agent_io);
    minisnn_action_schema_destroy(&demo->action_schema);
    minisnn_sensor_schema_destroy(&demo->sensor_schema);
    minisnn_destroy(&demo->network);
    memset(demo, 0, sizeof(*demo));
}

static int checkpoint_demo_create(CheckpointDemo *demo,
                                  const AgentCycleCheckpointDemoConfig *config)
{
    const MiniSNNSensorChannelSpec sensor_channels[] =
    {
        {10U, "input_a", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec action_channels[] =
    {
        {20U, "output_a", 0.0, 1.0, 0.0}
    };
    const MiniSNNSensorEncodingSpec encoding[] =
    {
        {10U, 0U, 1U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         0.0, 0.0, 0.0, 0.0, 0U}
    };
    const MiniSNNActionDecodingSpec decoding[] =
    {
        {20U, MINISNN_ACTION_DECODING_POPULATION_RATE,
         1U, 1U, 0U, 0U, 0.0, 1.0,
         0.0, 0.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0}
    };
    MiniSNNSensorEncodingSpec effective_encoding = encoding[0];
    MiniSNNConfig network_config;
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;

    if (demo == NULL || config == NULL)
        return 0;
    memset(demo, 0, sizeof(*demo));
    network_config = minisnn_default_config();
    network_config.neuron_count = (int)config->neuron_count;
    network_config.neuron_model = config->neuron_model;
    demo->network = minisnn_create_with_config(&network_config);
    demo->sensor_schema = minisnn_sensor_schema_create(sensor_channels, 1U, &io_error);
    demo->action_schema = minisnn_action_schema_create(action_channels, 1U, &io_error);
    demo->agent_io = minisnn_agent_io_create(demo->sensor_schema, demo->action_schema,
                                              &io_error);
    effective_encoding.gain = config->input_current;
    demo->encoder = minisnn_sensor_encoder_create(demo->sensor_schema,
                                                   &effective_encoding, 1U,
                                                   config->neuron_count,
                                                   config->brain_steps_per_tick,
                                                   &encoder_error);
    demo->decoder = minisnn_action_decoder_create(demo->action_schema, decoding, 1U,
                                                   config->neuron_count,
                                                   config->brain_steps_per_tick,
                                                   &decoder_error);
    demo->cycle = minisnn_agent_cycle_create(demo->network, demo->agent_io,
                                             demo->encoder, demo->decoder,
                                             &cycle_error);
    if (demo->network == NULL || demo->sensor_schema == NULL ||
        demo->action_schema == NULL || demo->agent_io == NULL || demo->encoder == NULL ||
        demo->decoder == NULL || demo->cycle == NULL ||
        !minisnn_connect_delayed(demo->network, 0, 1, 0.5, 1))
    {
        checkpoint_demo_destroy(demo);
        return 0;
    }
    return 1;
}

static int submit_and_run(CheckpointDemo *demo, uint64_t tick, double input)
{
    MiniSNNSensorFrame frame = {0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    int ok = demo != NULL && minisnn_sensor_frame_init(&frame, 1U) &&
        minisnn_sensor_frame_set_values(&frame, tick, &input, 1U, &error) &&
        minisnn_agent_io_submit_sensor_frame(demo->agent_io, &frame) &&
        minisnn_agent_cycle_run_tick(demo->cycle, NULL);
    minisnn_sensor_frame_destroy(&frame);
    return ok;
}

static int consume_action(CheckpointDemo *demo, uint64_t tick, double *out_value)
{
    MiniSNNActionFrame frame = {0};
    int ok = demo != NULL && out_value != NULL &&
        minisnn_action_frame_init(&frame, 1U) &&
        minisnn_agent_io_consume_action_frame(demo->agent_io, &frame) &&
        frame.tick == tick && isfinite(frame.values[0]);
    if (ok)
        *out_value = frame.values[0];
    minisnn_action_frame_destroy(&frame);
    return ok;
}

static double input_for_tick(uint64_t tick)
{
    return (tick & UINT64_C(1)) == 0U ? 1.0 : 0.5;
}

static int write_trace(const char *path, const double *values, uint32_t count)
{
    FILE *file;
    if (path == NULL || values == NULL || (file = fopen(path, "wb")) == NULL)
        return 0;
    if (fputs("tick,input,action\n", file) < 0)
    {
        fclose(file);
        return 0;
    }
    for (uint32_t tick = 0U; tick < count; tick++)
        if (fprintf(file, "%u,%.17g,%.17g\n", tick, input_for_tick(tick),
                    values[tick]) < 0)
        {
            fclose(file);
            return 0;
        }
    return fclose(file) == 0;
}

static int write_comparison(const char *path, const double *continuous,
                            const double *ready, const double *pending,
                            uint32_t count, int *out_equal)
{
    FILE *file;
    int equal = 1;
    if (path == NULL || continuous == NULL || ready == NULL || pending == NULL ||
        out_equal == NULL || (file = fopen(path, "wb")) == NULL)
        return 0;
    if (fputs("tick,continuous_action,ready_resume_action,action_pending_resume_action,identical\n",
              file) < 0)
    {
        fclose(file);
        return 0;
    }
    for (uint32_t tick = 0U; tick < count; tick++)
    {
        int identical = continuous[tick] == ready[tick] &&
            continuous[tick] == pending[tick];
        if (!identical)
            equal = 0;
        if (fprintf(file, "%u,%.17g,%.17g,%.17g,%s\n", tick,
                    continuous[tick], ready[tick], pending[tick],
                    identical ? "yes" : "no") < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fclose(file) != 0)
        return 0;
    *out_equal = equal;
    return 1;
}

static int write_summary(const char *path,
                         const AgentCycleCheckpointDemoConfig *config,
                         int equivalent)
{
    FILE *file;
    if (path == NULL || config == NULL || (file = fopen(path, "wb")) == NULL)
        return 0;
    if (fprintf(file,
                "run_name=%s\nmodel=%s\nneurons=%u\nbrain_steps_per_tick=%u\n"
                "ticks=%u\ncheckpoint_ready=verified\ncheckpoint_action_pending=verified\n"
                "checkpoint_integrity=fnv1a_components\ncheckpoint_provenance=miniSNN_core_c7\n"
                "replay_equivalent=%s\n",
                config->run_name, minisnn_neuron_model_name(config->neuron_model),
                config->neuron_count, config->brain_steps_per_tick, config->ticks,
                equivalent ? "yes" : "no") < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int write_report(const char *path,
                        const AgentCycleCheckpointDemoConfig *config,
                        int equivalent)
{
    FILE *file;
    if (path == NULL || config == NULL || (file = fopen(path, "wb")) == NULL)
        return 0;
    if (fprintf(file,
                "<!doctype html><html><head><meta charset=\"utf-8\"><title>Agent cycle checkpoint</title>"
                "<style>body{background:#121821;color:#e6edf3;font-family:system-ui;max-width:920px;"
                "margin:auto;padding:24px}table{border-collapse:collapse}th,td{border:1px solid #456;padding:7px}"
                "a{color:#8be9fd}</style></head><body><h1>Agent cycle checkpoint and replay</h1>"
                "<p>This local protocol persists generic interface, encoder, decoder, network and cycle state. "
                "It has no domain semantics.</p><table><tr><th>model</th><td>%s</td></tr>"
                "<tr><th>neurons</th><td>%u</td></tr><tr><th>brain steps/tick</th><td>%u</td></tr>"
                "<tr><th>stable boundary READY</th><td>verified</td></tr>"
                "<tr><th>stable boundary ACTION_PENDING</th><td>verified</td></tr>"
                "<tr><th>component integrity</th><td>versioned FNV-1a hashes</td></tr>"
                "<tr><th>compatibility</th><td>schema, model, dimensions, contracts and topology</td></tr>"
                "<tr><th>equivalent traces</th><td>%s</td></tr></table>"
                "<h2>Boundary and ownership</h2><p>Stable checkpoints are saved only before a sensor submission "
                "or after action publication. The caller owns the network, AgentIO, encoder and decoder; "
                "the cycle stores state, never internal pointers. A pending published action remains consumable "
                "exactly once after resume.</p>"
                "<h2>Persisted state</h2><p>The snapshot includes temporal network state, delayed currents, "
                "STDP and reward eligibility, homeostasis, structural state, future feedback and deterministic-rate "
                "encoder phases. The decoder is stateless between ticks and is restored by contract validation.</p>"
                "<h2>Replay coverage</h2><p>READY resume verifies continuation before a new sensor frame. "
                "ACTION_PENDING resume verifies exactly-once external consumption before the next tick. "
                "The trace comparison covers the same deterministic input sequence.</p>"
                "<h2>Limitations</h2><p>This is a numerical persistence and replay demonstration. It does not "
                "define an environment, task semantics, learning objective or external domain.</p>"
                "<p><a href=\"continuous_trace.csv\">continuous trace</a> | "
                "<a href=\"ready_resume_trace.csv\">READY resume trace</a> | "
                "<a href=\"action_pending_resume_trace.csv\">ACTION_PENDING resume trace</a> | "
                "<a href=\"checkpoint_comparison.csv\">comparison</a> | "
                "<a href=\"checkpoint_manifest_copy.txt\">manifest copy</a> | "
                "<a href=\"config_source.ini\">source configuration</a> | "
                "<a href=\"config_used.ini\">effective configuration</a></p></body></html>\n",
                minisnn_neuron_model_name(config->neuron_model), config->neuron_count,
                config->brain_steps_per_tick, equivalent ? "yes" : "no") < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int main(int argc, char **argv)
{
    const char *config_path = argc == 2 ? argv[1] : "configs/agent_cycle_checkpoint_demo.ini";
    AgentCycleCheckpointDemoConfig config;
    CheckpointDemo continuous = {0}, ready_source = {0}, ready_resumed = {0};
    CheckpointDemo pending_source = {0}, pending_resumed = {0};
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    char message[256] = "";
    char output_directory[CHECKPOINT_DEMO_PATH_MAX];
    char ready_directory[CHECKPOINT_DEMO_PATH_MAX];
    char pending_directory[CHECKPOINT_DEMO_PATH_MAX];
    char source_path[CHECKPOINT_DEMO_PATH_MAX];
    char used_path[CHECKPOINT_DEMO_PATH_MAX];
    char manifest_path[CHECKPOINT_DEMO_PATH_MAX];
    char manifest_copy_path[CHECKPOINT_DEMO_PATH_MAX];
    char continuous_path[CHECKPOINT_DEMO_PATH_MAX];
    char ready_path[CHECKPOINT_DEMO_PATH_MAX];
    char pending_path[CHECKPOINT_DEMO_PATH_MAX];
    char comparison_path[CHECKPOINT_DEMO_PATH_MAX];
    char summary_path[CHECKPOINT_DEMO_PATH_MAX];
    char report_path[CHECKPOINT_DEMO_PATH_MAX];
    double *continuous_values = NULL;
    double *ready_values = NULL;
    double *pending_values = NULL;
    int equivalent = 0;
    int exit_code = 1;

    if (argc > 2)
    {
        printf("Uso: agent_cycle_checkpoint_demo.exe [configs/agent_cycle_checkpoint_demo.ini]\n");
        goto cleanup;
    }
    if (!agent_cycle_checkpoint_demo_config_load_file(config_path, &config, message,
                                                       sizeof(message)))
    {
        printf("Erro ao carregar configuracao: %s.\n", message);
        goto cleanup;
    }
    if (!join_path(output_directory, sizeof(output_directory), CHECKPOINT_DEMO_OUTPUT_ROOT,
                   config.run_name) ||
        !join_path(ready_directory, sizeof(ready_directory), output_directory,
                   "checkpoint_ready") ||
        !join_path(pending_directory, sizeof(pending_directory), output_directory,
                   "checkpoint_action_pending") ||
        !join_path(source_path, sizeof(source_path), output_directory, "config_source.ini") ||
        !join_path(used_path, sizeof(used_path), output_directory, "config_used.ini") ||
        !join_path(manifest_path, sizeof(manifest_path), ready_directory, "manifest.txt") ||
        !join_path(manifest_copy_path, sizeof(manifest_copy_path), output_directory,
                   "checkpoint_manifest_copy.txt") ||
        !join_path(continuous_path, sizeof(continuous_path), output_directory,
                   "continuous_trace.csv") ||
        !join_path(ready_path, sizeof(ready_path), output_directory,
                   "ready_resume_trace.csv") ||
        !join_path(pending_path, sizeof(pending_path), output_directory,
                   "action_pending_resume_trace.csv") ||
        !join_path(comparison_path, sizeof(comparison_path), output_directory,
                   "checkpoint_comparison.csv") ||
        !join_path(summary_path, sizeof(summary_path), output_directory,
                   "agent_cycle_checkpoint_summary.txt") ||
        !join_path(report_path, sizeof(report_path), output_directory,
                   "agent_cycle_checkpoint_report.html"))
    {
        printf("Erro ao montar caminhos de saida.\n");
        goto cleanup;
    }
    if (!ensure_directory("results") || !ensure_directory(CHECKPOINT_DEMO_OUTPUT_ROOT) ||
        !ensure_directory(output_directory) || !ensure_directory(ready_directory) ||
        !ensure_directory(pending_directory) || !copy_file_bytes(config_path, source_path) ||
        !agent_cycle_checkpoint_demo_config_write_file(used_path, &config, message,
                                                        sizeof(message)))
    {
        printf("Erro ao preparar resultados: %s.\n", message);
        goto cleanup;
    }
    continuous_values = calloc(config.ticks, sizeof(*continuous_values));
    ready_values = calloc(config.ticks, sizeof(*ready_values));
    pending_values = calloc(config.ticks, sizeof(*pending_values));
    if (continuous_values == NULL || ready_values == NULL || pending_values == NULL ||
        !checkpoint_demo_create(&continuous, &config) ||
        !checkpoint_demo_create(&ready_source, &config) ||
        !checkpoint_demo_create(&ready_resumed, &config) ||
        !checkpoint_demo_create(&pending_source, &config) ||
        !checkpoint_demo_create(&pending_resumed, &config))
    {
        printf("Erro ao criar os componentes do demo.\n");
        goto cleanup;
    }
    for (uint32_t tick = 0U; tick < config.ticks; tick++)
        if (!submit_and_run(&continuous, tick, input_for_tick(tick)) ||
            !consume_action(&continuous, tick, &continuous_values[tick]))
        {
            printf("Erro no caminho continuo no tick %u.\n", tick);
            goto cleanup;
        }

    if (!submit_and_run(&ready_source, 0U, input_for_tick(0U)) ||
        !consume_action(&ready_source, 0U, &ready_values[0]) ||
        !minisnn_agent_cycle_save_checkpoint(ready_source.cycle, ready_directory, &cycle_error) ||
        !minisnn_agent_cycle_load_checkpoint(ready_resumed.cycle, ready_directory, &cycle_error))
    {
        printf("Erro no checkpoint READY: %s.\n", minisnn_agent_cycle_error_string(cycle_error));
        goto cleanup;
    }
    for (uint32_t tick = 1U; tick < config.ticks; tick++)
        if (!submit_and_run(&ready_resumed, tick, input_for_tick(tick)) ||
            !consume_action(&ready_resumed, tick, &ready_values[tick]))
        {
            printf("Erro no resume READY no tick %u.\n", tick);
            goto cleanup;
        }

    if (!submit_and_run(&pending_source, 0U, input_for_tick(0U)) ||
        !minisnn_agent_cycle_save_checkpoint(pending_source.cycle, pending_directory,
                                              &cycle_error) ||
        !minisnn_agent_cycle_load_checkpoint(pending_resumed.cycle, pending_directory,
                                              &cycle_error) ||
        !consume_action(&pending_resumed, 0U, &pending_values[0]))
    {
        printf("Erro no checkpoint ACTION_PENDING: %s.\n",
               minisnn_agent_cycle_error_string(cycle_error));
        goto cleanup;
    }
    for (uint32_t tick = 1U; tick < config.ticks; tick++)
        if (!submit_and_run(&pending_resumed, tick, input_for_tick(tick)) ||
            !consume_action(&pending_resumed, tick, &pending_values[tick]))
        {
            printf("Erro no resume ACTION_PENDING no tick %u.\n", tick);
            goto cleanup;
        }
    if (!copy_file_bytes(manifest_path, manifest_copy_path) ||
        !write_trace(continuous_path, continuous_values, config.ticks) ||
        !write_trace(ready_path, ready_values, config.ticks) ||
        !write_trace(pending_path, pending_values, config.ticks) ||
        !write_comparison(comparison_path, continuous_values, ready_values, pending_values,
                          config.ticks, &equivalent) ||
        !write_summary(summary_path, &config, equivalent) ||
        !write_report(report_path, &config, equivalent))
    {
        printf("Erro ao gravar os resultados do demo.\n");
        goto cleanup;
    }
    printf("Agent cycle checkpoint demo OK: %s\n", output_directory);
    exit_code = equivalent ? 0 : 1;

cleanup:
    free(continuous_values);
    free(ready_values);
    free(pending_values);
    checkpoint_demo_destroy(&continuous);
    checkpoint_demo_destroy(&ready_source);
    checkpoint_demo_destroy(&ready_resumed);
    checkpoint_demo_destroy(&pending_source);
    checkpoint_demo_destroy(&pending_resumed);
    return exit_code;
}
