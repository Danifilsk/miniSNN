#include "minisnn_worlds_domain.h"
#include "wd0_test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WD1_HEADER_SIZE 148U
#define WD1_DIGEST_OFFSET 44U
#define WD1_SPECIES_SIZE 40U
#define WD1_ORGANISM_SIZE 24U
#define WD1_FOOD_SIZE 16U
#define WD1_EVENT_SIZE 60U

#define WD1_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD1 snapshot test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static void write_u32(uint8_t *data, size_t offset, uint32_t value)
{
    size_t index;
    for (index = 0U; index < 4U; ++index) data[offset + index] = (uint8_t)(value >> (index * 8U));
}

static void write_u64(uint8_t *data, size_t offset, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index) data[offset + index] = (uint8_t)(value >> (index * 8U));
}

static uint64_t digest_bytes(const uint8_t *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < size; ++index)
    {
        uint8_t value = index >= WD1_DIGEST_OFFSET && index < WD1_DIGEST_OFFSET + 8U ? 0U : data[index];
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void refresh_digest(uint8_t *data, size_t size)
{
    write_u64(data, WD1_DIGEST_OFFSET, digest_bytes(data, size));
}

static int create_fixture(MiniSNNWorldsKernel **out_kernel,
                          MiniSNNWorldsDomain **out_domain,
                          MiniSNNWorldsKernelScalar food_x,
                          MiniSNNWorldsKernelEntityId *out_organism,
                          MiniSNNWorldsKernelEntityId *out_food)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;

    if (kernel == NULL || !wd0_make_entity(kernel, 0, 0, &organism) ||
        !wd0_make_entity(kernel, food_x, 0, &food) ||
        (domain = wd0_domain_with_species(kernel)) == NULL ||
        minisnn_worlds_domain_register_organism(domain, organism, species.species_id, 70U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_food(domain, food, 20U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        minisnn_worlds_domain_destroy(domain);
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }

    memset(&action, 0, sizeof(action));
    action.actor = organism;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (minisnn_worlds_domain_step(domain, &action, 1U, &result) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        result.status != MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
    {
        minisnn_worlds_domain_destroy(domain);
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    *out_kernel = kernel;
    *out_domain = domain;
    *out_organism = organism;
    *out_food = food;
    return 1;
}

static int domain_hash(const MiniSNNWorldsDomain *domain, uint64_t *out_hash)
{
    return minisnn_worlds_domain_state_hash(domain, out_hash) == MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

static int kernel_hash(const MiniSNNWorldsKernel *kernel, uint64_t *out_hash)
{
    return minisnn_worlds_kernel_state_hash(kernel, out_hash) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int compare_snapshots(const MiniSNNWorldsDomainSnapshot *left,
                             const MiniSNNWorldsDomainSnapshot *right)
{
    return minisnn_worlds_domain_snapshot_size(left) == minisnn_worlds_domain_snapshot_size(right) &&
           minisnn_worlds_domain_snapshot_digest(left) == minisnn_worlds_domain_snapshot_digest(right) &&
           memcmp(minisnn_worlds_domain_snapshot_data(left), minisnn_worlds_domain_snapshot_data(right),
                  minisnn_worlds_domain_snapshot_size(left)) == 0;
}

static int test_round_trip_and_kernel_binding(void)
{
    MiniSNNWorldsKernel *kernel_a = NULL;
    MiniSNNWorldsKernel *kernel_b = NULL;
    MiniSNNWorldsKernel *kernel_other = NULL;
    MiniSNNWorldsDomain *domain_a = NULL;
    MiniSNNWorldsDomain *domain_b = NULL;
    MiniSNNWorldsDomain *domain_other = NULL;
    MiniSNNWorldsKernelSnapshot *kernel_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *recaptured = NULL;
    MiniSNNWorldsDomainSnapshot *before_rejected = NULL;
    MiniSNNWorldsDomainSnapshot *after_rejected = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsKernelEntityId ignored;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    uint64_t kernel_hash_a;
    uint64_t kernel_hash_b;
    uint64_t domain_hash_a;
    uint64_t domain_hash_b;
    MiniSNNWorldsTick checkpoint_tick;
    int status = 1;

    if (!create_fixture(&kernel_a, &domain_a, 1000, &organism, &food) ||
        minisnn_worlds_kernel_snapshot_capture(kernel_a, &kernel_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain_a, &snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &kernel_b) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        (domain_b = minisnn_worlds_domain_create(kernel_b, NULL)) == NULL ||
        minisnn_worlds_domain_snapshot_restore(domain_b, snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !domain_hash(domain_a, &domain_hash_a) || !domain_hash(domain_b, &domain_hash_b) ||
        domain_hash_a != domain_hash_b ||
        minisnn_worlds_domain_snapshot_capture(domain_b, &recaptured) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE || !compare_snapshots(snapshot, recaptured))
    {
        status = 0;
        goto cleanup;
    }
    checkpoint_tick = minisnn_worlds_kernel_tick(kernel_a);
    if (!create_fixture(&kernel_other, &domain_other, 2000, &ignored, &ignored) ||
        minisnn_worlds_kernel_tick(kernel_other) != checkpoint_tick ||
        minisnn_worlds_domain_snapshot_capture(domain_other, &before_rejected) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain_other, snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INCOMPATIBLE_KERNEL ||
        minisnn_worlds_domain_snapshot_capture(domain_other, &after_rejected) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE || !compare_snapshots(before_rejected, after_rejected))
    {
        status = 0;
        goto cleanup;
    }
    memset(&action, 0, sizeof(action));
    action.actor = organism;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
    action.eat_target = food;
    if (minisnn_worlds_domain_step(domain_a, &action, 1U, &result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        result.status != MINISNN_WORLDS_DOMAIN_ACTION_APPLIED ||
        minisnn_worlds_domain_step(domain_b, &action, 1U, &result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        result.status != MINISNN_WORLDS_DOMAIN_ACTION_APPLIED ||
        !kernel_hash(kernel_a, &kernel_hash_a) || !kernel_hash(kernel_b, &kernel_hash_b) ||
        !domain_hash(domain_a, &domain_hash_a) || !domain_hash(domain_b, &domain_hash_b) ||
        kernel_hash_a != kernel_hash_b || domain_hash_a != domain_hash_b)
    {
        status = 0;
        goto cleanup;
    }

cleanup:
    minisnn_worlds_domain_snapshot_destroy(after_rejected);
    minisnn_worlds_domain_snapshot_destroy(before_rejected);
    minisnn_worlds_domain_snapshot_destroy(recaptured);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_snapshot_destroy(kernel_snapshot);
    minisnn_worlds_domain_destroy(domain_other);
    minisnn_worlds_domain_destroy(domain_b);
    minisnn_worlds_domain_destroy(domain_a);
    minisnn_worlds_kernel_destroy(kernel_other);
    minisnn_worlds_kernel_destroy(kernel_b);
    minisnn_worlds_kernel_destroy(kernel_a);
    return status;
}

static int test_tick_mismatch_rejected(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    int status = 1;

    if (!create_fixture(&kernel, &domain, 1000, &organism, &food) ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain, snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INCOMPATIBLE_KERNEL)
    {
        status = 0;
    }
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return status;
}
static int test_multiple_checkpoints(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *first = NULL;
    MiniSNNWorldsDomainSnapshot *middle = NULL;
    MiniSNNWorldsDomainSnapshot *final = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    int status = 1;

    if (!create_fixture(&kernel, &domain, 1000, &organism, &food) ||
        minisnn_worlds_domain_snapshot_capture(domain, &first) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
        goto cleanup;
    }
    memset(&action, 0, sizeof(action));
    action.actor = organism;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (minisnn_worlds_domain_step(domain, &action, 1U, &result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &middle) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_step(domain, &action, 1U, &result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &final) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        compare_snapshots(first, middle) || compare_snapshots(middle, final))
    {
        status = 0;
    }
cleanup:
    minisnn_worlds_domain_snapshot_destroy(final);
    minisnn_worlds_domain_snapshot_destroy(middle);
    minisnn_worlds_domain_snapshot_destroy(first);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return status;
}

static int malformed_snapshot_rejected(const uint8_t *data, size_t size)
{
    MiniSNNWorldsDomainSnapshot *result = NULL;
    MiniSNNWorldsDomainError error = minisnn_worlds_domain_snapshot_from_bytes(data, size, &result);
    minisnn_worlds_domain_snapshot_destroy(result);
    return error == MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT;
}

static int test_event_id_overflow_rejected(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *before = NULL;
    MiniSNNWorldsDomainSnapshot *after = NULL;
    MiniSNNWorldsDomainSnapshot *limit_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *recaptured = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    uint8_t *copy = NULL;
    size_t size;
    int status = 1;

    if (!create_fixture(&kernel, &domain, 1000, &organism, &food) ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
        goto cleanup;
    }
    size = minisnn_worlds_domain_snapshot_size(snapshot);
    if (size < WD1_HEADER_SIZE + WD1_EVENT_SIZE || (copy = malloc(size)) == NULL ||
        minisnn_worlds_domain_snapshot_capture(domain, &before) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
        goto cleanup;
    }

    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, size - WD1_EVENT_SIZE, UINT64_MAX);
    write_u64(copy, 60U, 0U);
    refresh_digest(copy, size);
    if (minisnn_worlds_domain_snapshot_from_bytes(copy, size, &limit_snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT ||
        limit_snapshot != NULL ||
        minisnn_worlds_domain_snapshot_capture(domain, &after) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !compare_snapshots(before, after))
    {
        status = 0;
        goto cleanup;
    }
    minisnn_worlds_domain_snapshot_destroy(after);
    after = NULL;

    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, size - WD1_EVENT_SIZE, UINT64_MAX - UINT64_C(1));
    write_u64(copy, 60U, UINT64_MAX);
    refresh_digest(copy, size);
    if (minisnn_worlds_domain_snapshot_from_bytes(copy, size, &limit_snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain, limit_snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &recaptured) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
    }

cleanup:
    free(copy);
    minisnn_worlds_domain_snapshot_destroy(recaptured);
    minisnn_worlds_domain_snapshot_destroy(limit_snapshot);
    minisnn_worlds_domain_snapshot_destroy(after);
    minisnn_worlds_domain_snapshot_destroy(before);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return status;
}

static int test_provisional_batch_snapshot_rejected(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *after_rollback = NULL;
    MiniSNNWorldsDomainSnapshot *blocked = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsTick domain_tick;
    MiniSNNWorldsTick kernel_tick;
    uint64_t domain_hash_before;
    uint64_t domain_hash_after;
    uint64_t kernel_hash_before;
    uint64_t kernel_hash_after;
    int status = 1;

    if (!create_fixture(&kernel, &domain, 1000, &organism, &food) ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !domain_hash(domain, &domain_hash_before) || !kernel_hash(kernel, &kernel_hash_before))
    {
        status = 0;
        goto cleanup;
    }
    domain_tick = minisnn_worlds_domain_tick(domain);
    kernel_tick = minisnn_worlds_kernel_tick(kernel);
    if (minisnn_worlds_kernel_command_batch_begin(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !minisnn_worlds_kernel_command_batch_active(kernel) ||
        minisnn_worlds_domain_snapshot_capture(domain, &blocked) != MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE ||
        blocked != NULL ||
        minisnn_worlds_domain_snapshot_restore(domain, snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_INVALID_STATE ||
        minisnn_worlds_domain_tick(domain) != domain_tick || minisnn_worlds_kernel_tick(kernel) != kernel_tick ||
        !domain_hash(domain, &domain_hash_after) || !kernel_hash(kernel, &kernel_hash_after) ||
        domain_hash_before != domain_hash_after || kernel_hash_before != kernel_hash_after ||
        !minisnn_worlds_kernel_command_batch_active(kernel) ||
        minisnn_worlds_kernel_command_batch_rollback(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_command_batch_active(kernel) ||
        minisnn_worlds_domain_snapshot_capture(domain, &after_rollback) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain, snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
    }

cleanup:
    minisnn_worlds_domain_snapshot_destroy(blocked);
    minisnn_worlds_domain_snapshot_destroy(after_rollback);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return status;
}
static int test_corruption_and_atomicity(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *before = NULL;
    MiniSNNWorldsDomainSnapshot *after = NULL;
    MiniSNNWorldsDomainSnapshot *unknown_entity = NULL;
    MiniSNNWorldsDomainSnapshot *two_species = NULL;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    uint8_t *copy = NULL;
    uint8_t *resized_copy;
    size_t size;
    size_t species_offset = WD1_HEADER_SIZE;
    size_t organism_offset;
    size_t food_offset;
    size_t event_offset;
    size_t two_species_size;
    MiniSNNWorldsDomainSpeciesConfig second_species;
    int status = 1;

    if (!create_fixture(&kernel, &domain, 1000, &organism, &food) ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        status = 0;
        goto cleanup;
    }
    size = minisnn_worlds_domain_snapshot_size(snapshot);
    organism_offset = species_offset + WD1_SPECIES_SIZE;
    food_offset = organism_offset + WD1_ORGANISM_SIZE;
    event_offset = food_offset + WD1_FOOD_SIZE;
    copy = malloc(size + 1U);
    if (copy == NULL) { status = 0; goto cleanup; }

    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    copy[0] ^= 1U;
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u32(copy, 8U, 2U); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    if (!malformed_snapshot_rejected(minisnn_worlds_domain_snapshot_data(snapshot), size - 1U)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    copy[size] = 0U;
    if (!malformed_snapshot_rejected(copy, size + 1U)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    copy[WD1_DIGEST_OFFSET] ^= 1U;
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, species_offset, 0U); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, organism_offset + 8U, UINT64_MAX); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, organism_offset + 16U, UINT64_MAX); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, food_offset, organism.value); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, event_offset + WD1_EVENT_SIZE, 1U); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u32(copy, event_offset + 16U, 99U); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, 60U, 0U); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }
    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, 68U, UINT64_MAX); refresh_digest(copy, size);
    if (!malformed_snapshot_rejected(copy, size)) { status = 0; goto cleanup; }

    memcpy(copy, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u64(copy, organism_offset, UINT64_C(999999)); refresh_digest(copy, size);
    if (minisnn_worlds_domain_snapshot_from_bytes(copy, size, &unknown_entity) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &before) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain, unknown_entity) !=
            MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_ENTITY_UNAVAILABLE ||
        minisnn_worlds_domain_snapshot_capture(domain, &after) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !compare_snapshots(before, after))
    {
        status = 0;
    }    second_species = wd0_species();
    second_species.species_id = 2U;
    if (status && (minisnn_worlds_domain_add_species(domain, &second_species) !=
                       MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
                   minisnn_worlds_domain_snapshot_capture(domain, &two_species) !=
                       MINISNN_WORLDS_DOMAIN_ERROR_NONE))
    {
        status = 0;
    }
    if (status)
    {
        two_species_size = minisnn_worlds_domain_snapshot_size(two_species);
        resized_copy = realloc(copy, two_species_size);
        if (resized_copy == NULL)
        {
            status = 0;
        }
        else
        {
            copy = resized_copy;
            memcpy(copy, minisnn_worlds_domain_snapshot_data(two_species), two_species_size);
            write_u64(copy, species_offset + WD1_SPECIES_SIZE, 1U);
            refresh_digest(copy, two_species_size);
            if (!malformed_snapshot_rejected(copy, two_species_size))
            {
                status = 0;
            }
        }
    }
cleanup:
    free(copy);
    minisnn_worlds_domain_snapshot_destroy(two_species);
    minisnn_worlds_domain_snapshot_destroy(unknown_entity);
    minisnn_worlds_domain_snapshot_destroy(after);
    minisnn_worlds_domain_snapshot_destroy(before);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return status;
}

int main(void)
{
    WD1_CHECK(test_round_trip_and_kernel_binding());
    WD1_CHECK(test_tick_mismatch_rejected());
    WD1_CHECK(test_multiple_checkpoints());
    WD1_CHECK(test_event_id_overflow_rejected());
    WD1_CHECK(test_provisional_batch_snapshot_rejected());
    WD1_CHECK(test_corruption_and_atomicity());
    puts("WD1 Domain snapshot tests OK");
    return 0;
}