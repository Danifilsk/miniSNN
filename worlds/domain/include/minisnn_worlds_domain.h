#ifndef MINISNN_WORLDS_DOMAIN_H
#define MINISNN_WORLDS_DOMAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel.h"

#define MINISNN_WORLDS_DOMAIN_STATE_HASH_VERSION_V1 UINT32_C(1)
#define MINISNN_WORLDS_DOMAIN_STATE_HASH_VERSION_V2 UINT32_C(2)
#define MINISNN_WORLDS_DOMAIN_STATE_HASH_VERSION \
    MINISNN_WORLDS_DOMAIN_STATE_HASH_VERSION_V2
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V1 UINT32_C(1)
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V2 UINT32_C(2)
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION \
    MINISNN_WORLDS_DOMAIN_SNAPSHOT_FORMAT_VERSION_V2

typedef struct MiniSNNWorldsDomain MiniSNNWorldsDomain;
typedef struct MiniSNNWorldsDomainSnapshot MiniSNNWorldsDomainSnapshot;

typedef uint64_t MiniSNNWorldsDomainSpeciesId;
typedef uint64_t MiniSNNWorldsDomainEnergy;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_ERROR_NONE = 0,
    MINISNN_WORLDS_DOMAIN_ERROR_NULL_ARGUMENT,
    MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT,
    MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE,
    MINISNN_WORLDS_DOMAIN_ERROR_ALLOCATION,
    MINISNN_WORLDS_DOMAIN_ERROR_DUPLICATE_ENTITY,
    MINISNN_WORLDS_DOMAIN_ERROR_DUPLICATE_SPECIES,
    MINISNN_WORLDS_DOMAIN_ERROR_UNKNOWN_SPECIES,
    MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE,
    MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE,
    MINISNN_WORLDS_DOMAIN_ERROR_TEMPORAL_DIVERGENCE,
    MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION,
    MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT,
    MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INCOMPATIBLE_KERNEL,
    MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_SIZE_OVERFLOW,
    MINISNN_WORLDS_DOMAIN_ERROR_ACTOR_DEAD
} MiniSNNWorldsDomainError;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_LIFE_ALIVE = 1,
    MINISNN_WORLDS_DOMAIN_LIFE_DEAD = 2
} MiniSNNWorldsDomainLifeState;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_NONE = 0,
    MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION = 1
} MiniSNNWorldsDomainDeathCause;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_ACTION_WAIT = 1,
    MINISNN_WORLDS_DOMAIN_ACTION_MOVE = 2,
    MINISNN_WORLDS_DOMAIN_ACTION_EAT = 3
} MiniSNNWorldsDomainActionType;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_ACTION_APPLIED = 1,
    MINISNN_WORLDS_DOMAIN_ACTION_REJECTED = 2
} MiniSNNWorldsDomainActionStatus;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE = 0,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_NOT_ORGANISM,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_FOOD,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_OUT_OF_RANGE,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_INSUFFICIENT_ENERGY,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_INVALID_ACTION,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR,
    MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD
} MiniSNNWorldsDomainActionReason;

typedef enum
{
    MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED = 1,
    MINISNN_WORLDS_DOMAIN_EVENT_ACTION_REJECTED = 2,
    MINISNN_WORLDS_DOMAIN_EVENT_FOOD_CONSUMED = 3,
    MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED = 4,
    MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED = 5
} MiniSNNWorldsDomainEventType;

typedef struct
{
    MiniSNNWorldsDomainSpeciesId species_id;
    MiniSNNWorldsDomainEnergy max_energy;
    MiniSNNWorldsDomainEnergy metabolism_per_tick;
    MiniSNNWorldsDomainEnergy move_energy_cost;
    MiniSNNWorldsKernelScalar eat_range;
} MiniSNNWorldsDomainSpeciesConfig;

typedef struct
{
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainActionType type;
    MiniSNNWorldsKernelPosition move_delta;
    MiniSNNWorldsKernelEntityId eat_target;
} MiniSNNWorldsDomainAction;

typedef struct
{
    MiniSNNWorldsDomainActionStatus status;
    MiniSNNWorldsDomainActionReason reason;
    MiniSNNWorldsDomainEnergy energy_before;
    MiniSNNWorldsDomainEnergy energy_after;
} MiniSNNWorldsDomainActionResult;

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsDomainSpeciesId species_id;
    MiniSNNWorldsDomainEnergy energy;
    MiniSNNWorldsDomainEnergy max_energy;
    MiniSNNWorldsDomainEnergy hunger;
    MiniSNNWorldsDomainLifeState life_state;
    MiniSNNWorldsDomainDeathCause death_cause;
    MiniSNNWorldsTick death_tick;
} MiniSNNWorldsDomainOrganismInfo;

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsDomainEnergy nutrition;
} MiniSNNWorldsDomainFoodInfo;

typedef struct
{
    MiniSNNWorldsDomainEnergy self_energy;
    MiniSNNWorldsDomainEnergy self_hunger;
    bool nearest_food_present;
    MiniSNNWorldsKernelEntityId nearest_food_entity;
    MiniSNNWorldsKernelScalar nearest_food_delta_x;
    MiniSNNWorldsKernelScalar nearest_food_delta_y;
    uint64_t nearest_food_distance;
} MiniSNNWorldsDomainPerception;

typedef struct
{
    uint64_t event_id;
    MiniSNNWorldsTick tick;
    MiniSNNWorldsDomainEventType type;
    MiniSNNWorldsKernelEntityId subject;
    MiniSNNWorldsKernelEntityId related;
    MiniSNNWorldsDomainActionType action_type;
    MiniSNNWorldsDomainActionReason reason;
    MiniSNNWorldsDomainEnergy energy_before;
    MiniSNNWorldsDomainEnergy energy_after;
    MiniSNNWorldsDomainDeathCause death_cause;
} MiniSNNWorldsDomainEvent;

typedef struct
{
    uint64_t total_actions;
    uint64_t total_actions_applied;
    uint64_t total_actions_rejected;
    uint64_t total_food_consumed;
    uint64_t total_energy_gained;
    uint64_t total_energy_spent;
} MiniSNNWorldsDomainDiagnostics;

MiniSNNWorldsDomain *minisnn_worlds_domain_create(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsDomainError *out_error);

void minisnn_worlds_domain_destroy(MiniSNNWorldsDomain *domain);

MiniSNNWorldsDomainError minisnn_worlds_domain_last_error(
    const MiniSNNWorldsDomain *domain);

MiniSNNWorldsTick minisnn_worlds_domain_tick(const MiniSNNWorldsDomain *domain);

MiniSNNWorldsDomainError minisnn_worlds_domain_add_species(
    MiniSNNWorldsDomain *domain,
    const MiniSNNWorldsDomainSpeciesConfig *config);

MiniSNNWorldsDomainError minisnn_worlds_domain_register_organism(
    MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsDomainSpeciesId species_id,
    MiniSNNWorldsDomainEnergy initial_energy);

MiniSNNWorldsDomainError minisnn_worlds_domain_register_food(
    MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsDomainEnergy nutrition);

size_t minisnn_worlds_domain_organism_count(const MiniSNNWorldsDomain *domain);
size_t minisnn_worlds_domain_food_count(const MiniSNNWorldsDomain *domain);
size_t minisnn_worlds_domain_event_count(const MiniSNNWorldsDomain *domain);

MiniSNNWorldsDomainError minisnn_worlds_domain_organism_at(
    const MiniSNNWorldsDomain *domain,
    size_t canonical_index,
    MiniSNNWorldsDomainOrganismInfo *out_info);

MiniSNNWorldsDomainError minisnn_worlds_domain_food_at(
    const MiniSNNWorldsDomain *domain,
    size_t canonical_index,
    MiniSNNWorldsDomainFoodInfo *out_info);

MiniSNNWorldsDomainError minisnn_worlds_domain_event_at(
    const MiniSNNWorldsDomain *domain,
    size_t canonical_index,
    MiniSNNWorldsDomainEvent *out_event);

MiniSNNWorldsDomainError minisnn_worlds_domain_get_diagnostics(
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsDomainDiagnostics *out_diagnostics);

MiniSNNWorldsDomainError minisnn_worlds_domain_perceive(
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId organism,
    MiniSNNWorldsDomainPerception *out_perception);

MiniSNNWorldsDomainError minisnn_worlds_domain_step(
    MiniSNNWorldsDomain *domain,
    const MiniSNNWorldsDomainAction *actions,
    size_t action_count,
    MiniSNNWorldsDomainActionResult *out_results);

MiniSNNWorldsDomainError minisnn_worlds_domain_state_hash(
    const MiniSNNWorldsDomain *domain,
    uint64_t *out_hash);

/* Domain Snapshot V2 is canonical little-endian state bound to a Kernel hash/tick.
 * Readers also accept V1 snapshots, whose organisms restore as ALIVE. */
MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_capture(
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsDomainSnapshot **out_snapshot);

void minisnn_worlds_domain_snapshot_destroy(
    MiniSNNWorldsDomainSnapshot *snapshot);

uint32_t minisnn_worlds_domain_snapshot_format_version(
    const MiniSNNWorldsDomainSnapshot *snapshot);

size_t minisnn_worlds_domain_snapshot_size(
    const MiniSNNWorldsDomainSnapshot *snapshot);

const uint8_t *minisnn_worlds_domain_snapshot_data(
    const MiniSNNWorldsDomainSnapshot *snapshot);

uint64_t minisnn_worlds_domain_snapshot_digest(
    const MiniSNNWorldsDomainSnapshot *snapshot);

/* Copies and validates an immutable Domain Snapshot V1 or V2 byte sequence. */
MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_from_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsDomainSnapshot **out_snapshot);

/* Restores only after Kernel binding and all Domain invariants have validated. */
MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_restore(
    MiniSNNWorldsDomain *domain,
    const MiniSNNWorldsDomainSnapshot *snapshot);

#ifdef MINISNN_WORLDS_DOMAIN_TESTING
typedef enum
{
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_ENTITY = 1,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_UNKNOWN_SPECIES,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_ENERGY_OVER_MAX,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_NUTRITION,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_MISSING_KERNEL_ENTITY,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_TICK_DIVERGENCE,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_EVENT_ID,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_COUNTER_MISMATCH,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_KIND,
    MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_LIFECYCLE
} MiniSNNWorldsDomainTestingCorruption;

MiniSNNWorldsDomainError minisnn_worlds_domain_testing_validate_invariants(
    const MiniSNNWorldsDomain *domain);

MiniSNNWorldsDomainError minisnn_worlds_domain_testing_inject_corruption(
    MiniSNNWorldsDomain *domain,
    MiniSNNWorldsDomainTestingCorruption corruption);
#endif

#endif
