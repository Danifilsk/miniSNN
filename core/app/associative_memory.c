#include "associative_memory.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSOCIATIVE_MEMORY_PATH_MAX 512

typedef struct
{
    int trial_count;
    int correct_trials;
    double accuracy;
    double mean_similarity;
    double mean_completion;
    double mean_latency;
    int response_count;
    int weights_changed;
} RecallAggregate;

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;

    if (value == 0U)
        value = 0x6d2b79f5U;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int group_start(int start, int group_size, int pair_id)
{
    return start + pair_id * group_size;
}

static int source_is_in_group(
    const ScenarioConfig *config,
    int pair_id,
    size_t source)
{
    int start = group_start(config->associative_memory_cue_start,
                            config->associative_memory_cue_group_size,
                            pair_id);

    return source >= (size_t)start &&
           source < (size_t)(start + config->associative_memory_cue_group_size);
}

static int target_is_in_group(
    const ScenarioConfig *config,
    int pair_id,
    size_t target)
{
    int start = group_start(config->associative_memory_target_start,
                            config->associative_memory_target_group_size,
                            pair_id);

    return target >= (size_t)start &&
           target < (size_t)(start + config->associative_memory_target_group_size);
}

static int select_pair(
    const ScenarioConfig *config,
    int trial,
    uint32_t *random_state)
{
    if (strcmp(config->associative_memory_pattern_mode, "seeded") == 0)
    {
        return (int)(next_random(random_state) %
                     (uint32_t)config->associative_memory_pair_count);
    }

    return trial % config->associative_memory_pair_count;
}

static int inputs_are_zero(const double *inputs, int count)
{
    for (int index = 0; index < count; index++)
    {
        if (inputs[index] != 0.0)
            return 0;
    }
    return 1;
}

static int make_path(
    char *out_path,
    size_t out_path_size,
    const char *directory,
    const char *filename)
{
    return out_path != NULL && directory != NULL && filename != NULL &&
           snprintf(out_path, out_path_size, "%s/%s", directory, filename) <
               (int)out_path_size;
}

static void write_html_text(FILE *file, const char *text)
{
    for (; text != NULL && *text != '\0'; text++)
    {
        if (*text == '&')
            fputs("&amp;", file);
        else if (*text == '<')
            fputs("&lt;", file);
        else if (*text == '>')
            fputs("&gt;", file);
        else if (*text == '"')
            fputs("&quot;", file);
        else
            fputc((unsigned char)*text, file);
    }
}

static int set_plasticity_enabled(MiniSNN *snn, int enabled)
{
    MiniSNNPlasticityConfig plasticity;

    return minisnn_get_plasticity_config(snn, &plasticity) &&
           ((plasticity.enabled = enabled), 1) &&
           minisnn_set_plasticity_config(snn, &plasticity);
}

static int create_runtime_network(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    int plasticity_enabled,
    MiniSNN **out_snn,
    char *error_message,
    size_t error_message_size)
{
    ScenarioConfig runtime_config;

    if (config == NULL || blueprint == NULL || out_snn == NULL)
    {
        set_error(error_message, error_message_size,
                  "argumento nulo na memoria associativa");
        return 0;
    }

    runtime_config = *config;
    runtime_config.plasticity_enabled = plasticity_enabled;
    if (!scenario_runtime_create_from_blueprint(
            &runtime_config, blueprint, out_snn, error_message,
            error_message_size) ||
        !scenario_runtime_configure_modules(
            *out_snn, &runtime_config, error_message, error_message_size))
    {
        minisnn_destroy(out_snn);
        return 0;
    }
    return 1;
}

static int step_with_inputs(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    int step,
    const double *inputs,
    ScenarioRuntimeStep *out_step,
    char *error_message,
    size_t error_message_size)
{
    return scenario_runtime_step_with_inputs(
        snn, config, blueprint->inhibitory_count, step, inputs,
        config->neurons, out_step, error_message, error_message_size);
}

static double mean_pair_weight(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    int pair_id)
{
    double sum = 0.0;
    int count = 0;

    for (size_t index = 0; index < minisnn_connection_count(snn); index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, index, &connection))
            return NAN;
        if (source_is_in_group(config, pair_id, connection.source) &&
            target_is_in_group(config, pair_id, connection.target))
        {
            sum += connection.weight;
            count++;
        }
    }

    return count > 0 ? sum / (double)count : NAN;
}

static int capture_weights(
    const MiniSNN *snn,
    AssociativeMemoryResult *result,
    int initial)
{
    size_t count;

    if (snn == NULL || result == NULL)
        return 0;
    count = minisnn_connection_count(snn);
    if (count != (size_t)result->connection_count ||
        (count > 0 && (result->initial_connections == NULL ||
                       result->final_weights == NULL)))
        return 0;
    for (size_t index = 0; index < count; index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, index, &connection))
            return 0;
        if (initial)
            result->initial_connections[index] = connection;
        result->final_weights[index] = connection.weight;
    }
    return 1;
}

static int initialize_weight_capture(
    const MiniSNN *snn,
    AssociativeMemoryResult *result)
{
    size_t count = minisnn_connection_count(snn);

    result->connection_count = (int)count;
    if (count == 0)
        return 1;
    result->initial_connections = calloc(
        count, sizeof(*result->initial_connections));
    result->final_weights = calloc(count, sizeof(*result->final_weights));
    if (result->initial_connections == NULL || result->final_weights == NULL)
        return 0;
    return capture_weights(snn, result, 1);
}

static int train_pairs(
    const ScenarioConfig *config,
    const ScenarioBlueprint *base_blueprint,
    int plasticity_enabled,
    int target_shift,
    MiniSNN **out_snn,
    AssociativeMemoryResult *result,
    char *error_message,
    size_t error_message_size)
{
    MiniSNN *snn = NULL;
    double inputs[SCENARIO_RUNTIME_MAX_NEURONS];
    int absolute_step = 0;

    if (!create_runtime_network(config, base_blueprint, plasticity_enabled,
                                &snn, error_message, error_message_size))
    {
        return 0;
    }
    if (result != NULL && !initialize_weight_capture(snn, result))
    {
        minisnn_destroy(&snn);
        set_error(error_message, error_message_size,
                  "memoria insuficiente para pesos associativos");
        return 0;
    }

    for (int epoch = 0; epoch < config->associative_memory_training_epochs;
         epoch++)
    {
        for (int pair_id = 0; pair_id < config->associative_memory_pair_count;
             pair_id++)
        {
            int target_pair_id =
                (pair_id + target_shift) % config->associative_memory_pair_count;
            int pair_steps = config->associative_memory_training_cue_steps +
                             config->associative_memory_training_gap_steps;

            for (int pair_step = 0; pair_step < pair_steps; pair_step++)
            {
                ScenarioRuntimeStep runtime_step;

                memset(inputs, 0,
                       (size_t)config->neurons * sizeof(*inputs));
                if (pair_step < config->associative_memory_training_cue_steps)
                {
                    int cue_start = group_start(
                        config->associative_memory_cue_start,
                        config->associative_memory_cue_group_size, pair_id);
                    int target_start = group_start(
                        config->associative_memory_target_start,
                        config->associative_memory_target_group_size,
                        target_pair_id);

                    for (int member = 0;
                         member < config->associative_memory_cue_group_size;
                         member++)
                    {
                        inputs[cue_start + member] = config->input_current;
                    }
                    /* Target activation starts one step later for causal STDP. */
                    if (pair_step > 0)
                    {
                        for (int member = 0;
                             member < config->associative_memory_target_group_size;
                             member++)
                        {
                            inputs[target_start + member] =
                                config->input_current;
                        }
                    }
                }
                if (!step_with_inputs(snn, config, base_blueprint,
                                      absolute_step, inputs, &runtime_step,
                                      error_message, error_message_size))
                {
                    minisnn_destroy(&snn);
                    return 0;
                }
                absolute_step++;
            }

            if (result != NULL)
            {
                AssociativeMemoryTrainingRecord *record =
                    &result->training_records[result->training_record_count++];
                record->epoch = epoch;
                record->pair_id = pair_id;
                record->mean_cue_target_weight =
                    mean_pair_weight(snn, config, pair_id);
                if (!isfinite(record->mean_cue_target_weight))
                {
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "conexao associativa ausente");
                    return 0;
                }
            }

            if (config->associative_memory_reset_between_pairs)
            {
                ScenarioBlueprint reset_blueprint = {0};

                if (!scenario_runtime_capture_network(
                        snn, base_blueprint->inhibitory_count,
                        base_blueprint->topology_signature, &reset_blueprint,
                        error_message, error_message_size))
                {
                    minisnn_destroy(&snn);
                    return 0;
                }
                minisnn_destroy(&snn);
                if (!create_runtime_network(
                        config, &reset_blueprint, plasticity_enabled, &snn,
                        error_message, error_message_size))
                {
                    scenario_blueprint_destroy(&reset_blueprint);
                    return 0;
                }
                scenario_blueprint_destroy(&reset_blueprint);
                absolute_step = 0;
            }
        }
    }

    if (result != NULL && !capture_weights(snn, result, 0))
    {
        minisnn_destroy(&snn);
        set_error(error_message, error_message_size,
                  "erro ao capturar pesos treinados");
        return 0;
    }
    *out_snn = snn;
    return 1;
}

static double build_partial_cue_mask(
    const ScenarioConfig *config,
    uint32_t *random_state,
    unsigned char *out_mask)
{
    int group_size = config->associative_memory_cue_group_size;
    int removed = (int)ceil(
        config->associative_memory_cue_corruption * (double)group_size);
    int remove_offset;

    if (removed >= group_size)
        removed = group_size - 1;
    if (removed < 0)
        removed = 0;
    remove_offset = group_size > 0 ?
        (int)(next_random(random_state) % (uint32_t)group_size) : 0;
    for (int member = 0; member < group_size; member++)
    {
        int relative = (member - remove_offset + group_size) % group_size;

        out_mask[member] = relative < removed ? 1U : 0U;
    }
    return (double)removed / (double)group_size;
}

static void apply_partial_cue(
    const ScenarioConfig *config,
    int pair_id,
    const unsigned char *mask,
    double *inputs)
{
    int cue_start = group_start(config->associative_memory_cue_start,
                                config->associative_memory_cue_group_size,
                                pair_id);

    for (int member = 0;
         member < config->associative_memory_cue_group_size;
         member++)
    {
        if (mask[member] == 0U)
            inputs[cue_start + member] = config->input_current;
    }
}

static unsigned int cue_mask_signature(const unsigned char *mask, int count)
{
    uint32_t signature = 2166136261U;

    for (int index = 0; index < count; index++)
    {
        signature ^= (uint32_t)mask[index];
        signature *= 16777619U;
    }
    return signature;
}

static int partial_cue_inputs_match(
    const ScenarioConfig *config,
    int pair_id,
    const unsigned char *mask,
    const double *inputs)
{
    int cue_start = group_start(config->associative_memory_cue_start,
                                config->associative_memory_cue_group_size,
                                pair_id);

    for (int neuron_id = 0; neuron_id < config->neurons; neuron_id++)
    {
        double expected = 0.0;

        if (neuron_id >= cue_start &&
            neuron_id < cue_start + config->associative_memory_cue_group_size)
        {
            int member = neuron_id - cue_start;

            if (mask[member] == 0U)
                expected = config->input_current;
        }
        if (inputs[neuron_id] != expected)
            return 0;
    }
    return 1;
}

int associative_memory_decode_target(
    const int *target_group_spikes,
    const int *target_unit_active,
    int pair_count,
    int target_group_size,
    int expected_pattern,
    double *out_similarity,
    double *out_completion_score,
    int *out_recalled_pattern)
{
    int total_spikes = 0;
    int maximum_spikes = -1;
    int recalled_pattern = -1;
    int observed_count = 0;
    int expected_observed = 0;

    if (target_group_spikes == NULL || target_unit_active == NULL ||
        pair_count < 2 || target_group_size <= 0 ||
        expected_pattern < 0 || expected_pattern >= pair_count ||
        out_similarity == NULL || out_completion_score == NULL ||
        out_recalled_pattern == NULL)
    {
        return 0;
    }
    for (int pair_id = 0; pair_id < pair_count; pair_id++)
    {
        int count = target_group_spikes[pair_id];

        if (count < 0)
            return 0;
        total_spikes += count;
        if (count > maximum_spikes)
        {
            maximum_spikes = count;
            recalled_pattern = pair_id;
        }
    }
    for (int unit = 0; unit < pair_count * target_group_size; unit++)
    {
        if (target_unit_active[unit] != 0 && target_unit_active[unit] != 1)
            return 0;
        observed_count += target_unit_active[unit];
        if (unit >= expected_pattern * target_group_size &&
            unit < (expected_pattern + 1) * target_group_size)
        {
            expected_observed += target_unit_active[unit];
        }
    }
    if (total_spikes == 0)
        recalled_pattern = -1;
    *out_similarity = total_spikes > 0 ?
        (double)target_group_spikes[expected_pattern] / (double)total_spikes :
        0.0;
    *out_completion_score = (expected_observed > 0 || observed_count > 0) ?
        (double)expected_observed /
            (double)(target_group_size + observed_count - expected_observed) :
        0.0;
    *out_recalled_pattern = recalled_pattern;
    return isfinite(*out_similarity) && isfinite(*out_completion_score);
}

static int run_recall_trials(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    AssociativeMemoryTrial *out_trials,
    RecallAggregate *out_aggregate,
    char *error_message,
    size_t error_message_size)
{
    double inputs[SCENARIO_RUNTIME_MAX_NEURONS];
    int group_spikes[SCENARIO_RUNTIME_MAX_NEURONS];
    int unit_active[SCENARIO_RUNTIME_MAX_NEURONS];
    uint32_t random_state = config->associative_memory_seed;
    RecallAggregate aggregate;

    memset(&aggregate, 0, sizeof(aggregate));
    aggregate.mean_latency = -1.0;
    for (int trial_index = 0;
         trial_index < config->associative_memory_trial_count; trial_index++)
    {
        MiniSNN *snn = NULL;
        AssociativeMemoryTrial trial;
        unsigned char cue_mask[SCENARIO_RUNTIME_MAX_NEURONS];
        int pair_id = select_pair(config, trial_index, &random_state);
        int expected_pattern = pair_id;
        int absolute_step = 0;
        int trial_steps = config->associative_memory_recall_cue_steps +
                          config->associative_memory_recall_delay_steps +
                          config->associative_memory_recall_probe_steps;
        double *weights_before = NULL;
        size_t connection_count;

        memset(&trial, 0, sizeof(trial));
        memset(group_spikes, 0, sizeof(group_spikes));
        memset(unit_active, 0, sizeof(unit_active));
        trial.trial = trial_index;
        trial.pair_id = pair_id;
        trial.expected_pattern = expected_pattern;
        trial.first_response_step = -1;
        trial.cue_mask_consistent = 1;
        trial.delay_probe_inputs_zero = 1;
        trial.cue_corruption = build_partial_cue_mask(
            config, &random_state, cue_mask);
        trial.cue_mask_signature = cue_mask_signature(
            cue_mask, config->associative_memory_cue_group_size);
        if (!create_runtime_network(config, blueprint,
                                    config->plasticity_enabled, &snn,
                                    error_message, error_message_size) ||
            (config->associative_memory_freeze_plasticity_during_recall &&
             !set_plasticity_enabled(snn, 0)))
        {
            minisnn_destroy(&snn);
            set_error(error_message, error_message_size,
                      "erro ao preparar recall associativo");
            return 0;
        }

        connection_count = minisnn_connection_count(snn);
        if (config->associative_memory_freeze_plasticity_during_recall)
        {
            weights_before = calloc(connection_count, sizeof(*weights_before));
            if (weights_before == NULL && connection_count > 0)
            {
                minisnn_destroy(&snn);
                set_error(error_message, error_message_size,
                          "memoria insuficiente no recall associativo");
                return 0;
            }
            for (size_t index = 0; index < connection_count; index++)
            {
                if (!minisnn_get_connection_weight(snn, index,
                                                    &weights_before[index]))
                {
                    free(weights_before);
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "erro ao ler peso antes do recall");
                    return 0;
                }
            }
        }

        for (int trial_step = 0; trial_step < trial_steps; trial_step++)
        {
            ScenarioRuntimeStep runtime_step;
            int in_cue =
                trial_step < config->associative_memory_recall_cue_steps;
            int in_probe = trial_step >=
                config->associative_memory_recall_cue_steps +
                    config->associative_memory_recall_delay_steps;

            memset(inputs, 0,
                   (size_t)config->neurons * sizeof(*inputs));
            if (in_cue)
            {
                apply_partial_cue(config, pair_id, cue_mask, inputs);
                if (!partial_cue_inputs_match(config, pair_id, cue_mask,
                                              inputs))
                {
                    trial.cue_mask_consistent = 0;
                }
            }
            else if (!inputs_are_zero(inputs, config->neurons))
            {
                trial.delay_probe_inputs_zero = 0;
                free(weights_before);
                minisnn_destroy(&snn);
                set_error(error_message, error_message_size,
                          "entrada inesperada fora do cue associativo");
                return 0;
            }
            if (!step_with_inputs(snn, config, blueprint, absolute_step,
                                  inputs, &runtime_step, error_message,
                                  error_message_size))
            {
                free(weights_before);
                minisnn_destroy(&snn);
                return 0;
            }
            if (in_probe)
            {
                int target_spikes = 0;

                for (int target_pair = 0;
                     target_pair < config->associative_memory_pair_count;
                     target_pair++)
                {
                    int start = group_start(
                        config->associative_memory_target_start,
                        config->associative_memory_target_group_size,
                        target_pair);
                    for (int member = 0;
                         member < config->associative_memory_target_group_size;
                         member++)
                    {
                        int spike = runtime_step.spikes[start + member];
                        int unit_index = target_pair *
                                             config->associative_memory_target_group_size +
                                         member;

                        group_spikes[target_pair] += spike;
                        target_spikes += spike;
                        if (spike)
                            unit_active[unit_index] = 1;
                    }
                }
                if (target_spikes > 0 && trial.first_response_step < 0)
                {
                    trial.first_response_step = trial_step -
                        config->associative_memory_recall_cue_steps -
                        config->associative_memory_recall_delay_steps;
                }
            }
            absolute_step++;
        }

        if (weights_before != NULL)
        {
            for (size_t index = 0; index < connection_count; index++)
            {
                double weight_after;

                if (!minisnn_get_connection_weight(snn, index, &weight_after))
                {
                    free(weights_before);
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "erro ao ler peso apos o recall");
                    return 0;
                }
                if (weight_after != weights_before[index])
                    aggregate.weights_changed = 1;
            }
        }
        free(weights_before);
        minisnn_destroy(&snn);

        if (!associative_memory_decode_target(
                group_spikes, unit_active,
                config->associative_memory_pair_count,
                config->associative_memory_target_group_size, expected_pattern,
                &trial.pattern_similarity, &trial.pattern_completion_score,
                &trial.recalled_pattern))
        {
            set_error(error_message, error_message_size,
                      "erro ao decodificar recall associativo");
            return 0;
        }
        trial.recall_correct =
            trial.recalled_pattern == trial.expected_pattern &&
            trial.pattern_similarity >= config->associative_memory_recall_threshold &&
            trial.pattern_completion_score >= config->associative_memory_recall_threshold;
        for (int target_pair = 0;
             target_pair < config->associative_memory_pair_count;
             target_pair++)
        {
            trial.mean_target_activity += (double)group_spikes[target_pair];
        }
        trial.mean_target_activity /=
            (double)(config->associative_memory_pair_count *
                     config->associative_memory_target_group_size *
                     config->associative_memory_recall_probe_steps);
        aggregate.trial_count++;
        aggregate.correct_trials += trial.recall_correct;
        aggregate.mean_similarity += trial.pattern_similarity;
        aggregate.mean_completion += trial.pattern_completion_score;
        if (trial.first_response_step >= 0)
        {
            aggregate.mean_latency += (double)trial.first_response_step;
            aggregate.response_count++;
        }
        if (out_trials != NULL)
            out_trials[trial_index] = trial;
    }

    aggregate.accuracy = (double)aggregate.correct_trials /
                         (double)aggregate.trial_count;
    aggregate.mean_similarity /= (double)aggregate.trial_count;
    aggregate.mean_completion /= (double)aggregate.trial_count;
    if (aggregate.response_count > 0)
    {
        aggregate.mean_latency = (aggregate.mean_latency + 1.0) /
                                 (double)aggregate.response_count;
    }
    *out_aggregate = aggregate;
    return 1;
}

void associative_memory_result_destroy(AssociativeMemoryResult *result)
{
    if (result == NULL)
        return;

    free(result->training_records);
    free(result->initial_connections);
    free(result->final_weights);
    free(result->trials);
    scenario_blueprint_destroy(&result->learned_blueprint);
    memset(result, 0, sizeof(*result));
}

int associative_memory_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    AssociativeMemoryResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    AssociativeMemoryResult result;
    ScenarioBlueprint learned_blueprint;
    ScenarioBlueprint frozen_blueprint;
    ScenarioBlueprint shuffled_blueprint;
    MiniSNN *trained_snn = NULL;
    MiniSNN *frozen_snn = NULL;
    MiniSNN *shuffled_snn = NULL;
    RecallAggregate real_recall;
    RecallAggregate untrained_recall;
    RecallAggregate shuffled_recall;
    RecallAggregate frozen_recall;
    int record_capacity;

    if (config == NULL || blueprint == NULL || out_result == NULL ||
        !config->associative_memory_enabled ||
        !scenario_config_validate(config, error_message, error_message_size))
    {
        if (error_message != NULL && error_message_size > 0 &&
            error_message[0] == '\0')
        {
            set_error(error_message, error_message_size,
                      "memoria associativa nao esta habilitada");
        }
        return 0;
    }

    memset(&result, 0, sizeof(result));
    memset(&learned_blueprint, 0, sizeof(learned_blueprint));
    memset(&frozen_blueprint, 0, sizeof(frozen_blueprint));
    memset(&shuffled_blueprint, 0, sizeof(shuffled_blueprint));
    memset(&real_recall, 0, sizeof(real_recall));
    memset(&untrained_recall, 0, sizeof(untrained_recall));
    memset(&shuffled_recall, 0, sizeof(shuffled_recall));
    memset(&frozen_recall, 0, sizeof(frozen_recall));
    result.trial_count = config->associative_memory_trial_count;
    record_capacity = config->associative_memory_training_epochs *
                      config->associative_memory_pair_count;
    result.trials = calloc((size_t)result.trial_count, sizeof(*result.trials));
    result.training_records = calloc((size_t)record_capacity,
                                     sizeof(*result.training_records));
    if (result.trials == NULL || result.training_records == NULL)
    {
        associative_memory_result_destroy(&result);
        set_error(error_message, error_message_size,
                  "memoria insuficiente para memoria associativa");
        return 0;
    }

    if (!train_pairs(config, blueprint, 1, 0, &trained_snn, &result,
                     error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            trained_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &learned_blueprint,
            error_message, error_message_size))
    {
        minisnn_destroy(&trained_snn);
        scenario_blueprint_destroy(&learned_blueprint);
        associative_memory_result_destroy(&result);
        return 0;
    }

    minisnn_destroy(&trained_snn);
    result.recall_reconstructed_from_blueprint = 1;

    if (!run_recall_trials(config, &learned_blueprint, result.trials,
                           &real_recall, error_message, error_message_size) ||
        !run_recall_trials(config, blueprint, NULL, &untrained_recall,
                           error_message, error_message_size) ||
        !train_pairs(config, blueprint, 1, 1, &shuffled_snn, NULL,
                     error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            shuffled_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &shuffled_blueprint,
            error_message, error_message_size) ||
        !run_recall_trials(config, &shuffled_blueprint, NULL,
                           &shuffled_recall, error_message, error_message_size) ||
        !train_pairs(config, blueprint, 0, 0, &frozen_snn, NULL,
                     error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            frozen_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &frozen_blueprint,
            error_message, error_message_size) ||
        !run_recall_trials(config, &frozen_blueprint, NULL,
                           &frozen_recall, error_message, error_message_size))
    {
        minisnn_destroy(&trained_snn);
        minisnn_destroy(&frozen_snn);
        minisnn_destroy(&shuffled_snn);
        scenario_blueprint_destroy(&learned_blueprint);
        scenario_blueprint_destroy(&frozen_blueprint);
        scenario_blueprint_destroy(&shuffled_blueprint);
        associative_memory_result_destroy(&result);
        return 0;
    }

    minisnn_destroy(&trained_snn);
    minisnn_destroy(&frozen_snn);
    minisnn_destroy(&shuffled_snn);
    scenario_blueprint_destroy(&frozen_blueprint);
    scenario_blueprint_destroy(&shuffled_blueprint);
    result.correct_trials = real_recall.correct_trials;
    result.recall_accuracy = real_recall.accuracy;
    result.mean_pattern_similarity = real_recall.mean_similarity;
    result.mean_completion_score = real_recall.mean_completion;
    result.mean_response_latency = real_recall.mean_latency;
    result.chance_accuracy = 1.0 / (double)config->associative_memory_pair_count;
    result.untrained_accuracy = untrained_recall.accuracy;
    result.shuffled_target_accuracy = shuffled_recall.accuracy;
    result.frozen_training_accuracy = frozen_recall.accuracy;
    result.control_accuracy = result.untrained_accuracy;
    if (result.shuffled_target_accuracy > result.control_accuracy)
        result.control_accuracy = result.shuffled_target_accuracy;
    if (result.frozen_training_accuracy > result.control_accuracy)
        result.control_accuracy = result.frozen_training_accuracy;
    result.association_margin = result.recall_accuracy - result.control_accuracy;
    result.recall_weights_changed = real_recall.weights_changed;
    for (int index = 0; index < result.connection_count; index++)
    {
        result.training_weight_absolute_change += fabs(
            result.final_weights[index] - result.initial_connections[index].weight);
    }
    result.learned_blueprint = learned_blueprint;
    memset(&learned_blueprint, 0, sizeof(learned_blueprint));
    *out_result = result;
    return 1;
}

#ifdef MINISNN_TESTING
int associative_memory_test_recall_accuracy(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    double *out_accuracy,
    char *error_message,
    size_t error_message_size)
{
    RecallAggregate aggregate;

    if (out_accuracy == NULL || config == NULL || blueprint == NULL ||
        !run_recall_trials(config, blueprint, NULL, &aggregate,
                           error_message, error_message_size) ||
        aggregate.trial_count <= 0)
    {
        return 0;
    }
    *out_accuracy = aggregate.accuracy;
    return 1;
}
#endif

static int write_training_csv(const AssociativeMemoryResult *result,
                              const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file, "epoch,pair_id,mean_cue_target_weight\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    for (int index = 0; index < result->training_record_count; index++)
    {
        const AssociativeMemoryTrainingRecord *record =
            &result->training_records[index];
        if (fprintf(file, "%d,%d,%.17g\n", record->epoch, record->pair_id,
                    record->mean_cue_target_weight) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_trials_csv(const AssociativeMemoryResult *result,
                            const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "trial,pair_id,cue_corruption,cue_mask_signature,cue_mask_consistent,delay_probe_inputs_zero,expected_pattern,recalled_pattern,pattern_similarity,pattern_completion_score,recall_correct,first_response_step,mean_target_activity\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const AssociativeMemoryTrial *trial = &result->trials[index];
        if (fprintf(file, "%d,%d,%.17g,%u,%d,%d,%d,%d,%.17g,%.17g,%d,%d,%.17g\n",
                    trial->trial, trial->pair_id, trial->cue_corruption,
                    trial->cue_mask_signature, trial->cue_mask_consistent,
                    trial->delay_probe_inputs_zero,
                    trial->expected_pattern, trial->recalled_pattern,
                    trial->pattern_similarity,
                    trial->pattern_completion_score, trial->recall_correct,
                    trial->first_response_step,
                    trial->mean_target_activity) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_summary(const AssociativeMemoryResult *result,
                         const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "trial_count=%d\ncorrect_trials=%d\nrecall_accuracy=%.17g\n"
                "mean_pattern_similarity=%.17g\nmean_completion_score=%.17g\n"
                "mean_response_latency=%.17g\nchance_accuracy=%.17g\n"
                "control_accuracy=%.17g\nassociation_margin=%.17g\n"
                "untrained_accuracy=%.17g\nshuffled_target_accuracy=%.17g\n"
                "frozen_training_accuracy=%.17g\n"
                "training_weight_absolute_change=%.17g\n"
                "recall_weights_changed=%d\n"
                "recall_reconstructed_from_blueprint=%d\n",
                result->trial_count, result->correct_trials,
                result->recall_accuracy, result->mean_pattern_similarity,
                result->mean_completion_score, result->mean_response_latency,
                result->chance_accuracy, result->control_accuracy,
                result->association_margin, result->untrained_accuracy,
                result->shuffled_target_accuracy,
                result->frozen_training_accuracy,
                result->training_weight_absolute_change,
                result->recall_weights_changed,
                result->recall_reconstructed_from_blueprint) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int write_html_report(
    const ScenarioConfig *config,
    const AssociativeMemoryResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fputs("<!doctype html><html><head><meta charset=\"utf-8\">"
              "<title>Memoria associativa</title><style>"
              "body{background:#101418;color:#e8edf2;font-family:system-ui,monospace;max-width:1200px;margin:2rem auto;padding:0 1rem}"
              "table{border-collapse:collapse;width:100%;margin:1rem 0}td,th{border:1px solid #53606b;padding:.4rem;text-align:right}th{background:#1b242c}code{color:#9fd3ff}</style></head><body>"
              "<h1>Memoria associativa e recuperacao parcial</h1><p>Treino por coativacao cue-alvo com STDP; recall usa somente cue corrompido, delay e probe sem entrada.</p>"
              "<h2>Configuracao</h2><ul><li>Modelo: <code>", file) == EOF)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    write_html_text(file, minisnn_neuron_model_name(config->neuron_model));
    if (fprintf(file,
                "</code></li><li>Pares: %d; epocas: %d; cue treino: %d; gap: %d</li>"
                "<li>Recall cue: %d; delay: %d; probe: %d; corrupcao: %.6f</li>"
                "<li>STDP: %s; recall congelado: %s</li></ul>"
                "<h2>Resultado</h2><p>Accuracy: %.6f; similaridade media: %.6f; completion medio: %.6f; latencia media: %.6f. "
                "Acaso: %.6f; controle conservador: %.6f; margem associativa: %.6f.</p>"
                "<p>Controles: nao treinada %.6f; alvos embaralhados %.6f; treino sem plasticidade %.6f.</p>"
                "<h2>Pesos antes/depois</h2><table><thead><tr><th>id</th><th>source</th><th>target</th><th>antes</th><th>depois</th><th>delta</th></tr></thead><tbody>",
                config->associative_memory_pair_count,
                config->associative_memory_training_epochs,
                config->associative_memory_training_cue_steps,
                config->associative_memory_training_gap_steps,
                config->associative_memory_recall_cue_steps,
                config->associative_memory_recall_delay_steps,
                config->associative_memory_recall_probe_steps,
                config->associative_memory_cue_corruption,
                config->plasticity_enabled ? "ativada" : "desativada",
                config->associative_memory_freeze_plasticity_during_recall ? "sim" : "nao",
                result->recall_accuracy, result->mean_pattern_similarity,
                result->mean_completion_score, result->mean_response_latency,
                result->chance_accuracy, result->control_accuracy,
                result->association_margin, result->untrained_accuracy,
                result->shuffled_target_accuracy,
                result->frozen_training_accuracy) < 0)
    {
        fclose(file);
        return 0;
    }
    for (int index = 0; index < result->connection_count; index++)
    {
        const MiniSNNConnectionInfo *connection =
            &result->initial_connections[index];
        double final_weight = result->final_weights[index];

        if (fprintf(file,
                    "<tr><td>%d</td><td>%zu</td><td>%zu</td><td>%.6f</td><td>%.6f</td><td>%.6f</td></tr>",
                    index, connection->source, connection->target,
                    connection->weight, final_weight,
                    final_weight - connection->weight) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    if (fputs("<h2>Pares e cue parcial</h2><table><thead><tr><th>par</th><th>cue completo</th><th>alvo completo</th><th>unidades mascaradas no cue</th></tr></thead><tbody>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    for (int pair_id = 0;
         pair_id < config->associative_memory_pair_count; pair_id++)
    {
        int cue_start = group_start(config->associative_memory_cue_start,
                                    config->associative_memory_cue_group_size,
                                    pair_id);
        int target_start = group_start(
            config->associative_memory_target_start,
            config->associative_memory_target_group_size, pair_id);
        int masked = (int)ceil(
            config->associative_memory_cue_corruption *
            (double)config->associative_memory_cue_group_size);

        if (masked >= config->associative_memory_cue_group_size)
            masked = config->associative_memory_cue_group_size - 1;
        if (fprintf(file,
                    "<tr><td>%d</td><td>%d-%d</td><td>%d-%d</td><td>%d</td></tr>",
                    pair_id, cue_start,
                    cue_start + config->associative_memory_cue_group_size - 1,
                    target_start,
                    target_start + config->associative_memory_target_group_size - 1,
                    masked) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table><h2>Trials</h2><table><thead><tr>"
              "<th>trial</th><th>par</th><th>corrupcao</th><th>esperado</th><th>recuperado</th><th>similaridade</th><th>completion</th><th>correto</th><th>latencia</th><th>atividade</th>"
              "</tr></thead><tbody>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const AssociativeMemoryTrial *trial = &result->trials[index];
        if (fprintf(file,
                    "<tr><td>%d</td><td>%d</td><td>%.6f</td><td>%d</td><td>%d</td><td>%.6f</td><td>%.6f</td><td>%s</td><td>%d</td><td>%.6f</td></tr>",
                    trial->trial, trial->pair_id, trial->cue_corruption,
                    trial->expected_pattern, trial->recalled_pattern,
                    trial->pattern_similarity,
                    trial->pattern_completion_score,
                    trial->recall_correct ? "sim" : "nao",
                    trial->first_response_step,
                    trial->mean_target_activity) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table><h2>Limitacoes</h2><p>O padrao recuperado vem do grupo alvo com maior atividade de spike durante o probe; o alvo esperado e usado somente depois para medir a resposta. O cue corrompido mascara parte dos neuronios do cue e nenhum estimulo externo e aplicado no delay ou probe. O resultado depende desta configuracao e nao demonstra memoria associativa geral.</p><p><a href=\"associative_memory_training.csv\">treino CSV</a> | <a href=\"associative_memory_trials.csv\">trials CSV</a> | <a href=\"associative_memory_summary.txt\">resumo</a></p></body></html>\n", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int associative_memory_write_outputs(
    const ScenarioConfig *config,
    const AssociativeMemoryResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size)
{
    char training_path[ASSOCIATIVE_MEMORY_PATH_MAX];
    char trials_path[ASSOCIATIVE_MEMORY_PATH_MAX];
    char summary_path[ASSOCIATIVE_MEMORY_PATH_MAX];
    char report_path[ASSOCIATIVE_MEMORY_PATH_MAX];
    char checkpoint_path[ASSOCIATIVE_MEMORY_PATH_MAX];

    if (config == NULL || result == NULL || output_directory == NULL ||
        result->trials == NULL ||
        !make_path(training_path, sizeof(training_path), output_directory,
                   "associative_memory_training.csv") ||
        !make_path(trials_path, sizeof(trials_path), output_directory,
                   "associative_memory_trials.csv") ||
        !make_path(summary_path, sizeof(summary_path), output_directory,
                   "associative_memory_summary.txt") ||
        !make_path(report_path, sizeof(report_path), output_directory,
                   "associative_memory_report.html") ||
        !make_path(checkpoint_path, sizeof(checkpoint_path), output_directory,
                   "associative_memory_checkpoint.txt") ||
        !write_training_csv(result, training_path) ||
        !write_trials_csv(result, trials_path) ||
        !write_summary(result, summary_path) ||
        !write_html_report(config, result, report_path) ||
        !scenario_blueprint_write_checkpoint(
            &result->learned_blueprint, checkpoint_path, error_message,
            error_message_size))
    {
        set_error(error_message, error_message_size,
                  "erro ao escrever saidas de memoria associativa");
        return 0;
    }
    return 1;
}
