#include "evolution_runner.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "evolution.h"
#include "evolution_config.h"
#include "minisnn.h"
#include "scenario_runner.h"
#include "scenario_runtime.h"

#define EVOLUTION_DEFAULT_OUTPUT_ROOT "results/evolution"
#define EVOLUTION_INDEX_HEADER \
    "timestamp,experiment_name,actual_experiment_name,experiment_path,config_path,base_scenario,population_size,generations,gene_count,best_fitness,best_individual_id,status\n"
#define EVOLUTION_FAILURE_MAX 159
#define EVOLUTION_VERSION "C3-v1"
#define EVOLUTION_HASH_OFFSET 1469598103934665603ULL
#define EVOLUTION_HASH_PRIME 1099511628211ULL

typedef struct
{
    int valid;
    char failure_reason[EVOLUTION_FAILURE_MAX + 1];
    double fitness;
    int total_spikes;
    int active_timesteps;
    int first_active_step;
    int last_active_step;
    int exc_spikes;
    int inh_spikes;
    int neuron_spikes[SCENARIO_RUNTIME_MAX_NEURONS];
    double final_weight_mean;
    double final_weight_std;
    double homeostasis_rate_error_final;
    double homeostasis_rate_error_mean_absolute;
    double reward_weight_total_signed_change;
    double reward_weight_total_absolute_change;
    double reward_modified_connection_fraction;
    double term_observed[EVOLUTION_MAX_FITNESS_TERMS];
    double term_scores[EVOLUTION_MAX_FITNESS_TERMS];
} EvolutionEvaluation;

typedef struct
{
    double evaluation_seconds;
    char failure_reason[EVOLUTION_FAILURE_MAX + 1];
} IndividualRunInfo;

typedef struct
{
    FILE *generations;
    FILE *individuals;
    FILE *replicates;
    FILE *fitness_terms;
    FILE *genomes;
    FILE *lineage;
} EvolutionOutputFiles;

typedef struct
{
    EvolutionExperimentConfig config;
    ScenarioConfig base;
    ScenarioBlueprint blueprint;
    EvolutionGeneMetadata *metadata;
    size_t gene_count;
    char config_path[EVOLUTION_OUTPUT_PATH_MAX];
    char signature[32];
    char output_root[EVOLUTION_OUTPUT_PATH_MAX];
    char output_directory[EVOLUTION_OUTPUT_PATH_MAX];
    char actual_experiment_name[EVOLUTION_ACTUAL_NAME_MAX];
} EvolutionRunContext;

typedef struct
{
    FILE *population;
    FILE *raster;
    FILE *weights_initial;
    FILE *weights_final;
    int enabled;
} BestRunObserver;

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static void close_file(FILE **file)
{
    if (file != NULL && *file != NULL)
    {
        fclose(*file);
        *file = NULL;
    }
}

static void output_files_close(EvolutionOutputFiles *files)
{
    if (files == NULL)
        return;
    close_file(&files->generations);
    close_file(&files->individuals);
    close_file(&files->replicates);
    close_file(&files->fitness_terms);
    close_file(&files->genomes);
    close_file(&files->lineage);
}

static int path_join(
    char *out_path,
    size_t out_size,
    const char *directory,
    const char *name)
{
    return out_path != NULL && directory != NULL && name != NULL &&
           snprintf(out_path, out_size, "%s/%s", directory, name) <
               (int)out_size;
}

static int file_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int directory_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int ensure_directory_tree(const char *path)
{
    char buffer[EVOLUTION_OUTPUT_PATH_MAX];
    size_t length;

    if (path == NULL || path[0] == '\0' ||
        snprintf(buffer, sizeof(buffer), "%s", path) >= (int)sizeof(buffer))
    {
        return 0;
    }

    length = strlen(buffer);
    for (size_t i = 0; i < length; i++)
    {
        if ((buffer[i] == '/' || buffer[i] == '\\') && i > 0 &&
            !(i == 2 && buffer[1] == ':'))
        {
            char separator = buffer[i];
            buffer[i] = '\0';
            if (!directory_exists(buffer) &&
                !CreateDirectoryA(buffer, NULL) &&
                GetLastError() != ERROR_ALREADY_EXISTS)
            {
                return 0;
            }
            buffer[i] = separator;
        }
    }

    return directory_exists(buffer) || CreateDirectoryA(buffer, NULL) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

static void current_timestamp(
    char *out_timestamp,
    size_t out_size,
    int filename_style)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    if (filename_style)
    {
        snprintf(out_timestamp, out_size, "%04d%02d%02d_%02d%02d%02d",
                 now.wYear, now.wMonth, now.wDay,
                 now.wHour, now.wMinute, now.wSecond);
    }
    else
    {
        snprintf(out_timestamp, out_size, "%04d-%02d-%02dT%02d:%02d:%02d",
                 now.wYear, now.wMonth, now.wDay,
                 now.wHour, now.wMinute, now.wSecond);
    }
}

static int copy_file_exact(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    FILE *output;
    unsigned char buffer[4096];
    size_t count;
    int ok = 1;

    if (input == NULL)
        return 0;
    output = fopen(destination, "wb");
    if (output == NULL)
    {
        fclose(input);
        return 0;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0)
    {
        if (fwrite(buffer, 1, count, output) != count)
        {
            ok = 0;
            break;
        }
    }
    if (ferror(input))
        ok = 0;
    if (fclose(input) != 0)
        ok = 0;
    if (fclose(output) != 0)
        ok = 0;
    return ok;
}

static void hash_bytes(
    unsigned long long *hash,
    const void *data,
    size_t size)
{
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; i++)
    {
        *hash ^= (unsigned long long)bytes[i];
        *hash *= EVOLUTION_HASH_PRIME;
    }
}

static void compute_signature(EvolutionRunContext *context)
{
    unsigned long long hash = EVOLUTION_HASH_OFFSET;

    hash_bytes(&hash, &context->config, sizeof(context->config));
    hash_bytes(&hash, &context->base, sizeof(context->base));
    hash_bytes(&hash, &context->blueprint.neuron_count,
               sizeof(context->blueprint.neuron_count));
    hash_bytes(&hash, &context->blueprint.inhibitory_count,
               sizeof(context->blueprint.inhibitory_count));
    hash_bytes(&hash, &context->blueprint.topology_signature,
               sizeof(context->blueprint.topology_signature));
    hash_bytes(&hash, &context->gene_count, sizeof(context->gene_count));
    hash_bytes(&hash, context->metadata,
               context->gene_count * sizeof(*context->metadata));
    snprintf(context->signature, sizeof(context->signature), "%016llx", hash);
}

static int apply_scalar_gene(
    ScenarioConfig *scenario,
    const char *path,
    double value)
{
#define APPLY(path_name, field) \
    if (strcmp(path, path_name) == 0) \
    { \
        scenario->field = value; \
        return 1; \
    }
    APPLY("plasticity.a_plus", plasticity_a_plus);
    APPLY("plasticity.a_minus", plasticity_a_minus);
    APPLY("plasticity.tau_plus", plasticity_tau_plus);
    APPLY("plasticity.tau_minus", plasticity_tau_minus);
    APPLY("reward.learning_rate", reward_learning_rate);
    APPLY("reward.eligibility_tau", reward_eligibility_tau);
    APPLY("homeostasis.target_rate", homeostasis_target_rate);
    APPLY("homeostasis.rate_tau", homeostasis_rate_tau);
    APPLY("homeostasis.threshold_eta", homeostasis_threshold_eta);
    APPLY("homeostasis.scaling_eta", homeostasis_scaling_eta);
    APPLY("homeostasis.inhibitory_gain_eta", homeostasis_inhibitory_gain_eta);
#undef APPLY
    return 0;
}

static int build_gene_metadata(
    EvolutionRunContext *context,
    char *error_message,
    size_t error_message_size)
{
    size_t exc_count = 0;
    size_t inh_count = 0;
    size_t gene_count;
    size_t gene_index = 0;

    for (size_t connection_id = 0;
         connection_id < context->blueprint.connection_count;
         connection_id++)
    {
        const MiniSNNConnectionInfo *connection =
            &context->blueprint.connections[connection_id];
        if (connection->source_type == MINISNN_NEURON_EXCITATORY)
            exc_count++;
        else
            inh_count++;
    }

    gene_count = (size_t)context->config.scalar_gene_count +
        (context->config.evolve_exc_weights ? exc_count : 0U) +
        (context->config.evolve_inh_magnitudes ? inh_count : 0U);
    if (gene_count == 0 ||
        gene_count > SIZE_MAX / sizeof(*context->metadata))
    {
        set_error(error_message, error_message_size, "quantidade de genes invalida");
        return 0;
    }

    context->metadata = calloc(gene_count, sizeof(*context->metadata));
    if (context->metadata == NULL)
    {
        set_error(error_message, error_message_size, "memoria insuficiente para genes");
        return 0;
    }
    context->gene_count = gene_count;

    for (int i = 0; i < context->config.scalar_gene_count; i++)
    {
        const EvolutionScalarGeneConfig *source =
            &context->config.scalar_genes[i];
        EvolutionGeneMetadata *gene = &context->metadata[gene_index];
        double baseline;

        if (!evolution_config_scalar_baseline(
                &context->base,
                source->parameter_path,
                &baseline))
        {
            set_error(error_message, error_message_size, "baseline escalar indisponivel");
            return 0;
        }
        gene->gene_index = gene_index;
        gene->gene_kind = EVOLUTION_GENE_SCALAR_PARAMETER;
        gene->minimum = source->minimum;
        gene->maximum = source->maximum;
        gene->baseline_value = baseline;
        gene->mutation_scale = source->mutation_scale;
        snprintf(gene->gene_name, sizeof(gene->gene_name), "%s", source->parameter_path);
        snprintf(gene->parameter_path, sizeof(gene->parameter_path), "%s",
                 source->parameter_path);
        gene_index++;
    }

    if (context->config.evolve_exc_weights)
    {
        for (size_t connection_id = 0;
             connection_id < context->blueprint.connection_count;
             connection_id++)
        {
            const MiniSNNConnectionInfo *connection =
                &context->blueprint.connections[connection_id];
            EvolutionGeneMetadata *gene;
            if (connection->source_type != MINISNN_NEURON_EXCITATORY)
                continue;
            gene = &context->metadata[gene_index];
            gene->gene_index = gene_index;
            gene->gene_kind = EVOLUTION_GENE_EXC_CONNECTION_WEIGHT;
            gene->minimum = context->config.exc_weight_min;
            gene->maximum = context->config.exc_weight_max;
            gene->baseline_value = connection->weight;
            gene->connection_id = connection_id;
            gene->has_connection_id = 1;
            snprintf(gene->gene_name, sizeof(gene->gene_name),
                     "exc_weight_%zu", connection_id);
            gene_index++;
        }
    }

    if (context->config.evolve_inh_magnitudes)
    {
        for (size_t connection_id = 0;
             connection_id < context->blueprint.connection_count;
             connection_id++)
        {
            const MiniSNNConnectionInfo *connection =
                &context->blueprint.connections[connection_id];
            EvolutionGeneMetadata *gene;
            if (connection->source_type != MINISNN_NEURON_INHIBITORY)
                continue;
            gene = &context->metadata[gene_index];
            gene->gene_index = gene_index;
            gene->gene_kind = EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE;
            gene->minimum = context->config.inh_magnitude_min;
            gene->maximum = context->config.inh_magnitude_max;
            gene->baseline_value = -connection->weight;
            gene->connection_id = connection_id;
            gene->has_connection_id = 1;
            snprintf(gene->gene_name, sizeof(gene->gene_name),
                     "inh_magnitude_%zu", connection_id);
            gene_index++;
        }
    }

    if (gene_index != gene_count ||
        !evolution_gene_metadata_is_valid(context->metadata, gene_count))
    {
        set_error(error_message, error_message_size, "metadados de genoma invalidos");
        return 0;
    }
    return 1;
}

static int context_load(
    EvolutionRunContext *context,
    const char *config_path,
    const char *output_root,
    char *error_message,
    size_t error_message_size)
{
    memset(context, 0, sizeof(*context));
    if (snprintf(context->config_path, sizeof(context->config_path), "%s",
                 config_path) >= (int)sizeof(context->config_path) ||
        snprintf(context->output_root, sizeof(context->output_root), "%s",
                 output_root) >= (int)sizeof(context->output_root))
    {
        set_error(error_message, error_message_size, "caminho muito longo");
        return 0;
    }

    if (!evolution_config_load_file(
            config_path,
            &context->config,
            &context->base,
            error_message,
            error_message_size) ||
        !scenario_runner_capture_blueprint(
            &context->base,
            &context->blueprint,
            error_message,
            error_message_size) ||
        !build_gene_metadata(context, error_message, error_message_size))
    {
        return 0;
    }

    compute_signature(context);
    return 1;
}

static void context_destroy(EvolutionRunContext *context)
{
    if (context == NULL)
        return;
    scenario_blueprint_destroy(&context->blueprint);
    free(context->metadata);
    memset(context, 0, sizeof(*context));
}

static int create_output_directory(
    EvolutionRunContext *context,
    char *error_message,
    size_t error_message_size)
{
    char timestamp[32];

    if (!ensure_directory_tree(context->output_root))
    {
        set_error(error_message, error_message_size, "erro ao criar results/evolution");
        return 0;
    }

    if (snprintf(context->actual_experiment_name,
                 sizeof(context->actual_experiment_name), "%s",
                 context->config.experiment_name) >=
        (int)sizeof(context->actual_experiment_name))
    {
        return 0;
    }

    if (context->config.auto_unique_run)
    {
        char candidate[EVOLUTION_OUTPUT_PATH_MAX];
        if (!path_join(candidate, sizeof(candidate), context->output_root,
                       context->actual_experiment_name))
            return 0;
        if (directory_exists(candidate))
        {
            current_timestamp(timestamp, sizeof(timestamp), 1);
            for (int suffix = 0; suffix < 1000; suffix++)
            {
                if (suffix == 0)
                {
                    snprintf(context->actual_experiment_name,
                             sizeof(context->actual_experiment_name), "%s_%s",
                             context->config.experiment_name, timestamp);
                }
                else
                {
                    snprintf(context->actual_experiment_name,
                             sizeof(context->actual_experiment_name), "%s_%s_%d",
                             context->config.experiment_name, timestamp, suffix + 1);
                }
                if (!path_join(candidate, sizeof(candidate), context->output_root,
                               context->actual_experiment_name))
                    return 0;
                if (!directory_exists(candidate))
                    break;
            }
        }
    }

    if (!path_join(context->output_directory, sizeof(context->output_directory),
                   context->output_root, context->actual_experiment_name) ||
        !ensure_directory_tree(context->output_directory))
    {
        set_error(error_message, error_message_size, "erro ao criar pasta do experimento");
        return 0;
    }
    return 1;
}

static int copy_used_configs(
    const EvolutionRunContext *context,
    char *error_message,
    size_t error_message_size)
{
    char evolution_path[EVOLUTION_OUTPUT_PATH_MAX];
    char base_path[EVOLUTION_OUTPUT_PATH_MAX];

    if (!path_join(evolution_path, sizeof(evolution_path),
                   context->output_directory, "evolution_config_used.ini") ||
        !path_join(base_path, sizeof(base_path),
                   context->output_directory, "base_scenario_used.ini") ||
        !copy_file_exact(context->config_path, evolution_path) ||
        !copy_file_exact(context->config.base_scenario, base_path))
    {
        set_error(error_message, error_message_size, "erro ao copiar configuracoes usadas");
        return 0;
    }
    return 1;
}

static int apply_genome(
    const EvolutionRunContext *context,
    const double *genes,
    ScenarioConfig *out_scenario,
    MiniSNN **out_snn,
    char *error_message,
    size_t error_message_size)
{
    ScenarioConfig scenario = context->base;
    MiniSNN *snn = NULL;

    for (size_t gene_index = 0; gene_index < context->gene_count; gene_index++)
    {
        const EvolutionGeneMetadata *metadata = &context->metadata[gene_index];
        double value = genes[gene_index];
        if (!isfinite(value) || value < metadata->minimum || value > metadata->maximum)
        {
            set_error(error_message, error_message_size, "gene fora dos limites");
            return 0;
        }
        if (metadata->gene_kind == EVOLUTION_GENE_SCALAR_PARAMETER &&
            !apply_scalar_gene(&scenario, metadata->parameter_path, value))
        {
            set_error(error_message, error_message_size, "gene escalar nao aplicavel");
            return 0;
        }
    }

    if (!scenario_runtime_create_from_blueprint(
            &scenario, &context->blueprint, &snn,
            error_message, error_message_size))
        return 0;

    for (size_t gene_index = 0; gene_index < context->gene_count; gene_index++)
    {
        const EvolutionGeneMetadata *metadata = &context->metadata[gene_index];
        double weight;
        if (metadata->gene_kind == EVOLUTION_GENE_SCALAR_PARAMETER)
            continue;
        weight = metadata->gene_kind == EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE ?
            -genes[gene_index] : genes[gene_index];
        if (!minisnn_set_connection_weight(snn, metadata->connection_id, weight))
        {
            minisnn_destroy(&snn);
            set_error(error_message, error_message_size, "erro ao aplicar gene de conexao");
            return 0;
        }
    }

    if (!scenario_runtime_configure_modules(
            snn, &scenario, error_message, error_message_size))
    {
        minisnn_destroy(&snn);
        return 0;
    }

    *out_scenario = scenario;
    *out_snn = snn;
    return 1;
}

static int collect_final_weights(
    const MiniSNN *snn,
    double *out_mean,
    double *out_std)
{
    size_t count = minisnn_connection_count(snn);
    double sum = 0.0;
    double square_sum = 0.0;

    for (size_t i = 0; i < count; i++)
    {
        double weight;
        if (!minisnn_get_connection_weight(snn, i, &weight) || !isfinite(weight))
            return 0;
        sum += weight;
        square_sum += weight * weight;
    }

    if (count == 0)
    {
        *out_mean = 0.0;
        *out_std = 0.0;
    }
    else
    {
        double variance;
        *out_mean = sum / (double)count;
        variance = square_sum / (double)count - *out_mean * *out_mean;
        if (variance < 0.0 && variance > -1e-12)
            variance = 0.0;
        if (variance < 0.0)
            return 0;
        *out_std = sqrt(variance);
    }
    return isfinite(*out_mean) && isfinite(*out_std);
}

static int observed_metric(
    const EvolutionEvaluation *evaluation,
    const EvolutionFitnessTermConfig *term,
    int steps,
    double *out_value)
{
    if (strcmp(term->metric, "activity_total_spikes") == 0)
        *out_value = (double)evaluation->total_spikes;
    else if (strcmp(term->metric, "activity_active_fraction") == 0)
        *out_value = (double)evaluation->active_timesteps / (double)steps;
    else if (strcmp(term->metric, "activity_mean_spikes_per_step") == 0)
        *out_value = (double)evaluation->total_spikes / (double)steps;
    else if (strcmp(term->metric, "activity_first_active_step") == 0)
        *out_value = (double)evaluation->first_active_step;
    else if (strcmp(term->metric, "activity_last_active_step") == 0)
        *out_value = (double)evaluation->last_active_step;
    else if (strcmp(term->metric, "exc_total_spikes") == 0)
        *out_value = (double)evaluation->exc_spikes;
    else if (strcmp(term->metric, "inh_total_spikes") == 0)
        *out_value = (double)evaluation->inh_spikes;
    else if (term->has_neuron_id)
        *out_value = (double)evaluation->neuron_spikes[term->neuron_id];
    else if (strcmp(term->metric, "network_final_weight_mean") == 0)
        *out_value = evaluation->final_weight_mean;
    else if (strcmp(term->metric, "network_final_weight_std") == 0)
        *out_value = evaluation->final_weight_std;
    else if (strcmp(term->metric, "homeostasis_rate_error_final") == 0)
        *out_value = evaluation->homeostasis_rate_error_final;
    else if (strcmp(term->metric, "homeostasis_rate_error_mean_absolute") == 0)
        *out_value = evaluation->homeostasis_rate_error_mean_absolute;
    else if (strcmp(term->metric, "reward_weight_total_signed_change") == 0)
        *out_value = evaluation->reward_weight_total_signed_change;
    else if (strcmp(term->metric, "reward_weight_total_absolute_change") == 0)
        *out_value = evaluation->reward_weight_total_absolute_change;
    else if (strcmp(term->metric, "reward_modified_connection_fraction") == 0)
        *out_value = evaluation->reward_modified_connection_fraction;
    else
        return 0;
    return isfinite(*out_value);
}

static int write_best_step(
    BestRunObserver *observer,
    const ScenarioConfig *scenario,
    int inhibitory_count,
    const ScenarioRuntimeStep *step)
{
    if (observer == NULL || !observer->enabled)
        return 1;

    if (fprintf(observer->population, "%d,%d,%d,%d,%.2f,%.2f\n",
                step->step, step->spikes_total, step->spikes_exc,
                step->spikes_inh,
                step->voltage_sum / (double)scenario->neurons,
                step->synaptic_current_sum / (double)scenario->neurons) < 0)
        return 0;
    for (int neuron_id = 0; neuron_id < scenario->neurons; neuron_id++)
    {
        if (step->spikes[neuron_id] &&
            fprintf(observer->raster, "%d,%d,%s\n", step->step, neuron_id,
                    scenario_runtime_type_name(neuron_id, scenario->neurons,
                                               inhibitory_count)) < 0)
            return 0;
    }
    return 1;
}

static int write_weight_snapshot(FILE *file, const MiniSNN *snn)
{
    size_t connection_count;

    if (file == NULL)
        return 1;
    connection_count = minisnn_connection_count(snn);
    for (size_t connection_id = 0;
         connection_id < connection_count;
         connection_id++)
    {
        MiniSNNConnectionInfo connection;
        if (!minisnn_get_connection(snn, connection_id, &connection) ||
            fprintf(file, "%zu,%zu,%zu,%s,%s,%u,%.17g\n",
                    connection_id, connection.source, connection.target,
                    connection.source_type == MINISNN_NEURON_INHIBITORY ? "INH" : "EXC",
                    connection.target_type == MINISNN_NEURON_INHIBITORY ? "INH" : "EXC",
                    connection.delay, connection.weight) < 0)
        {
            return 0;
        }
    }
    return 1;
}

static int evaluate_genome(
    const EvolutionRunContext *context,
    const double *genes,
    uint64_t evaluation_seed,
    BestRunObserver *observer,
    EvolutionEvaluation *out_evaluation)
{
    ScenarioConfig scenario;
    MiniSNN *snn = NULL;
    EvolutionEvaluation evaluation;
    char runtime_error[EVOLUTION_FAILURE_MAX + 1] = {0};
    double scores[EVOLUTION_MAX_FITNESS_TERMS];
    double weights[EVOLUTION_MAX_FITNESS_TERMS];

    (void)evaluation_seed;
    memset(&evaluation, 0, sizeof(evaluation));
    evaluation.first_active_step = -1;
    evaluation.last_active_step = -1;

    if (!apply_genome(context, genes, &scenario, &snn,
                      runtime_error, sizeof(runtime_error)))
    {
        snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                 "%s", runtime_error[0] != '\0' ? runtime_error : "falha ao criar fenotipo");
        *out_evaluation = evaluation;
        return 1;
    }

    if (observer != NULL && observer->enabled &&
        !write_weight_snapshot(observer->weights_initial, snn))
    {
        snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                 "falha ao registrar pesos iniciais");
        minisnn_destroy(&snn);
        *out_evaluation = evaluation;
        return 1;
    }

    for (int step = 0; step < scenario.steps; step++)
    {
        ScenarioRuntimeStep runtime_step;
        if (!scenario_runtime_step(
                snn, &scenario, context->blueprint.inhibitory_count,
                step, &runtime_step, runtime_error, sizeof(runtime_error)) ||
            !write_best_step(observer, &scenario,
                             context->blueprint.inhibitory_count, &runtime_step))
        {
            snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                     "%s", runtime_error[0] != '\0' ? runtime_error : "falha durante avaliacao");
            minisnn_destroy(&snn);
            *out_evaluation = evaluation;
            return 1;
        }

        evaluation.total_spikes += runtime_step.spikes_total;
        evaluation.exc_spikes += runtime_step.spikes_exc;
        evaluation.inh_spikes += runtime_step.spikes_inh;
        if (runtime_step.spikes_total > 0)
        {
            if (evaluation.first_active_step < 0)
                evaluation.first_active_step = step;
            evaluation.last_active_step = step;
            evaluation.active_timesteps++;
        }
        for (int neuron_id = 0; neuron_id < scenario.neurons; neuron_id++)
            evaluation.neuron_spikes[neuron_id] += runtime_step.spikes[neuron_id];
    }

    if (!collect_final_weights(
            snn,
            &evaluation.final_weight_mean,
            &evaluation.final_weight_std))
    {
        snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                 "pesos finais invalidos");
        minisnn_destroy(&snn);
        *out_evaluation = evaluation;
        return 1;
    }

    if (scenario.homeostasis_enabled)
    {
        MiniSNNHomeostasisStats stats;
        if (!minisnn_get_homeostasis_stats(snn, &stats))
        {
            snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                     "metricas de homeostase indisponiveis");
            minisnn_destroy(&snn);
            *out_evaluation = evaluation;
            return 1;
        }
        evaluation.homeostasis_rate_error_final =
            fabs(stats.final_population_rate - scenario.homeostasis_target_rate);
        evaluation.homeostasis_rate_error_mean_absolute =
            stats.update_count > 0 ?
            stats.rate_error_absolute_sum / (double)stats.update_count :
            evaluation.homeostasis_rate_error_final;
    }

    if (scenario.reward_enabled)
    {
        MiniSNNRewardStats stats;
        if (!minisnn_get_reward_stats(snn, &stats))
        {
            snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                     "metricas de reward indisponiveis");
            minisnn_destroy(&snn);
            *out_evaluation = evaluation;
            return 1;
        }
        evaluation.reward_weight_total_signed_change =
            stats.total_signed_weight_change;
        evaluation.reward_weight_total_absolute_change =
            stats.total_absolute_weight_change;
        evaluation.reward_modified_connection_fraction =
            stats.eligible_connection_count > 0 ?
            (double)stats.modified_connection_count /
                (double)stats.eligible_connection_count : 0.0;
    }

    for (int term_index = 0;
         term_index < context->config.fitness_term_count;
         term_index++)
    {
        const EvolutionFitnessTermConfig *term =
            &context->config.fitness_terms[term_index];
        double observed;
        if (!observed_metric(&evaluation, term, scenario.steps, &observed))
        {
            snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                     "metrica de fitness indisponivel: %s", term->metric);
            minisnn_destroy(&snn);
            *out_evaluation = evaluation;
            return 1;
        }
        evaluation.term_observed[term_index] = observed;
        evaluation.term_scores[term_index] = evolution_fitness_score(
            term->goal, observed, term->target, term->scale);
        scores[term_index] = evaluation.term_scores[term_index];
        weights[term_index] = term->weight;
    }

    evaluation.valid = evolution_fitness_weighted_mean(
        scores, weights, (size_t)context->config.fitness_term_count,
        &evaluation.fitness);
    if (!evaluation.valid)
        snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                 "fitness nao finito");

    if (observer != NULL && observer->enabled &&
        !write_weight_snapshot(observer->weights_final, snn))
    {
        evaluation.valid = 0;
        snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                 "falha ao registrar pesos finais");
    }

    minisnn_destroy(&snn);
    *out_evaluation = evaluation;
    return 1;
}

static int output_files_open(
    const EvolutionRunContext *context,
    int append,
    EvolutionOutputFiles *files,
    char *error_message,
    size_t error_message_size)
{
    static const char *names[] = {
        "generations.csv", "individuals.csv", "replicates.csv",
        "fitness_terms.csv", "genomes.csv", "lineage.csv"
    };
    FILE **targets[] = {
        &files->generations, &files->individuals, &files->replicates,
        &files->fitness_terms, &files->genomes, &files->lineage
    };

    memset(files, 0, sizeof(*files));
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        char path[EVOLUTION_OUTPUT_PATH_MAX];
        if (!path_join(path, sizeof(path), context->output_directory, names[i]))
        {
            output_files_close(files);
            return 0;
        }
        *targets[i] = fopen(path, append ? "a" : "w");
        if (*targets[i] == NULL)
        {
            output_files_close(files);
            set_error(error_message, error_message_size, "erro ao abrir CSV evolutivo");
            return 0;
        }
    }

    if (!append)
    {
        if (fprintf(files->generations,
                "generation,population_size,valid_individual_count,invalid_individual_count,fitness_best,fitness_mean,fitness_min,fitness_max,fitness_std,replicate_fitness_mean,replicate_fitness_std_mean,best_individual_id,global_best_individual_id,global_best_fitness,diversity_mean_gene_std,diversity_mean_pair_distance,elite_count,crossover_child_count,clone_child_count,mutated_child_count,mutation_count,evaluation_seconds,generation_wall_seconds\n") < 0 ||
            fprintf(files->individuals,
                "generation,individual_id,status,parent_a_id,parent_b_id,operation,fitness_selection,fitness_mean,fitness_std,fitness_min,fitness_max,valid_replicates,invalid_replicates,mutation_count,mutation_absolute_sum,mutation_max_absolute,crossover_applied,genes_from_parent_a,genes_from_parent_b,is_generation_best,is_global_best,evaluation_seconds,failure_reason\n") < 0 ||
            fprintf(files->replicates,
                "generation,individual_id,replicate_index,evaluation_seed,status,fitness,total_spikes,active_fraction,exc_spikes,inh_spikes,first_active_step,last_active_step,final_weight_mean,final_weight_std,homeostasis_rate_error_final,reward_weight_total_signed_change,failure_reason,evaluation_seconds\n") < 0 ||
            fprintf(files->fitness_terms,
                "generation,individual_id,replicate_index,term_index,metric,goal,observed_value,target,scale,weight,term_score,weighted_score,status\n") < 0 ||
            fprintf(files->genomes,
                "generation,individual_id,gene_index,gene_name,gene_kind,value,minimum,maximum,baseline_value,connection_id,parameter_path\n") < 0 ||
            fprintf(files->lineage,
                "child_generation,child_individual_id,parent_a_generation,parent_a_id,parent_b_generation,parent_b_id,operation,crossover_applied,mutation_count\n") < 0)
        {
            output_files_close(files);
            set_error(error_message, error_message_size, "erro ao escrever cabecalhos evolutivos");
            return 0;
        }
    }
    return 1;
}

static const char *gene_kind_name(EvolutionGeneKind kind)
{
    if (kind == EVOLUTION_GENE_SCALAR_PARAMETER)
        return "SCALAR_PARAMETER";
    if (kind == EVOLUTION_GENE_EXC_CONNECTION_WEIGHT)
        return "EXC_CONNECTION_WEIGHT";
    return "INH_CONNECTION_MAGNITUDE";
}

static int write_lineage_population(
    FILE *file,
    const EvolutionEngine *engine)
{
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionIndividual *individual = &engine->population[i];
        char parent_a_generation[32] = "NA";
        char parent_b_generation[32] = "NA";
        char parent_a_id[32] = "NA";
        char parent_b_id[32] = "NA";

        if (individual->has_parent_a)
        {
            snprintf(parent_a_generation, sizeof(parent_a_generation), "%d",
                     individual->generation - 1);
            snprintf(parent_a_id, sizeof(parent_a_id), "%llu",
                     (unsigned long long)individual->parent_a_id);
        }
        if (individual->has_parent_b)
        {
            snprintf(parent_b_generation, sizeof(parent_b_generation), "%d",
                     individual->generation - 1);
            snprintf(parent_b_id, sizeof(parent_b_id), "%llu",
                     (unsigned long long)individual->parent_b_id);
        }
        if (fprintf(file, "%d,%llu,%s,%s,%s,%s,%s,%d,%zu\n",
                    individual->generation,
                    (unsigned long long)individual->individual_id,
                    parent_a_generation, parent_a_id,
                    parent_b_generation, parent_b_id,
                    individual->operation,
                    individual->crossover_applied,
                    individual->mutation.mutation_count) < 0)
            return 0;
    }
    return fflush(file) == 0;
}

static int should_save_genome(
    const EvolutionRunContext *context,
    size_t population_index,
    const size_t *ranking)
{
    if (context->config.save_all_genomes)
        return 1;
    for (int i = 0; i < context->config.elite_count; i++)
    {
        if (ranking[i] == population_index)
            return 1;
    }
    return population_index == ranking[0];
}

static int write_generation_genomes(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    const size_t *ranking,
    FILE *file)
{
    for (size_t population_index = 0;
         population_index < engine->config.population_size;
         population_index++)
    {
        const EvolutionIndividual *individual =
            &engine->population[population_index];
        if (!should_save_genome(context, population_index, ranking))
            continue;
        for (size_t gene_index = 0; gene_index < context->gene_count; gene_index++)
        {
            const EvolutionGeneMetadata *metadata = &context->metadata[gene_index];
            char connection_id[32] = "NA";
            const char *parameter_path = metadata->parameter_path[0] != '\0' ?
                metadata->parameter_path : "NA";
            if (metadata->has_connection_id)
                snprintf(connection_id, sizeof(connection_id), "%zu",
                         metadata->connection_id);
            if (fprintf(file, "%d,%llu,%zu,%s,%s,%.17g,%.17g,%.17g,%.17g,%s,%s\n",
                        individual->generation,
                        (unsigned long long)individual->individual_id,
                        gene_index, metadata->gene_name,
                        gene_kind_name(metadata->gene_kind),
                        individual->genes[gene_index], metadata->minimum,
                        metadata->maximum, metadata->baseline_value,
                        connection_id, parameter_path) < 0)
                return 0;
        }
    }
    return fflush(file) == 0;
}

static int write_replicate(
    const EvolutionRunContext *context,
    const EvolutionIndividual *individual,
    int replicate_index,
    uint64_t seed,
    const EvolutionEvaluation *evaluation,
    double seconds,
    EvolutionOutputFiles *files)
{
    const char *status = evaluation->valid ? "OK" : "INVALID_EVALUATION";
    double active_fraction = (double)evaluation->active_timesteps /
        (double)context->base.steps;

    if (fprintf(files->replicates,
            "%d,%llu,%d,%llu,%s,%.17g,%d,%.17g,%d,%d,%d,%d,%.17g,%.17g,",
            individual->generation,
            (unsigned long long)individual->individual_id,
            replicate_index, (unsigned long long)seed, status,
            evaluation->valid ? evaluation->fitness : 0.0,
            evaluation->total_spikes, active_fraction,
            evaluation->exc_spikes, evaluation->inh_spikes,
            evaluation->first_active_step, evaluation->last_active_step,
            evaluation->final_weight_mean, evaluation->final_weight_std) < 0)
        return 0;

    if (context->base.homeostasis_enabled)
    {
        if (fprintf(files->replicates, "%.17g,",
                    evaluation->homeostasis_rate_error_final) < 0)
            return 0;
    }
    else if (fprintf(files->replicates, "NA,") < 0)
        return 0;

    if (context->base.reward_enabled)
    {
        if (fprintf(files->replicates, "%.17g,",
                    evaluation->reward_weight_total_signed_change) < 0)
            return 0;
    }
    else if (fprintf(files->replicates, "NA,") < 0)
        return 0;

    if (fprintf(files->replicates, "%s,%.6f\n",
                evaluation->valid ? "" : evaluation->failure_reason,
                seconds) < 0)
        return 0;

    for (int term_index = 0;
         term_index < context->config.fitness_term_count;
         term_index++)
    {
        const EvolutionFitnessTermConfig *term =
            &context->config.fitness_terms[term_index];
        double observed = evaluation->valid ?
            evaluation->term_observed[term_index] : 0.0;
        double score = evaluation->valid ?
            evaluation->term_scores[term_index] : 0.0;
        if (fprintf(files->fitness_terms,
                "%d,%llu,%d,%d,%s,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%s\n",
                individual->generation,
                (unsigned long long)individual->individual_id,
                replicate_index, term_index, term->metric,
                evolution_fitness_goal_name(term->goal), observed,
                term->target, term->scale, term->weight, score,
                score * term->weight, status) < 0)
            return 0;
    }
    return 1;
}

static int evaluate_population(
    EvolutionRunContext *context,
    EvolutionEngine *engine,
    EvolutionOutputFiles *files,
    IndividualRunInfo *run_info,
    double *out_evaluation_seconds)
{
    ULONGLONG population_start = GetTickCount64();

    for (size_t population_index = 0;
         population_index < engine->config.population_size;
         population_index++)
    {
        EvolutionIndividual *individual = &engine->population[population_index];
        double *replicate_fitness = calloc(
            (size_t)context->config.evaluation_replicates,
            sizeof(*replicate_fitness));
        double fitness_mean;
        double fitness_std;
        double fitness_selection;
        double fitness_min = 1.0;
        double fitness_max = 0.0;
        int valid_replicates = 0;
        ULONGLONG individual_start = GetTickCount64();

        if (replicate_fitness == NULL)
            return 0;
        run_info[population_index].failure_reason[0] = '\0';

        for (int replicate_index = 0;
             replicate_index < context->config.evaluation_replicates;
             replicate_index++)
        {
            EvolutionEvaluation evaluation;
            uint64_t evaluation_seed =
                context->config.evaluation_seed_base + (uint64_t)replicate_index;
            ULONGLONG replicate_start = GetTickCount64();
            double replicate_seconds;

            if (!evaluate_genome(context, individual->genes,
                                 evaluation_seed, NULL, &evaluation))
            {
                free(replicate_fitness);
                return 0;
            }
            replicate_seconds =
                (double)(GetTickCount64() - replicate_start) / 1000.0;
            replicate_fitness[replicate_index] =
                evaluation.valid ? evaluation.fitness : 0.0;
            if (evaluation.valid)
                valid_replicates++;
            else if (run_info[population_index].failure_reason[0] == '\0')
                snprintf(run_info[population_index].failure_reason,
                         sizeof(run_info[population_index].failure_reason), "%s",
                         evaluation.failure_reason);

            if (replicate_fitness[replicate_index] < fitness_min)
                fitness_min = replicate_fitness[replicate_index];
            if (replicate_fitness[replicate_index] > fitness_max)
                fitness_max = replicate_fitness[replicate_index];
            if (!write_replicate(context, individual, replicate_index,
                                 evaluation_seed, &evaluation,
                                 replicate_seconds, files))
            {
                free(replicate_fitness);
                return 0;
            }
        }

        if (!evolution_aggregate_replicates(
                replicate_fitness,
                (size_t)context->config.evaluation_replicates,
                context->config.replicate_std_penalty,
                &fitness_mean, &fitness_std, &fitness_selection) ||
            !evolution_engine_set_evaluation(
                engine, population_index, fitness_mean, fitness_std,
                fitness_min, fitness_max, valid_replicates,
                context->config.evaluation_replicates - valid_replicates))
        {
            free(replicate_fitness);
            return 0;
        }
        if (valid_replicates == 0)
        {
            individual->fitness_selection = 0.0;
            individual->fitness_mean = 0.0;
            individual->fitness_std = 0.0;
        }
        run_info[population_index].evaluation_seconds =
            (double)(GetTickCount64() - individual_start) / 1000.0;
        free(replicate_fitness);
    }

    *out_evaluation_seconds =
        (double)(GetTickCount64() - population_start) / 1000.0;
    return fflush(files->replicates) == 0 &&
           fflush(files->fitness_terms) == 0;
}

static void diversity_metrics(
    const EvolutionEngine *engine,
    double *out_mean_gene_std,
    double *out_mean_pair_distance)
{
    double gene_std_sum = 0.0;
    double distance_sum = 0.0;
    size_t pair_count = 0;

    for (size_t gene = 0; gene < engine->gene_count; gene++)
    {
        double mean = 0.0;
        double variance = 0.0;
        for (size_t i = 0; i < engine->config.population_size; i++)
            mean += engine->population[i].genes[gene];
        mean /= (double)engine->config.population_size;
        for (size_t i = 0; i < engine->config.population_size; i++)
        {
            double difference = engine->population[i].genes[gene] - mean;
            variance += difference * difference;
        }
        gene_std_sum += sqrt(variance / (double)engine->config.population_size);
    }

    for (size_t left = 0; left < engine->config.population_size; left++)
    {
        for (size_t right = left + 1;
             right < engine->config.population_size;
             right++)
        {
            double square_sum = 0.0;
            for (size_t gene = 0; gene < engine->gene_count; gene++)
            {
                double range = engine->metadata[gene].maximum -
                    engine->metadata[gene].minimum;
                double difference = (engine->population[left].genes[gene] -
                    engine->population[right].genes[gene]) / range;
                square_sum += difference * difference;
            }
            distance_sum += sqrt(square_sum / (double)engine->gene_count);
            pair_count++;
        }
    }

    *out_mean_gene_std = gene_std_sum / (double)engine->gene_count;
    *out_mean_pair_distance = pair_count > 0 ?
        distance_sum / (double)pair_count : 0.0;
}

static int write_generation_summary(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    const size_t *ranking,
    const IndividualRunInfo *run_info,
    double evaluation_seconds,
    double wall_seconds,
    EvolutionOutputFiles *files)
{
    double mean = 0.0;
    double variance = 0.0;
    double minimum = 1.0;
    double maximum = 0.0;
    double replicate_mean = 0.0;
    double replicate_std_mean = 0.0;
    double diversity_gene_std;
    double diversity_pair_distance;
    int valid_count = 0;
    size_t crossover_count = 0;
    size_t clone_count = 0;
    size_t mutated_count = 0;
    size_t mutation_count = 0;

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionIndividual *individual = &engine->population[i];
        mean += individual->fitness_selection;
        replicate_mean += individual->fitness_mean;
        replicate_std_mean += individual->fitness_std;
        if (individual->fitness_selection < minimum)
            minimum = individual->fitness_selection;
        if (individual->fitness_selection > maximum)
            maximum = individual->fitness_selection;
        if (individual->valid_replicates > 0)
            valid_count++;
        if (individual->crossover_applied)
            crossover_count++;
        else if (individual->has_parent_a &&
                 strcmp(individual->operation, "elite_copy") != 0)
            clone_count++;
        if (individual->mutation.mutation_count > 0)
            mutated_count++;
        mutation_count += individual->mutation.mutation_count;
    }
    mean /= (double)engine->config.population_size;
    replicate_mean /= (double)engine->config.population_size;
    replicate_std_mean /= (double)engine->config.population_size;
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        double difference = engine->population[i].fitness_selection - mean;
        variance += difference * difference;
    }
    variance = sqrt(variance / (double)engine->config.population_size);
    diversity_metrics(engine, &diversity_gene_std, &diversity_pair_distance);

    if (fprintf(files->generations,
            "%d,%zu,%d,%zu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,"
            "%llu,%llu,%.17g,%.17g,%.17g,%zu,%zu,%zu,%zu,%zu,%.6f,%.6f\n",
            engine->current_generation,
            engine->config.population_size,
            valid_count,
            engine->config.population_size - (size_t)valid_count,
            engine->population[ranking[0]].fitness_selection,
            mean, minimum, maximum, variance,
            replicate_mean, replicate_std_mean,
            (unsigned long long)engine->population[ranking[0]].individual_id,
            (unsigned long long)engine->global_best_individual_id,
            engine->global_best_fitness_selection,
            diversity_gene_std, diversity_pair_distance,
            engine->config.elite_count,
            crossover_count, clone_count, mutated_count, mutation_count,
            evaluation_seconds, wall_seconds) < 0)
        return 0;

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionIndividual *individual = &engine->population[i];
        char parent_a[32] = "NA";
        char parent_b[32] = "NA";
        const char *status = individual->valid_replicates > 0 ?
            "OK" : "INVALID_EVALUATION";
        if (individual->has_parent_a)
            snprintf(parent_a, sizeof(parent_a), "%llu",
                     (unsigned long long)individual->parent_a_id);
        if (individual->has_parent_b)
            snprintf(parent_b, sizeof(parent_b), "%llu",
                     (unsigned long long)individual->parent_b_id);
        if (fprintf(files->individuals,
                "%d,%llu,%s,%s,%s,%s,%.17g,%.17g,%.17g,%.17g,%.17g,"
                "%d,%d,%zu,%.17g,%.17g,%d,%zu,%zu,%d,%d,%.6f,%s\n",
                individual->generation,
                (unsigned long long)individual->individual_id,
                status, parent_a, parent_b, individual->operation,
                individual->fitness_selection, individual->fitness_mean,
                individual->fitness_std, individual->fitness_min,
                individual->fitness_max, individual->valid_replicates,
                individual->invalid_replicates,
                individual->mutation.mutation_count,
                individual->mutation.mutation_absolute_sum,
                individual->mutation.mutation_max_absolute,
                individual->crossover_applied,
                individual->genes_from_parent_a,
                individual->genes_from_parent_b,
                i == ranking[0],
                individual->individual_id == engine->global_best_individual_id,
                run_info[i].evaluation_seconds,
                run_info[i].failure_reason) < 0)
            return 0;
    }

    return write_generation_genomes(context, engine, ranking, files->genomes) &&
           fflush(files->generations) == 0 && fflush(files->individuals) == 0;
}

static int write_best_genome_atomic(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine)
{
    char final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;

    if (!path_join(final_path, sizeof(final_path), context->output_directory,
                   "best_genome.csv") ||
        !path_join(temp_path, sizeof(temp_path), context->output_directory,
                   "best_genome.tmp"))
        return 0;
    file = fopen(temp_path, "w");
    if (file == NULL)
        return 0;
    if (fprintf(file,
            "individual_id,generation,fitness_selection,gene_index,gene_name,gene_kind,value,minimum,maximum,baseline_value,connection_id,parameter_path\n") < 0)
    {
        fclose(file);
        DeleteFileA(temp_path);
        return 0;
    }
    for (size_t gene_index = 0; gene_index < context->gene_count; gene_index++)
    {
        const EvolutionGeneMetadata *metadata = &context->metadata[gene_index];
        char connection_id[32] = "NA";
        const char *path = metadata->parameter_path[0] != '\0' ?
            metadata->parameter_path : "NA";
        if (metadata->has_connection_id)
            snprintf(connection_id, sizeof(connection_id), "%zu",
                     metadata->connection_id);
        if (fprintf(file, "%llu,%d,%.17g,%zu,%s,%s,%.17g,%.17g,%.17g,%.17g,%s,%s\n",
                    (unsigned long long)engine->global_best_individual_id,
                    engine->global_best_generation,
                    engine->global_best_fitness_selection,
                    gene_index, metadata->gene_name,
                    gene_kind_name(metadata->gene_kind),
                    engine->global_best_genes[gene_index], metadata->minimum,
                    metadata->maximum, metadata->baseline_value,
                    connection_id, path) < 0)
        {
            fclose(file);
            DeleteFileA(temp_path);
            return 0;
        }
    }
    if (fclose(file) != 0 ||
        !MoveFileExA(temp_path, final_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temp_path);
        return 0;
    }
    return 1;
}

static int write_checkpoint_atomic(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    int next_generation,
    int completed)
{
    char final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;

    if (!path_join(final_path, sizeof(final_path), context->output_directory,
                   "checkpoint.txt") ||
        !path_join(temp_path, sizeof(temp_path), context->output_directory,
                   "checkpoint.tmp"))
        return 0;
    file = fopen(temp_path, "w");
    if (file == NULL)
        return 0;
    if (!evolution_engine_write_checkpoint(
            engine, file, context->signature, next_generation, completed) ||
        fclose(file) != 0)
    {
        DeleteFileA(temp_path);
        return 0;
    }
    if (!MoveFileExA(temp_path, final_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temp_path);
        return 0;
    }
    return 1;
}

static int write_best_network_initial(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine)
{
    char path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;
    double *weights;

    if (!path_join(path, sizeof(path), context->output_directory,
                   "best_network_initial.csv"))
        return 0;
    weights = malloc(context->blueprint.connection_count * sizeof(*weights));
    if (context->blueprint.connection_count > 0 && weights == NULL)
        return 0;
    for (size_t i = 0; i < context->blueprint.connection_count; i++)
        weights[i] = context->blueprint.connections[i].weight;
    for (size_t gene = 0; gene < context->gene_count; gene++)
    {
        const EvolutionGeneMetadata *metadata = &context->metadata[gene];
        if (!metadata->has_connection_id)
            continue;
        weights[metadata->connection_id] =
            metadata->gene_kind == EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE ?
            -engine->global_best_genes[gene] : engine->global_best_genes[gene];
    }

    file = fopen(path, "w");
    if (file == NULL)
    {
        free(weights);
        return 0;
    }
    if (fprintf(file, "connection_id,source,target,source_type,target_type,delay,initial_weight\n") < 0)
    {
        fclose(file);
        free(weights);
        return 0;
    }
    for (size_t i = 0; i < context->blueprint.connection_count; i++)
    {
        const MiniSNNConnectionInfo *connection = &context->blueprint.connections[i];
        if (fprintf(file, "%zu,%zu,%zu,%s,%s,%u,%.17g\n", i,
                    connection->source, connection->target,
                    connection->source_type == MINISNN_NEURON_INHIBITORY ? "INH" : "EXC",
                    connection->target_type == MINISNN_NEURON_INHIBITORY ? "INH" : "EXC",
                    connection->delay, weights[i]) < 0)
        {
            fclose(file);
            free(weights);
            return 0;
        }
    }
    free(weights);
    return fclose(file) == 0;
}

static int write_manifest_and_report(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    int resumed,
    double wall_seconds)
{
    char manifest_path[EVOLUTION_OUTPUT_PATH_MAX];
    char report_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *manifest;
    FILE *report;

    if (!path_join(manifest_path, sizeof(manifest_path), context->output_directory,
                   "evolution_manifest.txt") ||
        !path_join(report_path, sizeof(report_path), context->output_directory,
                   "evolution_report.txt"))
        return 0;
    manifest = fopen(manifest_path, "w");
    report = fopen(report_path, "w");
    if (manifest == NULL || report == NULL)
    {
        close_file(&manifest);
        close_file(&report);
        return 0;
    }

    if (fprintf(manifest,
            "format_version=%s\nconfiguration_signature=%s\n"
            "experiment_name=%s\nactual_experiment_name=%s\nbase_scenario=%s\n"
            "blueprint_signature=%llu\nneurons=%d\nconnections=%zu\n"
            "gene_count=%zu\nscalar_gene_count=%d\n"
            "evolve_exc_weights=%s\nevolve_inh_magnitudes=%s\n"
            "selection=tournament\ntournament_sampling=without_replacement\n"
            "crossover=uniform\nmutation=uniform_delta\n"
            "evolution_seed=%llu\nevaluation_seed_base=%llu\nreplicates=%d\n"
            "replicate_std_penalty=%.17g\nfitness_formula=weighted_mean_of_scores\n"
            "selection_fitness_formula=clamp(mean-penalty*std,0,1)\n"
            "inheritance=darwinian\nlamarckian_inheritance=disabled\n"
            "topology_evolution=disabled\ndelay_evolution=disabled\n"
            "parallel_evaluation=disabled\ncheckpoint_format=text_v1\nresume_used=%s\n"
            "save_all_genomes=%s\nwall_seconds=%.6f\n",
            EVOLUTION_VERSION, context->signature,
            context->config.experiment_name, context->actual_experiment_name,
            context->config.base_scenario,
            context->blueprint.topology_signature,
            context->blueprint.neuron_count,
            context->blueprint.connection_count,
            context->gene_count, context->config.scalar_gene_count,
            context->config.evolve_exc_weights ? "true" : "false",
            context->config.evolve_inh_magnitudes ? "true" : "false",
            (unsigned long long)context->config.evolution_seed,
            (unsigned long long)context->config.evaluation_seed_base,
            context->config.evaluation_replicates,
            context->config.replicate_std_penalty,
            resumed ? "true" : "false",
            context->config.save_all_genomes ? "true" : "false",
            wall_seconds) < 0)
    {
        close_file(&manifest);
        close_file(&report);
        return 0;
    }

    if (fprintf(report,
            "MINISNN - RELATORIO DE NEUROEVOLUCAO\n\n"
            "1. Identificacao\nExperimento: %s\nExecucao: %s\nStatus: completed\n\n"
            "2. Cenario-base\n%s\n\n"
            "3. Blueprint estrutural\nNeuronios: %d\nConexoes: %zu\nAssinatura: %llu\n\n"
            "4. Configuracao evolutiva\nPopulacao: %d\nGeracoes: %d\n"
            "Elites: %d\nTorneio: %d\n\n"
            "5. Representacao do genoma\nGenes: %zu\nTopologia fixa: sim\n\n"
            "6. Fitness\nTermos: %d\nMelhor fitness: %.17g\n\n"
            "7. Replicas\n%d por individuo; penalidade de desvio %.17g.\n\n"
            "8. Populacao inicial\nO individuo 0 preserva o baseline no modo baseline_plus_mutation.\n\n"
            "9. Evolucao por geracao\nConsulte generations.csv.\n\n"
            "10. Melhor individuo\nID %llu; geracao %d.\n\n"
            "11. Linhagem\nConsulte lineage.csv.\n\n"
            "12. Diversidade\nConsulte generations.csv.\n\n"
            "13. Plasticidade durante a vida\nPode afetar o fitness, mas nao reescreve o genoma.\n\n"
            "14. Checkpoint e retomada\nFormato textual atomico; resume usado: %s.\n\n"
            "15. Desempenho\nTempo total: %.6f s.\n\n"
            "16. Avisos e limitacoes\nO fitness aumentou apenas quando observado neste experimento. "
            "Melhor fitness nao prova inteligencia geral, solucao otima global ou adaptacao universal. "
            "O resultado depende do cenario, dos limites e das seeds. "
            "Topologia e delays nao evoluem; avaliacao paralela e trabalho futuro.\n",
            context->config.experiment_name, context->actual_experiment_name,
            context->config.base_scenario,
            context->blueprint.neuron_count,
            context->blueprint.connection_count,
            context->blueprint.topology_signature,
            context->config.population_size,
            context->config.generations,
            context->config.elite_count,
            context->config.tournament_size,
            context->gene_count,
            context->config.fitness_term_count,
            engine->global_best_fitness_selection,
            context->config.evaluation_replicates,
            context->config.replicate_std_penalty,
            (unsigned long long)engine->global_best_individual_id,
            engine->global_best_generation,
            resumed ? "sim" : "nao",
            wall_seconds) < 0)
    {
        close_file(&manifest);
        close_file(&report);
        return 0;
    }

    {
        int manifest_ok = fclose(manifest) == 0;
        int report_ok = fclose(report) == 0;
        return manifest_ok && report_ok;
    }
}

static int write_best_module_outputs(
    const char *directory,
    const EvolutionRunContext *context,
    const EvolutionEvaluation *evaluation)
{
    char path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;
    int write_ok;

    if (context->base.plasticity_enabled)
    {
        if (!path_join(path, sizeof(path), directory, "plasticity_metrics.csv"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "plasticity_enabled,learning_mode,final_weight_mean,final_weight_std\n"
                "true,%s,%.17g,%.17g\n",
                context->base.plasticity_learning_mode,
                evaluation->final_weight_mean,
                evaluation->final_weight_std) >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
        if (!path_join(path, sizeof(path), directory, "stdp_report.txt"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "STDP ativo durante a vida do melhor individuo.\n"
                "Os pesos finais aprendidos nao substituem o genoma inicial.\n") >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
    }

    if (context->base.homeostasis_enabled)
    {
        if (!path_join(path, sizeof(path), directory, "homeostasis_metrics.csv"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "homeostasis_enabled,homeostasis_target_rate,homeostasis_rate_error_final,homeostasis_rate_error_mean_absolute\n"
                "true,%.17g,%.17g,%.17g\n",
                context->base.homeostasis_target_rate,
                evaluation->homeostasis_rate_error_final,
                evaluation->homeostasis_rate_error_mean_absolute) >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
        if (!path_join(path, sizeof(path), directory, "homeostasis_report.txt"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "Homeostase ativa no best_run.\nErro final de taxa: %.17g\n",
                evaluation->homeostasis_rate_error_final) >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
    }

    if (context->base.reward_enabled)
    {
        if (!path_join(path, sizeof(path), directory, "reward_metrics.csv"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "reward_enabled,reward_weight_total_signed_change,reward_weight_total_absolute_change,reward_modified_connection_fraction\n"
                "true,%.17g,%.17g,%.17g\n",
                evaluation->reward_weight_total_signed_change,
                evaluation->reward_weight_total_absolute_change,
                evaluation->reward_modified_connection_fraction) >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
        if (!path_join(path, sizeof(path), directory, "reward_report.txt"))
            return 0;
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        write_ok = fprintf(file,
                "R-STDP ativo no best_run.\nMudanca assinada total: %.17g\n",
                evaluation->reward_weight_total_signed_change) >= 0;
        if (fclose(file) != 0)
            write_ok = 0;
        if (!write_ok)
            return 0;
    }
    return 1;
}

static int write_best_run(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine)
{
    char directory[EVOLUTION_OUTPUT_PATH_MAX];
    char population_path[EVOLUTION_OUTPUT_PATH_MAX];
    char raster_path[EVOLUTION_OUTPUT_PATH_MAX];
    char summary_path[EVOLUTION_OUTPUT_PATH_MAX];
    char metrics_path[EVOLUTION_OUTPUT_PATH_MAX];
    char manifest_path[EVOLUTION_OUTPUT_PATH_MAX];
    char weights_initial_path[EVOLUTION_OUTPUT_PATH_MAX];
    char weights_final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char metrics_report_path[EVOLUTION_OUTPUT_PATH_MAX];
    char metrics_html_path[EVOLUTION_OUTPUT_PATH_MAX];
    char weights_html_path[EVOLUTION_OUTPUT_PATH_MAX];
    BestRunObserver observer;
    EvolutionEvaluation evaluation;
    FILE *summary = NULL;
    FILE *metrics = NULL;
    FILE *manifest = NULL;
    FILE *metrics_report = NULL;
    FILE *metrics_html = NULL;
    FILE *weights_html = NULL;

    if (!path_join(directory, sizeof(directory), context->output_directory,
                   "best_run") || !ensure_directory_tree(directory) ||
        !path_join(population_path, sizeof(population_path), directory, "population.csv") ||
        !path_join(raster_path, sizeof(raster_path), directory, "raster.csv") ||
        !path_join(summary_path, sizeof(summary_path), directory, "summary.txt") ||
        !path_join(metrics_path, sizeof(metrics_path), directory, "metrics.csv") ||
        !path_join(manifest_path, sizeof(manifest_path), directory, "run_manifest.txt") ||
        !path_join(weights_initial_path, sizeof(weights_initial_path), directory, "weights_initial.csv") ||
        !path_join(weights_final_path, sizeof(weights_final_path), directory, "weights_final.csv") ||
        !path_join(metrics_report_path, sizeof(metrics_report_path), directory, "metrics_report.txt") ||
        !path_join(metrics_html_path, sizeof(metrics_html_path), directory, "metrics_report.html") ||
        !path_join(weights_html_path, sizeof(weights_html_path), directory, "weights_report.html"))
        return 0;

    memset(&observer, 0, sizeof(observer));
    observer.population = fopen(population_path, "w");
    observer.raster = fopen(raster_path, "w");
    observer.weights_initial = fopen(weights_initial_path, "w");
    observer.weights_final = fopen(weights_final_path, "w");
    if (observer.population == NULL || observer.raster == NULL ||
        observer.weights_initial == NULL || observer.weights_final == NULL)
    {
        close_file(&observer.population);
        close_file(&observer.raster);
        close_file(&observer.weights_initial);
        close_file(&observer.weights_final);
        return 0;
    }
    observer.enabled = 1;
    if (fprintf(observer.population,
                "tempo,spikes_total,spikes_exc,spikes_inh,mean_potential,mean_syn_current\n") < 0 ||
        fprintf(observer.raster, "tempo,neuronio,tipo\n") < 0 ||
        fprintf(observer.weights_initial,
                "connection_id,source,target,source_type,target_type,delay,weight\n") < 0 ||
        fprintf(observer.weights_final,
                "connection_id,source,target,source_type,target_type,delay,weight\n") < 0 ||
        !evaluate_genome(context, engine->global_best_genes,
                         context->config.evaluation_seed_base,
                         &observer, &evaluation))
    {
        close_file(&observer.population);
        close_file(&observer.raster);
        close_file(&observer.weights_initial);
        close_file(&observer.weights_final);
        return 0;
    }
    close_file(&observer.population);
    close_file(&observer.raster);
    close_file(&observer.weights_initial);
    close_file(&observer.weights_final);
    if (!evaluation.valid)
        return 0;

    summary = fopen(summary_path, "w");
    metrics = fopen(metrics_path, "w");
    manifest = fopen(manifest_path, "w");
    metrics_report = fopen(metrics_report_path, "w");
    metrics_html = fopen(metrics_html_path, "w");
    weights_html = fopen(weights_html_path, "w");
    if (summary == NULL || metrics == NULL || manifest == NULL ||
        metrics_report == NULL || metrics_html == NULL || weights_html == NULL)
    {
        close_file(&summary);
        close_file(&metrics);
        close_file(&manifest);
        close_file(&metrics_report);
        close_file(&metrics_html);
        close_file(&weights_html);
        return 0;
    }
    if (fprintf(summary,
            "best_individual_id=%llu\nbest_generation=%d\nfitness=%.17g\n"
            "total_spikes=%d\nfirst_active_step=%d\nlast_active_step=%d\n"
            "final_weight_mean=%.17g\nfinal_weight_std=%.17g\n",
            (unsigned long long)engine->global_best_individual_id,
            engine->global_best_generation,
            engine->global_best_fitness_selection,
            evaluation.total_spikes, evaluation.first_active_step,
            evaluation.last_active_step, evaluation.final_weight_mean,
            evaluation.final_weight_std) < 0 ||
        fprintf(metrics,
            "activity_total_spikes,activity_active_fraction,exc_total_spikes,inh_total_spikes,network_final_weight_mean,network_final_weight_std\n"
            "%d,%.17g,%d,%d,%.17g,%.17g\n",
            evaluation.total_spikes,
            (double)evaluation.active_timesteps / (double)context->base.steps,
            evaluation.exc_spikes, evaluation.inh_spikes,
            evaluation.final_weight_mean, evaluation.final_weight_std) < 0 ||
        fprintf(manifest,
            "source=evolution_best_genome\nindividual_id=%llu\n"
            "replicate_index=0\nevaluation_seed=%llu\n"
            "inheritance=darwinian\ninitial_genome_preserved=true\n",
            (unsigned long long)engine->global_best_individual_id,
            (unsigned long long)context->config.evaluation_seed_base) < 0 ||
        fprintf(metrics_report,
            "MINISNN - METRICAS DO MELHOR INDIVIDUO\n"
            "fitness=%.17g\ntotal_spikes=%d\nactive_fraction=%.17g\n"
            "final_weight_mean=%.17g\nfinal_weight_std=%.17g\n",
            engine->global_best_fitness_selection,
            evaluation.total_spikes,
            (double)evaluation.active_timesteps / (double)context->base.steps,
            evaluation.final_weight_mean,
            evaluation.final_weight_std) < 0 ||
        fprintf(metrics_html,
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Best run metrics</title>"
            "<style>body{background:#101418;color:#e8edf2;font-family:monospace;padding:2rem}"
            "table{border-collapse:collapse}td,th{border:1px solid #53606b;padding:.5rem}</style>"
            "</head><body><h1>Best run metrics</h1><table>"
            "<tr><th>fitness</th><td>%.9f</td></tr>"
            "<tr><th>total spikes</th><td>%d</td></tr>"
            "<tr><th>final weight mean</th><td>%.9f</td></tr>"
            "</table><p>Relatorio local; sem recursos externos.</p></body></html>\n",
            engine->global_best_fitness_selection,
            evaluation.total_spikes,
            evaluation.final_weight_mean) < 0 ||
        fprintf(weights_html,
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Best run weights</title>"
            "<style>body{background:#101418;color:#e8edf2;font-family:monospace;padding:2rem}"
            "a{color:#7cd7ff}</style></head><body><h1>Best run weights</h1>"
            "<p><a href=\"weights_initial.csv\">Pesos iniciais herdaveis</a></p>"
            "<p><a href=\"weights_final.csv\">Pesos finais apos a vida</a></p>"
            "<p>Os pesos finais nao substituem o genoma inicial.</p></body></html>\n") < 0)
    {
        close_file(&summary);
        close_file(&metrics);
        close_file(&manifest);
        close_file(&metrics_report);
        close_file(&metrics_html);
        close_file(&weights_html);
        return 0;
    }
    {
        int ok_summary = fclose(summary) == 0;
        int ok_metrics = fclose(metrics) == 0;
        int ok_manifest = fclose(manifest) == 0;
        int ok_report = fclose(metrics_report) == 0;
        int ok_metrics_html = fclose(metrics_html) == 0;
        int ok_weights_html = fclose(weights_html) == 0;
        return ok_summary && ok_metrics && ok_manifest && ok_report &&
               ok_metrics_html && ok_weights_html &&
               write_best_module_outputs(directory, context, &evaluation);
    }
}

static int append_index(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    const char *status)
{
    char index_path[EVOLUTION_OUTPUT_PATH_MAX];
    char timestamp[32];
    FILE *file;
    int new_file;

    if (!path_join(index_path, sizeof(index_path), context->output_root, "index.csv"))
        return 0;
    new_file = !file_exists(index_path);
    file = fopen(index_path, "a");
    if (file == NULL)
        return 0;
    if (new_file && fprintf(file, EVOLUTION_INDEX_HEADER) < 0)
    {
        fclose(file);
        return 0;
    }
    current_timestamp(timestamp, sizeof(timestamp), 0);
    if (fprintf(file, "%s,%s,%s,%s,%s,%s,%d,%d,%zu,%.17g,%llu,%s\n",
                timestamp, context->config.experiment_name,
                context->actual_experiment_name, context->output_directory,
                context->config_path, context->config.base_scenario,
                context->config.population_size, context->config.generations,
                context->gene_count, engine->global_best_fitness_selection,
                (unsigned long long)engine->global_best_individual_id,
                status) < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int update_last_experiment(const EvolutionRunContext *context)
{
    char final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;

    if (!path_join(final_path, sizeof(final_path), context->output_root,
                   "last_experiment.txt") ||
        !path_join(temp_path, sizeof(temp_path), context->output_root,
                   "last_experiment.tmp"))
        return 0;
    file = fopen(temp_path, "w");
    if (file == NULL)
        return 0;
    if (fprintf(file, "%s\n", context->output_directory) < 0)
    {
        fclose(file);
        DeleteFileA(temp_path);
        return 0;
    }
    if (fclose(file) != 0)
    {
        DeleteFileA(temp_path);
        return 0;
    }
    if (!MoveFileExA(temp_path, final_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temp_path);
        return 0;
    }
    return 1;
}

static int run_engine(
    EvolutionRunContext *context,
    EvolutionEngine *engine,
    int start_generation,
    int resumed,
    const EvolutionRunnerOptions *options,
    EvolutionRunResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    EvolutionOutputFiles files;
    IndividualRunInfo *run_info;
    ULONGLONG run_start = GetTickCount64();
    int completed = 0;

    if (!output_files_open(context, resumed, &files,
                           error_message, error_message_size))
        return 0;
    run_info = calloc(engine->config.population_size, sizeof(*run_info));
    if (run_info == NULL)
    {
        output_files_close(&files);
        set_error(error_message, error_message_size, "memoria insuficiente para avaliacao");
        return 0;
    }

    if (!resumed && !write_lineage_population(files.lineage, engine))
    {
        free(run_info);
        output_files_close(&files);
        set_error(error_message, error_message_size, "erro ao escrever linhagem inicial");
        return 0;
    }

    for (int generation = start_generation;
         generation < context->config.generations;
         generation++)
    {
        size_t *ranking = malloc(
            engine->config.population_size * sizeof(*ranking));
        double evaluation_seconds;
        ULONGLONG generation_start = GetTickCount64();
        double generation_seconds;

        if (ranking == NULL || engine->current_generation != generation ||
            !evaluate_population(context, engine, &files,
                                 run_info, &evaluation_seconds) ||
            !evolution_engine_rank_population(
                engine, ranking, engine->config.population_size))
        {
            free(ranking);
            free(run_info);
            output_files_close(&files);
            set_error(error_message, error_message_size, "falha ao avaliar geracao");
            return 0;
        }

        generation_seconds =
            (double)(GetTickCount64() - generation_start) / 1000.0;
        if (!write_generation_summary(
                context, engine, ranking, run_info,
                evaluation_seconds, generation_seconds, &files) ||
            !write_best_genome_atomic(context, engine))
        {
            free(ranking);
            free(run_info);
            output_files_close(&files);
            set_error(error_message, error_message_size, "erro ao gravar geracao");
            return 0;
        }
        free(ranking);

        out_result->generations_completed = generation + 1;
        if (generation + 1 >= context->config.generations)
        {
            completed = 1;
            if (!write_checkpoint_atomic(
                    context, engine, generation + 1, 1))
            {
                free(run_info);
                output_files_close(&files);
                set_error(error_message, error_message_size, "erro ao finalizar checkpoint");
                return 0;
            }
            break;
        }

        if (!evolution_engine_breed_next_generation(engine) ||
            !write_lineage_population(files.lineage, engine))
        {
            free(run_info);
            output_files_close(&files);
            set_error(error_message, error_message_size, "erro ao produzir proxima geracao");
            return 0;
        }

        if (((generation + 1) %
             context->config.checkpoint_interval_generations) == 0 ||
            (options->stop_after_generations > 0 &&
             generation + 1 >= options->stop_after_generations))
        {
            if (!write_checkpoint_atomic(
                    context, engine, generation + 1, 0))
            {
                free(run_info);
                output_files_close(&files);
                set_error(error_message, error_message_size, "erro ao gravar checkpoint");
                return 0;
            }
        }

        if (options->stop_after_generations > 0 &&
            generation + 1 >= options->stop_after_generations)
            break;
    }

    free(run_info);
    output_files_close(&files);
    out_result->completed = completed;
    out_result->gene_count = context->gene_count;
    out_result->best_individual_id = engine->global_best_individual_id;
    out_result->best_fitness = engine->global_best_fitness_selection;

    if (!completed)
        return 1;

    if (!write_best_network_initial(context, engine) ||
        (context->config.save_best_run && !write_best_run(context, engine)) ||
        !write_manifest_and_report(
            context, engine, resumed,
            (double)(GetTickCount64() - run_start) / 1000.0) ||
        (context->config.history_enabled &&
         !append_index(context, engine, "OK")) ||
        !update_last_experiment(context))
    {
        set_error(error_message, error_message_size, "erro ao finalizar artefatos evolutivos");
        return 0;
    }
    return 1;
}

void evolution_runner_default_options(EvolutionRunnerOptions *options)
{
    if (options == NULL)
        return;
    options->output_root = EVOLUTION_DEFAULT_OUTPUT_ROOT;
    options->stop_after_generations = 0;
}

static void fill_result_paths(
    const EvolutionRunContext *context,
    EvolutionRunResult *result)
{
    snprintf(result->output_directory, sizeof(result->output_directory), "%s",
             context->output_directory);
    snprintf(result->actual_experiment_name,
             sizeof(result->actual_experiment_name), "%s",
             context->actual_experiment_name);
}

int evolution_runner_execute(
    const char *config_path,
    const EvolutionRunnerOptions *provided_options,
    EvolutionRunResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    EvolutionRunnerOptions default_options;
    const EvolutionRunnerOptions *options = provided_options;
    EvolutionRunContext context;
    EvolutionEngineConfig engine_config;
    EvolutionEngine engine;
    int ok;

    memset(&context, 0, sizeof(context));
    memset(&engine, 0, sizeof(engine));
    if (config_path == NULL || out_result == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }
    if (options == NULL)
    {
        evolution_runner_default_options(&default_options);
        options = &default_options;
    }
    if (options->output_root == NULL || options->output_root[0] == '\0' ||
        options->stop_after_generations < 0)
    {
        set_error(error_message, error_message_size, "opcoes invalidas");
        return 0;
    }
    memset(out_result, 0, sizeof(*out_result));

    if (!context_load(&context, config_path, options->output_root,
                      error_message, error_message_size) ||
        !create_output_directory(&context, error_message, error_message_size) ||
        !copy_used_configs(&context, error_message, error_message_size))
    {
        context_destroy(&context);
        return 0;
    }
    fill_result_paths(&context, out_result);

    memset(&engine_config, 0, sizeof(engine_config));
    engine_config.population_size = (size_t)context.config.population_size;
    engine_config.elite_count = (size_t)context.config.elite_count;
    engine_config.tournament_size = (size_t)context.config.tournament_size;
    engine_config.crossover_rate = context.config.crossover_rate;
    engine_config.mutation_rate = context.config.mutation_rate;
    engine_config.mutation_scale = context.config.mutation_scale;
    engine_config.initialization_scale = context.config.initialization_scale;
    engine_config.replicate_std_penalty = context.config.replicate_std_penalty;
    engine_config.initialization = strcmp(
        context.config.initialization, "uniform") == 0 ?
        EVOLUTION_INITIALIZATION_UNIFORM :
        EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION;
    engine_config.evolution_seed = context.config.evolution_seed;

    ok = evolution_engine_init(
             &engine, &engine_config, context.metadata, context.gene_count) &&
         evolution_engine_initialize_population(&engine) &&
         run_engine(&context, &engine, 0, 0, options, out_result,
                    error_message, error_message_size);

    evolution_engine_destroy(&engine);
    context_destroy(&context);
    return ok;
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash;
    if (backslash != NULL && (last == NULL || backslash > last))
        last = backslash;
    return last == NULL ? path : last + 1;
}

int evolution_runner_resume(
    const char *experiment_directory,
    const EvolutionRunnerOptions *provided_options,
    EvolutionRunResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    EvolutionRunnerOptions default_options;
    const EvolutionRunnerOptions *options = provided_options;
    EvolutionRunContext context;
    EvolutionEngineConfig engine_config;
    EvolutionEngine engine;
    char config_path[EVOLUTION_OUTPUT_PATH_MAX];
    char checkpoint_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *checkpoint = NULL;
    int next_generation;
    int completed;
    int ok;

    memset(&context, 0, sizeof(context));
    memset(&engine, 0, sizeof(engine));
    if (experiment_directory == NULL || out_result == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }
    if (options == NULL)
    {
        evolution_runner_default_options(&default_options);
        options = &default_options;
    }
    if (!path_join(config_path, sizeof(config_path), experiment_directory,
                   "evolution_config_used.ini") ||
        !path_join(checkpoint_path, sizeof(checkpoint_path), experiment_directory,
                   "checkpoint.txt") ||
        !file_exists(config_path) || !file_exists(checkpoint_path))
    {
        set_error(error_message, error_message_size, "checkpoint ou config ausente");
        return 0;
    }
    memset(out_result, 0, sizeof(*out_result));

    if (!context_load(&context, config_path,
                      options->output_root != NULL ? options->output_root :
                          EVOLUTION_DEFAULT_OUTPUT_ROOT,
                      error_message, error_message_size) ||
        snprintf(context.output_directory, sizeof(context.output_directory), "%s",
                 experiment_directory) >= (int)sizeof(context.output_directory) ||
        snprintf(context.actual_experiment_name,
                 sizeof(context.actual_experiment_name), "%s",
                 path_basename(experiment_directory)) >=
            (int)sizeof(context.actual_experiment_name))
    {
        context_destroy(&context);
        return 0;
    }
    fill_result_paths(&context, out_result);

    memset(&engine_config, 0, sizeof(engine_config));
    engine_config.population_size = (size_t)context.config.population_size;
    engine_config.elite_count = (size_t)context.config.elite_count;
    engine_config.tournament_size = (size_t)context.config.tournament_size;
    engine_config.crossover_rate = context.config.crossover_rate;
    engine_config.mutation_rate = context.config.mutation_rate;
    engine_config.mutation_scale = context.config.mutation_scale;
    engine_config.initialization_scale = context.config.initialization_scale;
    engine_config.replicate_std_penalty = context.config.replicate_std_penalty;
    engine_config.initialization = strcmp(
        context.config.initialization, "uniform") == 0 ?
        EVOLUTION_INITIALIZATION_UNIFORM :
        EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION;
    engine_config.evolution_seed = context.config.evolution_seed;

    checkpoint = fopen(checkpoint_path, "r");
    ok = checkpoint != NULL && evolution_engine_read_checkpoint(
        &engine, checkpoint, &engine_config, context.metadata,
        context.gene_count, context.signature,
        &next_generation, &completed);
    if (checkpoint != NULL)
        fclose(checkpoint);
    if (!ok)
    {
        evolution_engine_destroy(&engine);
        context_destroy(&context);
        set_error(error_message, error_message_size, "checkpoint invalido ou incompativel");
        return 0;
    }

    if (completed)
    {
        out_result->completed = 1;
        out_result->generations_completed = context.config.generations;
        out_result->gene_count = context.gene_count;
        out_result->best_individual_id = engine.global_best_individual_id;
        out_result->best_fitness = engine.global_best_fitness_selection;
        evolution_engine_destroy(&engine);
        context_destroy(&context);
        return 1;
    }

    ok = run_engine(&context, &engine, next_generation, 1,
                    options, out_result, error_message, error_message_size);
    evolution_engine_destroy(&engine);
    context_destroy(&context);
    return ok;
}

#ifndef EVOLUTION_RUNNER_NO_MAIN
static void print_usage(const char *program)
{
    printf("Uso:\n");
    printf("  %s configs/evolution_demo.ini\n", program);
    printf("  %s --resume results/evolution/experimento\n", program);
}

int main(int argc, char **argv)
{
    EvolutionRunnerOptions options;
    EvolutionRunResult result;
    char error_message[512] = {0};
    int ok;

    evolution_runner_default_options(&options);
    if (!((argc == 2 && strcmp(argv[1], "--resume") != 0) ||
          (argc == 3 && strcmp(argv[1], "--resume") == 0) ||
          (argc == 4 && strcmp(argv[2], "--stop-after") == 0)))
    {
        print_usage(argv[0]);
        return 1;
    }
    if (argc == 4 && strcmp(argv[2], "--stop-after") == 0)
        options.stop_after_generations = atoi(argv[3]);

    if (argc == 3 && strcmp(argv[1], "--resume") == 0)
        ok = evolution_runner_resume(argv[2], &options, &result,
                                     error_message, sizeof(error_message));
    else
        ok = evolution_runner_execute(argv[1], &options, &result,
                                      error_message, sizeof(error_message));

    if (!ok)
    {
        fprintf(stderr, "Erro: %s\n", error_message);
        return 1;
    }

    printf("=== Neuroevolucao miniSNN ===\n");
    printf("Experimento: %s\n", result.actual_experiment_name);
    printf("Pasta: %s\n", result.output_directory);
    printf("Geracoes concluidas: %d\n", result.generations_completed);
    printf("Genes: %zu\n", result.gene_count);
    printf("Melhor individuo: %llu\n", result.best_individual_id);
    printf("Melhor fitness: %.9f\n", result.best_fitness);
    printf("Status: %s\n", result.completed ? "completed" : "checkpointed");
    return 0;
}
#endif
