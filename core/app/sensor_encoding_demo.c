#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "minisnn.h"
#include "sensor_encoding_demo_config.h"

#define DEMO_TICKS 3U
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
    int copy_failed = 0;

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
            copy_failed = 1;
            break;
        }
    }
    if (ferror(source))
        copy_failed = 1;
    if (fclose(source) != 0)
        copy_failed = 1;
    if (fclose(destination) != 0)
        copy_failed = 1;
    return !copy_failed;
}

static const char *mode_name(MiniSNNSensorEncodingMode mode)
{
    if (mode == MINISNN_SENSOR_ENCODING_LINEAR_CURRENT)
        return "linear_current";
    if (mode == MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT)
        return "bipolar_current";
    return "deterministic_rate";
}

static double normalized_value(double value, double minimum, double maximum)
{
    return minimum == maximum ? 0.0 : (value - minimum) / (maximum - minimum);
}

static double sample_value(const SensorEncodingDemoSensor *sensor, uint32_t tick)
{
    const double fraction = tick == 0U ? 0.0 : (tick == 1U ? 0.5 : 1.0);

    return sensor->minimum + (sensor->maximum - sensor->minimum) * fraction;
}

static int write_report(const char *filename, const SensorEncodingDemoConfig *config,
                        uint64_t schema_signature, uint64_t mapping_signature,
                        uint64_t contract_signature)
{
    FILE *file = fopen(filename, "wb");
    int write_failed = 0;

    if (file == NULL)
        return 0;
    if (fprintf(file,
                "<!doctype html><html><head><meta charset=\"utf-8\"><title>"
                "Sensor encoding demo</title></head><body><h1>Sensor encoding demo</h1>"
                "<p>model: %s; neurons: %u; brain steps per tick: %u</p>"
                "<h2>Schema</h2><table><tr><th>id</th><th>channel</th><th>range</th><th>default</th></tr>",
                minisnn_neuron_model_name(config->neuron_model), config->neuron_count,
                config->brain_steps_per_tick) < 0)
        write_failed = 1;
    for (uint32_t index = 0U; !write_failed && index < config->sensor_count; index++)
    {
        const SensorEncodingDemoSensor *sensor = &config->sensors[index];
        if (fprintf(file, "<tr><td>%u</td><td>%s</td><td>[%.17g, %.17g]</td><td>%.17g</td></tr>",
                    sensor->id, sensor->name, sensor->minimum, sensor->maximum,
                    sensor->default_value) < 0)
            write_failed = 1;
    }
    if (!write_failed && fprintf(file,
                                 "</table><h2>Mappings</h2><table><tr><th>channel</th>"
                                 "<th>neurons</th><th>mode</th><th>parameters</th></tr>") < 0)
        write_failed = 1;
    for (uint32_t index = 0U; !write_failed && index < config->mapping_count; index++)
    {
        const SensorEncodingDemoMapping *mapping = &config->mappings[index];
        const MiniSNNSensorEncodingSpec *spec = &mapping->spec;
        const char *name = config->sensors[mapping->sensor_index].name;

        if (spec->mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE)
            write_failed = fprintf(file,
                                   "<tr><td>%s</td><td>%u-%u</td><td>%s</td>"
                                   "<td>pulse=%.17g; rate=%.17g pulses/step; phase=%u/1000</td></tr>",
                                   name, spec->target_neuron_start,
                                   spec->target_neuron_start + spec->target_neuron_count - 1U,
                                   mode_name(spec->mode), spec->pulse_current,
                                   spec->maximum_rate, spec->phase_offset) < 0;
        else
            write_failed = fprintf(file,
                                   "<tr><td>%s</td><td>%u-%u</td><td>%s</td>"
                                   "<td>gain=%.17g; bias=%.17g</td></tr>",
                                   name, spec->target_neuron_start,
                                   spec->target_neuron_start + spec->target_neuron_count - 1U,
                                   mode_name(spec->mode), spec->gain, spec->bias) < 0;
    }
    if (!write_failed && fprintf(file,
                                 "</table><h2>Examples</h2><p>Every configured sensor is sampled at "
                                 "minimum, midpoint and maximum. Constant channels normalize to zero. "
                                 "Rate pulses are deterministic and distributed across neural steps.</p>"
                                 "<h2>Signatures</h2><ul><li>schema: %llu</li><li>mapping: %llu</li>"
                                 "<li>contract: %llu</li></ul><h2>Limitations</h2><p>The encoder has no "
                                 "domain semantics, uses no PRNG, does not force neural spikes, and does "
                                 "not advance the network. It writes external currents; the caller owns the "
                                 "neural step.</p><p><a href=\"sensor_encoding_trace.csv\">trace CSV</a> | "
                                 "<a href=\"sensor_encoding_summary.txt\">summary</a> | "
                                 "<a href=\"config_source.ini\">source configuration</a> | "
                                 "<a href=\"config_used.ini\">effective configuration</a></p></body></html>\n",
                                 (unsigned long long)schema_signature,
                                 (unsigned long long)mapping_signature,
                                 (unsigned long long)contract_signature) < 0)
        write_failed = 1;
    if (fclose(file) != 0)
        write_failed = 1;
    return !write_failed;
}

int main(int argc, char **argv)
{
    const char *config_path = argc == 2 ? argv[1] : "configs/sensor_encoding_demo.ini";
    SensorEncodingDemoConfig config;
    MiniSNNSensorChannelSpec channels[SENSOR_ENCODING_DEMO_MAX_CHANNELS];
    MiniSNNSensorEncodingSpec mappings[SENSOR_ENCODING_DEMO_MAX_MAPPINGS];
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = NULL;
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNNeuralInputFrame input = {0};
    MiniSNN *network = NULL;
    MiniSNNConfig network_config;
    FILE *trace = NULL;
    FILE *summary = NULL;
    char error_message[256];
    char output_dir[256];
    char config_source_path[320];
    char config_used_path[320];
    char trace_path[320];
    char summary_path[320];
    char report_path[320];
    int exit_code = 1;

    if (argc > 2)
    {
        printf("Uso: sensor_encoding_demo.exe [configs/sensor_encoding_demo.ini]\n");
        goto cleanup;
    }
    if (!sensor_encoding_demo_config_load_file(config_path, &config, error_message,
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
        snprintf(trace_path, sizeof(trace_path), "%s/sensor_encoding_trace.csv", output_dir) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/sensor_encoding_summary.txt", output_dir) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/sensor_encoding_report.html", output_dir) < 0)
    {
        printf("Erro ao montar caminhos de saida.\n");
        goto cleanup;
    }
    if (!ensure_directory("results") || !ensure_directory(DEMO_OUTPUT_ROOT) ||
        !ensure_directory(output_dir))
    {
        printf("Erro ao criar diretorio de resultados.\n");
        goto cleanup;
    }
    if (!copy_file(config_path, config_source_path))
    {
        printf("Erro de proveniencia da configuracao: %s.\n", error_message);
        goto cleanup;
    }
    for (uint32_t index = 0U; index < config.sensor_count; index++)
    {
        channels[index].id = config.sensors[index].id;
        channels[index].name = config.sensors[index].name;
        channels[index].minimum = config.sensors[index].minimum;
        channels[index].maximum = config.sensors[index].maximum;
        channels[index].default_value = config.sensors[index].default_value;
    }
    for (uint32_t index = 0U; index < config.mapping_count; index++)
        mappings[index] = config.mappings[index].spec;
    schema = minisnn_sensor_schema_create(channels, config.sensor_count, &io_error);
    encoder = minisnn_sensor_encoder_create(schema, mappings, config.mapping_count,
                                            config.neuron_count,
                                            config.brain_steps_per_tick, &encoder_error);
    if (schema == NULL || encoder == NULL ||
        !minisnn_sensor_frame_init(&sensor, config.sensor_count) ||
        !minisnn_neural_input_frame_init(&input, config.neuron_count,
                                          config.brain_steps_per_tick, &encoder_error))
    {
        printf("Erro ao criar encoder: %s.\n",
               minisnn_sensor_encoder_error_string(encoder_error));
        goto cleanup;
    }
    if (!sensor_encoding_demo_config_write_file(config_used_path, &config,
                                                 error_message, sizeof(error_message)))
    {
        printf("Erro ao gravar configuracao efetiva: %s.\n", error_message);
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
    trace = fopen(trace_path, "wb");
    summary = fopen(summary_path, "wb");
    if (trace == NULL || summary == NULL ||
        fprintf(trace, "tick,sensor_channel,sensor_value,normalized_value,mode,brain_step,target_neuron,encoded_current\n") < 0)
    {
        printf("Erro ao abrir saidas do demo.\n");
        goto cleanup;
    }
    for (uint32_t tick = 0U; tick < DEMO_TICKS; tick++)
    {
        for (uint32_t sensor_index = 0U; sensor_index < config.sensor_count;
             sensor_index++)
            sensor.values[sensor_index] = sample_value(&config.sensors[sensor_index], tick);
        sensor.tick = tick;
        if (!minisnn_sensor_encoder_encode_frame(encoder, &sensor, &input))
        {
            printf("Erro ao codificar tick %u: %s.\n", tick,
                   minisnn_sensor_encoder_error_string(
                       minisnn_sensor_encoder_last_error(encoder)));
            goto cleanup;
        }
        for (uint32_t step = 0U; step < config.brain_steps_per_tick; step++)
        {
            if (!minisnn_neural_input_frame_apply_step(&input, step, network,
                                                        &encoder_error) ||
                minisnn_step(network) < 0)
            {
                printf("Erro ao aplicar entrada neural: %s.\n",
                       minisnn_sensor_encoder_error_string(encoder_error));
                goto cleanup;
            }
            for (uint32_t mapping_index = 0U; mapping_index < config.mapping_count;
                 mapping_index++)
            {
                const SensorEncodingDemoMapping *mapping = &config.mappings[mapping_index];
                const SensorEncodingDemoSensor *channel =
                    &config.sensors[mapping->sensor_index];
                const MiniSNNSensorEncodingSpec *spec = &mapping->spec;
                double normalized = normalized_value(sensor.values[mapping->sensor_index],
                                                     channel->minimum, channel->maximum);

                for (uint32_t neuron = spec->target_neuron_start;
                     neuron < spec->target_neuron_start + spec->target_neuron_count;
                     neuron++)
                {
                    double current = 0.0;
                    if (!minisnn_neural_input_frame_get_current(&input, step, neuron,
                                                                &current, &encoder_error) ||
                        fprintf(trace, "%u,%s,%.6f,%.6f,%s,%u,%u,%.6f\n", tick,
                                channel->name, sensor.values[mapping->sensor_index],
                                normalized, mode_name(spec->mode), step, neuron,
                                current) < 0)
                        goto cleanup;
                }
            }
        }
    }
    {
        int write_failed = fprintf(
            summary,
            "sensor_encoding_demo\nconfig_source=%s\nmodel=%s\nneurons=%u\n"
            "brain_steps_per_tick=%u\nsensor_count=%u\nmapping_count=%u\n"
            "sensor_schema_signature=%llu\nmapping_signature=%llu\n"
            "contract_signature=%llu\nrate_unit=pulses_per_neural_step\n",
            config_path, minisnn_neuron_model_name(config.neuron_model), config.neuron_count,
            config.brain_steps_per_tick, config.sensor_count, config.mapping_count,
            (unsigned long long)minisnn_sensor_schema_signature(schema),
            (unsigned long long)minisnn_sensor_encoding_mapping_signature(encoder),
            (unsigned long long)minisnn_sensor_encoder_contract_signature(encoder)) < 0;

        if (fclose(trace) != 0)
            write_failed = 1;
        trace = NULL;
        if (fclose(summary) != 0)
            write_failed = 1;
        summary = NULL;
        if (write_failed)
            goto cleanup;
    }
    if (!write_report(report_path, &config, minisnn_sensor_schema_signature(schema),
                      minisnn_sensor_encoding_mapping_signature(encoder),
                      minisnn_sensor_encoder_contract_signature(encoder)))
    {
        printf("Erro ao escrever relatorio HTML.\n");
        goto cleanup;
    }
    printf("Sensor encoding demo concluido em %s\n", output_dir);
    exit_code = 0;

cleanup:
    if (trace != NULL)
        fclose(trace);
    if (summary != NULL)
        fclose(summary);
    minisnn_destroy(&network);
    minisnn_neural_input_frame_destroy(&input);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_schema_destroy(&schema);
    return exit_code;
}
