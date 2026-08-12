#include "minisnn_worlds_brain_bridge.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    MiniSNNWorldsKernelEntityId actor;
    MiniSNN *brain;
    int has_cached_decision;
    MiniSNNWorldsTick cached_tick;
    MiniSNNWorldsBrainDecisionReport cached_report;
} BrainBinding;

struct MiniSNNWorldsBrainBridge
{
    MiniSNNWorldsBrainBridgeConfig config;
    const MiniSNNWorldsDomain *bound_domain;
    BrainBinding *bindings;
    size_t binding_count;
    size_t binding_capacity;
    MiniSNNWorldsBrainBridgeDiagnostics diagnostics;
    MiniSNNWorldsBrainBridgeError last_error;
};

static MiniSNNWorldsBrainBridgeError bridge_fail(
    MiniSNNWorldsBrainBridge *bridge,
    MiniSNNWorldsBrainBridgeError error)
{
    if (bridge != NULL)
    {
        bridge->last_error = error;
    }
    return error;
}

static int id_equal(MiniSNNWorldsKernelEntityId left,
                    MiniSNNWorldsKernelEntityId right)
{
    return left.value == right.value;
}

static double clamp_double(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static double normalized_unsigned(uint64_t value, uint64_t maximum)
{
    if (maximum == 0U) return 0.0;
    return clamp_double((double)value / (double)maximum, 0.0, 1.0);
}

static double normalized_unsigned_scale(uint64_t value, double scale)
{
    if (!isfinite(scale) || scale <= 0.0) return 0.0;
    return clamp_double((double)value / scale, 0.0, 1.0);
}

static double normalized_signed(MiniSNNWorldsKernelScalar value, double scale)
{
    double result;
    if (value >= 0 && (double)value >= scale) return 1.0;
    if (value < 0 && -(double)value >= scale) return -1.0;
    result = (double)value / scale;
    return clamp_double(result, -1.0, 1.0);
}

static BrainBinding *find_binding(MiniSNNWorldsBrainBridge *bridge,
                                  MiniSNNWorldsKernelEntityId actor)
{
    size_t index;
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        if (id_equal(bridge->bindings[index].actor, actor)) return &bridge->bindings[index];
    }
    return NULL;
}

static const BrainBinding *find_binding_const(const MiniSNNWorldsBrainBridge *bridge,
                                              MiniSNNWorldsKernelEntityId actor)
{
    size_t index;
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        if (id_equal(bridge->bindings[index].actor, actor)) return &bridge->bindings[index];
    }
    return NULL;
}

static int domain_organism_info(const MiniSNNWorldsDomain *domain,
                                MiniSNNWorldsKernelEntityId actor,
                                MiniSNNWorldsDomainOrganismInfo *out_info)
{
    size_t index;
    MiniSNNWorldsDomainOrganismInfo info;
    if (domain == NULL || out_info == NULL) return 0;
    for (index = 0U; index < minisnn_worlds_domain_organism_count(domain); ++index)
    {
        if (minisnn_worlds_domain_organism_at(domain, index, &info) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE) return 0;
        if (id_equal(info.entity_id, actor))
        {
            *out_info = info;
            return 1;
        }
    }
    return 0;
}

static int domain_food_exists(const MiniSNNWorldsDomain *domain,
                              MiniSNNWorldsKernelEntityId actor)
{
    size_t index;
    MiniSNNWorldsDomainFoodInfo info;

    if (domain == NULL) return 0;
    for (index = 0U; index < minisnn_worlds_domain_food_count(domain); ++index)
    {
        if (minisnn_worlds_domain_food_at(domain, index, &info) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE) return 0;
        if (id_equal(info.entity_id, actor)) return 1;
    }
    return 0;
}

static MiniSNNWorldsBrainBridgeError actor_binding_error(
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsKernelEntityId actor)
{
    return domain_food_exists(domain, actor) ?
        MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_ORGANISM :
        MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_UNKNOWN_ACTOR;
}

static int increment_counter(uint64_t *value)
{
    if (*value == UINT64_MAX) return 0;
    ++*value;
    return 1;
}

static MiniSNNWorldsBrainBridgeError validate_config(
    const MiniSNNWorldsBrainBridgeConfig *config)
{
    uint32_t index;
    if (config == NULL || !isfinite(config->perception_scale) ||
        !isfinite(config->input_gain) || config->perception_scale <= 0.0 ||
        config->input_gain <= 0.0 || config->move_step <= 0 ||
        config->core_steps_per_decision == 0U ||
        config->core_steps_per_decision > MINISNN_WORLDS_BRAIN_BRIDGE_MAX_CORE_STEPS)
    {
        return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1; ++index)
    {
        if (config->sensor_neuron[index] < 0) return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++index)
    {
        if (config->action_neuron[index] < 0) return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT;
    }
    return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

MiniSNNWorldsBrainBridgeConfig minisnn_worlds_brain_bridge_config_default(void)
{
    MiniSNNWorldsBrainBridgeConfig result;
    uint32_t index;
    result.perception_scale = 1000.0;
    result.input_gain = 100.0;
    result.move_step = 1000;
    result.core_steps_per_decision = 8U;
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1; ++index)
        result.sensor_neuron[index] = (int)index;
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++index)
        result.action_neuron[index] = (int)index;
    return result;
}

int minisnn_worlds_brain_bridge_config_is_valid(const MiniSNNWorldsBrainBridgeConfig *config)
{
    return validate_config(config) == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

MiniSNNWorldsBrainBridge *minisnn_worlds_brain_bridge_create(
    const MiniSNNWorldsBrainBridgeConfig *config,
    MiniSNNWorldsBrainBridgeError *out_error)
{
    MiniSNNWorldsBrainBridge *bridge;
    MiniSNNWorldsBrainBridgeError error = validate_config(config);
    if (out_error != NULL) *out_error = error;
    if (error != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE) return NULL;
    bridge = (MiniSNNWorldsBrainBridge *)calloc(1U, sizeof(*bridge));
    if (bridge == NULL)
    {
        if (out_error != NULL) *out_error = MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_ALLOCATION;
        return NULL;
    }
    bridge->config = *config;
    bridge->last_error = MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
    return bridge;
}

void minisnn_worlds_brain_bridge_destroy(MiniSNNWorldsBrainBridge **bridge_ptr)
{
    if (bridge_ptr != NULL && *bridge_ptr != NULL)
    {
        free((*bridge_ptr)->bindings);
        free(*bridge_ptr);
        *bridge_ptr = NULL;
    }
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_last_error(
    const MiniSNNWorldsBrainBridge *bridge)
{
    return bridge == NULL ? MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT : bridge->last_error;
}

const char *minisnn_worlds_brain_bridge_error_string(MiniSNNWorldsBrainBridgeError error)
{
    switch (error)
    {
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE: return "none";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT: return "null argument";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT: return "invalid argument";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_ALLOCATION: return "allocation failed";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DUPLICATE_BINDING: return "duplicate binding";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_UNKNOWN_ACTOR: return "unknown actor";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_ORGANISM: return "actor is not an organism";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_BOUND: return "actor not bound";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH: return "domain mismatch";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_FAILURE: return "domain failure";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_CORE_FAILURE: return "core failure";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_TICK_REGRESSION: return "domain tick regressed";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVARIANT_VIOLATION: return "invariant violation";
        case MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW: return "counter overflow";
        default: return "unknown brain bridge error";
    }
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_bind(
    MiniSNNWorldsBrainBridge *bridge, const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor, MiniSNN *brain)
{
    MiniSNNWorldsDomainOrganismInfo info;
    BrainBinding *grown;
    size_t capacity;
    if (bridge == NULL || domain == NULL || brain == NULL)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT);
    if (bridge->bound_domain != NULL && bridge->bound_domain != domain)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH);
    if (find_binding_const(bridge, actor) != NULL)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DUPLICATE_BINDING);
    if (!domain_organism_info(domain, actor, &info))
        return bridge_fail(bridge, actor_binding_error(domain, actor));
    if (minisnn_neuron_count(brain) <= 0)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT);
    if (bridge->binding_count == bridge->binding_capacity)
    {
        capacity = bridge->binding_capacity == 0U ? 4U : bridge->binding_capacity * 2U;
        if (capacity < bridge->binding_capacity || capacity > SIZE_MAX / sizeof(*grown))
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_ALLOCATION);
        grown = (BrainBinding *)realloc(bridge->bindings, capacity * sizeof(*grown));
        if (grown == NULL) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_ALLOCATION);
        bridge->bindings = grown;
        bridge->binding_capacity = capacity;
    }
    memset(&bridge->bindings[bridge->binding_count], 0, sizeof(*bridge->bindings));
    bridge->bindings[bridge->binding_count].actor = actor;
    bridge->bindings[bridge->binding_count].brain = brain;
    if (bridge->binding_count == 0U) bridge->bound_domain = domain;
    ++bridge->binding_count;
    return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_unbind(
    MiniSNNWorldsBrainBridge *bridge, MiniSNNWorldsKernelEntityId actor)
{
    BrainBinding *binding;
    if (bridge == NULL) return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT;
    binding = find_binding(bridge, actor);
    if (binding == NULL) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_BOUND);
    *binding = bridge->bindings[bridge->binding_count - 1U];
    --bridge->binding_count;
    if (bridge->binding_count == 0U) bridge->bound_domain = NULL;
    return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
}

size_t minisnn_worlds_brain_bridge_binding_count(const MiniSNNWorldsBrainBridge *bridge)
{
    return bridge == NULL ? 0U : bridge->binding_count;
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_encode_perception_v1(
    const MiniSNNWorldsDomainPerception *perception,
    MiniSNNWorldsDomainEnergy max_energy, double perception_scale,
    MiniSNNWorldsBrainSensorFrameV1 *out_frame)
{
    if (perception == NULL || out_frame == NULL || !isfinite(perception_scale) ||
        perception_scale <= 0.0 || max_energy == 0U)
        return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT;
    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_SELF_ENERGY] = normalized_unsigned(perception->self_energy, max_energy);
    out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_SELF_HUNGER] = normalized_unsigned(perception->self_hunger, max_energy);
    if (perception->nearest_food_present)
    {
        out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_PRESENT] = 1.0;
        out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_X] = normalized_signed(perception->nearest_food_delta_x, perception_scale);
        out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_Y] = normalized_signed(perception->nearest_food_delta_y, perception_scale);
        out_frame->values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DISTANCE] = normalized_unsigned_scale(perception->nearest_food_distance, perception_scale);
    }
    return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_decode_scores_v1(
    const MiniSNNWorldsDomainPerception *perception,
    const uint32_t scores[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1],
    MiniSNNWorldsKernelEntityId actor, MiniSNNWorldsKernelScalar move_step,
    MiniSNNWorldsDomainAction *out_action, MiniSNNWorldsBrainActionChannel *out_selected,
    uint8_t *out_tie_break_used, MiniSNNWorldsBrainBridgeFallback *out_fallback)
{
    uint32_t index, winner = 0U, maximum = 0U;
    uint8_t tie = 0U;
    MiniSNNWorldsBrainBridgeFallback fallback = MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NONE;
    if (perception == NULL || scores == NULL || out_action == NULL || out_selected == NULL ||
        out_tie_break_used == NULL || out_fallback == NULL || move_step <= 0)
        return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++index)
    {
        if (scores[index] > maximum) { maximum = scores[index]; winner = index; tie = 0U; }
        else if (maximum > 0U && scores[index] == maximum) tie = 1U;
    }
    memset(out_action, 0, sizeof(*out_action));
    out_action->actor = actor;
    if (maximum == 0U) { winner = MINISNN_WORLDS_BRAIN_ACTION_WAIT; fallback = MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NO_OUTPUT; }
    if (winner == MINISNN_WORLDS_BRAIN_ACTION_EAT && !perception->nearest_food_present)
    { winner = MINISNN_WORLDS_BRAIN_ACTION_WAIT; fallback = MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_EAT_WITHOUT_FOOD; }
    if (winner == MINISNN_WORLDS_BRAIN_ACTION_EAT)
    { out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_EAT; out_action->eat_target = perception->nearest_food_entity; }
    else if (winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X || winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X ||
             winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y || winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y)
    {
        out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
        if (winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X) out_action->move_delta.x = move_step;
        if (winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X) out_action->move_delta.x = -move_step;
        if (winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y) out_action->move_delta.y = move_step;
        if (winner == MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y) out_action->move_delta.y = -move_step;
    }
    else out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    *out_selected = (MiniSNNWorldsBrainActionChannel)winner;
    *out_tie_break_used = tie;
    *out_fallback = fallback;
    return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_decide(
    MiniSNNWorldsBrainBridge *bridge, const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor, MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsBrainDecisionReport *out_report)
{
    BrainBinding *binding;
    MiniSNNWorldsDomainPerception perception;
    MiniSNNWorldsDomainOrganismInfo organism;
    MiniSNNWorldsBrainDecisionReport report;
    MiniSNNWorldsTick tick;
    uint32_t sensor, action, step;
    if (bridge == NULL || domain == NULL || out_action == NULL || out_report == NULL)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT);
    if (bridge->bound_domain != NULL && bridge->bound_domain != domain)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH);
    binding = find_binding(bridge, actor);
    if (binding == NULL) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_BOUND);
    tick = minisnn_worlds_domain_tick(domain);
    if (binding->has_cached_decision && tick < binding->cached_tick)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_TICK_REGRESSION);
    if (binding->has_cached_decision && tick == binding->cached_tick)
    {
        report = binding->cached_report;
        report.cache_hit = 1U;
        *out_action = report.action;
        *out_report = report;
        if (!increment_counter(&bridge->diagnostics.total_cache_hits))
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW);
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    }
    if (!domain_organism_info(domain, actor, &organism))
        return bridge_fail(bridge, actor_binding_error(domain, actor));
    if (minisnn_worlds_domain_perceive(domain, actor, &perception) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_FAILURE);
    memset(&report, 0, sizeof(report));
    report.domain_tick = tick;
    report.actor = actor;
    if (minisnn_worlds_brain_bridge_encode_perception_v1(&perception, organism.max_energy,
        bridge->config.perception_scale, &report.sensor_frame) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_FAILURE);
    for (sensor = 0U; sensor < MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1; ++sensor)
        if (bridge->config.sensor_neuron[sensor] >= minisnn_neuron_count(binding->brain))
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT);
    for (action = 0U; action < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++action)
        if (bridge->config.action_neuron[action] >= minisnn_neuron_count(binding->brain))
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT);
    minisnn_clear_inputs(binding->brain);
    for (sensor = 0U; sensor < MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1; ++sensor)
        if (!minisnn_set_input(binding->brain, bridge->config.sensor_neuron[sensor],
            report.sensor_frame.values[sensor] * bridge->config.input_gain))
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_CORE_FAILURE);
    for (step = 0U; step < bridge->config.core_steps_per_decision; ++step)
    {
        if (minisnn_step(binding->brain) < 0)
            return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_CORE_FAILURE);
        for (action = 0U; action < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++action)
        {
            int spike;
            if (!minisnn_get_spike(binding->brain, bridge->config.action_neuron[action], &spike) ||
                (spike != 0 && spike != 1) || (spike == 1 && report.output_scores[action] == UINT32_MAX))
                return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_CORE_FAILURE);
            report.output_scores[action] += (uint32_t)spike;
        }
    }
    report.core_steps_executed = bridge->config.core_steps_per_decision;
    if (minisnn_worlds_brain_bridge_decode_scores_v1(&perception, report.output_scores, actor,
        bridge->config.move_step, &report.action, &report.selected_channel,
        &report.tie_break_used, &report.fallback) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE)
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVARIANT_VIOLATION);
    if (!increment_counter(&bridge->diagnostics.total_decisions_computed))
        return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW);
    if (report.selected_channel == MINISNN_WORLDS_BRAIN_ACTION_WAIT)
        { if (!increment_counter(&bridge->diagnostics.total_waits)) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW); }
    else if (report.selected_channel == MINISNN_WORLDS_BRAIN_ACTION_EAT)
        { if (!increment_counter(&bridge->diagnostics.total_eats)) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW); }
    else if (!increment_counter(&bridge->diagnostics.total_moves)) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW);
    if (report.tie_break_used && !increment_counter(&bridge->diagnostics.total_ties)) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW);
    if (report.fallback != MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NONE && !increment_counter(&bridge->diagnostics.total_fallbacks)) return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW);
    binding->cached_tick = tick;
    binding->cached_report = report;
    binding->has_cached_decision = 1;
    *out_action = report.action;
    *out_report = report;
    return bridge_fail(bridge, MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
}

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_get_diagnostics(
    const MiniSNNWorldsBrainBridge *bridge, MiniSNNWorldsBrainBridgeDiagnostics *out_diagnostics)
{
    if (bridge == NULL || out_diagnostics == NULL)
        return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT;
    *out_diagnostics = bridge->diagnostics;
    return MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}