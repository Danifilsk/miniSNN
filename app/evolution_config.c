#include "evolution_config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVOLUTION_LINE_MAX 1024
#define EVOLUTION_FIXED_KEY_COUNT 30

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static void set_line_error(
    char *error_message,
    size_t error_message_size,
    int line_number,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
    {
        snprintf(
            error_message,
            error_message_size,
            "linha %d: %s",
            line_number,
            message);
    }
}

static char *trim(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

static void strip_inline_comment(char *text)
{
    for (char *cursor = text; *cursor != '\0'; cursor++)
    {
        if (*cursor == '#' || *cursor == ';')
        {
            *cursor = '\0';
            return;
        }
    }
}

static int parse_bool(const char *text, int *out_value)
{
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0)
    {
        *out_value = 1;
        return 1;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0)
    {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int parse_int(const char *text, int *out_value)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX)
    {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static int parse_uint64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;

    if (text[0] == '-')
        return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_double(const char *text, double *out_value)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value))
        return 0;
    *out_value = value;
    return 1;
}

static int valid_safe_name(const char *name)
{
    size_t length;

    if (name == NULL)
        return 0;
    length = strlen(name);
    if (length == 0 || length > EVOLUTION_EXPERIMENT_NAME_MAX)
        return 0;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char character = (unsigned char)name[i];
        if (!isalnum(character) && character != '_' && character != '-')
            return 0;
    }
    return 1;
}

static int valid_base_path(const char *path)
{
    return path != NULL && path[0] != '\0' &&
           strstr(path, "..") == NULL &&
           path[0] != '/' && path[0] != '\\' &&
           !(isalpha((unsigned char)path[0]) && path[1] == ':');
}

static int split_fields(
    char *text,
    char **fields,
    size_t expected_count)
{
    size_t count = 0;
    char *cursor = text;

    while (count < expected_count)
    {
        char *comma;
        fields[count++] = trim(cursor);
        comma = strchr(cursor, ',');
        if (comma == NULL)
            break;
        *comma = '\0';
        cursor = comma + 1;
    }

    if (count != expected_count || strchr(fields[expected_count - 1], ',') != NULL)
        return 0;
    for (size_t i = 0; i < expected_count; i++)
    {
        fields[i] = trim(fields[i]);
        if (fields[i][0] == '\0')
            return 0;
    }
    return 1;
}

static int parse_indexed_key(
    const char *key,
    const char *prefix,
    int maximum,
    int *out_index)
{
    size_t prefix_length = strlen(prefix);
    int index;

    if (strncmp(key, prefix, prefix_length) != 0 ||
        !parse_int(key + prefix_length, &index) ||
        index < 0 || index >= maximum)
    {
        return 0;
    }
    *out_index = index;
    return 1;
}

void evolution_config_default(EvolutionExperimentConfig *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
    config->enabled = 1;
    snprintf(config->experiment_name, sizeof(config->experiment_name), "evolution_demo");
    snprintf(config->base_scenario, sizeof(config->base_scenario), "configs/chain.ini");
    config->population_size = 24;
    config->generations = 20;
    config->elite_count = 2;
    snprintf(config->selection, sizeof(config->selection), "tournament");
    config->tournament_size = 3;
    snprintf(config->crossover, sizeof(config->crossover), "uniform");
    config->crossover_rate = 0.8;
    snprintf(config->mutation, sizeof(config->mutation), "uniform_delta");
    config->mutation_rate = 0.15;
    config->mutation_scale = 0.10;
    snprintf(
        config->initialization,
        sizeof(config->initialization),
        "baseline_plus_mutation");
    config->initialization_scale = 0.15;
    config->evolution_seed = 12345U;
    config->evaluation_replicates = 1;
    config->evaluation_seed_base = 50000U;
    config->replicate_std_penalty = 0.0;
    config->checkpoint_interval_generations = 1;
    config->save_all_genomes = 1;
    config->save_best_run = 1;
    config->auto_unique_run = 1;
    config->history_enabled = 1;
    config->evolve_exc_weights = 1;
    config->exc_weight_min = 0.0;
    config->exc_weight_max = 500.0;
    config->evolve_inh_magnitudes = 0;
    config->inh_magnitude_min = 0.0;
    config->inh_magnitude_max = 500.0;
}

const char *evolution_fitness_goal_name(EvolutionFitnessGoal goal)
{
    if (goal == EVOLUTION_FITNESS_TARGET)
        return "target";
    if (goal == EVOLUTION_FITNESS_MAXIMIZE)
        return "maximize";
    if (goal == EVOLUTION_FITNESS_MINIMIZE)
        return "minimize";
    return "invalid";
}

static int parse_goal(const char *text, EvolutionFitnessGoal *out_goal)
{
    if (strcmp(text, "target") == 0)
        *out_goal = EVOLUTION_FITNESS_TARGET;
    else if (strcmp(text, "maximize") == 0)
        *out_goal = EVOLUTION_FITNESS_MAXIMIZE;
    else if (strcmp(text, "minimize") == 0)
        *out_goal = EVOLUTION_FITNESS_MINIMIZE;
    else
        return 0;
    return 1;
}

static int parse_scalar_gene(
    int index,
    char *value,
    EvolutionScalarGeneConfig *out_gene)
{
    char *fields[4];

    if (!split_fields(value, fields, 4) ||
        strlen(fields[0]) > EVOLUTION_PARAMETER_PATH_MAX ||
        !parse_double(fields[1], &out_gene->minimum) ||
        !parse_double(fields[2], &out_gene->maximum) ||
        !parse_double(fields[3], &out_gene->mutation_scale))
    {
        return 0;
    }

    memset(out_gene->parameter_path, 0, sizeof(out_gene->parameter_path));
    snprintf(out_gene->parameter_path, sizeof(out_gene->parameter_path), "%s", fields[0]);
    out_gene->index = index;
    return 1;
}

static int parse_fitness_term(
    int index,
    char *value,
    EvolutionFitnessTermConfig *out_term)
{
    char *fields[5];
    const char *neuron_prefix = "neuron_spikes:";

    if (!split_fields(value, fields, 5) ||
        strlen(fields[0]) > EVOLUTION_METRIC_NAME_MAX ||
        !parse_goal(fields[1], &out_term->goal) ||
        !parse_double(fields[2], &out_term->target) ||
        !parse_double(fields[3], &out_term->scale) ||
        !parse_double(fields[4], &out_term->weight))
    {
        return 0;
    }

    snprintf(out_term->metric, sizeof(out_term->metric), "%s", fields[0]);
    out_term->index = index;
    out_term->neuron_id = -1;
    out_term->has_neuron_id = 0;
    if (strncmp(fields[0], neuron_prefix, strlen(neuron_prefix)) == 0)
    {
        if (!parse_int(fields[0] + strlen(neuron_prefix), &out_term->neuron_id))
            return 0;
        out_term->has_neuron_id = 1;
    }
    return 1;
}

enum FixedKey
{
    KEY_ENABLED,
    KEY_EXPERIMENT_NAME,
    KEY_BASE_SCENARIO,
    KEY_POPULATION_SIZE,
    KEY_GENERATIONS,
    KEY_ELITE_COUNT,
    KEY_SELECTION,
    KEY_TOURNAMENT_SIZE,
    KEY_CROSSOVER,
    KEY_CROSSOVER_RATE,
    KEY_MUTATION,
    KEY_MUTATION_RATE,
    KEY_MUTATION_SCALE,
    KEY_INITIALIZATION,
    KEY_INITIALIZATION_SCALE,
    KEY_EVOLUTION_SEED,
    KEY_EVALUATION_REPLICATES,
    KEY_EVALUATION_SEED_BASE,
    KEY_REPLICATE_STD_PENALTY,
    KEY_CHECKPOINT_INTERVAL,
    KEY_SAVE_ALL_GENOMES,
    KEY_SAVE_BEST_RUN,
    KEY_AUTO_UNIQUE_RUN,
    KEY_HISTORY_ENABLED,
    KEY_EVOLVE_EXC,
    KEY_EXC_MIN,
    KEY_EXC_MAX,
    KEY_EVOLVE_INH,
    KEY_INH_MIN,
    KEY_INH_MAX
};

static int mark_seen(
    unsigned char *seen,
    int key,
    int line,
    char *error,
    size_t error_size)
{
    if (seen[key])
    {
        set_line_error(error, error_size, line, "chave duplicada");
        return 0;
    }
    seen[key] = 1;
    return 1;
}

static int parse_fixed_key(
    EvolutionExperimentConfig *config,
    const char *key,
    const char *value,
    unsigned char *seen,
    int line,
    char *error,
    size_t error_size)
{
#define PARSE_KEY(name, id, expression) \
    if (strcmp(key, name) == 0) \
    { \
        if (!mark_seen(seen, id, line, error, error_size) || !(expression)) \
        { \
            if (error != NULL && error_size > 0 && error[0] == '\0') \
                set_line_error(error, error_size, line, "valor invalido"); \
            return -1; \
        } \
        return 1; \
    }

    PARSE_KEY("enabled", KEY_ENABLED, parse_bool(value, &config->enabled));
    PARSE_KEY("experiment_name", KEY_EXPERIMENT_NAME,
        snprintf(config->experiment_name, sizeof(config->experiment_name), "%s", value) <
            (int)sizeof(config->experiment_name));
    PARSE_KEY("base_scenario", KEY_BASE_SCENARIO,
        snprintf(config->base_scenario, sizeof(config->base_scenario), "%s", value) <
            (int)sizeof(config->base_scenario));
    PARSE_KEY("population_size", KEY_POPULATION_SIZE,
        parse_int(value, &config->population_size));
    PARSE_KEY("generations", KEY_GENERATIONS, parse_int(value, &config->generations));
    PARSE_KEY("elite_count", KEY_ELITE_COUNT, parse_int(value, &config->elite_count));
    PARSE_KEY("selection", KEY_SELECTION,
        snprintf(config->selection, sizeof(config->selection), "%s", value) <
            (int)sizeof(config->selection));
    PARSE_KEY("tournament_size", KEY_TOURNAMENT_SIZE,
        parse_int(value, &config->tournament_size));
    PARSE_KEY("crossover", KEY_CROSSOVER,
        snprintf(config->crossover, sizeof(config->crossover), "%s", value) <
            (int)sizeof(config->crossover));
    PARSE_KEY("crossover_rate", KEY_CROSSOVER_RATE,
        parse_double(value, &config->crossover_rate));
    PARSE_KEY("mutation", KEY_MUTATION,
        snprintf(config->mutation, sizeof(config->mutation), "%s", value) <
            (int)sizeof(config->mutation));
    PARSE_KEY("mutation_rate", KEY_MUTATION_RATE,
        parse_double(value, &config->mutation_rate));
    PARSE_KEY("mutation_scale", KEY_MUTATION_SCALE,
        parse_double(value, &config->mutation_scale));
    PARSE_KEY("initialization", KEY_INITIALIZATION,
        snprintf(config->initialization, sizeof(config->initialization), "%s", value) <
            (int)sizeof(config->initialization));
    PARSE_KEY("initialization_scale", KEY_INITIALIZATION_SCALE,
        parse_double(value, &config->initialization_scale));
    PARSE_KEY("evolution_seed", KEY_EVOLUTION_SEED,
        parse_uint64(value, &config->evolution_seed));
    PARSE_KEY("evaluation_replicates", KEY_EVALUATION_REPLICATES,
        parse_int(value, &config->evaluation_replicates));
    PARSE_KEY("evaluation_seed_base", KEY_EVALUATION_SEED_BASE,
        parse_uint64(value, &config->evaluation_seed_base));
    PARSE_KEY("replicate_std_penalty", KEY_REPLICATE_STD_PENALTY,
        parse_double(value, &config->replicate_std_penalty));
    PARSE_KEY("checkpoint_interval_generations", KEY_CHECKPOINT_INTERVAL,
        parse_int(value, &config->checkpoint_interval_generations));
    PARSE_KEY("save_all_genomes", KEY_SAVE_ALL_GENOMES,
        parse_bool(value, &config->save_all_genomes));
    PARSE_KEY("save_best_run", KEY_SAVE_BEST_RUN,
        parse_bool(value, &config->save_best_run));
    PARSE_KEY("auto_unique_run", KEY_AUTO_UNIQUE_RUN,
        parse_bool(value, &config->auto_unique_run));
    PARSE_KEY("history_enabled", KEY_HISTORY_ENABLED,
        parse_bool(value, &config->history_enabled));
    PARSE_KEY("evolve_exc_weights", KEY_EVOLVE_EXC,
        parse_bool(value, &config->evolve_exc_weights));
    PARSE_KEY("exc_weight_min", KEY_EXC_MIN,
        parse_double(value, &config->exc_weight_min));
    PARSE_KEY("exc_weight_max", KEY_EXC_MAX,
        parse_double(value, &config->exc_weight_max));
    PARSE_KEY("evolve_inh_magnitudes", KEY_EVOLVE_INH,
        parse_bool(value, &config->evolve_inh_magnitudes));
    PARSE_KEY("inh_magnitude_min", KEY_INH_MIN,
        parse_double(value, &config->inh_magnitude_min));
    PARSE_KEY("inh_magnitude_max", KEY_INH_MAX,
        parse_double(value, &config->inh_magnitude_max));
#undef PARSE_KEY
    return 0;
}

static int scalar_path_supported(
    const ScenarioConfig *base,
    const char *path)
{
    if (strncmp(path, "plasticity.", 11) == 0)
    {
        return base->plasticity_enabled &&
            (strcmp(path, "plasticity.a_plus") == 0 ||
             strcmp(path, "plasticity.a_minus") == 0 ||
             strcmp(path, "plasticity.tau_plus") == 0 ||
             strcmp(path, "plasticity.tau_minus") == 0);
    }
    if (strncmp(path, "reward.", 7) == 0)
    {
        return base->reward_enabled &&
            (strcmp(path, "reward.learning_rate") == 0 ||
             strcmp(path, "reward.eligibility_tau") == 0);
    }
    if (strncmp(path, "homeostasis.", 12) == 0)
    {
        if (!base->homeostasis_enabled)
            return 0;
        if (strcmp(path, "homeostasis.target_rate") == 0 ||
            strcmp(path, "homeostasis.rate_tau") == 0)
            return 1;
        if (strcmp(path, "homeostasis.threshold_eta") == 0)
            return base->homeostasis_intrinsic_enabled;
        if (strcmp(path, "homeostasis.scaling_eta") == 0)
            return base->homeostasis_synaptic_scaling_enabled;
        if (strcmp(path, "homeostasis.inhibitory_gain_eta") == 0)
            return base->homeostasis_inhibitory_gain_enabled;
    }
    return 0;
}

int evolution_config_scalar_baseline(
    const ScenarioConfig *base,
    const char *path,
    double *out_value)
{
    if (base == NULL || path == NULL || out_value == NULL)
        return 0;

#define BASELINE(field_path, field) \
    if (strcmp(path, field_path) == 0) \
    { \
        *out_value = base->field; \
        return isfinite(*out_value); \
    }
    BASELINE("plasticity.a_plus", plasticity_a_plus);
    BASELINE("plasticity.a_minus", plasticity_a_minus);
    BASELINE("plasticity.tau_plus", plasticity_tau_plus);
    BASELINE("plasticity.tau_minus", plasticity_tau_minus);
    BASELINE("reward.learning_rate", reward_learning_rate);
    BASELINE("reward.eligibility_tau", reward_eligibility_tau);
    BASELINE("homeostasis.target_rate", homeostasis_target_rate);
    BASELINE("homeostasis.rate_tau", homeostasis_rate_tau);
    BASELINE("homeostasis.threshold_eta", homeostasis_threshold_eta);
    BASELINE("homeostasis.scaling_eta", homeostasis_scaling_eta);
    BASELINE("homeostasis.inhibitory_gain_eta", homeostasis_inhibitory_gain_eta);
#undef BASELINE
    return 0;
}

static int metric_supported(
    const ScenarioConfig *base,
    const EvolutionFitnessTermConfig *term)
{
    static const char *always[] = {
        "activity_total_spikes",
        "activity_active_fraction",
        "activity_mean_spikes_per_step",
        "activity_first_active_step",
        "activity_last_active_step",
        "exc_total_spikes",
        "inh_total_spikes",
        "network_final_weight_mean",
        "network_final_weight_std"
    };

    if (term->has_neuron_id)
        return term->neuron_id >= 0 && term->neuron_id < base->neurons;
    for (size_t i = 0; i < sizeof(always) / sizeof(always[0]); i++)
    {
        if (strcmp(term->metric, always[i]) == 0)
            return 1;
    }
    if (strcmp(term->metric, "homeostasis_rate_error_final") == 0 ||
        strcmp(term->metric, "homeostasis_rate_error_mean_absolute") == 0)
        return base->homeostasis_enabled;
    if (strcmp(term->metric, "reward_weight_total_signed_change") == 0 ||
        strcmp(term->metric, "reward_weight_total_absolute_change") == 0 ||
        strcmp(term->metric, "reward_modified_connection_fraction") == 0)
        return base->reward_enabled;
    return 0;
}

int evolution_config_validate(
    const EvolutionExperimentConfig *config,
    const ScenarioConfig *base,
    char *error_message,
    size_t error_message_size)
{
    if (config == NULL || base == NULL)
    {
        set_error(error_message, error_message_size, "configuracao nula");
        return 0;
    }
    if (!config->enabled)
    {
        set_error(error_message, error_message_size, "evolution.enabled deve ser true");
        return 0;
    }
    if (!valid_safe_name(config->experiment_name) ||
        !valid_base_path(config->base_scenario))
    {
        set_error(error_message, error_message_size, "nome ou caminho do experimento invalido");
        return 0;
    }
    if (config->population_size < 2 || config->generations < 1 ||
        config->elite_count < 1 || config->elite_count >= config->population_size ||
        config->tournament_size < 2 ||
        config->tournament_size > config->population_size)
    {
        set_error(error_message, error_message_size, "tamanho de populacao, geracoes, elite ou torneio invalido");
        return 0;
    }
    if (strcmp(config->selection, "tournament") != 0 ||
        strcmp(config->crossover, "uniform") != 0 ||
        strcmp(config->mutation, "uniform_delta") != 0 ||
        (strcmp(config->initialization, "baseline_plus_mutation") != 0 &&
         strcmp(config->initialization, "uniform") != 0))
    {
        set_error(error_message, error_message_size, "metodo evolutivo nao suportado");
        return 0;
    }
    if (!isfinite(config->crossover_rate) || config->crossover_rate < 0.0 ||
        config->crossover_rate > 1.0 ||
        !isfinite(config->mutation_rate) || config->mutation_rate < 0.0 ||
        config->mutation_rate > 1.0 ||
        !isfinite(config->mutation_scale) || config->mutation_scale <= 0.0 ||
        config->mutation_scale > 1.0 ||
        !isfinite(config->initialization_scale) ||
        config->initialization_scale < 0.0 || config->initialization_scale > 1.0 ||
        !isfinite(config->replicate_std_penalty) ||
        config->replicate_std_penalty < 0.0)
    {
        set_error(error_message, error_message_size, "taxa ou escala evolutiva invalida");
        return 0;
    }
    if (config->evaluation_replicates < 1 ||
        config->checkpoint_interval_generations < 1)
    {
        set_error(error_message, error_message_size, "replicas ou intervalo de checkpoint invalido");
        return 0;
    }
    if (!isfinite(config->exc_weight_min) || !isfinite(config->exc_weight_max) ||
        config->exc_weight_min < 0.0 || config->exc_weight_min >= config->exc_weight_max ||
        !isfinite(config->inh_magnitude_min) || !isfinite(config->inh_magnitude_max) ||
        config->inh_magnitude_min < 0.0 ||
        config->inh_magnitude_min >= config->inh_magnitude_max)
    {
        set_error(error_message, error_message_size, "limites de peso invalidos");
        return 0;
    }
    if ((config->evolve_exc_weights &&
         (base->excitatory_weight < config->exc_weight_min ||
          base->excitatory_weight > config->exc_weight_max)) ||
        (config->evolve_inh_magnitudes &&
         (-base->inhibitory_weight < config->inh_magnitude_min ||
          -base->inhibitory_weight > config->inh_magnitude_max)))
    {
        set_error(error_message, error_message_size, "peso-base fora dos limites evolutivos");
        return 0;
    }
    if (!config->evolve_exc_weights && !config->evolve_inh_magnitudes &&
        config->scalar_gene_count == 0)
    {
        set_error(error_message, error_message_size, "nenhum gene evolutivo configurado");
        return 0;
    }
    if (config->fitness_term_count < 1)
    {
        set_error(error_message, error_message_size, "nenhum termo de fitness configurado");
        return 0;
    }

    for (int i = 0; i < config->scalar_gene_count; i++)
    {
        const EvolutionScalarGeneConfig *gene = &config->scalar_genes[i];
        double baseline;
        if (gene->index != i ||
            !scalar_path_supported(base, gene->parameter_path) ||
            !evolution_config_scalar_baseline(base, gene->parameter_path, &baseline) ||
            !isfinite(gene->minimum) || !isfinite(gene->maximum) ||
            gene->minimum >= gene->maximum || baseline < gene->minimum ||
            baseline > gene->maximum || !isfinite(gene->mutation_scale) ||
            gene->mutation_scale <= 0.0 || gene->mutation_scale > 1.0)
        {
            set_error(error_message, error_message_size, "gene escalar invalido ou indisponivel");
            return 0;
        }
        for (int previous = 0; previous < i; previous++)
        {
            if (strcmp(gene->parameter_path,
                       config->scalar_genes[previous].parameter_path) == 0)
            {
                set_error(error_message, error_message_size, "gene escalar duplicado");
                return 0;
            }
        }
    }

    for (int i = 0; i < config->fitness_term_count; i++)
    {
        const EvolutionFitnessTermConfig *term = &config->fitness_terms[i];
        if (term->index != i || !metric_supported(base, term) ||
            term->goal < EVOLUTION_FITNESS_TARGET ||
            term->goal > EVOLUTION_FITNESS_MINIMIZE ||
            !isfinite(term->target) || !isfinite(term->scale) ||
            term->scale <= 0.0 || !isfinite(term->weight) || term->weight <= 0.0)
        {
            set_error(error_message, error_message_size, "termo de fitness invalido ou indisponivel");
            return 0;
        }
        for (int previous = 0; previous < i; previous++)
        {
            const EvolutionFitnessTermConfig *other = &config->fitness_terms[previous];
            if (strcmp(term->metric, other->metric) == 0 &&
                term->goal == other->goal && term->target == other->target &&
                term->scale == other->scale && term->weight == other->weight)
            {
                set_error(error_message, error_message_size, "termo de fitness duplicado");
                return 0;
            }
        }
    }

    return 1;
}

int evolution_config_load_file(
    const char *filename,
    EvolutionExperimentConfig *out_config,
    ScenarioConfig *out_base_scenario,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    EvolutionExperimentConfig config;
    ScenarioConfig base;
    unsigned char seen[EVOLUTION_FIXED_KEY_COUNT] = {0};
    unsigned char scalar_seen[EVOLUTION_MAX_SCALAR_GENES] = {0};
    unsigned char term_seen[EVOLUTION_MAX_FITNESS_TERMS] = {0};
    char section[32] = "";
    char line_buffer[EVOLUTION_LINE_MAX];
    int line_number = 0;

    if (filename == NULL || out_config == NULL || out_base_scenario == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    file = fopen(filename, "r");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir config evolutiva");
        return 0;
    }

    evolution_config_default(&config);
    config.scalar_gene_count = 0;
    config.fitness_term_count = 0;
    if (error_message != NULL && error_message_size > 0)
        error_message[0] = '\0';

    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL)
    {
        char *line;
        char *equals;
        char *key;
        char *value;
        int result;
        int index;

        line_number++;
        if (strchr(line_buffer, '\n') == NULL && !feof(file))
        {
            set_line_error(error_message, error_message_size, line_number, "linha muito longa");
            fclose(file);
            return 0;
        }
        strip_inline_comment(line_buffer);
        line = trim(line_buffer);
        if (line[0] == '\0')
            continue;
        if (line[0] == '[')
        {
            size_t length = strlen(line);
            if (length < 3 || line[length - 1] != ']')
            {
                set_line_error(error_message, error_message_size, line_number, "secao invalida");
                fclose(file);
                return 0;
            }
            line[length - 1] = '\0';
            if (snprintf(section, sizeof(section), "%s", trim(line + 1)) >=
                (int)sizeof(section) ||
                (strcmp(section, "evolution") != 0 &&
                 strcmp(section, "genome") != 0 &&
                 strcmp(section, "fitness") != 0))
            {
                set_line_error(error_message, error_message_size, line_number, "secao desconhecida");
                fclose(file);
                return 0;
            }
            continue;
        }

        equals = strchr(line, '=');
        if (equals == NULL || section[0] == '\0')
        {
            set_line_error(error_message, error_message_size, line_number, "linha sem chave valida");
            fclose(file);
            return 0;
        }
        *equals = '\0';
        key = trim(line);
        value = trim(equals + 1);
        if (key[0] == '\0' || value[0] == '\0')
        {
            set_line_error(error_message, error_message_size, line_number, "chave ou valor vazio");
            fclose(file);
            return 0;
        }

        result = parse_fixed_key(
            &config, key, value, seen, line_number,
            error_message, error_message_size);
        if (result < 0)
        {
            fclose(file);
            return 0;
        }
        if (result > 0)
            continue;

        if (parse_indexed_key(
                key,
                "scalar_gene_",
                EVOLUTION_MAX_SCALAR_GENES,
                &index))
        {
            if (scalar_seen[index] ||
                !parse_scalar_gene(index, value, &config.scalar_genes[index]))
            {
                set_line_error(error_message, error_message_size, line_number,
                               scalar_seen[index] ? "chave duplicada" : "gene escalar invalido");
                fclose(file);
                return 0;
            }
            scalar_seen[index] = 1;
            if (index + 1 > config.scalar_gene_count)
                config.scalar_gene_count = index + 1;
            continue;
        }
        if (parse_indexed_key(
                key,
                "term_",
                EVOLUTION_MAX_FITNESS_TERMS,
                &index))
        {
            if (term_seen[index] ||
                !parse_fitness_term(index, value, &config.fitness_terms[index]))
            {
                set_line_error(error_message, error_message_size, line_number,
                               term_seen[index] ? "chave duplicada" : "termo de fitness invalido");
                fclose(file);
                return 0;
            }
            term_seen[index] = 1;
            if (index + 1 > config.fitness_term_count)
                config.fitness_term_count = index + 1;
            continue;
        }

        set_line_error(error_message, error_message_size, line_number, "chave desconhecida");
        fclose(file);
        return 0;
    }

    if (ferror(file))
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao ler config evolutiva");
        return 0;
    }
    fclose(file);

    for (int i = 0; i < config.scalar_gene_count; i++)
    {
        if (!scalar_seen[i])
        {
            set_error(error_message, error_message_size, "indices scalar_gene_N devem ser sequenciais");
            return 0;
        }
    }
    for (int i = 0; i < config.fitness_term_count; i++)
    {
        if (!term_seen[i])
        {
            set_error(error_message, error_message_size, "indices term_N devem ser sequenciais");
            return 0;
        }
    }

    if (!scenario_config_load_file(
            config.base_scenario,
            &base,
            error_message,
            error_message_size) ||
        !evolution_config_validate(
            &config,
            &base,
            error_message,
            error_message_size))
    {
        return 0;
    }

    *out_config = config;
    *out_base_scenario = base;
    return 1;
}

int evolution_config_save_file(
    const char *filename,
    const EvolutionExperimentConfig *config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;

    if (filename == NULL || config == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }
    file = fopen(filename, "w");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel salvar config evolutiva");
        return 0;
    }

    if (fprintf(file,
            "[evolution]\n"
            "enabled = %s\nexperiment_name = %s\nbase_scenario = %s\n"
            "population_size = %d\ngenerations = %d\nelite_count = %d\n"
            "selection = %s\ntournament_size = %d\n"
            "crossover = %s\ncrossover_rate = %.17g\n"
            "mutation = %s\nmutation_rate = %.17g\nmutation_scale = %.17g\n"
            "initialization = %s\ninitialization_scale = %.17g\n"
            "evolution_seed = %llu\nevaluation_replicates = %d\n"
            "evaluation_seed_base = %llu\nreplicate_std_penalty = %.17g\n"
            "checkpoint_interval_generations = %d\nsave_all_genomes = %s\n"
            "save_best_run = %s\nauto_unique_run = %s\nhistory_enabled = %s\n\n"
            "[genome]\nevolve_exc_weights = %s\nexc_weight_min = %.17g\n"
            "exc_weight_max = %.17g\nevolve_inh_magnitudes = %s\n"
            "inh_magnitude_min = %.17g\ninh_magnitude_max = %.17g\n",
            config->enabled ? "true" : "false",
            config->experiment_name,
            config->base_scenario,
            config->population_size,
            config->generations,
            config->elite_count,
            config->selection,
            config->tournament_size,
            config->crossover,
            config->crossover_rate,
            config->mutation,
            config->mutation_rate,
            config->mutation_scale,
            config->initialization,
            config->initialization_scale,
            (unsigned long long)config->evolution_seed,
            config->evaluation_replicates,
            (unsigned long long)config->evaluation_seed_base,
            config->replicate_std_penalty,
            config->checkpoint_interval_generations,
            config->save_all_genomes ? "true" : "false",
            config->save_best_run ? "true" : "false",
            config->auto_unique_run ? "true" : "false",
            config->history_enabled ? "true" : "false",
            config->evolve_exc_weights ? "true" : "false",
            config->exc_weight_min,
            config->exc_weight_max,
            config->evolve_inh_magnitudes ? "true" : "false",
            config->inh_magnitude_min,
            config->inh_magnitude_max) < 0)
    {
        fclose(file);
        return 0;
    }

    for (int i = 0; i < config->scalar_gene_count; i++)
    {
        const EvolutionScalarGeneConfig *gene = &config->scalar_genes[i];
        if (fprintf(file, "scalar_gene_%d = %s,%.17g,%.17g,%.17g\n",
                    i, gene->parameter_path, gene->minimum,
                    gene->maximum, gene->mutation_scale) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fprintf(file, "\n[fitness]\n") < 0)
    {
        fclose(file);
        return 0;
    }
    for (int i = 0; i < config->fitness_term_count; i++)
    {
        const EvolutionFitnessTermConfig *term = &config->fitness_terms[i];
        if (fprintf(file, "term_%d = %s,%s,%.17g,%.17g,%.17g\n",
                    i, term->metric, evolution_fitness_goal_name(term->goal),
                    term->target, term->scale, term->weight) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "erro ao fechar config evolutiva");
        return 0;
    }
    return 1;
}
