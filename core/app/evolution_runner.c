#include "evolution_runner.h"

#include <math.h>
#include <limits.h>
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
#include "structure.h"

#define EVOLUTION_DEFAULT_OUTPUT_ROOT "results/evolution"
#define EVOLUTION_INDEX_HEADER_LEGACY \
    "timestamp,experiment_name,actual_experiment_name,experiment_path,config_path,base_scenario,population_size,generations,gene_count,best_fitness,best_individual_id,status\n"
#define EVOLUTION_INDEX_HEADER \
    "timestamp,experiment_name,actual_experiment_name,experiment_path,config_path,base_scenario,population_size,generations,gene_count,best_fitness,best_individual_id,status,genome_mode,structure_enabled,best_connection_count,topology_unique_count,complexity_penalty,best_topology\n"
#define EVOLUTION_FAILURE_MAX 159
#define EVOLUTION_VERSION "C5-v1"
#define EVOLUTION_STRUCTURE_CHECKPOINT_MAGIC "MINISNN_STRUCTURE_CHECKPOINT_V2"
#define EVOLUTION_STRUCTURE_CHECKPOINT_MAGIC_C4 "MINISNN_STRUCTURE_CHECKPOINT_V1"
#define EVOLUTION_HASH_OFFSET 1469598103934665603ULL
#define EVOLUTION_HASH_PRIME 1099511628211ULL

static const char *evolution_legacy_neural_display_name(
    MiniSNNNeuronModel model)
{
    switch (model)
    {
    case MINISNN_NEURON_MODEL_LIF:
        return "LIF";
    case MINISNN_NEURON_MODEL_ADEX:
        return "AdEx";
    case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
        return "Hodgkin-Huxley";
    default:
        return "unknown";
    }
}

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
    size_t final_lifetime_connection_count;
    uint64_t final_lifetime_topology_signature;
} EvolutionEvaluation;

typedef struct
{
    StructureGenome genome;
    StructureGenome reproductive_base;
    StructureMutationStats structural_mutation;
    size_t initial_connection_count;
    size_t evaluated_initial_connection_count;
    size_t final_lifetime_connection_count;
    uint64_t topology_signature_initial;
    uint64_t topology_signature_final_lifetime;
    double behavior_fitness;
    double robust_fitness;
    double complexity_normalized;
    double complexity_penalty_value;
} EvolutionStructureIndividual;

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
    FILE *structures;
    FILE *structural_events;
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
    Neuron *structure_neurons;
    size_t structure_required_inputs[EVOLUTION_MAX_REQUIRED_NEURONS];
    size_t structure_required_outputs[EVOLUTION_MAX_REQUIRED_NEURONS];
    StructureConstraints structure_constraints;
    StructureGenome initial_structure;
    EvolutionStructureIndividual *structure_population;
    StructureGenome global_best_structure;
    uint64_t neuron_blueprint_signature;
    uint64_t global_best_structure_individual_id;
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
    close_file(&files->structures);
    close_file(&files->structural_events);
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

    gene_count = (size_t)context->config.scalar_gene_count;
    if (!context->config.structure_enabled)
    {
        gene_count += (context->config.evolve_exc_weights ? exc_count : 0U) +
            (context->config.evolve_inh_magnitudes ? inh_count : 0U);
    }
    if (gene_count > SIZE_MAX / sizeof(*context->metadata))
    {
        set_error(error_message, error_message_size, "quantidade de genes invalida");
        return 0;
    }

    if (gene_count > 0)
        context->metadata = calloc(gene_count, sizeof(*context->metadata));
    if (gene_count > 0 && context->metadata == NULL)
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

    if (!context->config.structure_enabled && context->config.evolve_exc_weights)
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

    if (!context->config.structure_enabled && context->config.evolve_inh_magnitudes)
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

static void structure_individual_destroy(
    EvolutionStructureIndividual *individual)
{
    if (individual == NULL)
        return;
    structure_genome_destroy(&individual->genome);
    structure_genome_destroy(&individual->reproductive_base);
    memset(individual, 0, sizeof(*individual));
}

static void structure_population_destroy(EvolutionRunContext *context)
{
    if (context == NULL || context->structure_population == NULL)
        return;
    for (int i = 0; i < context->config.population_size; i++)
        structure_individual_destroy(&context->structure_population[i]);
    free(context->structure_population);
    context->structure_population = NULL;
}

static int initialize_structure_blueprint(
    EvolutionRunContext *context,
    char *error_message,
    size_t error_message_size)
{
    MiniSNNConnectionGene *genes = NULL;
    size_t count;

    if (!context->config.structure_enabled)
        return 1;
    count = context->blueprint.connection_count;
    context->structure_neurons = calloc(
        (size_t)context->blueprint.neuron_count,
        sizeof(*context->structure_neurons));
    if (count > 0)
        genes = calloc(count, sizeof(*genes));
    if (context->structure_neurons == NULL || (count > 0 && genes == NULL))
    {
        free(genes);
        set_error(error_message, error_message_size,
                  "memoria insuficiente para blueprint estrutural");
        return 0;
    }

    for (int i = 0; i < context->blueprint.neuron_count; i++)
        context->structure_neurons[i].type = context->blueprint.neuron_types[i];
    for (size_t i = 0; i < count; i++)
    {
        const MiniSNNConnectionInfo *connection =
            &context->blueprint.connections[i];
        if (!structure_connection_key(
                (size_t)context->blueprint.neuron_count,
                connection->source, connection->target,
                &genes[i].connection_key))
        {
            free(genes);
            set_error(error_message, error_message_size,
                      "conexao invalida no blueprint inicial");
            return 0;
        }
        genes[i].source = connection->source;
        genes[i].target = connection->target;
        genes[i].magnitude = fabs(connection->weight);
        genes[i].delay = connection->delay;
        genes[i].inherited_from = 0U;
    }

    memset(&context->structure_constraints, 0,
           sizeof(context->structure_constraints));
    context->structure_constraints.neuron_count =
        (size_t)context->blueprint.neuron_count;
    context->structure_constraints.neurons = context->structure_neurons;
    context->structure_constraints.min_connections =
        (size_t)context->config.structure_min_connections;
    context->structure_constraints.max_connections =
        (size_t)context->config.structure_max_connections;
    context->structure_constraints.allow_self_connections =
        context->config.structure_allow_self_connections;
    context->structure_constraints.allow_inh_to_inh =
        context->config.structure_allow_inh_to_inh;
    context->structure_constraints.delay_min =
        (unsigned int)context->config.structure_delay_min;
    context->structure_constraints.delay_max =
        (unsigned int)context->config.structure_delay_max;
    context->structure_constraints.delay_mutation_max_delta =
        (unsigned int)context->config.structure_delay_mutation_max_delta;
    context->structure_constraints.new_exc_weight_min =
        context->config.structure_new_exc_weight_min;
    context->structure_constraints.new_exc_weight_max =
        context->config.structure_new_exc_weight_max;
    context->structure_constraints.new_inh_magnitude_min =
        context->config.structure_new_inh_magnitude_min;
    context->structure_constraints.new_inh_magnitude_max =
        context->config.structure_new_inh_magnitude_max;
    context->structure_constraints.preserve_required_reachability =
        context->config.structure_preserve_required_reachability;
    context->structure_constraints.required_inputs =
        context->structure_required_inputs;
    context->structure_constraints.required_input_count =
        (size_t)context->config.structure_required_input_count;
    context->structure_constraints.required_outputs =
        context->structure_required_outputs;
    context->structure_constraints.required_output_count =
        (size_t)context->config.structure_required_output_count;
    for (int i = 0; i < context->config.structure_required_input_count; i++)
    {
        context->structure_required_inputs[i] =
            (size_t)context->config.structure_required_input_neurons[i];
    }
    for (int i = 0; i < context->config.structure_required_output_count; i++)
    {
        context->structure_required_outputs[i] =
            (size_t)context->config.structure_required_output_neurons[i];
    }

    if (!structure_genome_set(&context->initial_structure, genes, count) ||
        !structure_genome_validate(
            &context->initial_structure,
            &context->structure_constraints))
    {
        free(genes);
        set_error(error_message, error_message_size,
                  "blueprint inicial viola limites, pares legais ou reachability");
        return 0;
    }
    free(genes);
    context->neuron_blueprint_signature =
        structure_neuron_blueprint_signature(
            context->structure_neurons,
            (size_t)context->blueprint.neuron_count,
            context->blueprint.neuron_model,
            context->blueprint.neuron_model_config_signature);
    if (context->neuron_blueprint_signature == 0U)
    {
        set_error(error_message, error_message_size,
                  "assinatura do blueprint de neuronios invalida");
        return 0;
    }
    return 1;
}

static void mutate_structure_magnitudes(
    const EvolutionRunContext *context,
    EvolutionEngine *engine,
    EvolutionIndividual *individual,
    StructureGenome *genome,
    int initialization);

static int initialize_structure_population(
    EvolutionRunContext *context,
    EvolutionEngine *engine)
{
    if (!context->config.structure_enabled)
        return 1;
    context->structure_population = calloc(
        engine->config.population_size,
        sizeof(*context->structure_population));
    if (context->structure_population == NULL)
        return 0;

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        EvolutionStructureIndividual *individual =
            &context->structure_population[i];
        if (!structure_genome_copy(
                &individual->genome,
                &context->initial_structure) ||
            !structure_genome_copy(
                &individual->reproductive_base,
                &context->initial_structure))
        {
            structure_population_destroy(context);
            return 0;
        }
        if (i > 0 && !structure_genome_mutate(
                &individual->genome,
                &context->structure_constraints,
                context->config.structure_allow_add,
                context->config.structure_allow_remove,
                context->config.structure_allow_rewire,
                context->config.structure_evolve_delays,
                context->config.structure_add_rate,
                context->config.structure_remove_rate,
                context->config.structure_rewire_rate,
                context->config.structure_delay_mutation_rate,
                (size_t)context->config.structure_max_mutations_per_child,
                &engine->prng,
                &individual->structural_mutation))
        {
            structure_population_destroy(context);
            return 0;
        }
        if (i > 0)
        {
            mutate_structure_magnitudes(
                context, engine, &engine->population[i],
                &individual->genome, 1);
        }
        individual->initial_connection_count =
            individual->genome.connection_count;
        individual->topology_signature_initial =
            structure_topology_signature(
                &individual->genome,
                context->neuron_blueprint_signature);
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
        !initialize_structure_blueprint(
            context,
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
    structure_population_destroy(context);
    structure_genome_destroy(&context->initial_structure);
    structure_genome_destroy(&context->global_best_structure);
    free(context->structure_neurons);
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

static int capture_minisnn_structure(
    const MiniSNN *snn,
    size_t neuron_count,
    StructureGenome *out_genome)
{
    size_t count = minisnn_connection_count(snn);
    MiniSNNConnectionGene *genes = NULL;
    int ok;

    if (snn == NULL || out_genome == NULL || neuron_count == 0)
        return 0;
    if (count > 0)
        genes = calloc(count, sizeof(*genes));
    if (count > 0 && genes == NULL)
        return 0;
    for (size_t i = 0; i < count; i++)
    {
        MiniSNNConnectionInfo connection;
        if (!minisnn_get_connection(snn, i, &connection) ||
            !structure_connection_key(
                neuron_count, connection.source, connection.target,
                &genes[i].connection_key))
        {
            free(genes);
            return 0;
        }
        genes[i].source = connection.source;
        genes[i].target = connection.target;
        genes[i].magnitude = fabs(connection.weight);
        genes[i].delay = connection.delay;
        genes[i].inherited_from = 0U;
    }
    ok = structure_genome_set(out_genome, genes, count);
    free(genes);
    return ok;
}

static int replace_minisnn_structure(
    MiniSNN *snn,
    const StructureGenome *genome,
    int allow_self_connections)
{
    size_t current_count;
    size_t operation_count;
    MiniSNNTopologyOperation *operations = NULL;
    MiniSNNTopologyPatchResult result;
    int ok;

    if (snn == NULL || genome == NULL)
        return 0;
    current_count = minisnn_connection_count(snn);
    if (current_count > SIZE_MAX - genome->connection_count)
        return 0;
    operation_count = current_count + genome->connection_count;
    if (operation_count > 0)
        operations = calloc(operation_count, sizeof(*operations));
    if (operation_count > 0 && operations == NULL)
        return 0;
    if ((current_count > 0 || genome->connection_count > 0) &&
        operations == NULL)
        return 0;
    if (operation_count == 0)
        return 1;

    for (size_t i = 0; i < current_count; i++)
    {
        MiniSNNConnectionInfo connection;
        if (!minisnn_get_connection(snn, i, &connection))
        {
            free(operations);
            return 0;
        }
        operations[i].type = MINISNN_TOPOLOGY_REMOVE;
        operations[i].source = connection.source;
        operations[i].target = connection.target;
    }
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[i];
        MiniSNNTopologyOperation *operation = &operations[current_count + i];
        operation->type = MINISNN_TOPOLOGY_ADD;
        operation->source = gene->source;
        operation->target = gene->target;
        operation->magnitude = gene->magnitude;
        operation->delay = gene->delay;
        operation->allow_self_connection = allow_self_connections;
    }
    ok = minisnn_apply_topology_patch(
        snn, operations, operation_count, &result);
    free(operations);
    return ok && result.applied_operations == operation_count;
}

static int apply_genome(
    const EvolutionRunContext *context,
    const double *genes,
    const StructureGenome *structure,
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

    if (!scenario_runtime_configure_modules(
            snn, &scenario, error_message, error_message_size))
    {
        minisnn_destroy(&snn);
        return 0;
    }

    if (context->config.structure_enabled &&
        (structure == NULL || !replace_minisnn_structure(
             snn, structure,
             context->config.structure_allow_self_connections)))
    {
        minisnn_destroy(&snn);
        set_error(error_message, error_message_size,
                  "erro ao aplicar genoma estrutural");
        return 0;
    }

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
    const StructureGenome *structure,
    uint64_t evaluation_seed,
    BestRunObserver *observer,
    StructureGenome *out_final_structure,
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

    if (!apply_genome(context, genes, structure, &scenario, &snn,
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

    if (evaluation.valid && context->config.structure_enabled)
    {
        StructureGenome captured = {0};
        if (!capture_minisnn_structure(
                snn,
                (size_t)context->blueprint.neuron_count,
                &captured))
        {
            evaluation.valid = 0;
            snprintf(evaluation.failure_reason, sizeof(evaluation.failure_reason),
                     "falha ao capturar topologia final da vida");
        }
        else
        {
            evaluation.final_lifetime_connection_count =
                captured.connection_count;
            evaluation.final_lifetime_topology_signature =
                structure_topology_signature(
                    &captured,
                    context->neuron_blueprint_signature);
            if (out_final_structure != NULL)
            {
                structure_genome_destroy(out_final_structure);
                *out_final_structure = captured;
                memset(&captured, 0, sizeof(captured));
            }
            structure_genome_destroy(&captured);
        }
    }

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
    if (context->config.structure_enabled)
    {
        char path[EVOLUTION_OUTPUT_PATH_MAX];
        if (!path_join(path, sizeof(path), context->output_directory,
                       "structures.csv") ||
            (files->structures = fopen(path, append ? "a" : "w")) == NULL ||
            !path_join(path, sizeof(path), context->output_directory,
                       "structural_events.csv") ||
            (files->structural_events = fopen(path, append ? "a" : "w")) == NULL)
        {
            output_files_close(files);
            set_error(error_message, error_message_size,
                      "erro ao abrir CSV estrutural evolutivo");
            return 0;
        }
    }

    if (!append)
    {
        if (fprintf(files->generations,
                "generation,population_size,valid_individual_count,invalid_individual_count,fitness_best,fitness_mean,fitness_min,fitness_max,fitness_std,replicate_fitness_mean,replicate_fitness_std_mean,best_individual_id,global_best_individual_id,global_best_fitness,diversity_mean_gene_std,diversity_mean_pair_distance,elite_count,crossover_child_count,clone_child_count,mutated_child_count,mutation_count,evaluation_seconds,generation_wall_seconds,connections_best,connections_mean,connections_min,connections_max,connections_std,topology_unique_count,topology_diversity_mean_distance,add_count,remove_count,rewire_count,delay_mutation_count,crossover_fallback_count,behavior_fitness_best,complexity_penalty_best,fitness_selection_best\n") < 0 ||
            fprintf(files->individuals,
                "generation,individual_id,status,parent_a_id,parent_b_id,operation,fitness_selection,fitness_mean,fitness_std,fitness_min,fitness_max,valid_replicates,invalid_replicates,mutation_count,mutation_absolute_sum,mutation_max_absolute,crossover_applied,genes_from_parent_a,genes_from_parent_b,is_generation_best,is_global_best,evaluation_seconds,failure_reason,initial_connection_count,evaluated_initial_connection_count,final_lifetime_connection_count,topology_signature_initial,topology_signature_final_lifetime,structural_add_count,structural_remove_count,structural_rewire_count,delay_mutation_count,behavior_fitness,robust_fitness,complexity_normalized,complexity_penalty_value\n") < 0 ||
            fprintf(files->replicates,
                "generation,individual_id,replicate_index,evaluation_seed,status,fitness,total_spikes,active_fraction,exc_spikes,inh_spikes,first_active_step,last_active_step,final_weight_mean,final_weight_std,homeostasis_rate_error_final,reward_weight_total_signed_change,failure_reason,evaluation_seconds\n") < 0 ||
            fprintf(files->fitness_terms,
                "generation,individual_id,replicate_index,term_index,metric,goal,observed_value,target,scale,weight,term_score,weighted_score,status\n") < 0 ||
            fprintf(files->genomes,
                "generation,individual_id,gene_index,gene_name,gene_kind,value,minimum,maximum,baseline_value,connection_id,parameter_path\n") < 0 ||
            fprintf(files->lineage,
                "child_generation,child_individual_id,parent_a_generation,parent_a_id,parent_b_generation,parent_b_id,operation,crossover_applied,mutation_count,initial_connection_count,final_connection_count,add_count,remove_count,rewire_count,delay_mutation_count,structure_crossover_common_count,structure_crossover_disjoint_count,structure_crossover_fallback,topology_signature\n") < 0 ||
            (context->config.structure_enabled &&
             (fprintf(files->structures,
                 "generation,individual_id,connection_key,source,target,source_type,target_type,magnitude,applied_weight,delay,origin\n") < 0 ||
              fprintf(files->structural_events,
                 "generation,child_individual_id,event_index,event_type,old_source,old_target,new_source,new_target,old_magnitude,new_magnitude,old_delay,new_delay,connection_key_before,connection_key_after,status,reason\n") < 0)))
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
    const EvolutionRunContext *context,
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
        if (fprintf(file, "%d,%llu,%s,%s,%s,%s,%s,%d,%zu,",
                    individual->generation,
                    (unsigned long long)individual->individual_id,
                    parent_a_generation, parent_a_id,
                    parent_b_generation, parent_b_id,
                    individual->operation,
                    individual->crossover_applied,
                    individual->mutation.mutation_count) < 0)
            return 0;
        if (context->config.structure_enabled)
        {
            const EvolutionStructureIndividual *structure =
                &context->structure_population[i];
            if (fprintf(file,
                    "%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%016llx\n",
                    structure->initial_connection_count,
                    structure->genome.connection_count,
                    structure->structural_mutation.add_count,
                    structure->structural_mutation.remove_count,
                    structure->structural_mutation.rewire_count,
                    structure->structural_mutation.delay_mutation_count,
                    structure->structural_mutation.common_inherited,
                    structure->structural_mutation.disjoint_inherited,
                    structure->structural_mutation.crossover_fallback,
                    (unsigned long long)structure->topology_signature_initial) < 0)
                return 0;
        }
        else if (fprintf(file,
                         "NA,NA,NA,NA,NA,NA,NA,NA,NA,NA\n") < 0)
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

static const char *structure_origin_name(
    const EvolutionIndividual *individual,
    const MiniSNNConnectionGene *gene)
{
    if (individual->generation == 0)
        return "initial_blueprint";
    if (gene->inherited_from == 1U)
        return "parent_a";
    if (gene->inherited_from == 2U)
        return "parent_b";
    return "reproductive_mutation";
}

static int write_generation_structures(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    const size_t *ranking,
    FILE *file)
{
    if (!context->config.structure_enabled)
        return 1;
    for (size_t population_index = 0;
         population_index < engine->config.population_size;
         population_index++)
    {
        const EvolutionIndividual *individual =
            &engine->population[population_index];
        const StructureGenome *genome =
            &context->structure_population[population_index].genome;
        if (!should_save_genome(context, population_index, ranking))
            continue;
        for (size_t i = 0; i < genome->connection_count; i++)
        {
            const MiniSNNConnectionGene *gene = &genome->connections[i];
            const char *source_type =
                context->structure_neurons[gene->source].type ==
                    NEURON_INHIBITORY ? "INH" : "EXC";
            const char *target_type =
                context->structure_neurons[gene->target].type ==
                    NEURON_INHIBITORY ? "INH" : "EXC";
            double applied_weight = strcmp(source_type, "INH") == 0 ?
                -gene->magnitude : gene->magnitude;
            if (fprintf(file,
                    "%d,%llu,%llu,%zu,%zu,%s,%s,%.17g,%.17g,%u,%s\n",
                    individual->generation,
                    (unsigned long long)individual->individual_id,
                    (unsigned long long)gene->connection_key,
                    gene->source, gene->target,
                    source_type, target_type,
                    gene->magnitude, applied_weight, gene->delay,
                    structure_origin_name(individual, gene)) < 0)
                return 0;
        }
    }
    return fflush(file) == 0;
}

static int write_structural_event_row(
    FILE *file,
    int generation,
    uint64_t child_id,
    size_t event_index,
    const char *event_type,
    const MiniSNNConnectionGene *old_gene,
    const MiniSNNConnectionGene *new_gene,
    const char *reason)
{
    char old_source[32] = "NA";
    char old_target[32] = "NA";
    char new_source[32] = "NA";
    char new_target[32] = "NA";
    char old_magnitude[48] = "NA";
    char new_magnitude[48] = "NA";
    char old_delay[32] = "NA";
    char new_delay[32] = "NA";
    char old_key[32] = "NA";
    char new_key[32] = "NA";
    if (old_gene != NULL)
    {
        snprintf(old_source, sizeof(old_source), "%zu", old_gene->source);
        snprintf(old_target, sizeof(old_target), "%zu", old_gene->target);
        snprintf(old_magnitude, sizeof(old_magnitude), "%.17g", old_gene->magnitude);
        snprintf(old_delay, sizeof(old_delay), "%u", old_gene->delay);
        snprintf(old_key, sizeof(old_key), "%llu",
                 (unsigned long long)old_gene->connection_key);
    }
    if (new_gene != NULL)
    {
        snprintf(new_source, sizeof(new_source), "%zu", new_gene->source);
        snprintf(new_target, sizeof(new_target), "%zu", new_gene->target);
        snprintf(new_magnitude, sizeof(new_magnitude), "%.17g", new_gene->magnitude);
        snprintf(new_delay, sizeof(new_delay), "%u", new_gene->delay);
        snprintf(new_key, sizeof(new_key), "%llu",
                 (unsigned long long)new_gene->connection_key);
    }
    return fprintf(file,
        "%d,%llu,%zu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
        generation, (unsigned long long)child_id, event_index, event_type,
        old_source, old_target, new_source, new_target,
        old_magnitude, new_magnitude, old_delay, new_delay,
        old_key, new_key, "applied", reason) >= 0;
}

static int write_structural_events_population(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    FILE *file)
{
    if (!context->config.structure_enabled)
        return 1;
    for (size_t population_index = 0;
         population_index < engine->config.population_size;
         population_index++)
    {
        const EvolutionIndividual *individual = &engine->population[population_index];
        const EvolutionStructureIndividual *data =
            &context->structure_population[population_index];
        const StructureGenome *before = &data->reproductive_base;
        const StructureGenome *after = &data->genome;
        size_t *removed = NULL;
        size_t *added = NULL;
        size_t removed_count = 0;
        size_t added_count = 0;
        size_t left = 0;
        size_t right = 0;
        size_t event_index = 0;

        if (before->connection_count > 0)
            removed = calloc(before->connection_count, sizeof(*removed));
        if (after->connection_count > 0)
            added = calloc(after->connection_count, sizeof(*added));
        if ((before->connection_count > 0 && removed == NULL) ||
            (after->connection_count > 0 && added == NULL))
        {
            free(removed);
            free(added);
            return 0;
        }

        if (individual->crossover_applied)
        {
            for (size_t i = 0; i < before->connection_count; i++)
            {
                if (!write_structural_event_row(
                        file, individual->generation,
                        individual->individual_id, event_index++,
                        "crossover_inherit", NULL,
                        &before->connections[i], "aligned_connection_key"))
                    goto event_fail;
            }
        }
        while (left < before->connection_count || right < after->connection_count)
        {
            if (left < before->connection_count && right < after->connection_count &&
                before->connections[left].connection_key ==
                    after->connections[right].connection_key)
            {
                if (before->connections[left].delay != after->connections[right].delay &&
                    !write_structural_event_row(
                        file, individual->generation,
                        individual->individual_id, event_index++,
                        "delay_mutation", &before->connections[left],
                        &after->connections[right], "bounded_delay_delta"))
                    goto event_fail;
                left++;
                right++;
            }
            else if (right >= after->connection_count ||
                     (left < before->connection_count &&
                      before->connections[left].connection_key <
                          after->connections[right].connection_key))
            {
                removed[removed_count++] = left++;
            }
            else
            {
                added[added_count++] = right++;
            }
        }
        {
            size_t rewire_count = data->structural_mutation.rewire_count;
            if (rewire_count > removed_count)
                rewire_count = removed_count;
            if (rewire_count > added_count)
                rewire_count = added_count;
            for (size_t i = 0; i < rewire_count; i++)
            {
                if (!write_structural_event_row(
                        file, individual->generation,
                        individual->individual_id, event_index++, "rewire",
                        &before->connections[removed[i]],
                        &after->connections[added[i]], "atomic_rewire"))
                    goto event_fail;
            }
            for (size_t i = rewire_count; i < removed_count; i++)
            {
                if (!write_structural_event_row(
                        file, individual->generation,
                        individual->individual_id, event_index++, "remove",
                        &before->connections[removed[i]], NULL,
                        "reproductive_mutation"))
                    goto event_fail;
            }
            for (size_t i = rewire_count; i < added_count; i++)
            {
                if (!write_structural_event_row(
                        file, individual->generation,
                        individual->individual_id, event_index++, "add",
                        NULL, &after->connections[added[i]],
                        "reproductive_mutation"))
                    goto event_fail;
            }
        }
        for (size_t i = 0; i < data->structural_mutation.skipped_count; i++)
        {
            if (!write_structural_event_row(
                    file, individual->generation,
                    individual->individual_id, event_index++,
                    "mutation_skipped", NULL, NULL, "no_valid_candidate"))
                goto event_fail;
        }
        free(removed);
        free(added);
        continue;
event_fail:
        free(removed);
        free(added);
        return 0;
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
        EvolutionStructureIndividual *structure_data =
            context->config.structure_enabled ?
                &context->structure_population[population_index] : NULL;

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

            if (!evaluate_genome(
                    context,
                    individual->genes,
                    context->config.structure_enabled ?
                        &context->structure_population[population_index].genome :
                        NULL,
                    evaluation_seed, NULL, NULL, &evaluation))
            {
                free(replicate_fitness);
                return 0;
            }
            replicate_seconds =
                (double)(GetTickCount64() - replicate_start) / 1000.0;
            replicate_fitness[replicate_index] =
                evaluation.valid ? evaluation.fitness : 0.0;
            if (evaluation.valid)
            {
                valid_replicates++;
                if (structure_data != NULL && valid_replicates == 1)
                {
                    structure_data->evaluated_initial_connection_count =
                        structure_data->genome.connection_count;
                    structure_data->final_lifetime_connection_count =
                        evaluation.final_lifetime_connection_count;
                    structure_data->topology_signature_final_lifetime =
                        evaluation.final_lifetime_topology_signature;
                }
            }
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
                &fitness_mean, &fitness_std, &fitness_selection))
        {
            free(replicate_fitness);
            return 0;
        }
        if (structure_data != NULL)
        {
            structure_data->behavior_fitness = fitness_mean;
            structure_data->robust_fitness = fitness_selection;
            structure_data->complexity_normalized =
                structure_complexity_normalized(
                    structure_data->genome.connection_count,
                    context->structure_constraints.min_connections,
                    context->structure_constraints.max_connections);
            structure_data->complexity_penalty_value =
                context->config.structure_complexity_penalty *
                structure_data->complexity_normalized;
            fitness_selection = structure_apply_complexity_penalty(
                fitness_selection,
                context->config.structure_complexity_penalty,
                structure_data->complexity_normalized);
        }
        if (!evolution_engine_set_evaluation_with_selection(
                engine, population_index, fitness_mean, fitness_std,
                fitness_min, fitness_max, fitness_selection,
                valid_replicates,
                context->config.evaluation_replicates - valid_replicates))
        {
            free(replicate_fitness);
            return 0;
        }
        if (structure_data != NULL &&
            engine->global_best_individual_id == individual->individual_id)
        {
            if (!structure_genome_copy(
                    &context->global_best_structure,
                    &structure_data->genome))
            {
                free(replicate_fitness);
                return 0;
            }
            context->global_best_structure_individual_id =
                individual->individual_id;
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

static size_t find_individual_by_id(
    const uint64_t *ids,
    size_t count,
    uint64_t individual_id)
{
    for (size_t i = 0; i < count; i++)
    {
        if (ids[i] == individual_id)
            return i;
    }
    return SIZE_MAX;
}

static void merge_structure_stats(
    StructureMutationStats *destination,
    const StructureMutationStats *source)
{
    destination->add_count += source->add_count;
    destination->remove_count += source->remove_count;
    destination->rewire_count += source->rewire_count;
    destination->delay_mutation_count += source->delay_mutation_count;
    destination->skipped_count += source->skipped_count;
    destination->common_inherited += source->common_inherited;
    destination->disjoint_inherited += source->disjoint_inherited;
    if (source->crossover_fallback)
        destination->crossover_fallback = 1;
}

static int magnitude_is_evolvable(
    const EvolutionRunContext *context,
    size_t source)
{
    return context->structure_neurons[source].type == NEURON_INHIBITORY ?
        context->config.evolve_inh_magnitudes :
        context->config.evolve_exc_weights;
}

static void mutate_structure_magnitudes(
    const EvolutionRunContext *context,
    EvolutionEngine *engine,
    EvolutionIndividual *individual,
    StructureGenome *genome,
    int initialization)
{
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        MiniSNNConnectionGene *gene = &genome->connections[i];
        double minimum;
        double maximum;
        double scale;
        double before;
        double after;
        double absolute_change;

        if (!magnitude_is_evolvable(context, gene->source))
            continue;
        if (!initialization &&
            evolution_prng_unit(&engine->prng) >= engine->config.mutation_rate)
            continue;
        if (context->structure_neurons[gene->source].type == NEURON_INHIBITORY)
        {
            minimum = context->config.inh_magnitude_min;
            maximum = context->config.inh_magnitude_max;
        }
        else
        {
            minimum = context->config.exc_weight_min;
            maximum = context->config.exc_weight_max;
        }
        scale = initialization ? engine->config.initialization_scale :
            engine->config.mutation_scale;
        before = gene->magnitude;
        after = before + (evolution_prng_unit(&engine->prng) * 2.0 - 1.0) *
            scale * (maximum - minimum);
        if (after < minimum)
        {
            after = minimum;
            individual->mutation.clamp_min_count++;
        }
        else if (after > maximum)
        {
            after = maximum;
            individual->mutation.clamp_max_count++;
        }
        absolute_change = fabs(after - before);
        gene->magnitude = after;
        individual->mutation.mutation_count++;
        individual->mutation.mutation_absolute_sum += absolute_change;
        if (absolute_change > individual->mutation.mutation_max_absolute)
            individual->mutation.mutation_max_absolute = absolute_change;
    }
}

static int breed_next_generation(
    EvolutionRunContext *context,
    EvolutionEngine *engine)
{
    EvolutionStructureIndividual *old_population;
    EvolutionStructureIndividual *new_population = NULL;
    uint64_t *old_ids = NULL;
    double *old_fitness = NULL;
    size_t population_size;

    if (!context->config.structure_enabled)
        return evolution_engine_breed_next_generation(engine);
    population_size = engine->config.population_size;
    old_population = context->structure_population;
    old_ids = calloc(population_size, sizeof(*old_ids));
    old_fitness = calloc(population_size, sizeof(*old_fitness));
    new_population = calloc(population_size, sizeof(*new_population));
    if (old_ids == NULL || old_fitness == NULL || new_population == NULL)
        goto fail;
    for (size_t i = 0; i < population_size; i++)
    {
        old_ids[i] = engine->population[i].individual_id;
        old_fitness[i] = engine->population[i].fitness_selection;
    }

    if (!evolution_engine_breed_next_generation_deferred_mutation(engine))
        goto fail;
    for (size_t i = 0; i < population_size; i++)
    {
        const EvolutionIndividual *child = &engine->population[i];
        EvolutionStructureIndividual *new_data = &new_population[i];
        size_t parent_a_index = find_individual_by_id(
            old_ids, population_size, child->parent_a_id);
        size_t parent_b_index = find_individual_by_id(
            old_ids, population_size, child->parent_b_id);
        StructureMutationStats crossover_stats = {0};
        StructureMutationStats mutation_stats = {0};

        if (parent_a_index == SIZE_MAX)
            goto fail_after_breed;
        if (child->crossover_applied)
        {
            if (parent_b_index == SIZE_MAX ||
                !structure_genome_crossover(
                    &old_population[parent_a_index].genome,
                    old_fitness[parent_a_index],
                    old_ids[parent_a_index],
                    &old_population[parent_b_index].genome,
                    old_fitness[parent_b_index],
                    old_ids[parent_b_index],
                    &context->structure_constraints,
                    &engine->prng,
                    &new_data->genome,
                    &crossover_stats))
            {
                goto fail_after_breed;
            }
        }
        else if (!structure_genome_copy(
                     &new_data->genome,
                     &old_population[parent_a_index].genome))
        {
            goto fail_after_breed;
        }

        new_data->initial_connection_count =
            new_data->genome.connection_count;
        if (!structure_genome_copy(
                &new_data->reproductive_base,
                &new_data->genome))
        {
            goto fail_after_breed;
        }
        merge_structure_stats(
            &new_data->structural_mutation,
            &crossover_stats);
        if (strcmp(child->operation, "elite_copy") != 0)
        {
            if (!structure_genome_mutate(
                    &new_data->genome,
                    &context->structure_constraints,
                    context->config.structure_allow_add,
                    context->config.structure_allow_remove,
                    context->config.structure_allow_rewire,
                    context->config.structure_evolve_delays,
                    context->config.structure_add_rate,
                    context->config.structure_remove_rate,
                    context->config.structure_rewire_rate,
                    context->config.structure_delay_mutation_rate,
                    (size_t)context->config.structure_max_mutations_per_child,
                    &engine->prng,
                    &mutation_stats))
            {
                goto fail_after_breed;
            }
            merge_structure_stats(
                &new_data->structural_mutation,
                &mutation_stats);
            if (!evolution_engine_mutate_population_individual(engine, i))
                goto fail_after_breed;
            mutate_structure_magnitudes(
                context, engine, &engine->population[i],
                &new_data->genome, 0);
        }
        new_data->topology_signature_initial =
            structure_topology_signature(
                &new_data->genome,
                context->neuron_blueprint_signature);
    }

    for (size_t i = 0; i < population_size; i++)
        structure_individual_destroy(&old_population[i]);
    free(old_population);
    context->structure_population = new_population;
    free(old_ids);
    free(old_fitness);
    return 1;

fail_after_breed:
    for (size_t i = 0; i < population_size; i++)
        structure_individual_destroy(&new_population[i]);
fail:
    free(new_population);
    free(old_ids);
    free(old_fitness);
    return 0;
}

static void diversity_metrics(
    const EvolutionEngine *engine,
    double *out_mean_gene_std,
    double *out_mean_pair_distance)
{
    double gene_std_sum = 0.0;
    double distance_sum = 0.0;
    size_t pair_count = 0;

    if (engine->gene_count == 0)
    {
        *out_mean_gene_std = 0.0;
        *out_mean_pair_distance = 0.0;
        return;
    }

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

static int write_structural_generation_columns(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    const size_t *ranking,
    FILE *file)
{
    double mean = 0.0;
    double variance = 0.0;
    size_t minimum = SIZE_MAX;
    size_t maximum = 0;
    size_t unique_count = 0;
    double diversity_sum = 0.0;
    size_t pair_count = 0;
    size_t add_count = 0;
    size_t remove_count = 0;
    size_t rewire_count = 0;
    size_t delay_count = 0;
    size_t fallback_count = 0;
    const EvolutionStructureIndividual *best;

    if (!context->config.structure_enabled)
    {
        return fprintf(file,
            ",NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA\n") >= 0;
    }
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionStructureIndividual *data =
            &context->structure_population[i];
        size_t count = data->genome.connection_count;
        int first_signature = 1;
        mean += (double)count;
        if (count < minimum)
            minimum = count;
        if (count > maximum)
            maximum = count;
        add_count += data->structural_mutation.add_count;
        remove_count += data->structural_mutation.remove_count;
        rewire_count += data->structural_mutation.rewire_count;
        delay_count += data->structural_mutation.delay_mutation_count;
        fallback_count += data->structural_mutation.crossover_fallback ? 1U : 0U;
        for (size_t previous = 0; previous < i; previous++)
        {
            if (context->structure_population[previous].topology_signature_initial ==
                data->topology_signature_initial)
            {
                first_signature = 0;
                break;
            }
        }
        if (first_signature)
            unique_count++;
        for (size_t right = i + 1;
             right < engine->config.population_size;
             right++)
        {
            diversity_sum += structure_jaccard_distance(
                &data->genome,
                &context->structure_population[right].genome);
            pair_count++;
        }
    }
    mean /= (double)engine->config.population_size;
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        double difference =
            (double)context->structure_population[i].genome.connection_count - mean;
        variance += difference * difference;
    }
    variance = sqrt(variance / (double)engine->config.population_size);
    best = &context->structure_population[ranking[0]];
    return fprintf(file,
        ",%zu,%.17g,%zu,%zu,%.17g,%zu,%.17g,%zu,%zu,%zu,%zu,%zu,%.17g,%.17g,%.17g\n",
        best->genome.connection_count, mean, minimum, maximum, variance,
        unique_count,
        pair_count > 0 ? diversity_sum / (double)pair_count : 0.0,
        add_count, remove_count, rewire_count, delay_count, fallback_count,
        best->behavior_fitness,
        best->complexity_penalty_value,
        engine->population[ranking[0]].fitness_selection) >= 0;
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
            "%llu,%llu,%.17g,%.17g,%.17g,%zu,%zu,%zu,%zu,%zu,%.6f,%.6f",
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
            evaluation_seconds, wall_seconds) < 0 ||
        !write_structural_generation_columns(
            context, engine, ranking, files->generations))
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
                "%d,%d,%zu,%.17g,%.17g,%d,%zu,%zu,%d,%d,%.6f,%s",
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
        if (context->config.structure_enabled)
        {
            const EvolutionStructureIndividual *structure =
                &context->structure_population[i];
            if (fprintf(files->individuals,
                    ",%zu,%zu,%zu,%016llx,%016llx,%zu,%zu,%zu,%zu,%.17g,%.17g,%.17g,%.17g\n",
                    structure->initial_connection_count,
                    structure->evaluated_initial_connection_count,
                    structure->final_lifetime_connection_count,
                    (unsigned long long)structure->topology_signature_initial,
                    (unsigned long long)structure->topology_signature_final_lifetime,
                    structure->structural_mutation.add_count,
                    structure->structural_mutation.remove_count,
                    structure->structural_mutation.rewire_count,
                    structure->structural_mutation.delay_mutation_count,
                    structure->behavior_fitness,
                    structure->robust_fitness,
                    structure->complexity_normalized,
                    structure->complexity_penalty_value) < 0)
                return 0;
        }
        else if (fprintf(files->individuals,
                         ",NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA,NA\n") < 0)
            return 0;
    }

    return write_generation_genomes(context, engine, ranking, files->genomes) &&
           write_generation_structures(
               context, engine, ranking, files->structures) &&
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

static int write_structure_checkpoint(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    FILE *file,
    int next_generation,
    int completed)
{
    if (!context->config.structure_enabled || file == NULL ||
        context->structure_population == NULL)
        return 0;
    if (fprintf(file,
            "%s\nsignature=%s\nnext_generation=%d\ncompleted=%d\n"
            "neuron_blueprint_signature=%llu\nneuron_model=%s\n"
            "neuron_model_config_signature=%llu\npopulation_size=%zu\n"
            "current_generation=%d\nprng_state=%llu\nprng_increment=%llu\n"
            "global_best_structure_individual_id=%llu\n"
            "global_best_connection_count=%zu\n",
            EVOLUTION_STRUCTURE_CHECKPOINT_MAGIC,
            context->signature,
            next_generation,
            completed ? 1 : 0,
            (unsigned long long)context->neuron_blueprint_signature,
            minisnn_neuron_model_name(context->blueprint.neuron_model),
            (unsigned long long)context->blueprint.neuron_model_config_signature,
            engine->config.population_size,
            engine->current_generation,
            (unsigned long long)engine->prng.state,
            (unsigned long long)engine->prng.increment,
            (unsigned long long)context->global_best_structure_individual_id,
            context->global_best_structure.connection_count) < 0)
        return 0;
    for (size_t i = 0; i < context->global_best_structure.connection_count; i++)
    {
        const MiniSNNConnectionGene *gene =
            &context->global_best_structure.connections[i];
        if (fprintf(file,
                "global_gene,%zu,%llu,%zu,%zu,%.17g,%u,%u\n",
                i, (unsigned long long)gene->connection_key,
                gene->source, gene->target, gene->magnitude,
                gene->delay, gene->inherited_from) < 0)
            return 0;
    }
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionStructureIndividual *state =
            &context->structure_population[i];
        const EvolutionIndividual *individual = &engine->population[i];
        if (fprintf(file,
                "structure_state,%zu,%llu,%zu,%zu,%zu,%zu,%llu,%llu,"
                "%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%.17g,%.17g,%.17g,%.17g\n",
                i, (unsigned long long)individual->individual_id,
                state->genome.connection_count,
                state->initial_connection_count,
                state->evaluated_initial_connection_count,
                state->final_lifetime_connection_count,
                (unsigned long long)state->topology_signature_initial,
                (unsigned long long)state->topology_signature_final_lifetime,
                state->structural_mutation.add_count,
                state->structural_mutation.remove_count,
                state->structural_mutation.rewire_count,
                state->structural_mutation.delay_mutation_count,
                state->structural_mutation.skipped_count,
                state->structural_mutation.common_inherited,
                state->structural_mutation.disjoint_inherited,
                state->structural_mutation.crossover_fallback,
                state->behavior_fitness,
                state->robust_fitness,
                state->complexity_normalized,
                state->complexity_penalty_value) < 0)
            return 0;
        for (size_t gene_index = 0;
             gene_index < state->genome.connection_count;
             gene_index++)
        {
            const MiniSNNConnectionGene *gene =
                &state->genome.connections[gene_index];
            if (fprintf(file,
                    "structure_gene,%zu,%zu,%llu,%zu,%zu,%.17g,%u,%u\n",
                    i, gene_index,
                    (unsigned long long)gene->connection_key,
                    gene->source, gene->target, gene->magnitude,
                    gene->delay, gene->inherited_from) < 0)
                return 0;
        }
    }
    return fprintf(file, "END\n") >= 0;
}

static int read_checkpoint_key_string(
    FILE *file,
    const char *expected_key,
    char *out_value,
    size_t out_size)
{
    char line[512];
    char key[128];
    char value[384];
    if (fgets(line, sizeof(line), file) == NULL ||
        sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2 ||
        strcmp(key, expected_key) != 0 ||
        snprintf(out_value, out_size, "%s", value) >= (int)out_size)
        return 0;
    return 1;
}

static int read_checkpoint_key_ull(
    FILE *file,
    const char *expected_key,
    unsigned long long *out_value)
{
    char value[128];
    char *end = NULL;
    if (!read_checkpoint_key_string(
            file, expected_key, value, sizeof(value)))
        return 0;
    *out_value = strtoull(value, &end, 10);
    return end != value && *end == '\0';
}

static int read_structure_gene_line(
    const char *line,
    const char *prefix,
    size_t expected_population_index,
    size_t expected_gene_index,
    MiniSNNConnectionGene *out_gene)
{
    size_t population_index;
    size_t gene_index;
    unsigned long long key;
    unsigned int inherited_from;
    char format[128];
    if (snprintf(format, sizeof(format),
            "%s,%%zu,%%zu,%%llu,%%zu,%%zu,%%lf,%%u,%%u", prefix) >=
        (int)sizeof(format) ||
        sscanf(line, format,
            &population_index, &gene_index, &key,
            &out_gene->source, &out_gene->target,
            &out_gene->magnitude, &out_gene->delay,
            &inherited_from) != 8 ||
        population_index != expected_population_index ||
        gene_index != expected_gene_index || !isfinite(out_gene->magnitude))
        return 0;
    out_gene->connection_key = (uint64_t)key;
    out_gene->inherited_from = inherited_from;
    return 1;
}

static int read_structure_checkpoint(
    EvolutionRunContext *context,
    const EvolutionEngine *engine,
    FILE *file,
    int expected_next_generation,
    int expected_completed)
{
    char line[1024];
    char signature[64];
    unsigned long long value;
    unsigned long long neuron_signature;
    unsigned long long prng_state;
    unsigned long long prng_increment;
    unsigned long long global_best_id;
    size_t population_size;
    size_t global_count;
    int next_generation;
    int completed;
    int current_generation;
    MiniSNNConnectionGene *genes = NULL;
    int legacy_c4;

    if (!context->config.structure_enabled || engine == NULL || file == NULL ||
        fgets(line, sizeof(line), file) == NULL)
        return 0;
    line[strcspn(line, "\r\n")] = '\0';
    legacy_c4 = strcmp(line, EVOLUTION_STRUCTURE_CHECKPOINT_MAGIC_C4) == 0;
    if ((!legacy_c4 && strcmp(line, EVOLUTION_STRUCTURE_CHECKPOINT_MAGIC) != 0) ||
        !read_checkpoint_key_string(file, "signature", signature, sizeof(signature)) ||
        strcmp(signature, context->signature) != 0 ||
        !read_checkpoint_key_ull(file, "next_generation", &value) ||
        value > INT_MAX || (next_generation = (int)value) != expected_next_generation ||
        !read_checkpoint_key_ull(file, "completed", &value) ||
        value > 1U || (completed = (int)value) != expected_completed ||
        !read_checkpoint_key_ull(file, "neuron_blueprint_signature", &neuron_signature) ||
        (legacy_c4 ?
            (context->blueprint.neuron_model != MINISNN_NEURON_MODEL_LIF ||
             neuron_signature != structure_neuron_blueprint_signature_legacy(
                 context->structure_neurons,
                 (size_t)context->blueprint.neuron_count)) :
            neuron_signature != context->neuron_blueprint_signature) ||
        (!legacy_c4 &&
         (!read_checkpoint_key_string(file, "neuron_model", line, sizeof(line)) ||
          strcmp(line, minisnn_neuron_model_name(
              context->blueprint.neuron_model)) != 0 ||
          !read_checkpoint_key_ull(
              file, "neuron_model_config_signature", &value) ||
          value != context->blueprint.neuron_model_config_signature)) ||
        !read_checkpoint_key_ull(file, "population_size", &value) ||
        value > SIZE_MAX ||
        (population_size = (size_t)value) != engine->config.population_size ||
        !read_checkpoint_key_ull(file, "current_generation", &value) ||
        value > INT_MAX || (current_generation = (int)value) != engine->current_generation ||
        !read_checkpoint_key_ull(file, "prng_state", &prng_state) ||
        prng_state != engine->prng.state ||
        !read_checkpoint_key_ull(file, "prng_increment", &prng_increment) ||
        prng_increment != engine->prng.increment ||
        !read_checkpoint_key_ull(file,
            "global_best_structure_individual_id", &global_best_id) ||
        !read_checkpoint_key_ull(file, "global_best_connection_count", &value) ||
        value > SIZE_MAX || (global_count = (size_t)value) >
            context->structure_constraints.max_connections)
        return 0;

    if (global_count > 0)
        genes = calloc(global_count, sizeof(*genes));
    if (global_count > 0 && genes == NULL)
        return 0;
    for (size_t i = 0; i < global_count; i++)
    {
        size_t stored_index;
        unsigned long long key;
        unsigned int inherited_from;
        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(line, "global_gene,%zu,%llu,%zu,%zu,%lf,%u,%u",
                &stored_index, &key, &genes[i].source, &genes[i].target,
                &genes[i].magnitude, &genes[i].delay,
                &inherited_from) != 7 || stored_index != i ||
            !isfinite(genes[i].magnitude))
        {
            free(genes);
            return 0;
        }
        genes[i].connection_key = (uint64_t)key;
        genes[i].inherited_from = inherited_from;
    }
    if (!structure_genome_set(
            &context->global_best_structure, genes, global_count) ||
        !structure_genome_validate(
            &context->global_best_structure,
            &context->structure_constraints))
    {
        free(genes);
        return 0;
    }
    free(genes);
    genes = NULL;
    context->global_best_structure_individual_id = (uint64_t)global_best_id;
    context->structure_population = calloc(
        population_size, sizeof(*context->structure_population));
    if (context->structure_population == NULL)
        return 0;

    for (size_t i = 0; i < population_size; i++)
    {
        EvolutionStructureIndividual *state = &context->structure_population[i];
        size_t stored_index;
        unsigned long long individual_id;
        unsigned long long signature_initial;
        unsigned long long signature_final;
        size_t connection_count;
        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(line,
                "structure_state,%zu,%llu,%zu,%zu,%zu,%zu,%llu,%llu,"
                "%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%lf,%lf,%lf,%lf",
                &stored_index, &individual_id, &connection_count,
                &state->initial_connection_count,
                &state->evaluated_initial_connection_count,
                &state->final_lifetime_connection_count,
                &signature_initial, &signature_final,
                &state->structural_mutation.add_count,
                &state->structural_mutation.remove_count,
                &state->structural_mutation.rewire_count,
                &state->structural_mutation.delay_mutation_count,
                &state->structural_mutation.skipped_count,
                &state->structural_mutation.common_inherited,
                &state->structural_mutation.disjoint_inherited,
                &state->structural_mutation.crossover_fallback,
                &state->behavior_fitness, &state->robust_fitness,
                &state->complexity_normalized,
                &state->complexity_penalty_value) != 20 ||
            stored_index != i ||
            individual_id != engine->population[i].individual_id ||
            connection_count < context->structure_constraints.min_connections ||
            connection_count > context->structure_constraints.max_connections)
            goto read_fail;
        state->topology_signature_initial = (uint64_t)signature_initial;
        state->topology_signature_final_lifetime = (uint64_t)signature_final;
        if (connection_count > 0)
            genes = calloc(connection_count, sizeof(*genes));
        if (connection_count > 0 && genes == NULL)
            goto read_fail;
        for (size_t gene_index = 0; gene_index < connection_count; gene_index++)
        {
            if (fgets(line, sizeof(line), file) == NULL ||
                !read_structure_gene_line(
                    line, "structure_gene", i, gene_index,
                    &genes[gene_index]))
                goto read_fail;
        }
        if (!structure_genome_set(&state->genome, genes, connection_count) ||
            !structure_genome_validate(
                &state->genome, &context->structure_constraints) ||
            structure_topology_signature(
                &state->genome,
                context->neuron_blueprint_signature) !=
                    state->topology_signature_initial)
            goto read_fail;
        free(genes);
        genes = NULL;
    }
    if (fgets(line, sizeof(line), file) == NULL)
        goto read_fail;
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, "END") != 0)
        goto read_fail;
    return 1;

read_fail:
    free(genes);
    structure_population_destroy(context);
    return 0;
}

static int write_checkpoint_atomic(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine,
    int next_generation,
    int completed)
{
    char final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    char structure_final_path[EVOLUTION_OUTPUT_PATH_MAX];
    char structure_temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;
    FILE *structure_file = NULL;

    if (!path_join(final_path, sizeof(final_path), context->output_directory,
                   "checkpoint.txt") ||
        !path_join(temp_path, sizeof(temp_path), context->output_directory,
                   "checkpoint.tmp"))
        return 0;
    structure_final_path[0] = '\0';
    structure_temp_path[0] = '\0';
    if (context->config.structure_enabled &&
        (!path_join(structure_final_path, sizeof(structure_final_path),
                    context->output_directory, "checkpoint_structure.txt") ||
         !path_join(structure_temp_path, sizeof(structure_temp_path),
                    context->output_directory, "checkpoint_structure.tmp")))
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
    if (context->config.structure_enabled)
    {
        int structure_ok;
        structure_file = fopen(structure_temp_path, "w");
        if (structure_file == NULL)
        {
            DeleteFileA(temp_path);
            DeleteFileA(structure_temp_path);
            return 0;
        }
        structure_ok = write_structure_checkpoint(
            context, engine, structure_file,
            next_generation, completed);
        if (fclose(structure_file) != 0)
            structure_ok = 0;
        structure_file = NULL;
        if (!structure_ok)
        {
            DeleteFileA(temp_path);
            DeleteFileA(structure_temp_path);
            return 0;
        }
        if (!MoveFileExA(
                structure_temp_path, structure_final_path,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileA(temp_path);
            DeleteFileA(structure_temp_path);
            return 0;
        }
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
    if (context->config.structure_enabled)
    {
        file = fopen(path, "w");
        if (file == NULL)
            return 0;
        if (fprintf(file,
                "connection_id,source,target,source_type,target_type,delay,initial_weight\n") < 0)
        {
            fclose(file);
            return 0;
        }
        for (size_t i = 0;
             i < context->global_best_structure.connection_count;
             i++)
        {
            const MiniSNNConnectionGene *gene =
                &context->global_best_structure.connections[i];
            int inhibitory = context->structure_neurons[gene->source].type ==
                NEURON_INHIBITORY;
            if (fprintf(file, "%zu,%zu,%zu,%s,%s,%u,%.17g\n",
                    i, gene->source, gene->target,
                    inhibitory ? "INH" : "EXC",
                    context->structure_neurons[gene->target].type ==
                        NEURON_INHIBITORY ? "INH" : "EXC",
                    gene->delay,
                    inhibitory ? -gene->magnitude : gene->magnitude) < 0)
            {
                fclose(file);
                return 0;
            }
        }
        return fclose(file) == 0;
    }
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

static int write_structure_topology_csv(
    const EvolutionRunContext *context,
    const char *filename,
    const StructureGenome *genome,
    const char *topology_role)
{
    char path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *file;
    int ok = 1;
    if (!path_join(path, sizeof(path), context->output_directory, filename))
        return 0;
    file = fopen(path, "w");
    if (file == NULL)
        return 0;
    if (fprintf(file,
            "connection_key,source,target,source_type,target_type,magnitude,applied_weight,delay,topology_role\n") < 0)
        ok = 0;
    for (size_t i = 0; ok && i < genome->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[i];
        int inhibitory = context->structure_neurons[gene->source].type ==
            NEURON_INHIBITORY;
        if (fprintf(file,
                "%llu,%zu,%zu,%s,%s,%.17g,%.17g,%u,%s\n",
                (unsigned long long)gene->connection_key,
                gene->source, gene->target,
                inhibitory ? "INH" : "EXC",
                context->structure_neurons[gene->target].type ==
                    NEURON_INHIBITORY ? "INH" : "EXC",
                gene->magnitude,
                inhibitory ? -gene->magnitude : gene->magnitude,
                gene->delay, topology_role) < 0)
            ok = 0;
    }
    if (fclose(file) != 0)
        ok = 0;
    return ok;
}

static int write_best_inherited_topologies(
    const EvolutionRunContext *context)
{
    if (!context->config.structure_enabled)
        return 1;
    return write_structure_topology_csv(
               context, "best_topology.csv",
               &context->global_best_structure,
               "heritable_genome") &&
           write_structure_topology_csv(
               context, "best_topology_initial.csv",
               &context->global_best_structure,
               "heritable_initial");
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
            "genome_mode=%s\nstructure_enabled=%s\n"
            "neuron_blueprint_signature=%llu\n"
            "neural_model=%s\nneuron_model=%s\n"
            "neuron_model_config_signature=%llu\n"
            "neuron_integration_method=%s\n"
            "intrinsic_homeostasis_supported=%s\n"
            "evolve_exc_weights=%s\nevolve_inh_magnitudes=%s\n"
            "selection=tournament\ntournament_sampling=without_replacement\n"
            "crossover=uniform\nmutation=uniform_delta\n"
            "evolution_seed=%llu\nevaluation_seed_base=%llu\nreplicates=%d\n"
            "replicate_std_penalty=%.17g\nfitness_formula=weighted_mean_of_scores\n"
            "selection_fitness_formula=clamp(mean-penalty*std,0,1)\n"
            "inheritance=darwinian\nlamarckian_inheritance=disabled\n"
            "topology_evolution=%s\ndelay_evolution=%s\n"
            "complexity_penalty=%.17g\n"
            "best_connection_count=%zu\nbest_topology_signature=%llu\n"
            "parallel_evaluation=disabled\ncheckpoint_format=%s\nresume_used=%s\n"
            "save_all_genomes=%s\nwall_seconds=%.6f\n",
            EVOLUTION_VERSION,
            context->signature,
            context->config.experiment_name, context->actual_experiment_name,
            context->config.base_scenario,
            context->blueprint.topology_signature,
            context->blueprint.neuron_count,
            context->blueprint.connection_count,
            context->gene_count, context->config.scalar_gene_count,
            context->config.genome_mode,
            context->config.structure_enabled ? "true" : "false",
            (unsigned long long)context->neuron_blueprint_signature,
            evolution_legacy_neural_display_name(
                context->blueprint.neuron_model),
            minisnn_neuron_model_name(context->blueprint.neuron_model),
            (unsigned long long)
                context->blueprint.neuron_model_config_signature,
            minisnn_neuron_model_integration_method(
                context->blueprint.neuron_model),
            minisnn_neuron_model_capabilities(
                context->blueprint.neuron_model)
                    .supports_homeostatic_threshold ? "yes" : "no",
            context->config.evolve_exc_weights ? "true" : "false",
            context->config.evolve_inh_magnitudes ? "true" : "false",
            (unsigned long long)context->config.evolution_seed,
            (unsigned long long)context->config.evaluation_seed_base,
            context->config.evaluation_replicates,
            context->config.replicate_std_penalty,
            context->config.structure_enabled ? "enabled" : "disabled",
            context->config.structure_enabled &&
                context->config.structure_evolve_delays ? "enabled" : "disabled",
            context->config.structure_enabled ?
                context->config.structure_complexity_penalty : 0.0,
            context->config.structure_enabled ?
                context->global_best_structure.connection_count :
                context->blueprint.connection_count,
            (unsigned long long)(context->config.structure_enabled ?
                structure_topology_signature(
                    &context->global_best_structure,
                    context->neuron_blueprint_signature) :
                context->blueprint.topology_signature),
            context->config.structure_enabled ?
                "text_v1+structure_sidecar_v2" : "text_v1",
            resumed ? "true" : "false",
            context->config.save_all_genomes ? "true" : "false",
            wall_seconds) < 0)
    {
        close_file(&manifest);
        close_file(&report);
        return 0;
    }

    if (context->config.structure_enabled)
    {
        uint64_t best_signature = structure_topology_signature(
            &context->global_best_structure,
            context->neuron_blueprint_signature);
        if (fprintf(report,
                "MINISNN - RELATORIO DE TOPOLOGIA ADAPTATIVA EVOLUTIVA\n\n"
                "1. Identificacao\nExperimento: %s\nExecucao: %s\n\n"
                "2. Estrutura inicial\nBlueprint com %zu conexoes; neuronios fixos: %d.\n\n"
                "3. Configuracao\nGenome mode: structural_connections; limites: %d..%d.\n\n"
                "4. Ordem temporal\nA estrutura herdavel cria o fenotipo antes da avaliacao.\n\n"
                "5. Eventos de crescimento\nConsulte structural_events.csv.\n\n"
                "6. Eventos de poda\nRemocoes reprodutivas sao auditadas no mesmo CSV.\n\n"
                "7. Reconexoes\nReconexoes usam transacao atomica.\n\n"
                "8. Delays\nEvolucao de delay: %s.\n\n"
                "9. Estrutura final\nMelhor genoma: %zu conexoes; assinatura %016llx.\n\n"
                "10. Integracao com STDP\n%s\n\n"
                "11. Integracao com R-STDP\n%s\n\n"
                "12. Integracao com homeostase\n%s\n\n"
                "13. Complexidade\nPenalidade configurada: %.17g.\n\n"
                "14. Assinaturas e reprodutibilidade\nBlueprint: %016llx; checkpoint C4 sidecar validado.\n\n"
                "15. Desempenho\nTempo total: %.6f s.\n\n"
                "16. Limitacoes\nA heuristica modifica arestas, mas nao garante melhora comportamental. "
                "Coatividade nao implica causalidade. Complexidade menor nao e sempre melhor. "
                "Crossover pode destruir estruturas uteis. A topologia final depende da seed. "
                "Neuronios nao sao criados ou removidos; NEAT e speciation nao foram implementados. "
                "Estrutura aprendida durante a vida nao e herdada.\n",
                context->config.experiment_name,
                context->actual_experiment_name,
                context->blueprint.connection_count,
                context->blueprint.neuron_count,
                context->config.structure_min_connections,
                context->config.structure_max_connections,
                context->config.structure_evolve_delays ? "ativa" : "inativa",
                context->global_best_structure.connection_count,
                (unsigned long long)best_signature,
                context->base.plasticity_enabled ? "STDP pode alterar pesos durante a vida; o genoma inicial permanece separado." : "STDP inativo.",
                context->base.reward_enabled ? "R-STDP pode alterar pesos durante a vida; eligibilities nao sao herdadas." : "R-STDP inativo.",
                context->base.homeostasis_enabled ? "Estado por neuronio e preservado durante rebuilds atomicos." : "Homeostase inativa.",
                context->config.structure_complexity_penalty,
                (unsigned long long)context->neuron_blueprint_signature,
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
    StructureGenome lifetime_final_structure = {0};
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
        structure_genome_destroy(&lifetime_final_structure);
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
        !evaluate_genome(
            context,
            engine->global_best_genes,
            context->config.structure_enabled ?
                &context->global_best_structure : NULL,
            context->config.evaluation_seed_base,
            &observer,
            context->config.structure_enabled ?
                &lifetime_final_structure : NULL,
            &evaluation))
    {
        close_file(&observer.population);
        close_file(&observer.raster);
        close_file(&observer.weights_initial);
        close_file(&observer.weights_final);
        structure_genome_destroy(&lifetime_final_structure);
        return 0;
    }
    close_file(&observer.population);
    close_file(&observer.raster);
    close_file(&observer.weights_initial);
    close_file(&observer.weights_final);
    if (!evaluation.valid)
    {
        structure_genome_destroy(&lifetime_final_structure);
        return 0;
    }
    if (context->config.structure_enabled &&
        !write_structure_topology_csv(
            context, "best_topology_lifetime_final.csv",
            &lifetime_final_structure, "lifetime_final_phenotype"))
    {
        structure_genome_destroy(&lifetime_final_structure);
        return 0;
    }
    structure_genome_destroy(&lifetime_final_structure);

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

static int ensure_evolution_index_schema(const char *path)
{
    char temp_path[EVOLUTION_OUTPUT_PATH_MAX];
    char header[1024];
    FILE *input;
    FILE *output;
    int ok = 1;
    if (!file_exists(path))
        return 1;
    input = fopen(path, "r");
    if (input == NULL || fgets(header, sizeof(header), input) == NULL)
    {
        if (input != NULL)
            fclose(input);
        return 0;
    }
    if (strcmp(header, EVOLUTION_INDEX_HEADER) == 0)
    {
        fclose(input);
        return 1;
    }
    if (strcmp(header, EVOLUTION_INDEX_HEADER_LEGACY) != 0 ||
        snprintf(temp_path, sizeof(temp_path), "%s.tmp", path) >=
            (int)sizeof(temp_path))
    {
        fclose(input);
        return 0;
    }
    output = fopen(temp_path, "w");
    if (output == NULL)
    {
        fclose(input);
        return 0;
    }
    if (fprintf(output, EVOLUTION_INDEX_HEADER) < 0)
        ok = 0;
    while (ok && fgets(header, sizeof(header), input) != NULL)
    {
        header[strcspn(header, "\r\n")] = '\0';
        if (fprintf(output, "%s,NA,false,NA,NA,NA,NA\n", header) < 0)
            ok = 0;
    }
    if (ferror(input))
        ok = 0;
    if (fclose(input) != 0)
        ok = 0;
    if (fclose(output) != 0)
        ok = 0;
    if (!ok || !MoveFileExA(
            temp_path, path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temp_path);
        return 0;
    }
    return 1;
}

static size_t current_topology_unique_count(
    const EvolutionRunContext *context,
    const EvolutionEngine *engine)
{
    size_t count = 0;
    if (!context->config.structure_enabled)
        return 0;
    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        int unique = 1;
        for (size_t previous = 0; previous < i; previous++)
        {
            if (context->structure_population[previous].topology_signature_initial ==
                context->structure_population[i].topology_signature_initial)
            {
                unique = 0;
                break;
            }
        }
        if (unique)
            count++;
    }
    return count;
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
    if (!ensure_evolution_index_schema(index_path))
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
    if (fprintf(file,
                "%s,%s,%s,%s,%s,%s,%d,%d,%zu,%.17g,%llu,%s,%s,%s,",
                timestamp, context->config.experiment_name,
                context->actual_experiment_name, context->output_directory,
                context->config_path, context->config.base_scenario,
                context->config.population_size, context->config.generations,
                context->gene_count, engine->global_best_fitness_selection,
                (unsigned long long)engine->global_best_individual_id,
                status,
                context->config.genome_mode,
                context->config.structure_enabled ? "true" : "false") < 0)
    {
        fclose(file);
        return 0;
    }
    if (context->config.structure_enabled)
    {
        if (fprintf(file, "%zu,%zu,%.17g,best_topology.csv\n",
                    context->global_best_structure.connection_count,
                    current_topology_unique_count(context, engine),
                    context->config.structure_complexity_penalty) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    else if (fprintf(file, "NA,NA,NA,NA\n") < 0)
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

    if (!resumed &&
        (!write_lineage_population(files.lineage, context, engine) ||
         !write_structural_events_population(
             context, engine, files.structural_events)))
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

        if (!breed_next_generation(context, engine) ||
            !write_lineage_population(files.lineage, context, engine) ||
            !write_structural_events_population(
                context, engine, files.structural_events))
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
        !write_best_inherited_topologies(context) ||
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
         initialize_structure_population(&context, &engine) &&
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
    char structure_checkpoint_path[EVOLUTION_OUTPUT_PATH_MAX];
    FILE *checkpoint = NULL;
    FILE *structure_checkpoint = NULL;
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
        !path_join(structure_checkpoint_path, sizeof(structure_checkpoint_path),
                   experiment_directory, "checkpoint_structure.txt") ||
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
    if (ok && context.config.structure_enabled)
    {
        if (!file_exists(structure_checkpoint_path))
            ok = 0;
        else
        {
            structure_checkpoint = fopen(structure_checkpoint_path, "r");
            ok = structure_checkpoint != NULL &&
                read_structure_checkpoint(
                    &context, &engine, structure_checkpoint,
                    next_generation, completed);
            if (structure_checkpoint != NULL)
                fclose(structure_checkpoint);
        }
    }
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
