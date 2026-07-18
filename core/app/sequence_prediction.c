#include "sequence_prediction.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEQUENCE_PREDICTION_PATH_MAX 512

typedef struct
{
    int trial_count;
    int correct_predictions;
    double mean_similarity;
    double mean_error;
    double mean_latency;
    int response_count;
    int weights_changed;
} PredictionAggregate;

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static int total_patterns(const ScenarioConfig *config)
{
    return config->sequence_prediction_sequence_count *
           config->sequence_prediction_sequence_length;
}

int sequence_prediction_pattern_id(
    const ScenarioConfig *config,
    int sequence_id,
    int position)
{
    int sequence_length;
    int offset;

    if (config == NULL || sequence_id < 0 ||
        sequence_id >= config->sequence_prediction_sequence_count ||
        position < 0 ||
        position >= config->sequence_prediction_sequence_length)
    {
        return -1;
    }

    sequence_length = config->sequence_prediction_sequence_length;
    if (strcmp(config->sequence_prediction_pattern_mode, "contextual") == 0 &&
        position == config->sequence_prediction_prefix_length - 1)
    {
        /* The final prefix symbol is deliberately shared across sequences. */
        return position;
    }
    if (strcmp(config->sequence_prediction_pattern_mode, "seeded") == 0)
    {
        offset = (int)((config->sequence_prediction_seed +
                        (unsigned int)(sequence_id * 17)) %
                       (unsigned int)sequence_length);
        position = (position + offset) % sequence_length;
    }
    return sequence_id * sequence_length + position;
}

static int input_group_start(
    const ScenarioConfig *config,
    int pattern_id)
{
    return config->sequence_prediction_input_start +
           pattern_id * config->sequence_prediction_input_group_size;
}

static int prediction_group_start(
    const ScenarioConfig *config,
    int pattern_id)
{
    return config->sequence_prediction_prediction_start +
           pattern_id * config->sequence_prediction_prediction_group_size;
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
                  "argumento nulo na predicao de sequencia");
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

static int stimulate_input_pattern(
    const ScenarioConfig *config,
    int pattern_id,
    double *inputs)
{
    int start = input_group_start(config, pattern_id);

    if (pattern_id < 0 || pattern_id >= total_patterns(config) ||
        start < 0 || start + config->sequence_prediction_input_group_size >
            config->neurons)
    {
        return 0;
    }
    for (int member = 0;
         member < config->sequence_prediction_input_group_size;
         member++)
    {
        inputs[start + member] = config->input_current;
    }
    return 1;
}

static int stimulate_prediction_pattern(
    const ScenarioConfig *config,
    int pattern_id,
    double *inputs)
{
    int start = prediction_group_start(config, pattern_id);

    if (pattern_id < 0 || pattern_id >= total_patterns(config) ||
        start < 0 || start + config->sequence_prediction_prediction_group_size >
            config->neurons)
    {
        return 0;
    }
    for (int member = 0;
         member < config->sequence_prediction_prediction_group_size;
         member++)
    {
        inputs[start + member] = config->input_current;
    }
    return 1;
}

static int training_position(
    const ScenarioConfig *config,
    int sequence_id,
    int presentation_index,
    int shuffled_order)
{
    int length = config->sequence_prediction_sequence_length;

    if (!shuffled_order)
        return presentation_index;

    /* A deterministic permutation changes only the training presentation. */
    return (length - 1 - presentation_index + sequence_id) % length;
}

static double mean_transition_weight(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    int source_pattern,
    int target_pattern)
{
    int source_start = input_group_start(config, source_pattern);
    int target_start = prediction_group_start(config, target_pattern);
    double sum = 0.0;
    int count = 0;

    for (size_t index = 0; index < minisnn_connection_count(snn); index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, index, &connection))
            return NAN;
        if (connection.source >= (size_t)source_start &&
            connection.source < (size_t)(source_start +
                config->sequence_prediction_input_group_size) &&
            connection.target >= (size_t)target_start &&
            connection.target < (size_t)(target_start +
                config->sequence_prediction_prediction_group_size))
        {
            sum += connection.weight;
            count++;
        }
    }
    return count > 0 ? sum / (double)count : NAN;
}

static int capture_weights(
    const MiniSNN *snn,
    SequencePredictionResult *result,
    int initial)
{
    size_t count;

    if (snn == NULL || result == NULL)
        return 0;
    count = minisnn_connection_count(snn);
    if (count != (size_t)result->connection_count ||
        (count > 0 && (result->initial_connections == NULL ||
                       result->final_weights == NULL)))
    {
        return 0;
    }
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
    SequencePredictionResult *result)
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

static int train_sequences(
    const ScenarioConfig *config,
    const ScenarioBlueprint *base_blueprint,
    int plasticity_enabled,
    int shuffled_order,
    MiniSNN **out_snn,
    SequencePredictionResult *result,
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
                  "memoria insuficiente para pesos de sequencia");
        return 0;
    }

    for (int epoch = 0; epoch < config->sequence_prediction_training_epochs;
         epoch++)
    {
        for (int sequence_id = 0;
             sequence_id < config->sequence_prediction_sequence_count;
             sequence_id++)
        {
            for (int presentation = 0;
                 presentation < config->sequence_prediction_sequence_length;
                 presentation++)
            {
                int position = training_position(
                    config, sequence_id, presentation, shuffled_order);
                int pattern_id = sequence_prediction_pattern_id(
                    config, sequence_id, position);
                int next_pattern_id = presentation + 1 <
                    config->sequence_prediction_sequence_length ?
                    sequence_prediction_pattern_id(
                        config, sequence_id,
                        training_position(
                            config, sequence_id, presentation + 1,
                            shuffled_order)) : -1;
                int total_steps = config->sequence_prediction_pattern_steps +
                    config->sequence_prediction_inter_pattern_gap_steps;

                for (int pattern_step = 0;
                     pattern_step < total_steps;
                     pattern_step++)
                {
                    ScenarioRuntimeStep runtime_step;

                    memset(inputs, 0,
                           (size_t)config->neurons * sizeof(*inputs));
                    if (pattern_step < config->sequence_prediction_pattern_steps &&
                        !stimulate_input_pattern(config, pattern_id, inputs))
                    {
                        minisnn_destroy(&snn);
                        set_error(error_message, error_message_size,
                                  "padrao de entrada invalido no treino");
                        return 0;
                    }
                    if (next_pattern_id >= 0 &&
                        pattern_step == config->sequence_prediction_pattern_steps &&
                        /* Teacher pulse supervisionado: somente no treino. */
                        !stimulate_prediction_pattern(
                            config, next_pattern_id, inputs))
                    {
                        minisnn_destroy(&snn);
                        set_error(error_message, error_message_size,
                                  "padrao de previsao invalido no treino");
                        return 0;
                    }
                    if (!step_with_inputs(
                            snn, config, base_blueprint, absolute_step,
                            inputs, &runtime_step, error_message,
                            error_message_size))
                    {
                        minisnn_destroy(&snn);
                        return 0;
                    }
                    absolute_step++;
                }
            }

            if (result != NULL)
            {
                for (int position = 0;
                     position + 1 < config->sequence_prediction_sequence_length;
                     position++)
                {
                    SequencePredictionTrainingRecord *record =
                        &result->training_records[result->training_record_count++];
                    int source_pattern = sequence_prediction_pattern_id(
                        config, sequence_id, position);
                    int target_pattern = sequence_prediction_pattern_id(
                        config, sequence_id, position + 1);

                    record->epoch = epoch;
                    record->sequence_id = sequence_id;
                    record->position = position;
                    record->mean_transition_weight = mean_transition_weight(
                        snn, config, source_pattern, target_pattern);
                    if (!isfinite(record->mean_transition_weight))
                    {
                        minisnn_destroy(&snn);
                        set_error(error_message, error_message_size,
                                  "conexao temporal ausente");
                        return 0;
                    }
                }
            }

            if (config->sequence_prediction_reset_between_sequences)
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
                  "erro ao capturar pesos temporais treinados");
        return 0;
    }
    *out_snn = snn;
    return 1;
}

static int append_prefix_pattern(
    char *out_prefix,
    size_t out_prefix_size,
    int pattern_id)
{
    size_t length = strlen(out_prefix);
    int written = snprintf(out_prefix + length, out_prefix_size - length,
                           "%s%d", length == 0 ? "" : ">", pattern_id);

    return written >= 0 && (size_t)written < out_prefix_size - length;
}

static int decode_prediction(
    const int *group_spikes,
    int pattern_count,
    int *out_predicted_pattern,
    int *out_total_spikes)
{
    int predicted_pattern = -1;
    int maximum_spikes = 0;
    int total_spikes = 0;

    if (group_spikes == NULL || out_predicted_pattern == NULL ||
        out_total_spikes == NULL || pattern_count <= 0)
    {
        return 0;
    }
    for (int pattern_id = 0; pattern_id < pattern_count; pattern_id++)
    {
        if (group_spikes[pattern_id] < 0)
            return 0;
        total_spikes += group_spikes[pattern_id];
        if (group_spikes[pattern_id] > maximum_spikes)
        {
            maximum_spikes = group_spikes[pattern_id];
            predicted_pattern = pattern_id;
        }
    }
    *out_predicted_pattern = predicted_pattern;
    *out_total_spikes = total_spikes;
    return 1;
}

static int accumulate_probe_frame(
    const int *frame_spikes,
    int pattern_count,
    int *in_out_group_spikes,
    int *out_frame_spikes)
{
    int frame_total = 0;

    if (frame_spikes == NULL || in_out_group_spikes == NULL ||
        out_frame_spikes == NULL || pattern_count <= 0)
    {
        return 0;
    }
    for (int pattern_id = 0; pattern_id < pattern_count; pattern_id++)
    {
        int spike = frame_spikes[pattern_id];

        if (spike < 0)
            return 0;
        in_out_group_spikes[pattern_id] += spike;
        frame_total += spike;
    }
    *out_frame_spikes = frame_total;
    return 1;
}

#ifdef MINISNN_TESTING
int sequence_prediction_test_decode_probe(
    const int *probe_frames,
    int frame_count,
    int pattern_count,
    int *out_predicted_pattern,
    double *out_similarity)
{
    int group_spikes[SCENARIO_RUNTIME_MAX_NEURONS];
    int total_spikes;

    if (out_similarity == NULL || pattern_count > SCENARIO_RUNTIME_MAX_NEURONS ||
        probe_frames == NULL || frame_count <= 0)
    {
        return 0;
    }
    memset(group_spikes, 0, (size_t)pattern_count * sizeof(*group_spikes));
    for (int frame = 0; frame < frame_count; frame++)
    {
        int ignored_frame_spikes;

        if (!accumulate_probe_frame(
                probe_frames + frame * pattern_count, pattern_count,
                group_spikes, &ignored_frame_spikes))
        {
            return 0;
        }
    }
    if (!decode_prediction(group_spikes, pattern_count, out_predicted_pattern,
                           &total_spikes))
    {
        return 0;
    }
    *out_similarity = total_spikes > 0 ?
        (double)group_spikes[*out_predicted_pattern] / (double)total_spikes :
        0.0;
    return 1;
}
#endif

static int permuted_label(const ScenarioConfig *config, int pattern_id)
{
    return (pattern_id + 1) % total_patterns(config);
}

static int run_prediction_trials(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    SequencePredictionTrial *out_trials,
    PredictionAggregate *out_aggregate,
    int permute_labels,
    int last_symbol_only,
    char *error_message,
    size_t error_message_size)
{
    double inputs[SCENARIO_RUNTIME_MAX_NEURONS];
    int group_spikes[SCENARIO_RUNTIME_MAX_NEURONS];
    PredictionAggregate aggregate;
    int pattern_count = total_patterns(config);

    memset(&aggregate, 0, sizeof(aggregate));
    aggregate.mean_latency = -1.0;
    for (int trial_index = 0;
         trial_index < config->sequence_prediction_trial_count;
         trial_index++)
    {
        MiniSNN *snn = NULL;
        SequencePredictionTrial trial;
        int sequence_id = trial_index % config->sequence_prediction_sequence_count;
        int absolute_step = 0;
        int presented_pattern_count = last_symbol_only ? 1 :
            config->sequence_prediction_prefix_length;
        int prefix_steps = presented_pattern_count *
                config->sequence_prediction_pattern_steps +
            (presented_pattern_count - 1) *
                config->sequence_prediction_inter_pattern_gap_steps;
        int prediction_start_step = prefix_steps +
            config->sequence_prediction_prediction_delay_steps;
        int trial_steps = prediction_start_step +
            config->sequence_prediction_prediction_probe_steps;
        double *weights_before = NULL;
        size_t connection_count;
        int predicted_pattern;
        int total_spikes;
        int scored_pattern;
        int prediction_activity_total = 0;

        memset(&trial, 0, sizeof(trial));
        memset(group_spikes, 0, sizeof(group_spikes));
        trial.trial = trial_index;
        trial.sequence_id = sequence_id;
        trial.expected_next_pattern = -1;
        trial.predicted_pattern = -1;
        trial.first_prediction_step = -1;
        trial.prefix_is_incomplete =
            config->sequence_prediction_prefix_length <
            config->sequence_prediction_sequence_length;
        trial.delay_probe_inputs_zero = 1;
        for (int presentation = 0;
             presentation < presented_pattern_count;
             presentation++)
        {
            int position = last_symbol_only ?
                config->sequence_prediction_prefix_length - 1 : presentation;

            if (!append_prefix_pattern(
                    trial.prefix, sizeof(trial.prefix),
                    sequence_prediction_pattern_id(config, sequence_id,
                                                   position)))
            {
                set_error(error_message, error_message_size,
                          "prefixo de sequencia muito longo");
                return 0;
            }
        }
        if (!create_runtime_network(config, blueprint,
                                    config->plasticity_enabled, &snn,
                                    error_message, error_message_size) ||
            (config->sequence_prediction_freeze_plasticity_during_evaluation &&
             !set_plasticity_enabled(snn, 0)))
        {
            minisnn_destroy(&snn);
            set_error(error_message, error_message_size,
                      "erro ao preparar avaliacao de sequencia");
            return 0;
        }

        connection_count = minisnn_connection_count(snn);
        if (config->sequence_prediction_freeze_plasticity_during_evaluation)
        {
            weights_before = calloc(connection_count, sizeof(*weights_before));
            if (weights_before == NULL && connection_count > 0)
            {
                minisnn_destroy(&snn);
                set_error(error_message, error_message_size,
                          "memoria insuficiente na avaliacao de sequencia");
                return 0;
            }
            for (size_t index = 0; index < connection_count; index++)
            {
                if (!minisnn_get_connection_weight(
                        snn, index, &weights_before[index]))
                {
                    free(weights_before);
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "erro ao ler peso antes da avaliacao");
                    return 0;
                }
            }
        }

        for (int trial_step = 0; trial_step < trial_steps; trial_step++)
        {
            ScenarioRuntimeStep runtime_step;
            int probe_frame_spikes[SCENARIO_RUNTIME_MAX_NEURONS];
            int prefix_position = -1;
            int in_prefix_pattern = 0;
            int in_probe = trial_step >= prediction_start_step;

            for (int presentation = 0, cursor = 0;
                 presentation < presented_pattern_count;
                 presentation++)
            {
                if (trial_step >= cursor &&
                    trial_step < cursor +
                        config->sequence_prediction_pattern_steps)
                {
                    prefix_position = last_symbol_only ?
                        config->sequence_prediction_prefix_length - 1 :
                        presentation;
                    in_prefix_pattern = 1;
                    break;
                }
                cursor += config->sequence_prediction_pattern_steps;
                if (presentation + 1 < presented_pattern_count)
                    cursor += config->sequence_prediction_inter_pattern_gap_steps;
            }

            memset(inputs, 0,
                   (size_t)config->neurons * sizeof(*inputs));
            if (in_prefix_pattern)
            {
                int prefix_pattern = sequence_prediction_pattern_id(
                    config, sequence_id, prefix_position);

                if (!stimulate_input_pattern(config, prefix_pattern, inputs))
                {
                    free(weights_before);
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "prefixo de entrada invalido");
                    return 0;
                }
            }
            else if (!inputs_are_zero(inputs, config->neurons))
            {
                trial.delay_probe_inputs_zero = 0;
                free(weights_before);
                minisnn_destroy(&snn);
                set_error(error_message, error_message_size,
                          "entrada inesperada fora do prefixo");
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
                int probe_spikes = 0;

                for (int pattern_id = 0;
                     pattern_id < pattern_count;
                     pattern_id++)
                {
                    int start = prediction_group_start(config, pattern_id);

                    probe_frame_spikes[pattern_id] = 0;

                    for (int member = 0;
                         member < config->sequence_prediction_prediction_group_size;
                         member++)
                    {
                        int spike = runtime_step.spikes[start + member];

                        probe_frame_spikes[pattern_id] += spike;
                    }
                }
                if (!accumulate_probe_frame(
                        probe_frame_spikes, pattern_count, group_spikes,
                        &probe_spikes))
                {
                    free(weights_before);
                    minisnn_destroy(&snn);
                    set_error(error_message, error_message_size,
                              "spike invalido no probe de predicao");
                    return 0;
                }
                if (probe_spikes > 0 && trial.first_prediction_step < 0)
                {
                    trial.first_prediction_step = trial_step -
                        prediction_start_step;
                }
                prediction_activity_total += probe_spikes;
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
                              "erro ao ler peso apos a avaliacao");
                    return 0;
                }
                if (weight_after != weights_before[index])
                    aggregate.weights_changed = 1;
            }
        }
        free(weights_before);
        minisnn_destroy(&snn);

        if (!decode_prediction(group_spikes, pattern_count,
                               &predicted_pattern, &total_spikes))
        {
            set_error(error_message, error_message_size,
                      "erro ao decodificar predicao");
            return 0;
        }
        /* The next pattern is consulted only after neural decoding. */
        scored_pattern = sequence_prediction_pattern_id(
            config, sequence_id, config->sequence_prediction_prefix_length);
        if (permute_labels)
            scored_pattern = permuted_label(config, scored_pattern);
        trial.predicted_pattern = predicted_pattern;
        trial.expected_next_pattern = scored_pattern;
        trial.prediction_similarity = total_spikes > 0 ?
            (double)group_spikes[scored_pattern] / (double)total_spikes : 0.0;
        trial.prediction_error = 1.0 - trial.prediction_similarity;
        trial.prediction_correct =
            trial.predicted_pattern == trial.expected_next_pattern &&
            trial.prediction_similarity >=
                config->sequence_prediction_prediction_threshold;
        trial.mean_prediction_activity = (double)prediction_activity_total /
            (double)(pattern_count *
                     config->sequence_prediction_prediction_group_size *
                     config->sequence_prediction_prediction_probe_steps);

        aggregate.trial_count++;
        aggregate.correct_predictions += trial.prediction_correct;
        aggregate.mean_similarity += trial.prediction_similarity;
        aggregate.mean_error += trial.prediction_error;
        if (trial.first_prediction_step >= 0)
        {
            aggregate.mean_latency += (double)trial.first_prediction_step;
            aggregate.response_count++;
        }
        if (out_trials != NULL)
            out_trials[trial_index] = trial;
    }

    aggregate.mean_similarity /= (double)aggregate.trial_count;
    aggregate.mean_error /= (double)aggregate.trial_count;
    if (aggregate.response_count > 0)
    {
        aggregate.mean_latency = (aggregate.mean_latency + 1.0) /
                                 (double)aggregate.response_count;
    }
    *out_aggregate = aggregate;
    return 1;
}

void sequence_prediction_result_destroy(SequencePredictionResult *result)
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

int sequence_prediction_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    SequencePredictionResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    SequencePredictionResult result;
    ScenarioBlueprint learned_blueprint;
    ScenarioBlueprint shuffled_blueprint;
    ScenarioBlueprint frozen_blueprint;
    MiniSNN *trained_snn = NULL;
    MiniSNN *shuffled_snn = NULL;
    MiniSNN *frozen_snn = NULL;
    PredictionAggregate real_prediction;
    PredictionAggregate untrained_prediction;
    PredictionAggregate shuffled_prediction;
    PredictionAggregate frozen_prediction;
    PredictionAggregate permuted_prediction;
    PredictionAggregate last_symbol_prediction;
    int record_capacity;

    if (config == NULL || blueprint == NULL || out_result == NULL ||
        !config->sequence_prediction_enabled ||
        !scenario_config_validate(config, error_message, error_message_size))
    {
        if (error_message != NULL && error_message_size > 0 &&
            error_message[0] == '\0')
        {
            set_error(error_message, error_message_size,
                      "predicao de sequencia nao esta habilitada");
        }
        return 0;
    }

    memset(&result, 0, sizeof(result));
    memset(&learned_blueprint, 0, sizeof(learned_blueprint));
    memset(&shuffled_blueprint, 0, sizeof(shuffled_blueprint));
    memset(&frozen_blueprint, 0, sizeof(frozen_blueprint));
    memset(&real_prediction, 0, sizeof(real_prediction));
    memset(&untrained_prediction, 0, sizeof(untrained_prediction));
    memset(&shuffled_prediction, 0, sizeof(shuffled_prediction));
    memset(&frozen_prediction, 0, sizeof(frozen_prediction));
    memset(&permuted_prediction, 0, sizeof(permuted_prediction));
    memset(&last_symbol_prediction, 0, sizeof(last_symbol_prediction));
    result.trial_count = config->sequence_prediction_trial_count;
    record_capacity = config->sequence_prediction_training_epochs *
        config->sequence_prediction_sequence_count *
        (config->sequence_prediction_sequence_length - 1);
    result.trials = calloc((size_t)result.trial_count, sizeof(*result.trials));
    result.training_records = calloc((size_t)record_capacity,
                                     sizeof(*result.training_records));
    if (result.trials == NULL || result.training_records == NULL)
    {
        sequence_prediction_result_destroy(&result);
        set_error(error_message, error_message_size,
                  "memoria insuficiente para predicao de sequencia");
        return 0;
    }

    if (!train_sequences(config, blueprint, 1, 0, &trained_snn, &result,
                         error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            trained_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &learned_blueprint,
            error_message, error_message_size))
    {
        minisnn_destroy(&trained_snn);
        scenario_blueprint_destroy(&learned_blueprint);
        sequence_prediction_result_destroy(&result);
        return 0;
    }

    minisnn_destroy(&trained_snn);
    result.evaluation_reconstructed_from_blueprint = 1;

    if (!run_prediction_trials(config, &learned_blueprint, result.trials,
                               &real_prediction, 0, 0, error_message,
                               error_message_size) ||
        !run_prediction_trials(config, blueprint, NULL, &untrained_prediction,
                               0, 0, error_message, error_message_size) ||
        !train_sequences(config, blueprint, 1, 1, &shuffled_snn, NULL,
                         error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            shuffled_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &shuffled_blueprint,
            error_message, error_message_size) ||
        !run_prediction_trials(config, &shuffled_blueprint, NULL,
                               &shuffled_prediction, 0, 0, error_message,
                               error_message_size) ||
        !train_sequences(config, blueprint, 0, 0, &frozen_snn, NULL,
                         error_message, error_message_size) ||
        !scenario_runtime_capture_network(
            frozen_snn, blueprint->inhibitory_count,
            blueprint->topology_signature, &frozen_blueprint,
            error_message, error_message_size) ||
        !run_prediction_trials(config, &frozen_blueprint, NULL,
                               &frozen_prediction, 0, 0, error_message,
                               error_message_size) ||
        !run_prediction_trials(config, &learned_blueprint, NULL,
                               &permuted_prediction, 1, 0, error_message,
                               error_message_size) ||
        !run_prediction_trials(config, &learned_blueprint, NULL,
                               &last_symbol_prediction, 0, 1, error_message,
                               error_message_size))
    {
        minisnn_destroy(&shuffled_snn);
        minisnn_destroy(&frozen_snn);
        scenario_blueprint_destroy(&learned_blueprint);
        scenario_blueprint_destroy(&shuffled_blueprint);
        scenario_blueprint_destroy(&frozen_blueprint);
        sequence_prediction_result_destroy(&result);
        return 0;
    }

    minisnn_destroy(&shuffled_snn);
    minisnn_destroy(&frozen_snn);
    scenario_blueprint_destroy(&shuffled_blueprint);
    scenario_blueprint_destroy(&frozen_blueprint);
    result.correct_predictions = real_prediction.correct_predictions;
    result.next_pattern_accuracy = (double)real_prediction.correct_predictions /
        (double)real_prediction.trial_count;
    result.mean_prediction_similarity = real_prediction.mean_similarity;
    result.mean_prediction_error = real_prediction.mean_error;
    result.mean_prediction_latency = real_prediction.mean_latency;
    result.chance_accuracy = 1.0 / (double)total_patterns(config);
    result.untrained_control_accuracy =
        (double)untrained_prediction.correct_predictions /
        (double)untrained_prediction.trial_count;
    result.shuffled_order_control_accuracy =
        (double)shuffled_prediction.correct_predictions /
        (double)shuffled_prediction.trial_count;
    result.frozen_training_control_accuracy =
        (double)frozen_prediction.correct_predictions /
        (double)frozen_prediction.trial_count;
    result.permuted_labels_control_accuracy =
        (double)permuted_prediction.correct_predictions /
        (double)permuted_prediction.trial_count;
    result.context_accuracy = result.next_pattern_accuracy;
    result.last_symbol_only_control_accuracy =
        (double)last_symbol_prediction.correct_predictions /
        (double)last_symbol_prediction.trial_count;
    result.context_margin = result.context_accuracy -
        result.last_symbol_only_control_accuracy;
    result.control_accuracy = result.untrained_control_accuracy;
    if (result.shuffled_order_control_accuracy > result.control_accuracy)
        result.control_accuracy = result.shuffled_order_control_accuracy;
    if (result.frozen_training_control_accuracy > result.control_accuracy)
        result.control_accuracy = result.frozen_training_control_accuracy;
    if (result.permuted_labels_control_accuracy > result.control_accuracy)
        result.control_accuracy = result.permuted_labels_control_accuracy;
    result.prediction_margin = result.next_pattern_accuracy -
                               result.control_accuracy;
    result.evaluation_weights_changed = real_prediction.weights_changed;
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
int sequence_prediction_test_accuracy(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    double *out_accuracy,
    char *error_message,
    size_t error_message_size)
{
    PredictionAggregate aggregate;

    if (out_accuracy == NULL || config == NULL || blueprint == NULL ||
        !run_prediction_trials(config, blueprint, NULL, &aggregate, 0, 0,
                               error_message, error_message_size) ||
        aggregate.trial_count <= 0)
    {
        return 0;
    }
    *out_accuracy = (double)aggregate.correct_predictions /
                    (double)aggregate.trial_count;
    return 1;
}
#endif

static int write_training_csv(
    const SequencePredictionResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "epoch,sequence_id,position,mean_transition_weight\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    for (int index = 0; index < result->training_record_count; index++)
    {
        const SequencePredictionTrainingRecord *record =
            &result->training_records[index];

        if (fprintf(file, "%d,%d,%d,%.17g\n", record->epoch,
                    record->sequence_id, record->position,
                    record->mean_transition_weight) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_trials_csv(
    const SequencePredictionResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "trial,sequence_id,prefix,expected_next_pattern,predicted_pattern,prediction_similarity,prediction_correct,prediction_error,first_prediction_step,mean_prediction_activity\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const SequencePredictionTrial *trial = &result->trials[index];

        if (fprintf(file, "%d,%d,%s,%d,%d,%.17g,%d,%.17g,%d,%.17g\n",
                    trial->trial, trial->sequence_id, trial->prefix,
                    trial->expected_next_pattern, trial->predicted_pattern,
                    trial->prediction_similarity, trial->prediction_correct,
                    trial->prediction_error, trial->first_prediction_step,
                    trial->mean_prediction_activity) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_summary(
    const SequencePredictionResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "trial_count=%d\ncorrect_predictions=%d\n"
                "next_pattern_accuracy=%.17g\n"
                "mean_prediction_similarity=%.17g\n"
                "mean_prediction_error=%.17g\n"
                "mean_prediction_latency=%.17g\n"
                "chance_accuracy=%.17g\n"
                "context_accuracy=%.17g\n"
                "last_symbol_only_control_accuracy=%.17g\n"
                "context_margin=%.17g\n"
                "untrained_control_accuracy=%.17g\n"
                "shuffled_order_control_accuracy=%.17g\n"
                "frozen_training_control_accuracy=%.17g\n"
                "permuted_labels_control_accuracy=%.17g\n"
                "control_accuracy=%.17g\n"
                "prediction_margin=%.17g\n"
                "training_weight_absolute_change=%.17g\n"
                "evaluation_weights_changed=%d\n"
                "evaluation_reconstructed_from_blueprint=%d\n",
                result->trial_count, result->correct_predictions,
                result->next_pattern_accuracy,
                result->mean_prediction_similarity,
                result->mean_prediction_error,
                result->mean_prediction_latency, result->chance_accuracy,
                result->context_accuracy,
                result->last_symbol_only_control_accuracy,
                result->context_margin,
                result->untrained_control_accuracy,
                result->shuffled_order_control_accuracy,
                result->frozen_training_control_accuracy,
                result->permuted_labels_control_accuracy,
                result->control_accuracy, result->prediction_margin,
                result->training_weight_absolute_change,
                result->evaluation_weights_changed,
                result->evaluation_reconstructed_from_blueprint) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int write_html_report(
    const ScenarioConfig *config,
    const SequencePredictionResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fputs("<!doctype html><html><head><meta charset=\"utf-8\">"
              "<title>Predicao de sequencias</title><style>"
              "body{background:#101418;color:#e8edf2;font-family:system-ui,monospace;max-width:1200px;margin:2rem auto;padding:0 1rem}"
              "table{border-collapse:collapse;width:100%;margin:1rem 0}td,th{border:1px solid #53606b;padding:.4rem;text-align:right}th{background:#1b242c}code{color:#9fd3ff}</style></head><body>"
              "<h1>Sequencias temporais e predicao</h1><p>Durante o treino, os grupos de entrada sao apresentados em ordem e um teacher pulse supervisionado ativa o grupo alvo de previsao para gerar atividade pos-sinaptica para STDP. Durante a avaliacao, somente o prefixo recebe entrada externa; delay e probe nao recebem entrada externa.</p>"
              "<h2>Configuracao</h2><ul><li>Modelo: <code>", file) == EOF)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    write_html_text(file, minisnn_neuron_model_name(config->neuron_model));
    if (fprintf(file,
                "</code></li><li>Sequencias: %d; elementos: %d; epocas: %d</li>"
                "<li>Modo: %s; padrao: %d; gap: %d; prefixo: %d; delay: %d; probe: %d</li>"
                "<li>STDP: %s; avaliacao congelada: %s</li></ul>"
                "<h2>Metricas</h2><p>Accuracy: %.6f; similaridade media: %.6f; erro medio: %.6f; latencia media: %.6f. Acaso: %.6f; controle conservador: %.6f; margem: %.6f.</p>"
                "<p>Contexto: accuracy %.6f; controle somente ultimo simbolo %.6f; margem contextual %.6f. A decisao usa a atividade acumulada em todos os passos do probe.</p>"
                "<p>Controles: nao treinada %.6f; ordem embaralhada %.6f; treino sem STDP %.6f; labels permutados %.6f.</p>"
                "<h2>Pesos antes/depois</h2><table><thead><tr><th>id</th><th>source</th><th>target</th><th>antes</th><th>depois</th><th>delta</th></tr></thead><tbody>",
                config->sequence_prediction_sequence_count,
                config->sequence_prediction_sequence_length,
                config->sequence_prediction_training_epochs,
                config->sequence_prediction_pattern_mode,
                config->sequence_prediction_pattern_steps,
                config->sequence_prediction_inter_pattern_gap_steps,
                config->sequence_prediction_prefix_length,
                config->sequence_prediction_prediction_delay_steps,
                config->sequence_prediction_prediction_probe_steps,
                config->plasticity_enabled ? "ativada" : "desativada",
                config->sequence_prediction_freeze_plasticity_during_evaluation ?
                    "sim" : "nao",
                result->next_pattern_accuracy,
                result->mean_prediction_similarity,
                result->mean_prediction_error,
                result->mean_prediction_latency,
                result->chance_accuracy, result->control_accuracy,
                result->prediction_margin,
                result->context_accuracy,
                result->last_symbol_only_control_accuracy,
                result->context_margin,
                result->untrained_control_accuracy,
                result->shuffled_order_control_accuracy,
                result->frozen_training_control_accuracy,
                result->permuted_labels_control_accuracy) < 0)
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
    if (fputs("</tbody></table><h2>Sequencias e prefixos</h2><table><thead><tr><th>sequencia</th><th>posicoes</th><th>prefixo</th><th>proximo</th></tr></thead><tbody>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    for (int sequence_id = 0;
         sequence_id < config->sequence_prediction_sequence_count;
         sequence_id++)
    {
        char positions[SEQUENCE_PREDICTION_PREFIX_TEXT_MAX] = "";
        char prefix[SEQUENCE_PREDICTION_PREFIX_TEXT_MAX] = "";
        int next_pattern;

        for (int position = 0;
             position < config->sequence_prediction_sequence_length;
             position++)
        {
            int pattern_id = sequence_prediction_pattern_id(
                config, sequence_id, position);

            if (!append_prefix_pattern(positions, sizeof(positions), pattern_id) ||
                (position < config->sequence_prediction_prefix_length &&
                 !append_prefix_pattern(prefix, sizeof(prefix), pattern_id)))
            {
                fclose(file);
                return 0;
            }
        }
        next_pattern = sequence_prediction_pattern_id(
            config, sequence_id, config->sequence_prediction_prefix_length);
        if (fprintf(file, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td></tr>",
                    sequence_id, positions, prefix, next_pattern) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table><h2>Trials</h2><table><thead><tr><th>trial</th><th>sequencia</th><th>prefixo</th><th>esperado</th><th>previsto</th><th>similaridade</th><th>erro</th><th>correto</th><th>latencia</th><th>atividade</th></tr></thead><tbody>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const SequencePredictionTrial *trial = &result->trials[index];

        if (fprintf(file,
                    "<tr><td>%d</td><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%.6f</td><td>%.6f</td><td>%s</td><td>%d</td><td>%.6f</td></tr>",
                    trial->trial, trial->sequence_id, trial->prefix,
                    trial->expected_next_pattern, trial->predicted_pattern,
                    trial->prediction_similarity, trial->prediction_error,
                    trial->prediction_correct ? "sim" : "nao",
                    trial->first_prediction_step,
                    trial->mean_prediction_activity) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table><h2>Limitacoes</h2><p>O padrao previsto e decodificado pela maior atividade de spike da populacao de predicao durante o probe. O proximo padrao esperado e consultado somente apos a decodificacao para medir a resposta. Esta configuracao demonstra transicoes treinadas por STDP e nao estabelece capacidade geral de planejamento ou linguagem.</p><p><a href=\"sequence_prediction_training.csv\">treino CSV</a> | <a href=\"sequence_prediction_trials.csv\">trials CSV</a> | <a href=\"sequence_prediction_summary.txt\">resumo</a></p></body></html>\n", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int sequence_prediction_write_outputs(
    const ScenarioConfig *config,
    const SequencePredictionResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size)
{
    char training_path[SEQUENCE_PREDICTION_PATH_MAX];
    char trials_path[SEQUENCE_PREDICTION_PATH_MAX];
    char summary_path[SEQUENCE_PREDICTION_PATH_MAX];
    char report_path[SEQUENCE_PREDICTION_PATH_MAX];
    char checkpoint_path[SEQUENCE_PREDICTION_PATH_MAX];

    if (config == NULL || result == NULL || output_directory == NULL ||
        !make_path(training_path, sizeof(training_path), output_directory,
                   "sequence_prediction_training.csv") ||
        !make_path(trials_path, sizeof(trials_path), output_directory,
                   "sequence_prediction_trials.csv") ||
        !make_path(summary_path, sizeof(summary_path), output_directory,
                   "sequence_prediction_summary.txt") ||
        !make_path(report_path, sizeof(report_path), output_directory,
                   "sequence_prediction_report.html") ||
        !make_path(checkpoint_path, sizeof(checkpoint_path), output_directory,
                   "sequence_prediction_checkpoint.txt") ||
        !write_training_csv(result, training_path) ||
        !write_trials_csv(result, trials_path) ||
        !write_summary(result, summary_path) ||
        !write_html_report(config, result, report_path) ||
        !scenario_blueprint_write_checkpoint(
            &result->learned_blueprint, checkpoint_path, error_message,
            error_message_size))
    {
        set_error(error_message, error_message_size,
                  "erro ao gravar saidas de predicao de sequencia");
        return 0;
    }
    return 1;
}
