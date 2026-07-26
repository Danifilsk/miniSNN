#include <stdio.h>
#include <string.h>

#include "app_filesystem.h"
#include "action_decoding_demo_config.h"

#define DEMO_TICKS 4U
#define DEMO_OUTPUT_ROOT "results/scenarios"
#define DEMO_DRIVE_CURRENT 4000.0

#define ensure_directory app_filesystem_ensure_directory
#define copy_file app_filesystem_copy_file

static const char *mode_name(MiniSNNActionDecodingMode mode)
{
    if (mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
        return "population_rate";
    if (mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
        return "bipolar_difference";
    if (mode == MINISNN_ACTION_DECODING_THRESHOLD)
        return "threshold";
    return "wta_member";
}

static int drive_range(MiniSNN *network, uint32_t start, uint32_t count,
                       double current)
{
    for (uint32_t neuron = start; neuron < start + count; neuron++)
    {
        if (!minisnn_set_input(network, (int)neuron, current))
            return 0;
    }
    return 1;
}

/* The demo chooses only abstract protocol cases; mappings still control all ranges. */
static int apply_demo_drive(MiniSNN *network,
                            const ActionDecodingDemoConfig *config,
                            uint32_t tick, uint32_t brain_step)
{
    minisnn_clear_inputs(network);
    if (tick == 0U)
        return 1;
    for (uint32_t index = 0U; index < config->mapping_count; index++)
    {
        const ActionDecodingDemoMapping *mapping = &config->mappings[index];
        const MiniSNNActionDecodingSpec *spec = &mapping->spec;
        int drive_primary = 0;
        int drive_secondary = 0;

        if (tick == 1U)
        {
            if (spec->mode != MINISNN_ACTION_DECODING_WTA_MEMBER)
                drive_primary = 1;
            else
            {
                uint32_t smallest_id = spec->action_channel_id;
                for (uint32_t other = 0U; other < config->mapping_count; other++)
                {
                    const MiniSNNActionDecodingSpec *candidate =
                        &config->mappings[other].spec;
                    if (candidate->mode == MINISNN_ACTION_DECODING_WTA_MEMBER &&
                        candidate->competition_group_id == spec->competition_group_id &&
                        candidate->action_channel_id < smallest_id)
                        smallest_id = candidate->action_channel_id;
                }
                drive_primary = spec->action_channel_id == smallest_id;
            }
            drive_secondary = 0;
        }
        else if (tick == 2U)
        {
            drive_primary = spec->mode == MINISNN_ACTION_DECODING_WTA_MEMBER;
        }
        else if (tick == 3U && brain_step + 1U == config->brain_steps_per_tick)
        {
            drive_primary = spec->mode == MINISNN_ACTION_DECODING_POPULATION_RATE;
        }
        if (drive_primary && !drive_range(network, spec->primary_neuron_start,
                                          spec->primary_neuron_count,
                                          DEMO_DRIVE_CURRENT))
            return 0;
        if (drive_secondary && !drive_range(network, spec->secondary_neuron_start,
                                            spec->secondary_neuron_count,
                                            DEMO_DRIVE_CURRENT))
            return 0;
    }
    return 1;
}

static int write_report(const char *filename, const ActionDecodingDemoConfig *config,
                        uint64_t schema_signature, uint64_t mapping_signature,
                        uint64_t contract_signature)
{
    FILE *file = fopen(filename, "wb");
    int failed = 0;
    if (file == NULL)
        return 0;
    if (fprintf(file, "<!doctype html><html><head><meta charset=\"utf-8\">"
                      "<title>Action decoding demo</title></head><body>"
                      "<h1>Action decoding demo</h1><p>model: %s; neurons: %u; "
                      "brain steps per tick: %u</p><h2>Action schema</h2>"
                      "<table><tr><th>id</th><th>channel</th><th>range</th><th>default</th></tr>",
                minisnn_neuron_model_name(config->neuron_model), config->neuron_count,
                config->brain_steps_per_tick) < 0)
        failed = 1;
    for (uint32_t index = 0U; !failed && index < config->action_count; index++)
    {
        const ActionDecodingDemoAction *action = &config->actions[index];
        failed = fprintf(file, "<tr><td>%u</td><td>%s</td><td>[%.17g, %.17g]</td>"
                               "<td>%.17g</td></tr>", action->id, action->name,
                         action->minimum, action->maximum, action->default_value) < 0;
    }
    if (!failed && fprintf(file, "</table><h2>Mappings</h2><table><tr><th>channel</th>"
                                 "<th>mode</th><th>primary range</th><th>secondary range</th>"
                                 "<th>parameters</th></tr>") < 0)
        failed = 1;
    for (uint32_t index = 0U; !failed && index < config->mapping_count; index++)
    {
        const ActionDecodingDemoMapping *mapping = &config->mappings[index];
        const MiniSNNActionDecodingSpec *spec = &mapping->spec;
        failed = fprintf(file, "<tr><td>%s</td><td>%s</td><td>%u-%u</td><td>%u-%u</td>"
                               "<td>group=%u</td></tr>",
                         config->actions[mapping->action_index].name, mode_name(spec->mode),
                         spec->primary_neuron_start,
                         spec->primary_neuron_start + spec->primary_neuron_count - 1U,
                         spec->secondary_neuron_start,
                         spec->secondary_neuron_count == 0U ? 0U :
                             spec->secondary_neuron_start + spec->secondary_neuron_count - 1U,
                         spec->competition_group_id) < 0;
    }
    if (!failed && fprintf(file, "</table><h2>Signatures</h2><ul><li>schema: %llu</li>"
                                 "<li>mapping: %llu</li><li>contract: %llu</li></ul>"
                                 "<h2>Observed cases</h2><p>The run includes silence/defaults, a "
                                 "population-rate response, bipolar activity, a threshold, a WTA "
                                 "winner, an ID-tie, and a late readout step. The decoder creates "
                                 "abstract numeric intentions only; their external meaning belongs to "
                                 "a later integration layer.</p><p><a href=\"action_decoding_activity.csv\">"
                                 "activity CSV</a> | <a href=\"action_decoding_trace.csv\">trace CSV</a> | "
                                 "<a href=\"action_decoder.txt\">decoder file</a> | "
                                 "<a href=\"config_source.ini\">source configuration</a> | "
                                 "<a href=\"config_used.ini\">effective configuration</a></p>"
                                 "</body></html>\n", (unsigned long long)schema_signature,
                         (unsigned long long)mapping_signature,
                         (unsigned long long)contract_signature) < 0)
        failed = 1;
    if (fclose(file) != 0)
        failed = 1;
    return !failed;
}

int main(int argc, char **argv)
{
    const char *config_path = argc == 2 ? argv[1] : "configs/action_decoding_demo.ini";
    ActionDecodingDemoConfig config;
    MiniSNNActionChannelSpec channels[ACTION_DECODING_DEMO_MAX_CHANNELS];
    MiniSNNActionDecodingSpec mappings[ACTION_DECODING_DEMO_MAX_MAPPINGS];
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = NULL;
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNNeuralActivityFrame activity = {0};
    MiniSNNActionFrame action = {0};
    MiniSNNActionDecodingDiagnostics diagnostics = {0};
    MiniSNN *network = NULL;
    MiniSNNConfig network_config;
    FILE *activity_file = NULL;
    FILE *trace_file = NULL;
    FILE *summary_file = NULL;
    char error_message[256];
    char output_dir[256];
    char source_path[320];
    char used_path[320];
    char activity_path[320];
    char trace_path[320];
    char summary_path[320];
    char report_path[320];
    char decoder_path[320];
    int exit_code = 1;

    if (argc > 2)
    {
        printf("Uso: action_decoding_demo.exe [configs/action_decoding_demo.ini]\n");
        goto cleanup;
    }
    if (!action_decoding_demo_config_load_file(config_path, &config, error_message,
                                                sizeof(error_message)))
    {
        printf("Erro ao carregar configuracao: %s.\n", error_message);
        goto cleanup;
    }
    if (snprintf(output_dir, sizeof(output_dir), "%s/%s", DEMO_OUTPUT_ROOT,
                 config.run_name) < 0 ||
        snprintf(source_path, sizeof(source_path), "%s/config_source.ini", output_dir) < 0 ||
        snprintf(used_path, sizeof(used_path), "%s/config_used.ini", output_dir) < 0 ||
        snprintf(activity_path, sizeof(activity_path), "%s/action_decoding_activity.csv", output_dir) < 0 ||
        snprintf(trace_path, sizeof(trace_path), "%s/action_decoding_trace.csv", output_dir) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/action_decoding_summary.txt", output_dir) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/action_decoding_report.html", output_dir) < 0 ||
        snprintf(decoder_path, sizeof(decoder_path), "%s/action_decoder.txt", output_dir) < 0)
    {
        printf("Erro ao montar caminhos de saida.\n");
        goto cleanup;
    }
    if (!ensure_directory("results") || !ensure_directory(DEMO_OUTPUT_ROOT) ||
        !ensure_directory(output_dir) || !copy_file(config_path, source_path))
    {
        printf("Erro ao preparar a pasta de resultados.\n");
        goto cleanup;
    }
    for (uint32_t index = 0U; index < config.action_count; index++)
    {
        channels[index].id = config.actions[index].id;
        channels[index].name = config.actions[index].name;
        channels[index].minimum = config.actions[index].minimum;
        channels[index].maximum = config.actions[index].maximum;
        channels[index].default_value = config.actions[index].default_value;
    }
    for (uint32_t index = 0U; index < config.mapping_count; index++)
        mappings[index] = config.mappings[index].spec;
    schema = minisnn_action_schema_create(channels, config.action_count, &io_error);
    decoder = minisnn_action_decoder_create(schema, mappings, config.mapping_count,
                                            config.neuron_count,
                                            config.brain_steps_per_tick, &decoder_error);
    if (schema == NULL || decoder == NULL ||
        !minisnn_neural_activity_frame_init(&activity, config.neuron_count,
                                            config.brain_steps_per_tick, &decoder_error) ||
        !minisnn_action_frame_init(&action, config.action_count) ||
        !minisnn_action_decoding_diagnostics_init(&diagnostics, config.action_count))
    {
        printf("Erro ao criar decoder: %s.\n",
               minisnn_action_decoder_error_string(decoder_error));
        goto cleanup;
    }
    if (!action_decoding_demo_config_write_file(used_path, &config, error_message,
                                                 sizeof(error_message)) ||
        !minisnn_action_decoder_write_file(decoder, decoder_path, &decoder_error))
    {
        printf("Erro ao gravar configuracao ou decoder efetivo.\n");
        goto cleanup;
    }
    network_config = minisnn_default_config();
    network_config.neuron_count = (int)config.neuron_count;
    network_config.neuron_model = config.neuron_model;
    network = minisnn_create_with_config(&network_config);
    if (network == NULL)
    {
        printf("Erro ao criar rede do demo.\n");
        goto cleanup;
    }
    activity_file = fopen(activity_path, "wb");
    trace_file = fopen(trace_path, "wb");
    summary_file = fopen(summary_path, "wb");
    if (activity_file == NULL || trace_file == NULL || summary_file == NULL ||
        fprintf(activity_file, "tick,brain_step,neuron,spike\n") < 0 ||
        fprintf(trace_file, "tick,action_channel,mode,raw_score,confidence,selected,decoded_value\n") < 0)
    {
        printf("Erro ao abrir saidas do demo.\n");
        goto cleanup;
    }
    for (uint32_t tick = 0U; tick < DEMO_TICKS; tick++)
    {
        if (!minisnn_neural_activity_frame_reset(&activity, tick, &decoder_error))
            goto cleanup;
        for (uint32_t step = 0U; step < config.brain_steps_per_tick; step++)
        {
            if (!apply_demo_drive(network, &config, tick, step) ||
                minisnn_step(network) < 0 ||
                !minisnn_neural_activity_frame_capture_step(&activity, step, network,
                                                            &decoder_error))
            {
                printf("Erro ao gerar atividade neural no tick %u.\n", tick);
                goto cleanup;
            }
            for (uint32_t neuron = 0U; neuron < config.neuron_count; neuron++)
            {
                uint8_t spike = 0U;
                if (!minisnn_neural_activity_frame_get_spike(&activity, step, neuron,
                                                              &spike, &decoder_error) ||
                    fprintf(activity_file, "%u,%u,%u,%u\n", tick, step, neuron,
                            (unsigned int)spike) < 0)
                    goto cleanup;
            }
        }
        if (!minisnn_action_decoder_decode(decoder, &activity, &action, &diagnostics))
        {
            printf("Erro ao decodificar tick %u: %s.\n", tick,
                   minisnn_action_decoder_error_string(
                       minisnn_action_decoder_last_error(decoder)));
            goto cleanup;
        }
        for (uint32_t index = 0U; index < config.action_count; index++)
        {
            const char *mode = "default";
            for (uint32_t mapping_index = 0U; mapping_index < config.mapping_count;
                 mapping_index++)
            {
                if (config.mappings[mapping_index].action_index == index)
                {
                    mode = mode_name(config.mappings[mapping_index].spec.mode);
                    break;
                }
            }
            if (fprintf(trace_file, "%u,%s,%s,%.17g,%.17g,%u,%.17g\n", tick,
                        config.actions[index].name, mode, diagnostics.raw_scores[index],
                        diagnostics.confidences[index], (unsigned int)diagnostics.selected[index],
                        diagnostics.values[index]) < 0)
                goto cleanup;
        }
    }
    if (fprintf(summary_file, "action_decoding_demo\nconfig_source=%s\nmodel=%s\n"
                              "neurons=%u\nbrain_steps_per_tick=%u\naction_count=%u\n"
                              "mapping_count=%u\naction_schema_signature=%llu\n"
                              "mapping_signature=%llu\ncontract_signature=%llu\n",
                config_path, minisnn_neuron_model_name(config.neuron_model),
                config.neuron_count, config.brain_steps_per_tick, config.action_count,
                config.mapping_count, (unsigned long long)minisnn_action_schema_signature(schema),
                (unsigned long long)minisnn_action_decoding_mapping_signature(decoder),
                (unsigned long long)minisnn_action_decoder_contract_signature(decoder)) < 0 ||
        fclose(activity_file) != 0 || fclose(trace_file) != 0 || fclose(summary_file) != 0)
        goto cleanup;
    activity_file = NULL;
    trace_file = NULL;
    summary_file = NULL;
    if (!write_report(report_path, &config, minisnn_action_schema_signature(schema),
                      minisnn_action_decoding_mapping_signature(decoder),
                      minisnn_action_decoder_contract_signature(decoder)))
    {
        printf("Erro ao escrever relatorio HTML.\n");
        goto cleanup;
    }
    printf("Action decoding demo concluido em %s\n", output_dir);
    exit_code = 0;

cleanup:
    if (activity_file != NULL)
        fclose(activity_file);
    if (trace_file != NULL)
        fclose(trace_file);
    if (summary_file != NULL)
        fclose(summary_file);
    minisnn_destroy(&network);
    minisnn_action_decoding_diagnostics_destroy(&diagnostics);
    minisnn_action_frame_destroy(&action);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_schema_destroy(&schema);
    return exit_code;
}
