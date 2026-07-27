#include <stdio.h>
#include <string.h>

#include "app_filesystem.h"
#include "minisnn.h"
#include "scenario_config.h"

#define CORRUPTION_DIRECTORY "build/tests/d1_b_corruption"
#define CONFIG_PATH CORRUPTION_DIRECTORY "/scenario.ini"
#define SCHEMA_PATH CORRUPTION_DIRECTORY "/sensor.schema"

static int fail(const char *message)
{
    fprintf(stderr, "D1-B corruption test failed: %s\n", message);
    app_filesystem_remove_tree(CORRUPTION_DIRECTORY);
    return 0;
}

static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    int written;
    int closed;

    if (file == NULL)
        return 0;
    written = fputs(text, file) >= 0;
    closed = fclose(file) == 0;
    return written && closed;
}

static int config_is_unchanged(const ScenarioConfig *left,
                               const ScenarioConfig *right)
{
    return left->neurons == right->neurons && left->steps == right->steps &&
        left->seed == right->seed && left->neuron_model == right->neuron_model &&
        strcmp(left->run_name, right->run_name) == 0 &&
        strcmp(left->topology, right->topology) == 0;
}

static int test_scenario_parser_mutations(void)
{
    static const char *const valid =
        "[run]\r\nrun_name = d1_b_valid\r\n"
        "[network]\r\ntopology = chain\r\nneurons = 3\r\n"
        "inhibitory_fraction = 0\r\nconnection_probability = 1\r\n"
        "seed = 1\r\ndelay = 1\r\nmax_synaptic_delay = 2\r\n"
        "[weights]\r\nexcitatory_weight = 20\r\ninhibitory_weight = -20\r\n"
        "[input]\r\nsource_count = 1\r\ninput_current = 20\r\n"
        "[simulation]\r\nsteps = 10\r\ndt = 0.1\r\ntau = 20\r\n"
        "v_rest = -65\r\nv_reset = -65\r\nv_threshold = -50\r\n"
        "resistance = 1\r\nsynaptic_decay = 0.95\r\n"
        "[recording]\r\nrecord_neuron = 0";
    static const char *const invalid_values[] =
    {
        "[network]\nneurons = 12abc\n",
        "[input]\ninput_current = nan\n",
        "[simulation]\nsteps = 1e99999\n",
        "[network]\nneurons = 3\nneurons = 4\n",
        "[unknown]\nunknown_key = 1\n",
        "[network]\nseed = -1\n",
        "[network]\nconnection_probability = inf\n"
    };
    ScenarioConfig config;
    ScenarioConfig before;
    char error[256];

    if (!write_text(CONFIG_PATH, valid) ||
        !scenario_config_load_file(CONFIG_PATH, &config, error, sizeof(error)) ||
        config.neurons != 3 || config.steps != 10)
        return fail("configuracao CRLF valida sem newline final");

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "%s", "preserve_me");
    before = config;
    for (size_t index = 0U;
         index < sizeof(invalid_values) / sizeof(invalid_values[0]); index++)
    {
        if (!write_text(CONFIG_PATH, invalid_values[index]) ||
            scenario_config_load_file(CONFIG_PATH, &config, error, sizeof(error)) ||
            error[0] == '\0' || !config_is_unchanged(&config, &before))
            return fail("parser aceitou mutacao ou publicou estado parcial");
    }
    if (scenario_config_load_file("build/tests/d1_b_corruption/missing.ini",
                                  &config, error, sizeof(error)) ||
        error[0] == '\0' || !config_is_unchanged(&config, &before))
        return fail("arquivo ausente alterou configuracao viva");

    {
        char long_line[700];
        memset(long_line, 'x', sizeof(long_line));
        long_line[0] = 'a';
        long_line[1] = '=';
        long_line[sizeof(long_line) - 2U] = '\n';
        long_line[sizeof(long_line) - 1U] = '\0';
        if (!write_text(CONFIG_PATH, long_line) ||
            scenario_config_load_file(CONFIG_PATH, &config, error, sizeof(error)) ||
            !config_is_unchanged(&config, &before))
            return fail("linha excessiva nao foi rejeitada atomicamente");
    }
    return 1;
}

static int test_schema_mutations(void)
{
    const MiniSNNSensorChannelSpec channels[] =
    {
        {10U, "signal one", 0.0, 1.0, 0.25}
    };
    static const char *const mutations[] =
    {
        "",
        "minisnn_agent_io_sensor_schema_v1\n",
        "wrong_version\nchannel_count=1\n",
        "minisnn_agent_io_sensor_schema_v1\nchannel_count=999999\n",
        "minisnn_agent_io_sensor_schema_v1\nchannel_count=1\nchannel=10|bad%7f|0000000000000000|3ff0000000000000|3fd0000000000000\n",
        "minisnn_agent_io_sensor_schema_v1\nchannel_count=1\nchannel=10|signal|0000000000000000|3ff0000000000000|3fd0000000000000\nextra\n"
    };
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *schema;
    MiniSNNSensorSchema *loaded;
    uint64_t signature;

    schema = minisnn_sensor_schema_create(channels, 1U, &error);
    if (schema == NULL || !minisnn_sensor_schema_write_file(schema, SCHEMA_PATH, &error))
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("schema valido nao foi criado");
    }
    signature = minisnn_sensor_schema_signature(schema);
    loaded = minisnn_sensor_schema_read_file(SCHEMA_PATH, &error);
    if (loaded == NULL || minisnn_sensor_schema_signature(loaded) != signature)
    {
        minisnn_sensor_schema_destroy(&schema);
        minisnn_sensor_schema_destroy(&loaded);
        return fail("schema valido nao fez round-trip");
    }
    minisnn_sensor_schema_destroy(&loaded);

    for (size_t index = 0U; index < sizeof(mutations) / sizeof(mutations[0]); index++)
    {
        if (!write_text(SCHEMA_PATH, mutations[index]) ||
            minisnn_sensor_schema_read_file(SCHEMA_PATH, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_FORMAT ||
            minisnn_sensor_schema_signature(schema) != signature)
        {
            minisnn_sensor_schema_destroy(&schema);
            return fail("schema corrompido foi aceito ou alterou objeto vivo");
        }
    }
    minisnn_sensor_schema_destroy(&schema);
    return 1;
}

int main(void)
{
    if (!app_filesystem_ensure_directory_tree(CORRUPTION_DIRECTORY))
        return fail("nao foi possivel criar diretorio temporario");
    if (!test_scenario_parser_mutations() || !test_schema_mutations())
        return 1;
    app_filesystem_remove_tree(CORRUPTION_DIRECTORY);
    printf("D1-B deterministic corruption and parser validation OK\n");
    return 0;
}
