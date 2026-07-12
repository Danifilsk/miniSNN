#include <stdio.h>
#include <string.h>

#include "scenario_config.h"

#define TEMP_PATH "build/test_scenario_config_tmp.ini"
#define TEMP_SAVE_PATH "build/test_scenario_config_saved.ini"

static int double_close(double a, double b)
{
    double diff = a - b;

    if (diff < 0.0)
        diff = -diff;

    return diff < 0.000000001;
}

static int fail(const char *message)
{
    remove(TEMP_PATH);
    remove(TEMP_SAVE_PATH);
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
        "[connectivity]\n"
        "allow_self_connections = false\n"
        "allow_inh_to_inh = true\n"
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
        "[topology_options]\n"
        "small_world_neighbors = 4\n"
        "small_world_rewire_probability = 0.10\n"
        "feedforward_layers = 3\n"
        "\n"
        "[recording]\n"
        "record_neuron = 0\n"
        "\n"
        "[output]\n"
        "auto_unique_run = true\n"
        "history_enabled = true\n"
        "\n"
        "[diagnostics]\n"
        "level = full\n"
        "time_bin_steps = 20\n"
        "burst_z_threshold = 2.5\n"
        "min_burst_steps = 2\n"
        "isi_min_spikes = 5\n"
        "correlation_sample_size = 64\n"
        "neuron_sample_limit = 500\n"
        "sample_stride = 2\n";
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

    if (config.allow_self_connections != 0 ||
        config.allow_inh_to_inh != 1 ||
        config.small_world_neighbors != 4 ||
        config.feedforward_layers != 3 ||
        !double_close(config.small_world_rewire_probability, 0.10))
    {
        return fail("new topology option values were not loaded");
    }

    if (config.auto_unique_run != 1 || config.history_enabled != 1)
        return fail("output option values were not loaded");

    if (strcmp(config.diagnostics_level, "full") != 0 ||
        config.time_bin_steps != 20 ||
        !double_close(config.burst_z_threshold, 2.5) ||
        config.min_burst_steps != 2 ||
        config.isi_min_spikes != 5 ||
        config.correlation_sample_size != 64 ||
        config.neuron_sample_limit != 500 ||
        config.sample_stride != 2)
    {
        return fail("diagnostic values were not loaded");
    }

    return 1;
}

static int configs_match(
    const ScenarioConfig *a,
    const ScenarioConfig *b)
{
    return strcmp(a->run_name, b->run_name) == 0 &&
           strcmp(a->topology, b->topology) == 0 &&
           a->neurons == b->neurons &&
           double_close(a->inhibitory_fraction, b->inhibitory_fraction) &&
           double_close(a->connection_probability, b->connection_probability) &&
           a->seed == b->seed &&
           a->delay == b->delay &&
           a->max_synaptic_delay == b->max_synaptic_delay &&
           a->allow_self_connections == b->allow_self_connections &&
           a->allow_inh_to_inh == b->allow_inh_to_inh &&
           double_close(a->excitatory_weight, b->excitatory_weight) &&
           double_close(a->inhibitory_weight, b->inhibitory_weight) &&
           a->source_count == b->source_count &&
           double_close(a->input_current, b->input_current) &&
           a->steps == b->steps &&
           double_close(a->dt, b->dt) &&
           double_close(a->tau, b->tau) &&
           double_close(a->v_rest, b->v_rest) &&
           double_close(a->v_reset, b->v_reset) &&
           double_close(a->v_threshold, b->v_threshold) &&
           double_close(a->resistance, b->resistance) &&
           double_close(a->synaptic_decay, b->synaptic_decay) &&
           a->small_world_neighbors == b->small_world_neighbors &&
           double_close(
               a->small_world_rewire_probability,
               b->small_world_rewire_probability) &&
           a->feedforward_layers == b->feedforward_layers &&
           a->record_neuron == b->record_neuron &&
           a->auto_unique_run == b->auto_unique_run &&
           a->history_enabled == b->history_enabled &&
           strcmp(a->diagnostics_level, b->diagnostics_level) == 0 &&
           a->time_bin_steps == b->time_bin_steps &&
           double_close(a->burst_z_threshold, b->burst_z_threshold) &&
           a->min_burst_steps == b->min_burst_steps &&
           a->isi_min_spikes == b->isi_min_spikes &&
           a->correlation_sample_size == b->correlation_sample_size &&
           a->neuron_sample_limit == b->neuron_sample_limit &&
           a->sample_stride == b->sample_stride;
}

static int check_save_and_reload(void)
{
    ScenarioConfig config;
    ScenarioConfig loaded;
    char error[256];

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "saved_parser_demo");
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 5;
    config.inhibitory_fraction = 0.0;
    config.connection_probability = 1.0;
    config.source_count = 1;
    config.steps = 50;
    config.allow_self_connections = 1;
    config.allow_inh_to_inh = 0;
    config.small_world_neighbors = 2;
    config.small_world_rewire_probability = 0.35;
    config.feedforward_layers = 2;
    config.auto_unique_run = 1;
    config.history_enabled = 0;
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "full");

    if (!scenario_config_save_file(
            TEMP_SAVE_PATH,
            &config,
            error,
            sizeof(error)))
    {
        printf("Unexpected save error: %s\n", error);
        return fail("could not save valid config");
    }

    if (!scenario_config_load_file(
            TEMP_SAVE_PATH,
            &loaded,
            error,
            sizeof(error)))
    {
        printf("Unexpected reload error: %s\n", error);
        return fail("could not reload saved config");
    }

    if (!configs_match(&config, &loaded))
        return fail("saved and reloaded configs differ");

    remove(TEMP_SAVE_PATH);
    return 1;
}

int main(void)
{
    ScenarioConfig legacy_config;
    ScenarioConfig empty_config;
    char error[256];

    if (!check_default_config_is_valid())
        return 1;

    if (!check_valid_file_loads_correctly())
        return 1;

    if (scenario_config_load_file(
            "build/file_that_does_not_exist.ini",
            &empty_config,
            error,
            sizeof(error)))
    {
        return fail("missing file was accepted");
    }

    if (!load_text_expect_success("", &empty_config))
        return fail("empty file did not use valid defaults");

    if (!load_text_expect_success(
            "run_name = sectionless\ntopology = chain\nneurons = 2\nsource_count = 1\nrecord_neuron = 0\n",
            &empty_config))
    {
        return fail("sectionless compatible config was rejected");
    }

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
            "run_name = invalid:name\n",
            "Windows-invalid run_name was accepted") ||
        !load_text_expect_failure(
            "run_name = \n",
            "empty run_name was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "dt = NaN\n",
            "NaN was accepted") ||
        !load_text_expect_failure(
            "tau = inf\n",
            "positive infinity was accepted") ||
        !load_text_expect_failure(
            "input_current = -inf\n",
            "negative infinity was accepted") ||
        !load_text_expect_failure(
            "connection_probability = 1.1\n",
            "out-of-range probability was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "seed = -1\n",
            "negative seed was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "topology = small_world\nneurons = 20\nsmall_world_neighbors = 3\n",
            "odd small_world_neighbors was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "topology = small_world\nneurons = 20\nsmall_world_rewire_probability = 1.5\n",
            "invalid small_world_rewire_probability was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "topology = feedforward\nneurons = 20\nfeedforward_layers = 1\n",
            "too-small feedforward_layers was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "auto_unique_run = talvez\n",
            "invalid auto_unique_run was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "history_enabled = talvez\n",
            "invalid history_enabled was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_failure(
            "level = expensive\n",
            "invalid diagnostics level was accepted") ||
        !load_text_expect_failure(
            "time_bin_steps = 0\n",
            "invalid time_bin_steps was accepted") ||
        !load_text_expect_failure(
            "correlation_sample_size = 0\n",
            "invalid correlation_sample_size was accepted") ||
        !load_text_expect_failure(
            "neuron_sample_limit = 0\n",
            "invalid neuron_sample_limit was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_success(
            "run_name = legacy_demo\ntopology = chain\nneurons = 2\nsource_count = 1\nrecord_neuron = 0\n",
            &legacy_config) ||
        strcmp(legacy_config.diagnostics_level, "off") != 0)
    {
        return fail("legacy config did not default diagnostics to off");
    }

    if (!check_save_and_reload())
        return 1;

    printf("Scenario configuration validation OK\n");
    return 0;
}
