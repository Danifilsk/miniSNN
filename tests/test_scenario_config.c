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
        "sample_stride = 2\n"
        "\n"
        "[plasticity]\n"
        "enabled = true\n"
        "rule = stdp_pair_trace\n"
        "a_plus = 0.5\n"
        "a_minus = 0.25\n"
        "tau_plus = 2.0\n"
        "tau_minus = 3.0\n"
        "trace_increment = 1.5\n"
        "weight_min = 0.0\n"
        "weight_max = 250.0\n"
        "record_weights = true\n"
        "record_history = false\n"
        "record_interval_steps = 5\n"
        "record_connection_limit = 32\n";
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

    if (!config.plasticity_enabled ||
        strcmp(config.plasticity_rule, "stdp_pair_trace") != 0 ||
        !double_close(config.plasticity_a_plus, 0.5) ||
        !double_close(config.plasticity_a_minus, 0.25) ||
        !double_close(config.plasticity_tau_plus, 2.0) ||
        !double_close(config.plasticity_tau_minus, 3.0) ||
        !double_close(config.plasticity_trace_increment, 1.5) ||
        !double_close(config.plasticity_weight_min, 0.0) ||
        !double_close(config.plasticity_weight_max, 250.0) ||
        !config.plasticity_record_weights ||
        config.plasticity_record_history ||
        config.plasticity_record_interval_steps != 5 ||
        config.plasticity_record_connection_limit != 32)
    {
        return fail("plasticity values were not loaded");
    }

    return 1;
}

static int configs_match(
    const ScenarioConfig *a,
    const ScenarioConfig *b)
{
    int event_index;

    if (a->reward_event_count != b->reward_event_count)
        return 0;
    for (event_index = 0; event_index < a->reward_event_count; event_index++)
    {
        if (a->reward_events[event_index].index != b->reward_events[event_index].index ||
            a->reward_events[event_index].step != b->reward_events[event_index].step ||
            !double_close(a->reward_events[event_index].value,
                          b->reward_events[event_index].value))
        {
            return 0;
        }
    }

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
           a->sample_stride == b->sample_stride &&
           a->plasticity_enabled == b->plasticity_enabled &&
           strcmp(a->plasticity_rule, b->plasticity_rule) == 0 &&
           strcmp(a->plasticity_learning_mode,
               b->plasticity_learning_mode) == 0 &&
           double_close(a->plasticity_a_plus, b->plasticity_a_plus) &&
           double_close(a->plasticity_a_minus, b->plasticity_a_minus) &&
           double_close(a->plasticity_tau_plus, b->plasticity_tau_plus) &&
           double_close(a->plasticity_tau_minus, b->plasticity_tau_minus) &&
           double_close(
               a->plasticity_trace_increment,
               b->plasticity_trace_increment) &&
           double_close(a->plasticity_weight_min, b->plasticity_weight_min) &&
           double_close(a->plasticity_weight_max, b->plasticity_weight_max) &&
           a->plasticity_record_weights == b->plasticity_record_weights &&
           a->plasticity_record_history == b->plasticity_record_history &&
           a->plasticity_record_interval_steps ==
               b->plasticity_record_interval_steps &&
           a->plasticity_record_connection_limit ==
               b->plasticity_record_connection_limit &&
           a->homeostasis_enabled == b->homeostasis_enabled &&
           a->homeostasis_intrinsic_enabled == b->homeostasis_intrinsic_enabled &&
           double_close(a->homeostasis_target_rate, b->homeostasis_target_rate) &&
           double_close(a->homeostasis_rate_tau, b->homeostasis_rate_tau) &&
           a->homeostasis_update_interval_steps ==
               b->homeostasis_update_interval_steps &&
           double_close(a->homeostasis_threshold_eta, b->homeostasis_threshold_eta) &&
           double_close(a->homeostasis_threshold_min, b->homeostasis_threshold_min) &&
           double_close(a->homeostasis_threshold_max, b->homeostasis_threshold_max) &&
           a->homeostasis_synaptic_scaling_enabled ==
               b->homeostasis_synaptic_scaling_enabled &&
           strcmp(a->homeostasis_scaling_target_mode,
               b->homeostasis_scaling_target_mode) == 0 &&
           double_close(a->homeostasis_scaling_eta, b->homeostasis_scaling_eta) &&
           double_close(a->homeostasis_scaling_min_factor,
               b->homeostasis_scaling_min_factor) &&
           double_close(a->homeostasis_scaling_max_factor,
               b->homeostasis_scaling_max_factor) &&
           double_close(a->homeostasis_scaling_weight_min,
               b->homeostasis_scaling_weight_min) &&
           double_close(a->homeostasis_scaling_weight_max,
               b->homeostasis_scaling_weight_max) &&
           a->homeostasis_inhibitory_gain_enabled ==
               b->homeostasis_inhibitory_gain_enabled &&
           double_close(a->homeostasis_inhibitory_gain_initial,
               b->homeostasis_inhibitory_gain_initial) &&
           double_close(a->homeostasis_inhibitory_gain_eta,
               b->homeostasis_inhibitory_gain_eta) &&
           double_close(a->homeostasis_inhibitory_gain_min,
               b->homeostasis_inhibitory_gain_min) &&
           double_close(a->homeostasis_inhibitory_gain_max,
               b->homeostasis_inhibitory_gain_max) &&
           a->homeostasis_record_history == b->homeostasis_record_history &&
           a->homeostasis_record_interval_steps ==
               b->homeostasis_record_interval_steps &&
           a->homeostasis_record_neuron_limit ==
               b->homeostasis_record_neuron_limit &&
           a->reward_enabled == b->reward_enabled &&
           strcmp(a->reward_mode, b->reward_mode) == 0 &&
           double_close(a->reward_learning_rate, b->reward_learning_rate) &&
           double_close(a->reward_eligibility_tau, b->reward_eligibility_tau) &&
           double_close(a->reward_eligibility_min, b->reward_eligibility_min) &&
           double_close(a->reward_eligibility_max, b->reward_eligibility_max) &&
           double_close(a->reward_min, b->reward_min) &&
           double_close(a->reward_max, b->reward_max) &&
           a->reward_clip == b->reward_clip &&
           a->reward_record_history == b->reward_record_history &&
           a->reward_record_interval_steps == b->reward_record_interval_steps &&
           a->reward_record_connection_limit == b->reward_record_connection_limit;
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
    config.plasticity_enabled = 1;
    snprintf(config.plasticity_learning_mode,
        sizeof(config.plasticity_learning_mode), "reward_modulated_stdp");
    config.plasticity_a_plus = 0.75;
    config.plasticity_record_history = 0;
    config.plasticity_record_interval_steps = 7;
    config.plasticity_record_connection_limit = 19;
    config.homeostasis_enabled = 1;
    config.homeostasis_target_rate = 0.1;
    config.homeostasis_synaptic_scaling_enabled = 1;
    config.homeostasis_inhibitory_gain_enabled = 1;
    config.homeostasis_record_interval_steps = 7;
    config.homeostasis_record_neuron_limit = 19;
    config.reward_enabled = 1;
    config.reward_learning_rate = 0.75;
    config.reward_eligibility_tau = 80.0;
    config.reward_record_interval_steps = 7;
    config.reward_record_connection_limit = 19;
    config.reward_event_count = 2;
    config.reward_events[0].index = 0;
    config.reward_events[0].step = 10;
    config.reward_events[0].value = 1.0;
    config.reward_events[1].index = 1;
    config.reward_events[1].step = 20;
    config.reward_events[1].value = -0.5;

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

    if (!load_text_expect_failure(
            "enabled = talvez\n",
            "invalid plasticity enabled was accepted") ||
        !load_text_expect_failure(
            "rule = triplet\n",
            "unknown plasticity rule was accepted") ||
        !load_text_expect_failure(
            "a_plus = NaN\n",
            "NaN plasticity amplitude was accepted") ||
        !load_text_expect_failure(
            "a_minus = inf\n",
            "infinite plasticity amplitude was accepted") ||
        !load_text_expect_failure(
            "tau_plus = 0\n",
            "zero tau_plus was accepted") ||
        !load_text_expect_failure(
            "tau_minus = -1\n",
            "negative tau_minus was accepted") ||
        !load_text_expect_failure(
            "weight_min = 2\nweight_max = 2\n",
            "invalid plasticity weight interval was accepted") ||
        !load_text_expect_failure(
            "record_interval_steps = 0\n",
            "zero plasticity record interval was accepted") ||
        !load_text_expect_failure(
            "record_connection_limit = 0\n",
            "zero plasticity connection limit was accepted") ||
        !load_text_expect_failure(
            "a_plus = 1\na_plus = 2\n",
            "duplicate plasticity key was accepted"))
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


    if (legacy_config.plasticity_enabled != 0)
        return fail("legacy config did not default plasticity to off");

    if (legacy_config.homeostasis_enabled != 0)
        return fail("legacy config did not default homeostasis to off");

    if (legacy_config.reward_enabled != 0 ||
        strcmp(legacy_config.plasticity_learning_mode, "direct_stdp") != 0)
    {
        return fail("legacy config did not default reward off and direct STDP");
    }

    if (!load_text_expect_success(
            "steps = 100\n[plasticity]\nenabled = true\n"
            "learning_mode = reward_modulated_stdp\n"
            "[reward]\nenabled = true\nmode = rstdp\nlearning_rate = 0.5\n"
            "eligibility_tau = 50\neligibility_min = -10\neligibility_max = 10\n"
            "reward_min = -2\nreward_max = 2\nclip_reward = true\n"
            "record_history = true\nrecord_interval_steps = 5\n"
            "record_connection_limit = 8\n"
            "[reward_events]\nevent_1 = 20,-0.5\nevent_0 = 10,1.0\n",
            &empty_config) ||
        !empty_config.reward_enabled || empty_config.reward_event_count != 2 ||
        empty_config.reward_events[0].index != 0 ||
        empty_config.reward_events[0].step != 10 ||
        empty_config.reward_events[1].index != 1 ||
        empty_config.reward_events[1].step != 20)
    {
        return fail("valid reward section/events were not loaded and sorted");
    }

    if (!load_text_expect_failure(
            "[reward]\nenabled = true\n",
            "reward enabled with plasticity off was accepted") ||
        !load_text_expect_failure(
            "[plasticity]\nenabled = true\nlearning_mode = direct_stdp\n"
            "[reward]\nenabled = true\n",
            "reward enabled with direct STDP was accepted") ||
        !load_text_expect_failure(
            "[plasticity]\nenabled = true\nlearning_mode = reward_modulated_stdp\n",
            "R-STDP with reward off was accepted") ||
        !load_text_expect_failure(
            "[reward]\nmode = dopamine\n",
            "unknown reward mode was accepted") ||
        !load_text_expect_failure(
            "[reward]\nlearning_rate = NaN\n",
            "non-finite reward learning rate was accepted") ||
        !load_text_expect_failure(
            "[reward]\neligibility_tau = 0\n",
            "zero eligibility tau was accepted") ||
        !load_text_expect_failure(
            "[reward]\neligibility_min = 0\n",
            "non-negative eligibility minimum was accepted") ||
        !load_text_expect_failure(
            "[reward]\nreward_min = 1\n",
            "positive reward minimum was accepted") ||
        !load_text_expect_failure(
            "[reward]\nclip_reward = talvez\n",
            "invalid reward boolean was accepted") ||
        !load_text_expect_failure(
            "[reward]\nrecord_interval_steps = 0\n",
            "zero reward record interval was accepted") ||
        !load_text_expect_failure(
            "[reward]\nunknown = 1\n",
            "unknown reward key was accepted") ||
        !load_text_expect_failure(
            "[reward]\nmode = rstdp\nmode = rstdp\n",
            "duplicate reward key was accepted") ||
        !load_text_expect_failure(
            "[reward_events]\nevent_0 = 10,1.0\nevent_0 = 20,-1.0\n",
            "duplicate reward event index was accepted") ||
        !load_text_expect_failure(
            "[reward_events]\nevent_1 = 10,1.0\n",
            "reward event index gap was accepted") ||
        !load_text_expect_failure(
            "[reward_events]\nevent_0 = -1,1.0\n",
            "negative reward event step was accepted") ||
        !load_text_expect_failure(
            "[reward_events]\nevent_0 = 10,NaN\n",
            "non-finite reward event was accepted") ||
        !load_text_expect_failure(
            "steps = 10\n[reward_events]\nevent_0 = 10,1.0\n",
            "reward event outside run was accepted") ||
        !load_text_expect_failure(
            "[reward_events]\nevent_0 = 10,1.0,extra\n",
            "reward event trailing text was accepted"))
    {
        return 1;
    }

    if (!load_text_expect_success(
            "run_name = legacy_threshold\ntopology = chain\nneurons = 2\n"
            "source_count = 1\nrecord_neuron = 0\nv_threshold = -61\n",
            &legacy_config))
    {
        return fail("disabled homeostasis constrained a legacy threshold");
    }

    if (!load_text_expect_success(
            "[homeostasis]\n"
            "enabled = true\n"
            "intrinsic_enabled = true\n"
            "target_rate = 0.1\n"
            "rate_tau = 50\n"
            "update_interval_steps = 5\n"
            "threshold_eta = 0.2\n"
            "threshold_min = -60\n"
            "threshold_max = -40\n"
            "synaptic_scaling_enabled = true\n"
            "scaling_target_mode = initial_incoming_sum\n"
            "scaling_eta = 0.2\n"
            "scaling_min_factor = 0.5\n"
            "scaling_max_factor = 2\n"
            "scaling_weight_min = 0\n"
            "scaling_weight_max = 500\n"
            "inhibitory_gain_enabled = true\n"
            "inhibitory_gain_initial = 1\n"
            "inhibitory_gain_eta = 0.1\n"
            "inhibitory_gain_min = 0.25\n"
            "inhibitory_gain_max = 4\n"
            "record_history = true\n"
            "record_interval_steps = 5\n"
            "record_neuron_limit = 10\n",
            &empty_config) ||
        !empty_config.homeostasis_enabled ||
        !empty_config.homeostasis_synaptic_scaling_enabled ||
        !empty_config.homeostasis_inhibitory_gain_enabled)
    {
        return fail("valid homeostasis section was not loaded");
    }

    if (!load_text_expect_failure(
            "[homeostasis]\nenabled = true\nintrinsic_enabled = false\n"
            "synaptic_scaling_enabled = false\ninhibitory_gain_enabled = false\n",
            "homeostasis without mechanisms was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nrate_tau = 0\n",
            "zero homeostasis rate tau was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nupdate_interval_steps = 0\n",
            "zero homeostasis interval was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nthreshold_min = -40\nthreshold_max = -60\n",
            "inverted homeostasis threshold was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nscaling_target_mode = fixed\n",
            "unknown scaling target mode was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nscaling_eta = 2\n",
            "invalid scaling eta was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\ninhibitory_gain_initial = 10\n",
            "out-of-bounds inhibitory gain was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\nenabled = true\nenabled = false\n",
            "duplicate homeostasis key was accepted") ||
        !load_text_expect_failure(
            "[homeostasis]\ntarget_rate = NaN\n",
            "NaN homeostasis target was accepted") ||
        !load_text_expect_failure(
            "v_threshold = -61\n[homeostasis]\nenabled = true\n"
            "intrinsic_enabled = true\n",
            "active intrinsic homeostasis accepted base threshold outside bounds"))
    {
        return 1;
    }

    if (!check_save_and_reload())
        return 1;

    printf("Scenario configuration validation OK\n");
    return 0;
}
