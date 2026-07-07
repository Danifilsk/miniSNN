#include <stdio.h>
#include <string.h>

#include "scenario_config.h"

#define TEMP_PATH "build/test_scenario_config_tmp.ini"

static int fail(const char *message)
{
    remove(TEMP_PATH);
    printf("FAIL: %s\n", message);
    return 0;
}

static int write_temp_file(const char *content)
{
    FILE *file = fopen(TEMP_PATH, "w");

    if (file == NULL)
        return 0;

    if (fputs(content, file) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static const char *valid_config_text(void)
{
    return
        "# comentario inicial\n"
        "[run]\n"
        "run_name = parser_demo\n"
        "\n"
        "[network]\n"
        "topology = random_balanced\n"
        "neurons = 20\n"
        "inhibitory_fraction = 0.20\n"
        "connection_probability = 0.25\n"
        "seed = 1\n"
        "delay = 1\n"
        "max_synaptic_delay = 8\n"
        "\n"
        "[weights]\n"
        "excitatory_weight = 200.0\n"
        "inhibitory_weight = -400.0\n"
        "\n"
        "[input]\n"
        "source_count = 3\n"
        "input_current = 20.0\n"
        "\n"
        "[simulation]\n"
        "steps = 1000\n"
        "dt = 0.1\n"
        "tau = 20.0\n"
        "v_rest = -65.0\n"
        "v_reset = -65.0\n"
        "v_threshold = -50.0\n"
        "resistance = 1.0\n"
        "synaptic_decay = 0.95\n"
        "\n"
        "[recording]\n"
        "record_neuron = 0\n";
}

static int load_text_expect_success(const char *content, ScenarioConfig *config)
{
    char error[256];

    if (!write_temp_file(content))
        return fail("could not write temp scenario");

    if (!scenario_config_load_file(
            TEMP_PATH,
            config,
            error,
            sizeof(error)))
    {
        printf("Unexpected error: %s\n", error);
        return fail("valid config was rejected");
    }

    remove(TEMP_PATH);
    return 1;
}

static int load_text_expect_failure(const char *content, const char *label)
{
    ScenarioConfig config;
    char error[256];

    if (!write_temp_file(content))
        return fail("could not write temp invalid scenario");

    if (scenario_config_load_file(
            TEMP_PATH,
            &config,
            error,
            sizeof(error)))
    {
        remove(TEMP_PATH);
        return fail(label);
    }

    remove(TEMP_PATH);
    return 1;
}

static int check_default_config_is_valid(void)
{
    ScenarioConfig config;
    char error[256];

    scenario_config_default(&config);

    if (!scenario_config_validate(&config, error, sizeof(error)))
    {
        printf("Unexpected default error: %s\n", error);
        return fail("default config was invalid");
    }

    return 1;
}

static int check_valid_file_loads_correctly(void)
{
    ScenarioConfig config;

    if (!load_text_expect_success(valid_config_text(), &config))
        return 0;

    if (strcmp(config.run_name, "parser_demo") != 0)
        return fail("run_name was not loaded");

    if (strcmp(config.topology, "random_balanced") != 0)
        return fail("topology was not loaded");

    if (config.neurons != 20 ||
        config.source_count != 3 ||
        config.record_neuron != 0 ||
        config.delay != 1 ||
        config.max_synaptic_delay != 8)
    {
        return fail("integer values were not loaded");
    }

    if (config.inhibitory_fraction < 0.199 ||
        config.inhibitory_fraction > 0.201 ||
        config.connection_probability < 0.249 ||
        config.connection_probability > 0.251 ||
        config.excitatory_weight != 200.0 ||
        config.inhibitory_weight != -400.0)
    {
        return fail("numeric values were not loaded");
    }

    return 1;
}

int main(void)
{
    if (!check_default_config_is_valid())
        return 1;

    if (!check_valid_file_loads_correctly())
        return 1;

    if (!load_text_expect_failure(
            "topology = bad_topology\n",
            "invalid topology was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "unknown_key = 1\n",
            "unknown key was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "neurons = 20\nneurons = 30\n",
            "duplicate key was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "neurons = 0\n",
            "invalid neurons value was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "excitatory_weight = -1.0\n",
            "negative excitatory weight was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "inhibitory_weight = 1.0\n",
            "positive inhibitory weight was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "delay = 9\nmax_synaptic_delay = 8\n",
            "delay above max was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "run_name = invalid name\n",
            "invalid run_name was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "seed = -1\n",
            "negative seed was accepted"))
    {
        return 1;
    }

    printf("Scenario configuration validation OK\n");
    return 0;
}
