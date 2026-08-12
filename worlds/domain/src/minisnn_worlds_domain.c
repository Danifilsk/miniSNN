#include "minisnn_worlds_domain.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    MiniSNNWorldsDomainSpeciesConfig config;
} DomainSpecies;

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsDomainSpeciesId species_id;
    MiniSNNWorldsDomainEnergy energy;
} DomainOrganism;

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsDomainEnergy nutrition;
} DomainFood;

typedef struct
{
    size_t input_index;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsKernelCommandId command_id;
    int kind;
    int reserved_food;
} DomainPendingAction;

struct MiniSNNWorldsDomain
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsTick tick;
    MiniSNNWorldsDomainError last_error;
    DomainSpecies *species;
    size_t species_count;
    size_t species_capacity;
    DomainOrganism *organisms;
    size_t organism_count;
    size_t organism_capacity;
    DomainFood *foods;
    size_t food_count;
    size_t food_capacity;
    MiniSNNWorldsDomainEvent *events;
    size_t event_count;
    size_t event_capacity;
    uint64_t next_event_id;
    MiniSNNWorldsDomainDiagnostics diagnostics;
};

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId result = { UINT64_C(0) };
    return result;
}

static void set_error(MiniSNNWorldsDomain *domain, MiniSNNWorldsDomainError error)
{
    if (domain != NULL)
    {
        domain->last_error = error;
    }
}

static int entity_compare(MiniSNNWorldsKernelEntityId left, MiniSNNWorldsKernelEntityId right)
{
    return (left.value > right.value) - (left.value < right.value);
}

static int species_compare(MiniSNNWorldsDomainSpeciesId left, MiniSNNWorldsDomainSpeciesId right)
{
    return (left > right) - (left < right);
}

static int valid_action_type(MiniSNNWorldsDomainActionType type)
{
    return type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT ||
           type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE ||
           type == MINISNN_WORLDS_DOMAIN_ACTION_EAT;
}

static int valid_event_type(MiniSNNWorldsDomainEventType type)
{
    return type >= MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED &&
           type <= MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED;
}

static int valid_reason(MiniSNNWorldsDomainActionReason reason)
{
    return reason >= MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE &&
           reason <= MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR;
}

static int reserve_array(void **items, size_t *capacity, size_t required, size_t element_size)
{
    void *replacement;
    size_t next_capacity;

    if (required <= *capacity)
    {
        return 1;
    }
    next_capacity = *capacity == 0U ? 8U : *capacity;
    while (next_capacity < required)
    {
        if (next_capacity > SIZE_MAX / 2U)
        {
            next_capacity = required;
            break;
        }
        next_capacity *= 2U;
    }
    if (next_capacity > SIZE_MAX / element_size)
    {
        return 0;
    }
    replacement = realloc(*items, next_capacity * element_size);
    if (replacement == NULL)
    {
        return 0;
    }
    *items = replacement;
    *capacity = next_capacity;
    return 1;
}

static size_t species_index(const MiniSNNWorldsDomain *domain,
                            MiniSNNWorldsDomainSpeciesId species_id, int *found)
{
    size_t low = 0U;
    size_t high = domain->species_count;

    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        int comparison = species_compare(domain->species[middle].config.species_id, species_id);
        if (comparison < 0)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    *found = low < domain->species_count &&
             domain->species[low].config.species_id == species_id;
    return low;
}

static size_t organism_index(const MiniSNNWorldsDomain *domain,
                             MiniSNNWorldsKernelEntityId entity_id, int *found)
{
    size_t low = 0U;
    size_t high = domain->organism_count;

    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        int comparison = entity_compare(domain->organisms[middle].entity_id, entity_id);
        if (comparison < 0)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    *found = low < domain->organism_count &&
             domain->organisms[low].entity_id.value == entity_id.value;
    return low;
}

static size_t food_index(const MiniSNNWorldsDomain *domain,
                         MiniSNNWorldsKernelEntityId entity_id, int *found)
{
    size_t low = 0U;
    size_t high = domain->food_count;

    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        int comparison = entity_compare(domain->foods[middle].entity_id, entity_id);
        if (comparison < 0)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    *found = low < domain->food_count && domain->foods[low].entity_id.value == entity_id.value;
    return low;
}

static const DomainSpecies *find_species(const MiniSNNWorldsDomain *domain,
                                         MiniSNNWorldsDomainSpeciesId species_id)
{
    int found;
    size_t index = species_index(domain, species_id, &found);
    return found ? &domain->species[index] : NULL;
}

static uint64_t u64_difference(MiniSNNWorldsKernelScalar left,
                               MiniSNNWorldsKernelScalar right)
{
    if (left >= right)
    {
        return (uint64_t)left - (uint64_t)right;
    }
    return (uint64_t)right - (uint64_t)left;
}

static uint64_t add_saturating_u64(uint64_t left, uint64_t right)
{
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static int add_checked_u64(uint64_t left, uint64_t right, uint64_t *out)
{
    if (out == NULL || UINT64_MAX - left < right)
    {
        return 0;
    }
    *out = left + right;
    return 1;
}

static MiniSNNWorldsKernelScalar subtract_saturating_scalar(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right)
{
    if (right > 0 && left < INT64_MIN + right)
    {
        return INT64_MIN;
    }
    if (right < 0 && left > INT64_MAX + right)
    {
        return INT64_MAX;
    }
    return left - right;
}

static uint64_t manhattan_distance(const MiniSNNWorldsKernelTransform *left,
                                   const MiniSNNWorldsKernelTransform *right)
{
    return add_saturating_u64(
        u64_difference(left->position.x, right->position.x),
        u64_difference(left->position.y, right->position.y));
}

static int can_reference_entity(const MiniSNNWorldsDomain *domain,
                                MiniSNNWorldsKernelEntityId entity_id)
{
    MiniSNNWorldsKernelTransform transform;

    return entity_id.value != 0U &&
           minisnn_worlds_kernel_entity_exists(domain->kernel, entity_id) &&
           minisnn_worlds_kernel_entity_transform(domain->kernel, entity_id, &transform) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int reserve_events(MiniSNNWorldsDomain *domain, size_t extra)
{
    if (extra > SIZE_MAX - domain->event_count)
    {
        return 0;
    }
    return reserve_array((void **)&domain->events, &domain->event_capacity,
                         domain->event_count + extra, sizeof(*domain->events));
}

static int append_event(MiniSNNWorldsDomain *domain, MiniSNNWorldsDomainEventType type,
                        MiniSNNWorldsKernelEntityId subject,
                        MiniSNNWorldsKernelEntityId related,
                        MiniSNNWorldsDomainActionType action_type,
                        MiniSNNWorldsDomainActionReason reason,
                        MiniSNNWorldsDomainEnergy before,
                        MiniSNNWorldsDomainEnergy after)
{
    MiniSNNWorldsDomainEvent *event;

    if (domain->event_count == domain->event_capacity || domain->next_event_id == UINT64_MAX)
    {
        return 0;
    }
    event = &domain->events[domain->event_count++];
    event->event_id = domain->next_event_id++;
    event->tick = domain->tick;
    event->type = type;
    event->subject = subject;
    event->related = related;
    event->action_type = action_type;
    event->reason = reason;
    event->energy_before = before;
    event->energy_after = after;
    return 1;
}

static void account_energy(MiniSNNWorldsDomain *domain,
                           MiniSNNWorldsDomainEnergy before,
                           MiniSNNWorldsDomainEnergy after)
{
    uint64_t difference;

    if (after > before)
    {
        difference = after - before;
        domain->diagnostics.total_energy_gained += difference;
    }
    else if (before > after)
    {
        difference = before - after;
        domain->diagnostics.total_energy_spent += difference;
    }
}

static int action_pending_compare(const void *left, const void *right)
{
    const DomainPendingAction *a = left;
    const DomainPendingAction *b = right;
    int comparison = entity_compare(a->action.actor, b->action.actor);

    if (comparison != 0)
    {
        return comparison;
    }
    return (a->input_index > b->input_index) - (a->input_index < b->input_index);
}

static int event_for_command(const MiniSNNWorldsDomain *domain,
                             MiniSNNWorldsKernelCommandId command_id,
                             MiniSNNWorldsKernelEvent *out_event)
{
    size_t index;
    size_t count = minisnn_worlds_kernel_last_tick_event_count(domain->kernel);

    for (index = 0U; index < count; ++index)
    {
        MiniSNNWorldsKernelEvent event;
        if (minisnn_worlds_kernel_last_tick_event_at(domain->kernel, index, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (event.command_id.value == command_id.value)
        {
            *out_event = event;
            return 1;
        }
    }
    return 0;
}

static void set_result(MiniSNNWorldsDomainActionResult *result,
                       MiniSNNWorldsDomainActionStatus status,
                       MiniSNNWorldsDomainActionReason reason,
                       MiniSNNWorldsDomainEnergy before,
                       MiniSNNWorldsDomainEnergy after)
{
    result->status = status;
    result->reason = reason;
    result->energy_before = before;
    result->energy_after = after;
}

static int expected_event_capacity(size_t action_count, size_t organism_count, size_t *out_capacity)
{
    if (action_count > (SIZE_MAX - organism_count) / 3U)
    {
        return 0;
    }
    *out_capacity = action_count * 3U + organism_count;
    return 1;
}

static int preflight_step_headroom(const MiniSNNWorldsDomain *domain,
                                   const DomainPendingAction *pending,
                                   size_t pending_count,
                                   size_t event_reserve)
{
    uint64_t action_extra = (uint64_t)pending_count;
    uint64_t food_extra = 0U;
    uint64_t gained_extra = 0U;
    uint64_t spent_extra = 0U;
    uint64_t ignored;
    size_t index;

    if (pending_count > UINT64_MAX || event_reserve > UINT64_MAX)
    {
        return 0;
    }
    if ((uint64_t)event_reserve > UINT64_MAX - domain->next_event_id)
    {
        return 0;
    }
    if (!add_checked_u64(domain->diagnostics.total_actions, action_extra, &ignored) ||
        !add_checked_u64(domain->diagnostics.total_actions_applied, action_extra, &ignored) ||
        !add_checked_u64(domain->diagnostics.total_actions_rejected, action_extra, &ignored))
    {
        return 0;
    }

    for (index = 0U; index < pending_count; ++index)
    {
        const DomainPendingAction *entry = &pending[index];
        int found;
        size_t actor_slot;
        const DomainSpecies *species;

        if (entry->kind == 0)
        {
            continue;
        }
        actor_slot = organism_index(domain, entry->action.actor, &found);
        if (!found)
        {
            return 0;
        }
        species = find_species(domain, domain->organisms[actor_slot].species_id);
        if (species == NULL)
        {
            return 0;
        }
        if (entry->kind == 2)
        {
            if (!add_checked_u64(spent_extra, species->config.move_energy_cost, &spent_extra))
            {
                return 0;
            }
        }
        else if (entry->kind == 3)
        {
            int food_found;
            size_t food_slot = food_index(domain, entry->action.eat_target, &food_found);
            if (!food_found || food_extra == UINT64_MAX ||
                !add_checked_u64(gained_extra, domain->foods[food_slot].nutrition, &gained_extra))
            {
                return 0;
            }
            ++food_extra;
        }
    }
    for (index = 0U; index < domain->organism_count; ++index)
    {
        const DomainSpecies *species = find_species(domain, domain->organisms[index].species_id);
        uint64_t metabolic;
        if (species == NULL)
        {
            return 0;
        }
        metabolic = species->config.metabolism_per_tick;
        if (!add_checked_u64(spent_extra, metabolic, &spent_extra))
        {
            return 0;
        }
    }
    return add_checked_u64(domain->diagnostics.total_food_consumed, food_extra, &ignored) &&
           add_checked_u64(domain->diagnostics.total_energy_gained, gained_extra, &ignored) &&
           add_checked_u64(domain->diagnostics.total_energy_spent, spent_extra, &ignored);
}

static void hash_byte(uint64_t *hash, uint8_t value)
{
    *hash ^= value;
    *hash *= UINT64_C(1099511628211);
}

static void hash_u64(uint64_t *hash, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index)
    {
        hash_byte(hash, (uint8_t)(value >> (index * 8U)));
    }
}

static int validate_invariants(const MiniSNNWorldsDomain *domain)
{
    size_t index;
    uint64_t actions = 0U;
    uint64_t applied = 0U;
    uint64_t rejected = 0U;
    uint64_t foods = 0U;
    uint64_t gained = 0U;
    uint64_t spent = 0U;
    uint64_t prior_event_id = 0U;

    if (domain == NULL || domain->kernel == NULL || domain->tick != minisnn_worlds_kernel_tick(domain->kernel))
    {
        return 0;
    }
    for (index = 0U; index < domain->species_count; ++index)
    {
        const MiniSNNWorldsDomainSpeciesConfig *config = &domain->species[index].config;
        if (config->species_id == 0U || config->max_energy == 0U ||
            config->eat_range < 0 ||
            (index > 0U && config->species_id <= domain->species[index - 1U].config.species_id))
        {
            return 0;
        }
    }
    for (index = 0U; index < domain->organism_count; ++index)
    {
        const DomainOrganism *organism = &domain->organisms[index];
        const DomainSpecies *species = find_species(domain, organism->species_id);
        int food_found;

        if (organism->entity_id.value == 0U || species == NULL ||
            organism->energy > species->config.max_energy ||
            !can_reference_entity(domain, organism->entity_id) ||
            (index > 0U && organism->entity_id.value <= domain->organisms[index - 1U].entity_id.value))
        {
            return 0;
        }
        (void)food_index(domain, organism->entity_id, &food_found);
        if (food_found)
        {
            return 0;
        }
    }
    for (index = 0U; index < domain->food_count; ++index)
    {
        const DomainFood *food = &domain->foods[index];
        int organism_found;

        if (food->entity_id.value == 0U || food->nutrition == 0U ||
            !can_reference_entity(domain, food->entity_id) ||
            (index > 0U && food->entity_id.value <= domain->foods[index - 1U].entity_id.value))
        {
            return 0;
        }
        (void)organism_index(domain, food->entity_id, &organism_found);
        if (organism_found)
        {
            return 0;
        }
    }
    for (index = 0U; index < domain->event_count; ++index)
    {
        const MiniSNNWorldsDomainEvent *event = &domain->events[index];
        if (event->event_id == 0U || event->event_id <= prior_event_id ||
            event->tick > domain->tick || !valid_event_type(event->type) ||
            !valid_reason(event->reason) || !valid_action_type(event->action_type))
        {
            return 0;
        }
        prior_event_id = event->event_id;
        if (event->type == MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED)
        {
            ++actions;
            ++applied;
        }
        else if (event->type == MINISNN_WORLDS_DOMAIN_EVENT_ACTION_REJECTED)
        {
            ++actions;
            ++rejected;
        }
        else if (event->type == MINISNN_WORLDS_DOMAIN_EVENT_FOOD_CONSUMED)
        {
            ++foods;
        }
        else if (event->type == MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED)
        {
            if (event->energy_after > event->energy_before)
            {
                gained = add_saturating_u64(gained, event->energy_after - event->energy_before);
            }
            else
            {
                spent = add_saturating_u64(spent, event->energy_before - event->energy_after);
            }
        }
    }
    if (prior_event_id == UINT64_MAX ||
        domain->next_event_id != prior_event_id + UINT64_C(1) ||
        domain->diagnostics.total_actions != actions ||
        domain->diagnostics.total_actions_applied != applied ||
        domain->diagnostics.total_actions_rejected != rejected ||
        domain->diagnostics.total_food_consumed != foods ||
        domain->diagnostics.total_energy_gained != gained ||
        domain->diagnostics.total_energy_spent != spent)
    {
        return 0;
    }
    return 1;
}

MiniSNNWorldsDomain *minisnn_worlds_domain_create(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsDomainError *out_error)
{
    MiniSNNWorldsDomain *domain;

    if (out_error != NULL)
    {
        *out_error = MINISNN_WORLDS_DOMAIN_ERROR_NONE;
    }
    if (kernel == NULL)
    {
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
        }
        return NULL;
    }
    domain = calloc(1U, sizeof(*domain));
    if (domain == NULL)
    {
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION;
        }
        return NULL;
    }
    domain->kernel = kernel;
    domain->tick = minisnn_worlds_kernel_tick(kernel);
    domain->next_event_id = UINT64_C(1);
    domain->last_error = MINISNN_WORLDS_DOMAIN_ERROR_NONE;
    return domain;
}

void minisnn_worlds_domain_destroy(MiniSNNWorldsDomain *domain)
{
    if (domain != NULL)
    {
        free(domain->species);
        free(domain->organisms);
        free(domain->foods);
        free(domain->events);
        free(domain);
    }
}

MiniSNNWorldsDomainError minisnn_worlds_domain_last_error(const MiniSNNWorldsDomain *domain)
{
    return domain == NULL ? MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT : domain->last_error;
}

MiniSNNWorldsTick minisnn_worlds_domain_tick(const MiniSNNWorldsDomain *domain)
{
    return domain == NULL ? MINISNN_WORLDS_TICK_INITIAL : domain->tick;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_add_species(
    MiniSNNWorldsDomain *domain,
    const MiniSNNWorldsDomainSpeciesConfig *config)
{
    size_t index;
    int found;

    if (domain == NULL || config == NULL)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT);
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (config->species_id == 0U || config->max_energy == 0U || config->eat_range < 0)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT);
        return domain->last_error;
    }
    index = species_index(domain, config->species_id, &found);
    if (found)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_DUPLICATE_SPECIES);
        return domain->last_error;
    }
    if (!reserve_array((void **)&domain->species, &domain->species_capacity,
                       domain->species_count + 1U, sizeof(*domain->species)))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION);
        return domain->last_error;
    }
    memmove(&domain->species[index + 1U], &domain->species[index],
            (domain->species_count - index) * sizeof(*domain->species));
    domain->species[index].config = *config;
    ++domain->species_count;
    set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    return domain->last_error;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_register_organism(
    MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsDomainSpeciesId species_id,
    MiniSNNWorldsDomainEnergy initial_energy)
{
    const DomainSpecies *species;
    size_t index;
    int found;
    int other_found;

    if (domain == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    species = find_species(domain, species_id);
    if (species == NULL)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_UNKNOWN_SPECIES);
        return domain->last_error;
    }
    if (initial_energy > species->config.max_energy)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT);
        return domain->last_error;
    }
    if (!can_reference_entity(domain, entity_id))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE);
        return domain->last_error;
    }
    index = organism_index(domain, entity_id, &found);
    (void)food_index(domain, entity_id, &other_found);
    if (found || other_found)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_DUPLICATE_ENTITY);
        return domain->last_error;
    }
    if (!reserve_array((void **)&domain->organisms, &domain->organism_capacity,
                       domain->organism_count + 1U, sizeof(*domain->organisms)))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION);
        return domain->last_error;
    }
    memmove(&domain->organisms[index + 1U], &domain->organisms[index],
            (domain->organism_count - index) * sizeof(*domain->organisms));
    domain->organisms[index].entity_id = entity_id;
    domain->organisms[index].species_id = species_id;
    domain->organisms[index].energy = initial_energy;
    ++domain->organism_count;
    set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    return domain->last_error;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_register_food(
    MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsDomainEnergy nutrition)
{
    size_t index;
    int found;
    int other_found;

    if (domain == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (nutrition == 0U)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT);
        return domain->last_error;
    }
    if (!can_reference_entity(domain, entity_id))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE);
        return domain->last_error;
    }
    index = food_index(domain, entity_id, &found);
    (void)organism_index(domain, entity_id, &other_found);
    if (found || other_found)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_DUPLICATE_ENTITY);
        return domain->last_error;
    }
    if (!reserve_array((void **)&domain->foods, &domain->food_capacity,
                       domain->food_count + 1U, sizeof(*domain->foods)))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION);
        return domain->last_error;
    }
    memmove(&domain->foods[index + 1U], &domain->foods[index],
            (domain->food_count - index) * sizeof(*domain->foods));
    domain->foods[index].entity_id = entity_id;
    domain->foods[index].nutrition = nutrition;
    ++domain->food_count;
    set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    return domain->last_error;
}

size_t minisnn_worlds_domain_organism_count(const MiniSNNWorldsDomain *domain)
{
    return domain == NULL ? 0U : domain->organism_count;
}

size_t minisnn_worlds_domain_food_count(const MiniSNNWorldsDomain *domain)
{
    return domain == NULL ? 0U : domain->food_count;
}

size_t minisnn_worlds_domain_event_count(const MiniSNNWorldsDomain *domain)
{
    return domain == NULL ? 0U : domain->event_count;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_organism_at(
    const MiniSNNWorldsDomain *domain, size_t canonical_index,
    MiniSNNWorldsDomainOrganismInfo *out_info)
{
    const DomainOrganism *organism;
    const DomainSpecies *species;

    if (domain == NULL || out_info == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= domain->organism_count)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
    }
    organism = &domain->organisms[canonical_index];
    species = find_species(domain, organism->species_id);
    if (species == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
    }
    out_info->entity_id = organism->entity_id;
    out_info->species_id = organism->species_id;
    out_info->energy = organism->energy;
    out_info->max_energy = species->config.max_energy;
    out_info->hunger = species->config.max_energy - organism->energy;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_food_at(
    const MiniSNNWorldsDomain *domain, size_t canonical_index,
    MiniSNNWorldsDomainFoodInfo *out_info)
{
    if (domain == NULL || out_info == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= domain->food_count)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
    }
    out_info->entity_id = domain->foods[canonical_index].entity_id;
    out_info->nutrition = domain->foods[canonical_index].nutrition;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_event_at(
    const MiniSNNWorldsDomain *domain, size_t canonical_index,
    MiniSNNWorldsDomainEvent *out_event)
{
    if (domain == NULL || out_event == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= domain->event_count)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
    }
    *out_event = domain->events[canonical_index];
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_get_diagnostics(
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsDomainDiagnostics *out_diagnostics)
{
    if (domain == NULL || out_diagnostics == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    *out_diagnostics = domain->diagnostics;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_perceive(
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsKernelEntityId organism_id,
    MiniSNNWorldsDomainPerception *out_perception)
{
    int found;
    size_t organism_slot;
    size_t index;
    MiniSNNWorldsKernelTransform self_transform;
    const DomainSpecies *species;

    if (domain == NULL || out_perception == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    organism_slot = organism_index(domain, organism_id, &found);
    if (!found)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
    }
    if (minisnn_worlds_kernel_entity_transform(domain->kernel, organism_id, &self_transform) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE;
    }
    species = find_species(domain, domain->organisms[organism_slot].species_id);
    if (species == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
    }
    memset(out_perception, 0, sizeof(*out_perception));
    out_perception->self_energy = domain->organisms[organism_slot].energy;
    out_perception->self_hunger = species->config.max_energy - out_perception->self_energy;
    for (index = 0U; index < domain->food_count; ++index)
    {
        MiniSNNWorldsKernelTransform food_transform;
        uint64_t distance;

        if (minisnn_worlds_kernel_entity_transform(domain->kernel,
                                                    domain->foods[index].entity_id,
                                                    &food_transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE;
        }
        distance = manhattan_distance(&self_transform, &food_transform);
        if (!out_perception->nearest_food_present ||
            distance < out_perception->nearest_food_distance ||
            (distance == out_perception->nearest_food_distance &&
             domain->foods[index].entity_id.value < out_perception->nearest_food_entity.value))
        {
            out_perception->nearest_food_present = true;
            out_perception->nearest_food_entity = domain->foods[index].entity_id;
            out_perception->nearest_food_distance = distance;
            out_perception->nearest_food_delta_x = subtract_saturating_scalar(
                food_transform.position.x, self_transform.position.x);
            out_perception->nearest_food_delta_y = subtract_saturating_scalar(
                food_transform.position.y, self_transform.position.y);
        }
    }
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_step(
    MiniSNNWorldsDomain *domain, const MiniSNNWorldsDomainAction *actions,
    size_t action_count, MiniSNNWorldsDomainActionResult *out_results)
{
    DomainPendingAction *pending = NULL;
    size_t pending_count = action_count;
    size_t index;
    size_t event_reserve;
    MiniSNNWorldsTick target_tick;
    MiniSNNWorldsDomainError result = MINISNN_WORLDS_DOMAIN_ERROR_NONE;
    int batch_open = 0;

    if (domain == NULL || (action_count > 0U && (actions == NULL || out_results == NULL)))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT);
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (domain->tick != minisnn_worlds_kernel_tick(domain->kernel))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_TEMPORAL_DIVERGENCE);
        return domain->last_error;
    }
    if (domain->tick == UINT64_MAX || minisnn_worlds_kernel_command_batch_active(domain->kernel))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE);
        return domain->last_error;
    }
    if (!validate_invariants(domain))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION);
        return domain->last_error;
    }
    if (!expected_event_capacity(action_count, domain->organism_count, &event_reserve))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE);
        return domain->last_error;
    }
    if (action_count > 0U)
    {
        pending = calloc(action_count, sizeof(*pending));
        if (pending == NULL)
        {
            set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION);
            return domain->last_error;
        }
        for (index = 0U; index < action_count; ++index)
        {
            pending[index].input_index = index;
            pending[index].action = actions[index];
            pending[index].kind = 0;
            pending[index].command_id.value = UINT64_C(0);
            set_result(&out_results[index], MINISNN_WORLDS_DOMAIN_ACTION_REJECTED,
                       MINISNN_WORLDS_DOMAIN_ACTION_REASON_INVALID_ACTION, 0U, 0U);
        }
        qsort(pending, pending_count, sizeof(*pending), action_pending_compare);
    }

    /* Semantic preflight. No Kernel command has been submitted yet. */
    for (index = 0U; index < pending_count; ++index)
    {
        DomainPendingAction *entry = &pending[index];
        MiniSNNWorldsDomainActionResult *action_result = &out_results[entry->input_index];
        int actor_found;
        size_t actor_index = organism_index(domain, entry->action.actor, &actor_found);
        const DomainSpecies *species;

        if (index > 0U && entry->action.actor.value == pending[index - 1U].action.actor.value)
        {
            set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_REJECTED,
                       MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR, 0U, 0U);
            continue;
        }
        if (!actor_found)
        {
            set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_REJECTED,
                       MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_NOT_ORGANISM, 0U, 0U);
            continue;
        }
        species = find_species(domain, domain->organisms[actor_index].species_id);
        if (species == NULL)
        {
            result = MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
            goto finish;
        }
        action_result->energy_before = domain->organisms[actor_index].energy;
        action_result->energy_after = action_result->energy_before;
        if (!valid_action_type(entry->action.type))
        {
            action_result->status = MINISNN_WORLDS_DOMAIN_ACTION_REJECTED;
            action_result->reason = MINISNN_WORLDS_DOMAIN_ACTION_REASON_INVALID_ACTION;
            continue;
        }
        if (entry->action.type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT)
        {
            entry->kind = 1;
            continue;
        }
        if (entry->action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE)
        {
            if (domain->organisms[actor_index].energy < species->config.move_energy_cost)
            {
                action_result->status = MINISNN_WORLDS_DOMAIN_ACTION_REJECTED;
                action_result->reason = MINISNN_WORLDS_DOMAIN_ACTION_REASON_INSUFFICIENT_ENERGY;
                continue;
            }
            entry->kind = 2;
            continue;
        }
        if (entry->action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT)
        {
            int food_found;
            size_t food_slot = food_index(domain, entry->action.eat_target, &food_found);
            MiniSNNWorldsKernelTransform actor_transform;
            MiniSNNWorldsKernelTransform food_transform;
            uint64_t distance;
            size_t prior;
            int claimed = 0;

            if (!food_found)
            {
                action_result->status = MINISNN_WORLDS_DOMAIN_ACTION_REJECTED;
                action_result->reason = minisnn_worlds_kernel_entity_exists(
                    domain->kernel, entry->action.eat_target) ?
                    MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_FOOD :
                    MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE;
                continue;
            }
            for (prior = 0U; prior < index; ++prior)
            {
                if (pending[prior].reserved_food &&
                    pending[prior].action.eat_target.value == entry->action.eat_target.value)
                {
                    claimed = 1;
                    break;
                }
            }
            if (claimed)
            {
                action_result->status = MINISNN_WORLDS_DOMAIN_ACTION_REJECTED;
                action_result->reason = MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE;
                continue;
            }
            if (minisnn_worlds_kernel_entity_transform(domain->kernel, entry->action.actor,
                                                        &actor_transform) !=
                    MINISNN_WORLDS_KERNEL_ERROR_NONE ||
                minisnn_worlds_kernel_entity_transform(domain->kernel, entry->action.eat_target,
                                                        &food_transform) !=
                    MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE;
                goto finish;
            }
            distance = manhattan_distance(&actor_transform, &food_transform);
            if (distance > (uint64_t)species->config.eat_range)
            {
                action_result->status = MINISNN_WORLDS_DOMAIN_ACTION_REJECTED;
                action_result->reason = MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_OUT_OF_RANGE;
                continue;
            }
            (void)food_slot;
            entry->kind = 3;
            entry->reserved_food = 1;
        }
    }

    if (!preflight_step_headroom(domain, pending, pending_count, event_reserve))
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
        goto finish;
    }
    if (!reserve_events(domain, event_reserve))
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION;
        goto finish;
    }

    target_tick = domain->tick + UINT64_C(1);
    if (minisnn_worlds_kernel_command_batch_begin(domain->kernel) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
        goto finish;
    }
    batch_open = 1;

    for (index = 0U; index < pending_count; ++index)
    {
        DomainPendingAction *entry = &pending[index];
        MiniSNNWorldsKernelError kernel_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
        if (entry->kind == 2)
        {
            kernel_error = minisnn_worlds_kernel_queue_move_entity(
                domain->kernel, target_tick, MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT,
                entry->action.actor, entry->action.actor, entry->action.move_delta.x,
                entry->action.move_delta.y, &entry->command_id);
        }
        else if (entry->kind == 3)
        {
            kernel_error = minisnn_worlds_kernel_queue_destroy_entity(
                domain->kernel, target_tick, MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT,
                entry->action.actor, entry->action.eat_target, &entry->command_id);
        }
        if (kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
            goto finish;
        }
    }

    if (minisnn_worlds_kernel_step(domain->kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
        goto finish;
    }
    if (minisnn_worlds_kernel_command_batch_commit(domain->kernel) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        batch_open = 0;
        result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
        goto finish;
    }
    batch_open = 0;
    domain->tick = minisnn_worlds_kernel_tick(domain->kernel);

    /* Post-commit is allocation-free. Preflight reserved all capacity and headroom. */
    for (index = 0U; index < pending_count; ++index)
    {
        DomainPendingAction *entry = &pending[index];
        MiniSNNWorldsDomainActionResult *action_result = &out_results[entry->input_index];
        int actor_found;
        size_t actor_index = organism_index(domain, entry->action.actor, &actor_found);

        if (entry->kind == 1)
        {
            set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_APPLIED,
                       MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE,
                       action_result->energy_before, action_result->energy_before);
        }
        else if (entry->kind == 2 || entry->kind == 3)
        {
            MiniSNNWorldsKernelEvent event;
            if (!event_for_command(domain, entry->command_id, &event))
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
                goto finish;
            }
            if (event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED)
            {
                set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_REJECTED,
                           MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED,
                           action_result->energy_before, action_result->energy_before);
            }
            else if (entry->kind == 2 &&
                     event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED && actor_found)
            {
                const DomainSpecies *species = find_species(
                    domain, domain->organisms[actor_index].species_id);
                MiniSNNWorldsDomainEnergy before = domain->organisms[actor_index].energy;
                MiniSNNWorldsDomainEnergy after = before - species->config.move_energy_cost;
                domain->organisms[actor_index].energy = after;
                set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_APPLIED,
                           MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after);
                account_energy(domain, before, after);
                if (!append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED,
                                  entry->action.actor, no_entity(), entry->action.type,
                                  MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after))
                {
                    result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
                    goto finish;
                }
            }
            else if (entry->kind == 3 &&
                     event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED && actor_found)
            {
                int food_found;
                size_t food_slot = food_index(domain, entry->action.eat_target, &food_found);
                const DomainSpecies *species = find_species(
                    domain, domain->organisms[actor_index].species_id);
                MiniSNNWorldsDomainEnergy before = domain->organisms[actor_index].energy;
                MiniSNNWorldsDomainEnergy nutrition;
                MiniSNNWorldsDomainEnergy after;

                if (!food_found || species == NULL)
                {
                    result = MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
                    goto finish;
                }
                nutrition = domain->foods[food_slot].nutrition;
                after = nutrition > species->config.max_energy - before ?
                        species->config.max_energy : before + nutrition;
                memmove(&domain->foods[food_slot], &domain->foods[food_slot + 1U],
                        (domain->food_count - food_slot - 1U) * sizeof(*domain->foods));
                --domain->food_count;
                domain->organisms[actor_index].energy = after;
                set_result(action_result, MINISNN_WORLDS_DOMAIN_ACTION_APPLIED,
                           MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after);
                ++domain->diagnostics.total_food_consumed;
                account_energy(domain, before, after);
                if (!append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_FOOD_CONSUMED,
                                  entry->action.actor, entry->action.eat_target, entry->action.type,
                                  MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after) ||
                    !append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED,
                                  entry->action.actor, entry->action.eat_target, entry->action.type,
                                  MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after))
                {
                    result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
                    goto finish;
                }
            }
            else
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
                goto finish;
            }
        }
        if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
        {
            ++domain->diagnostics.total_actions;
            ++domain->diagnostics.total_actions_applied;
            if (!append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED,
                              entry->action.actor, entry->action.eat_target, entry->action.type,
                              MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE,
                              action_result->energy_before, action_result->energy_after))
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
                goto finish;
            }
        }
        else
        {
            ++domain->diagnostics.total_actions;
            ++domain->diagnostics.total_actions_rejected;
            if (!append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_ACTION_REJECTED,
                              entry->action.actor, entry->action.eat_target, entry->action.type,
                              action_result->reason, action_result->energy_before,
                              action_result->energy_after))
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
                goto finish;
            }
        }
    }
    for (index = 0U; index < domain->organism_count; ++index)
    {
        DomainOrganism *organism = &domain->organisms[index];
        const DomainSpecies *species = find_species(domain, organism->species_id);
        MiniSNNWorldsDomainEnergy before = organism->energy;
        MiniSNNWorldsDomainEnergy after = before > species->config.metabolism_per_tick ?
                before - species->config.metabolism_per_tick : 0U;
        if (before != after)
        {
            organism->energy = after;
            account_energy(domain, before, after);
            if (!append_event(domain, MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED,
                              organism->entity_id, no_entity(), MINISNN_WORLDS_DOMAIN_ACTION_WAIT,
                              MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE, before, after))
            {
                result = MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
                goto finish;
            }
        }
    }
    if (!validate_invariants(domain))
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
        goto finish;
    }

finish:
    if (batch_open)
    {
        MiniSNNWorldsKernelError rollback_error =
            minisnn_worlds_kernel_command_batch_rollback(domain->kernel);
        if (rollback_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
        }
    }
    free(pending);
    set_error(domain, result);
    return result;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_state_hash(
    const MiniSNNWorldsDomain *domain, uint64_t *out_hash)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    if (domain == NULL || out_hash == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (!validate_invariants(domain))
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
    }
    hash_u64(&hash, MINISNN_WORLDS_DOMAIN_STATE_HASH_VERSION_V1);
    hash_u64(&hash, domain->tick);
    hash_u64(&hash, domain->species_count);
    for (index = 0U; index < domain->species_count; ++index)
    {
        const MiniSNNWorldsDomainSpeciesConfig *config = &domain->species[index].config;
        hash_u64(&hash, config->species_id);
        hash_u64(&hash, config->max_energy);
        hash_u64(&hash, config->metabolism_per_tick);
        hash_u64(&hash, config->move_energy_cost);
        hash_u64(&hash, (uint64_t)config->eat_range);
    }
    hash_u64(&hash, domain->organism_count);
    for (index = 0U; index < domain->organism_count; ++index)
    {
        hash_u64(&hash, domain->organisms[index].entity_id.value);
        hash_u64(&hash, domain->organisms[index].species_id);
        hash_u64(&hash, domain->organisms[index].energy);
    }
    hash_u64(&hash, domain->food_count);
    for (index = 0U; index < domain->food_count; ++index)
    {
        hash_u64(&hash, domain->foods[index].entity_id.value);
        hash_u64(&hash, domain->foods[index].nutrition);
    }
    hash_u64(&hash, domain->next_event_id);
    hash_u64(&hash, domain->diagnostics.total_actions);
    hash_u64(&hash, domain->diagnostics.total_actions_applied);
    hash_u64(&hash, domain->diagnostics.total_actions_rejected);
    hash_u64(&hash, domain->diagnostics.total_food_consumed);
    hash_u64(&hash, domain->diagnostics.total_energy_gained);
    hash_u64(&hash, domain->diagnostics.total_energy_spent);
    hash_u64(&hash, domain->event_count);
    for (index = 0U; index < domain->event_count; ++index)
    {
        const MiniSNNWorldsDomainEvent *event = &domain->events[index];
        hash_u64(&hash, event->event_id);
        hash_u64(&hash, event->tick);
        hash_u64(&hash, event->type);
        hash_u64(&hash, event->subject.value);
        hash_u64(&hash, event->related.value);
        hash_u64(&hash, event->action_type);
        hash_u64(&hash, event->reason);
        hash_u64(&hash, event->energy_before);
        hash_u64(&hash, event->energy_after);
    }
    *out_hash = hash;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

#ifdef MINISNN_WORLDS_DOMAIN_TESTING
MiniSNNWorldsDomainError minisnn_worlds_domain_testing_validate_invariants(
    const MiniSNNWorldsDomain *domain)
{
    return validate_invariants(domain) ? MINISNN_WORLDS_DOMAIN_ERROR_NONE :
           MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_testing_inject_corruption(
    MiniSNNWorldsDomain *domain, MiniSNNWorldsDomainTestingCorruption corruption)
{
    if (domain == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    switch (corruption)
    {
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_ENTITY:
            if (domain->organism_count == 0U || domain->food_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->foods[0U].entity_id = domain->organisms[0U].entity_id;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_UNKNOWN_SPECIES:
            if (domain->organism_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->organisms[0U].species_id = UINT64_MAX;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_ENERGY_OVER_MAX:
            if (domain->organism_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->organisms[0U].energy = UINT64_MAX;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_NUTRITION:
            if (domain->food_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->foods[0U].nutrition = 0U;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_MISSING_KERNEL_ENTITY:
            if (domain->food_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->foods[0U].entity_id.value = UINT64_MAX;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_TICK_DIVERGENCE:
            ++domain->tick;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_EVENT_ID:
            if (domain->event_count < 2U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->events[1U].event_id = domain->events[0U].event_id;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_COUNTER_MISMATCH:
            ++domain->diagnostics.total_actions;
            break;
        case MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_KIND:
            if (domain->event_count == 0U) return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
            domain->events[0U].type = (MiniSNNWorldsDomainEventType)99;
            break;
        default:
            return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT;
    }
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}
#endif

#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC "MSWDOMS1"
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC_SIZE 8U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE 148U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_DIGEST_OFFSET 44U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE 40U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE 24U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_FOOD_SIZE 16U
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_EVENT_SIZE 60U

struct MiniSNNWorldsDomainSnapshot
{
    uint8_t *data;
    size_t size;
    uint64_t digest;
};

typedef struct
{
    MiniSNNWorldsTick kernel_tick;
    uint64_t kernel_hash;
    uint64_t digest;
    MiniSNNWorldsTick domain_tick;
    uint64_t next_event_id;
    size_t species_count;
    size_t organism_count;
    size_t food_count;
    size_t event_count;
    MiniSNNWorldsDomainDiagnostics diagnostics;
    size_t species_offset;
    size_t organism_offset;
    size_t food_offset;
    size_t event_offset;
} DomainSnapshotLayout;

static int domain_snapshot_size_add(size_t left, size_t right, size_t *out_result)
{
    if (out_result == NULL || left > SIZE_MAX - right)
    {
        return 0;
    }
    *out_result = left + right;
    return 1;
}

static int domain_snapshot_size_multiply(size_t left, size_t right, size_t *out_result)
{
    if (out_result == NULL || (left != 0U && right > SIZE_MAX / left))
    {
        return 0;
    }
    *out_result = left * right;
    return 1;
}

static void domain_snapshot_write_u32(uint8_t *data, size_t *offset, uint32_t value)
{
    size_t index;
    for (index = 0U; index < 4U; ++index)
    {
        data[*offset + index] = (uint8_t)(value >> (index * 8U));
    }
    *offset += 4U;
}

static void domain_snapshot_write_u64(uint8_t *data, size_t *offset, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index)
    {
        data[*offset + index] = (uint8_t)(value >> (index * 8U));
    }
    *offset += 8U;
}

static uint32_t domain_snapshot_read_u32(const uint8_t *data, size_t offset)
{
    uint32_t value = 0U;
    size_t index;
    for (index = 0U; index < 4U; ++index)
    {
        value |= (uint32_t)data[offset + index] << (index * 8U);
    }
    return value;
}

static uint64_t domain_snapshot_read_u64(const uint8_t *data, size_t offset)
{
    uint64_t value = 0U;
    size_t index;
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)data[offset + index] << (index * 8U);
    }
    return value;
}

static void domain_snapshot_store_u64(uint8_t *data, size_t offset, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index)
    {
        data[offset + index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t domain_snapshot_digest_bytes(const uint8_t *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < size; ++index)
    {
        uint8_t value = (index >= MINISNN_WORLDS_DOMAIN_SNAPSHOT_DIGEST_OFFSET &&
                         index < MINISNN_WORLDS_DOMAIN_SNAPSHOT_DIGEST_OFFSET + 8U) ? 0U : data[index];
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int domain_snapshot_count_from_u64(uint64_t value, size_t *out_count)
{
    if (out_count == NULL || value > (uint64_t)SIZE_MAX)
    {
        return 0;
    }
    *out_count = (size_t)value;
    return 1;
}

static int domain_snapshot_compute_size(const DomainSnapshotLayout *layout, size_t *out_size)
{
    size_t size = MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE;
    size_t part;

    if (layout == NULL || out_size == NULL ||
        !domain_snapshot_size_multiply(layout->species_count,
                                       MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE, &part) ||
        !domain_snapshot_size_add(size, part, &size) ||
        !domain_snapshot_size_multiply(layout->organism_count,
                                       MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE, &part) ||
        !domain_snapshot_size_add(size, part, &size) ||
        !domain_snapshot_size_multiply(layout->food_count,
                                       MINISNN_WORLDS_DOMAIN_SNAPSHOT_FOOD_SIZE, &part) ||
        !domain_snapshot_size_add(size, part, &size) ||
        !domain_snapshot_size_multiply(layout->event_count,
                                       MINISNN_WORLDS_DOMAIN_SNAPSHOT_EVENT_SIZE, &part) ||
        !domain_snapshot_size_add(size, part, &size))
    {
        return 0;
    }
    *out_size = size;
    return 1;
}

static int domain_snapshot_parse_layout(const uint8_t *data, size_t size,
                                        DomainSnapshotLayout *out_layout)
{
    DomainSnapshotLayout layout;
    size_t expected_size;
    size_t offset = MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC_SIZE;

    if (data == NULL || out_layout == NULL || size < MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE ||
        memcmp(data, MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC,
               MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC_SIZE) != 0 ||
        domain_snapshot_read_u32(data, offset) != MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V1)
    {
        return 0;
    }
    offset += 4U;
    if (domain_snapshot_read_u32(data, offset) != MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE)
    {
        return 0;
    }
    offset += 4U;
    if (domain_snapshot_read_u64(data, offset) != (uint64_t)size)
    {
        return 0;
    }
    offset += 8U;
    if (domain_snapshot_read_u32(data, offset) != MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION)
    {
        return 0;
    }
    offset += 4U;
    memset(&layout, 0, sizeof(layout));
    layout.kernel_tick = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.kernel_hash = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.digest = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.domain_tick = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.next_event_id = domain_snapshot_read_u64(data, offset); offset += 8U;
    if (!domain_snapshot_count_from_u64(domain_snapshot_read_u64(data, offset), &layout.species_count)) return 0;
    offset += 8U;
    if (!domain_snapshot_count_from_u64(domain_snapshot_read_u64(data, offset), &layout.organism_count)) return 0;
    offset += 8U;
    if (!domain_snapshot_count_from_u64(domain_snapshot_read_u64(data, offset), &layout.food_count)) return 0;
    offset += 8U;
    if (!domain_snapshot_count_from_u64(domain_snapshot_read_u64(data, offset), &layout.event_count)) return 0;
    offset += 8U;
    layout.diagnostics.total_actions = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.diagnostics.total_actions_applied = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.diagnostics.total_actions_rejected = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.diagnostics.total_food_consumed = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.diagnostics.total_energy_gained = domain_snapshot_read_u64(data, offset); offset += 8U;
    layout.diagnostics.total_energy_spent = domain_snapshot_read_u64(data, offset); offset += 8U;
    if (offset != MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE ||
        layout.kernel_tick != layout.domain_tick ||
        !domain_snapshot_compute_size(&layout, &expected_size) || expected_size != size ||
        layout.digest != domain_snapshot_digest_bytes(data, size))
    {
        return 0;
    }
    layout.species_offset = MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE;
    layout.organism_offset = layout.species_offset +
                             layout.species_count * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE;
    layout.food_offset = layout.organism_offset +
                         layout.organism_count * MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE;
    layout.event_offset = layout.food_offset +
                          layout.food_count * MINISNN_WORLDS_DOMAIN_SNAPSHOT_FOOD_SIZE;
    *out_layout = layout;
    return 1;
}

static int domain_snapshot_species_max(const uint8_t *data,
                                       const DomainSnapshotLayout *layout,
                                       MiniSNNWorldsDomainSpeciesId id,
                                       MiniSNNWorldsDomainEnergy *out_max)
{
    size_t low = 0U;
    size_t high = layout->species_count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        uint64_t current = domain_snapshot_read_u64(
            data, layout->species_offset + middle * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE);
        if (current < id)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    if (low == layout->species_count ||
        domain_snapshot_read_u64(data, layout->species_offset +
                                 low * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE) != id)
    {
        return 0;
    }
    if (out_max != NULL)
    {
        *out_max = domain_snapshot_read_u64(
            data, layout->species_offset + low * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE + 8U);
    }
    return 1;
}

static int domain_snapshot_organism_contains(const uint8_t *data,
                                             const DomainSnapshotLayout *layout,
                                             MiniSNNWorldsKernelEntityId entity_id)
{
    size_t low = 0U;
    size_t high = layout->organism_count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        uint64_t current = domain_snapshot_read_u64(
            data, layout->organism_offset + middle * MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE);
        if (current < entity_id.value)
        {
            low = middle + 1U;
        }
        else
        {
            high = middle;
        }
    }
    return low < layout->organism_count && domain_snapshot_read_u64(
               data, layout->organism_offset + low * MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE) ==
               entity_id.value;
}

static int domain_snapshot_validate_payload(const uint8_t *data,
                                            const DomainSnapshotLayout *layout)
{
    size_t index;
    uint64_t prior = 0U;
    uint64_t actions = 0U;
    uint64_t applied = 0U;
    uint64_t rejected = 0U;
    uint64_t foods = 0U;
    uint64_t gained = 0U;
    uint64_t spent = 0U;

    for (index = 0U; index < layout->species_count; ++index)
    {
        size_t offset = layout->species_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE;
        uint64_t id = domain_snapshot_read_u64(data, offset);
        uint64_t max_energy = domain_snapshot_read_u64(data, offset + 8U);
        uint64_t eat_range = domain_snapshot_read_u64(data, offset + 32U);
        if (id == 0U || max_energy == 0U || eat_range > (uint64_t)INT64_MAX ||
            (index > 0U && id <= prior))
        {
            return 0;
        }
        prior = id;
    }
    prior = 0U;
    for (index = 0U; index < layout->organism_count; ++index)
    {
        size_t offset = layout->organism_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE;
        MiniSNNWorldsDomainEnergy max_energy;
        uint64_t id = domain_snapshot_read_u64(data, offset);
        uint64_t species_id = domain_snapshot_read_u64(data, offset + 8U);
        uint64_t energy = domain_snapshot_read_u64(data, offset + 16U);
        if (id == 0U || (index > 0U && id <= prior) ||
            !domain_snapshot_species_max(data, layout, species_id, &max_energy) || energy > max_energy)
        {
            return 0;
        }
        prior = id;
    }
    prior = 0U;
    for (index = 0U; index < layout->food_count; ++index)
    {
        size_t offset = layout->food_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_FOOD_SIZE;
        MiniSNNWorldsKernelEntityId id = { domain_snapshot_read_u64(data, offset) };
        uint64_t nutrition = domain_snapshot_read_u64(data, offset + 8U);
        if (id.value == 0U || nutrition == 0U || (index > 0U && id.value <= prior) ||
            domain_snapshot_organism_contains(data, layout, id))
        {
            return 0;
        }
        prior = id.value;
    }
    prior = 0U;
    for (index = 0U; index < layout->event_count; ++index)
    {
        size_t offset = layout->event_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_EVENT_SIZE;
        uint64_t event_id = domain_snapshot_read_u64(data, offset);
        uint64_t tick = domain_snapshot_read_u64(data, offset + 8U);
        MiniSNNWorldsDomainEventType type =
            (MiniSNNWorldsDomainEventType)domain_snapshot_read_u32(data, offset + 16U);
        MiniSNNWorldsDomainActionType action =
            (MiniSNNWorldsDomainActionType)domain_snapshot_read_u32(data, offset + 36U);
        MiniSNNWorldsDomainActionReason reason =
            (MiniSNNWorldsDomainActionReason)domain_snapshot_read_u32(data, offset + 40U);
        uint64_t before = domain_snapshot_read_u64(data, offset + 44U);
        uint64_t after = domain_snapshot_read_u64(data, offset + 52U);
        if (event_id == 0U || event_id <= prior || tick > layout->domain_tick ||
            !valid_event_type(type) || !valid_action_type(action) || !valid_reason(reason))
        {
            return 0;
        }
        prior = event_id;
        if (type == MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED)
        {
            ++actions;
            ++applied;
        }
        else if (type == MINISNN_WORLDS_DOMAIN_EVENT_ACTION_REJECTED)
        {
            ++actions;
            ++rejected;
        }
        else if (type == MINISNN_WORLDS_DOMAIN_EVENT_FOOD_CONSUMED)
        {
            ++foods;
        }
        else if (after > before)
        {
            gained = add_saturating_u64(gained, after - before);
        }
        else
        {
            spent = add_saturating_u64(spent, before - after);
        }
    }
    return prior != UINT64_MAX &&
           layout->next_event_id == prior + UINT64_C(1) &&
           layout->diagnostics.total_actions == actions &&
           layout->diagnostics.total_actions_applied == applied &&
           layout->diagnostics.total_actions_rejected == rejected &&
           layout->diagnostics.total_food_consumed == foods &&
           layout->diagnostics.total_energy_gained == gained &&
           layout->diagnostics.total_energy_spent == spent;
}

static int domain_snapshot_validate_bytes(const uint8_t *data, size_t size,
                                          DomainSnapshotLayout *out_layout)
{
    DomainSnapshotLayout layout;
    if (!domain_snapshot_parse_layout(data, size, &layout) ||
        !domain_snapshot_validate_payload(data, &layout))
    {
        return 0;
    }
    if (out_layout != NULL)
    {
        *out_layout = layout;
    }
    return 1;
}

static int domain_snapshot_allocate_state(MiniSNNWorldsDomain *candidate,
                                          const DomainSnapshotLayout *layout)
{
    if ((layout->species_count != 0U &&
         (layout->species_count > SIZE_MAX / sizeof(*candidate->species) ||
          (candidate->species = malloc(layout->species_count * sizeof(*candidate->species))) == NULL)) ||
        (layout->organism_count != 0U &&
         (layout->organism_count > SIZE_MAX / sizeof(*candidate->organisms) ||
          (candidate->organisms = malloc(layout->organism_count * sizeof(*candidate->organisms))) == NULL)) ||
        (layout->food_count != 0U &&
         (layout->food_count > SIZE_MAX / sizeof(*candidate->foods) ||
          (candidate->foods = malloc(layout->food_count * sizeof(*candidate->foods))) == NULL)) ||
        (layout->event_count != 0U &&
         (layout->event_count > SIZE_MAX / sizeof(*candidate->events) ||
          (candidate->events = malloc(layout->event_count * sizeof(*candidate->events))) == NULL)))
    {
        free(candidate->species);
        free(candidate->organisms);
        free(candidate->foods);
        free(candidate->events);
        candidate->species = NULL;
        candidate->organisms = NULL;
        candidate->foods = NULL;
        candidate->events = NULL;
        return 0;
    }
    candidate->species_count = layout->species_count;
    candidate->species_capacity = layout->species_count;
    candidate->organism_count = layout->organism_count;
    candidate->organism_capacity = layout->organism_count;
    candidate->food_count = layout->food_count;
    candidate->food_capacity = layout->food_count;
    candidate->event_count = layout->event_count;
    candidate->event_capacity = layout->event_count;
    return 1;
}

static void domain_snapshot_free_state(MiniSNNWorldsDomain *domain)
{
    free(domain->species);
    free(domain->organisms);
    free(domain->foods);
    free(domain->events);
}

static void domain_snapshot_copy_payload(MiniSNNWorldsDomain *candidate,
                                         const uint8_t *data,
                                         const DomainSnapshotLayout *layout)
{
    size_t index;
    candidate->tick = layout->domain_tick;
    candidate->next_event_id = layout->next_event_id;
    candidate->diagnostics = layout->diagnostics;
    for (index = 0U; index < layout->species_count; ++index)
    {
        size_t offset = layout->species_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_SPECIES_SIZE;
        candidate->species[index].config.species_id = domain_snapshot_read_u64(data, offset);
        candidate->species[index].config.max_energy = domain_snapshot_read_u64(data, offset + 8U);
        candidate->species[index].config.metabolism_per_tick = domain_snapshot_read_u64(data, offset + 16U);
        candidate->species[index].config.move_energy_cost = domain_snapshot_read_u64(data, offset + 24U);
        candidate->species[index].config.eat_range =
            (MiniSNNWorldsKernelScalar)domain_snapshot_read_u64(data, offset + 32U);
    }
    for (index = 0U; index < layout->organism_count; ++index)
    {
        size_t offset = layout->organism_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_ORGANISM_SIZE;
        candidate->organisms[index].entity_id.value = domain_snapshot_read_u64(data, offset);
        candidate->organisms[index].species_id = domain_snapshot_read_u64(data, offset + 8U);
        candidate->organisms[index].energy = domain_snapshot_read_u64(data, offset + 16U);
    }
    for (index = 0U; index < layout->food_count; ++index)
    {
        size_t offset = layout->food_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_FOOD_SIZE;
        candidate->foods[index].entity_id.value = domain_snapshot_read_u64(data, offset);
        candidate->foods[index].nutrition = domain_snapshot_read_u64(data, offset + 8U);
    }
    for (index = 0U; index < layout->event_count; ++index)
    {
        size_t offset = layout->event_offset + index * MINISNN_WORLDS_DOMAIN_SNAPSHOT_EVENT_SIZE;
        MiniSNNWorldsDomainEvent *event = &candidate->events[index];
        event->event_id = domain_snapshot_read_u64(data, offset);
        event->tick = domain_snapshot_read_u64(data, offset + 8U);
        event->type = (MiniSNNWorldsDomainEventType)domain_snapshot_read_u32(data, offset + 16U);
        event->subject.value = domain_snapshot_read_u64(data, offset + 20U);
        event->related.value = domain_snapshot_read_u64(data, offset + 28U);
        event->action_type = (MiniSNNWorldsDomainActionType)domain_snapshot_read_u32(data, offset + 36U);
        event->reason = (MiniSNNWorldsDomainActionReason)domain_snapshot_read_u32(data, offset + 40U);
        event->energy_before = domain_snapshot_read_u64(data, offset + 44U);
        event->energy_after = domain_snapshot_read_u64(data, offset + 52U);
    }
}

MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_capture(
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsDomainSnapshot **out_snapshot)
{
    DomainSnapshotLayout layout;
    MiniSNNWorldsDomainSnapshot *snapshot;
    uint64_t kernel_hash;
    size_t size;
    size_t offset = 0U;
    size_t index;

    if (out_snapshot == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    *out_snapshot = NULL;
    if (domain == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (minisnn_worlds_kernel_command_batch_active(domain->kernel))
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
    }
    if (!validate_invariants(domain))
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
    }
    if (minisnn_worlds_kernel_state_hash(domain->kernel, &kernel_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE;
    }
    memset(&layout, 0, sizeof(layout));
    layout.kernel_tick = minisnn_worlds_kernel_tick(domain->kernel);
    layout.kernel_hash = kernel_hash;
    layout.domain_tick = domain->tick;
    layout.next_event_id = domain->next_event_id;
    layout.species_count = domain->species_count;
    layout.organism_count = domain->organism_count;
    layout.food_count = domain->food_count;
    layout.event_count = domain->event_count;
    layout.diagnostics = domain->diagnostics;
    if (!domain_snapshot_compute_size(&layout, &size))
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_SIZE_OVERFLOW;
    }
    snapshot = malloc(sizeof(*snapshot));
    if (snapshot == NULL || (snapshot->data = malloc(size)) == NULL)
    {
        free(snapshot);
        return MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION;
    }
    snapshot->size = size;
    memcpy(snapshot->data + offset, MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC,
           MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC_SIZE);
    offset += MINISNN_WORLDS_DOMAIN_SNAPSHOT_MAGIC_SIZE;
    domain_snapshot_write_u32(snapshot->data, &offset, MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V1);
    domain_snapshot_write_u32(snapshot->data, &offset, MINISNN_WORLDS_DOMAIN_SNAPSHOT_HEADER_SIZE);
    domain_snapshot_write_u64(snapshot->data, &offset, (uint64_t)size);
    domain_snapshot_write_u32(snapshot->data, &offset, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.kernel_tick);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.kernel_hash);
    domain_snapshot_write_u64(snapshot->data, &offset, 0U);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.domain_tick);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.next_event_id);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.species_count);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.organism_count);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.food_count);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.event_count);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_actions);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_actions_applied);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_actions_rejected);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_food_consumed);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_energy_gained);
    domain_snapshot_write_u64(snapshot->data, &offset, layout.diagnostics.total_energy_spent);
    for (index = 0U; index < domain->species_count; ++index)
    {
        const MiniSNNWorldsDomainSpeciesConfig *config = &domain->species[index].config;
        domain_snapshot_write_u64(snapshot->data, &offset, config->species_id);
        domain_snapshot_write_u64(snapshot->data, &offset, config->max_energy);
        domain_snapshot_write_u64(snapshot->data, &offset, config->metabolism_per_tick);
        domain_snapshot_write_u64(snapshot->data, &offset, config->move_energy_cost);
        domain_snapshot_write_u64(snapshot->data, &offset, (uint64_t)config->eat_range);
    }
    for (index = 0U; index < domain->organism_count; ++index)
    {
        domain_snapshot_write_u64(snapshot->data, &offset, domain->organisms[index].entity_id.value);
        domain_snapshot_write_u64(snapshot->data, &offset, domain->organisms[index].species_id);
        domain_snapshot_write_u64(snapshot->data, &offset, domain->organisms[index].energy);
    }
    for (index = 0U; index < domain->food_count; ++index)
    {
        domain_snapshot_write_u64(snapshot->data, &offset, domain->foods[index].entity_id.value);
        domain_snapshot_write_u64(snapshot->data, &offset, domain->foods[index].nutrition);
    }
    for (index = 0U; index < domain->event_count; ++index)
    {
        const MiniSNNWorldsDomainEvent *event = &domain->events[index];
        domain_snapshot_write_u64(snapshot->data, &offset, event->event_id);
        domain_snapshot_write_u64(snapshot->data, &offset, event->tick);
        domain_snapshot_write_u32(snapshot->data, &offset, (uint32_t)event->type);
        domain_snapshot_write_u64(snapshot->data, &offset, event->subject.value);
        domain_snapshot_write_u64(snapshot->data, &offset, event->related.value);
        domain_snapshot_write_u32(snapshot->data, &offset, (uint32_t)event->action_type);
        domain_snapshot_write_u32(snapshot->data, &offset, (uint32_t)event->reason);
        domain_snapshot_write_u64(snapshot->data, &offset, event->energy_before);
        domain_snapshot_write_u64(snapshot->data, &offset, event->energy_after);
    }
    if (offset != size)
    {
        minisnn_worlds_domain_snapshot_destroy(snapshot);
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
    }
    snapshot->digest = domain_snapshot_digest_bytes(snapshot->data, size);
    domain_snapshot_store_u64(snapshot->data, MINISNN_WORLDS_DOMAIN_SNAPSHOT_DIGEST_OFFSET,
                              snapshot->digest);
    *out_snapshot = snapshot;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

void minisnn_worlds_domain_snapshot_destroy(MiniSNNWorldsDomainSnapshot *snapshot)
{
    if (snapshot != NULL)
    {
        free(snapshot->data);
        free(snapshot);
    }
}

uint32_t minisnn_worlds_domain_snapshot_format_version(
    const MiniSNNWorldsDomainSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V1;
}

size_t minisnn_worlds_domain_snapshot_size(const MiniSNNWorldsDomainSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : snapshot->size;
}

const uint8_t *minisnn_worlds_domain_snapshot_data(const MiniSNNWorldsDomainSnapshot *snapshot)
{
    return snapshot == NULL ? NULL : snapshot->data;
}

uint64_t minisnn_worlds_domain_snapshot_digest(const MiniSNNWorldsDomainSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : snapshot->digest;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_from_bytes(
    const uint8_t *data, size_t size, MiniSNNWorldsDomainSnapshot **out_snapshot)
{
    DomainSnapshotLayout layout;
    MiniSNNWorldsDomainSnapshot *snapshot;
    if (out_snapshot == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    *out_snapshot = NULL;
    if (data == NULL)
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (!domain_snapshot_validate_bytes(data, size, &layout))
    {
        return MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    snapshot = malloc(sizeof(*snapshot));
    if (snapshot == NULL || (snapshot->data = malloc(size)) == NULL)
    {
        free(snapshot);
        return MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION;
    }
    memcpy(snapshot->data, data, size);
    snapshot->size = size;
    snapshot->digest = layout.digest;
    *out_snapshot = snapshot;
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_restore(
    MiniSNNWorldsDomain *domain, const MiniSNNWorldsDomainSnapshot *snapshot)
{
    DomainSnapshotLayout layout;
    MiniSNNWorldsDomain candidate;
    MiniSNNWorldsDomain old;
    uint64_t current_hash;
    size_t index;
    MiniSNNWorldsDomainError result = MINISNN_WORLDS_DOMAIN_ERROR_NONE;

    if (domain == NULL || snapshot == NULL)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT);
        return MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT;
    }
    if (domain->kernel == NULL)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE);
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
    }
    if (minisnn_worlds_kernel_command_batch_active(domain->kernel))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE);
        return MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE;
    }
    if (!domain_snapshot_validate_bytes(snapshot->data, snapshot->size, &layout))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT);
        return MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    if (minisnn_worlds_kernel_tick(domain->kernel) != layout.kernel_tick ||
        minisnn_worlds_kernel_state_hash(domain->kernel, &current_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        current_hash != layout.kernel_hash)
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INCOMPATIBLE_KERNEL);
        return MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INCOMPATIBLE_KERNEL;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.kernel = domain->kernel;
    candidate.last_error = MINISNN_WORLDS_DOMAIN_ERROR_NONE;
    if (!domain_snapshot_allocate_state(&candidate, &layout))
    {
        set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION);
        return MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION;
    }
    domain_snapshot_copy_payload(&candidate, snapshot->data, &layout);
    for (index = 0U; index < candidate.organism_count; ++index)
    {
        if (!can_reference_entity(&candidate, candidate.organisms[index].entity_id))
        {
            result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE;
            break;
        }
    }
    for (index = 0U; result == MINISNN_WORLDS_DOMAIN_ERROR_NONE && index < candidate.food_count; ++index)
    {
        if (!can_reference_entity(&candidate, candidate.foods[index].entity_id))
        {
            result = MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE;
        }
    }
    if (result == MINISNN_WORLDS_DOMAIN_ERROR_NONE && !validate_invariants(&candidate))
    {
        result = MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION;
    }
    if (result != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        domain_snapshot_free_state(&candidate);
        set_error(domain, result);
        return result;
    }
    old = *domain;
    *domain = candidate;
    domain_snapshot_free_state(&old);
    set_error(domain, MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    return MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}