#include "wd0_test_support.h"
#include "minisnn_worlds_kernel_snapshot.h"

#include <stdlib.h>
#include <string.h>

#define WD2_HEADER_SIZE 148U
#define WD2_DIGEST_OFFSET 44U
#define WD2_SPECIES_SIZE 40U
#define WD2_ORGANISM_V1_SIZE 24U
#define WD2_ORGANISM_V2_SIZE 40U
#define WD2_FOOD_SIZE 16U
#define WD2_EVENT_V1_SIZE 60U
#define WD2_EVENT_V2_SIZE 64U
#define WD2_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD2 persistence test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static uint32_t read_u32(const uint8_t *data, size_t offset)
{
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1U] << 8U) |
           ((uint32_t)data[offset + 2U] << 16U) | ((uint32_t)data[offset + 3U] << 24U);
}
static uint64_t read_u64(const uint8_t *data, size_t offset)
{
    uint64_t value = 0U; size_t index;
    for (index = 0U; index < 8U; ++index) value |= (uint64_t)data[offset + index] << (index * 8U);
    return value;
}
static void write_u32(uint8_t *data, size_t offset, uint32_t value)
{
    size_t index; for (index = 0U; index < 4U; ++index) data[offset + index] = (uint8_t)(value >> (index * 8U));
}
static void write_u64(uint8_t *data, size_t offset, uint64_t value)
{
    size_t index; for (index = 0U; index < 8U; ++index) data[offset + index] = (uint8_t)(value >> (index * 8U));
}
static uint64_t digest_bytes(const uint8_t *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037); size_t index;
    for (index = 0U; index < size; ++index)
    {
        uint8_t byte = index >= WD2_DIGEST_OFFSET && index < WD2_DIGEST_OFFSET + 8U ? 0U : data[index];
        hash ^= byte; hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static void refresh_digest(uint8_t *data, size_t size)
{
    write_u64(data, WD2_DIGEST_OFFSET, digest_bytes(data, size));
}

static int make_domain(MiniSNNWorldsKernel **out_kernel, MiniSNNWorldsDomain **out_domain,
                       MiniSNNWorldsKernelEntityId *out_actor, uint64_t energy)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    if (kernel == NULL || !wd0_make_entity(kernel, 0, 0, out_actor))
    {
        minisnn_worlds_kernel_destroy(kernel); return 0;
    }
    species.metabolism_per_tick = 1U;
    domain = minisnn_worlds_domain_create(kernel, NULL);
    if (domain == NULL || minisnn_worlds_domain_add_species(domain, &species) != 0 ||
        minisnn_worlds_domain_register_organism(domain, *out_actor, 1U, energy) != 0)
    {
        minisnn_worlds_domain_destroy(domain); minisnn_worlds_kernel_destroy(kernel); return 0;
    }
    *out_kernel = kernel; *out_domain = domain; return 1;
}

static int snapshot_equal(const MiniSNNWorldsDomainSnapshot *left,
                          const MiniSNNWorldsDomainSnapshot *right)
{
    return minisnn_worlds_domain_snapshot_size(left) == minisnn_worlds_domain_snapshot_size(right) &&
           memcmp(minisnn_worlds_domain_snapshot_data(left), minisnn_worlds_domain_snapshot_data(right),
                  minisnn_worlds_domain_snapshot_size(left)) == 0;
}

static int convert_v2_to_v1(const MiniSNNWorldsDomainSnapshot *source,
                             uint8_t **out_data, size_t *out_size)
{
    const uint8_t *input = minisnn_worlds_domain_snapshot_data(source);
    size_t input_size = minisnn_worlds_domain_snapshot_size(source);
    uint64_t species_count, organism_count, food_count, event_count;
    size_t species_offset = WD2_HEADER_SIZE, organism_offset, food_offset, event_offset;
    size_t destination_size, offset, index;
    uint8_t *data;

    if (input == NULL || input_size < WD2_HEADER_SIZE || read_u32(input, 8U) != 2U) return 0;
    species_count = read_u64(input, 68U); organism_count = read_u64(input, 76U);
    food_count = read_u64(input, 84U); event_count = read_u64(input, 92U);
    if (species_count > SIZE_MAX / WD2_SPECIES_SIZE || organism_count > SIZE_MAX / WD2_ORGANISM_V2_SIZE ||
        food_count > SIZE_MAX / WD2_FOOD_SIZE || event_count > SIZE_MAX / WD2_EVENT_V2_SIZE) return 0;
    organism_offset = species_offset + (size_t)species_count * WD2_SPECIES_SIZE;
    food_offset = organism_offset + (size_t)organism_count * WD2_ORGANISM_V2_SIZE;
    event_offset = food_offset + (size_t)food_count * WD2_FOOD_SIZE;
    destination_size = WD2_HEADER_SIZE + (size_t)species_count * WD2_SPECIES_SIZE +
                       (size_t)organism_count * WD2_ORGANISM_V1_SIZE +
                       (size_t)food_count * WD2_FOOD_SIZE + (size_t)event_count * WD2_EVENT_V1_SIZE;
    data = malloc(destination_size);
    if (data == NULL) return 0;
    memcpy(data, input, WD2_HEADER_SIZE);
    write_u32(data, 8U, 1U); write_u64(data, 16U, destination_size); write_u64(data, WD2_DIGEST_OFFSET, 0U);
    offset = WD2_HEADER_SIZE;
    memcpy(data + offset, input + species_offset, (size_t)species_count * WD2_SPECIES_SIZE);
    offset += (size_t)species_count * WD2_SPECIES_SIZE;
    for (index = 0U; index < (size_t)organism_count; ++index)
    {
        memcpy(data + offset, input + organism_offset + index * WD2_ORGANISM_V2_SIZE, WD2_ORGANISM_V1_SIZE);
        offset += WD2_ORGANISM_V1_SIZE;
    }
    memcpy(data + offset, input + food_offset, (size_t)food_count * WD2_FOOD_SIZE);
    offset += (size_t)food_count * WD2_FOOD_SIZE;
    for (index = 0U; index < (size_t)event_count; ++index)
    {
        memcpy(data + offset, input + event_offset + index * WD2_EVENT_V2_SIZE, WD2_EVENT_V1_SIZE);
        offset += WD2_EVENT_V1_SIZE;
    }
    refresh_digest(data, destination_size);
    *out_data = data; *out_size = destination_size; return 1;
}

static int test_dead_snapshot_restore_and_invalid_lifecycle(void)
{
    MiniSNNWorldsKernel *kernel = NULL, *restored_kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL, *restored = NULL;
    MiniSNNWorldsKernelSnapshot *kernel_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL, *before = NULL, *after = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainAction action = {0};
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsDomainOrganismInfo info;
    uint64_t hash_a, hash_b;
    uint8_t *corrupt = NULL;
    size_t size, organism_offset;

    WD2_CHECK(make_domain(&kernel, &domain, &actor, 1U));
    action.actor = actor; action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) == 0 &&
              minisnn_worlds_domain_snapshot_capture(domain, &snapshot) == 0 &&
              minisnn_worlds_domain_snapshot_format_version(snapshot) == 2U &&
              minisnn_worlds_kernel_snapshot_capture(kernel, &kernel_snapshot) == 0 &&
              minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &restored_kernel) == 0);
    restored = minisnn_worlds_domain_create(restored_kernel, NULL);
    WD2_CHECK(restored != NULL && minisnn_worlds_domain_snapshot_restore(restored, snapshot) == 0 &&
              minisnn_worlds_domain_organism_at(restored, 0U, &info) == 0 &&
              info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD &&
              info.death_cause == MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION &&
              minisnn_worlds_domain_state_hash(domain, &hash_a) == 0 &&
              minisnn_worlds_domain_state_hash(restored, &hash_b) == 0 && hash_a == hash_b);
    WD2_CHECK(minisnn_worlds_domain_step(restored, &action, 1U, &result) == 0 &&
              result.reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD);

    size = minisnn_worlds_domain_snapshot_size(snapshot);
    corrupt = malloc(size);
    organism_offset = WD2_HEADER_SIZE + WD2_SPECIES_SIZE;
    WD2_CHECK(corrupt != NULL && minisnn_worlds_domain_snapshot_capture(restored, &before) == 0);
    memcpy(corrupt, minisnn_worlds_domain_snapshot_data(snapshot), size);
    write_u32(corrupt, organism_offset + 24U, (uint32_t)MINISNN_WORLDS_DOMAIN_LIFE_ALIVE);
    refresh_digest(corrupt, size);
    WD2_CHECK(minisnn_worlds_domain_snapshot_from_bytes(corrupt, size, &after) ==
              MINISNN_WORLDS_DOMAIN_ERROR_SNAPSHOT_INVALID_FORMAT && after == NULL &&
              minisnn_worlds_domain_snapshot_capture(restored, &after) == 0 && snapshot_equal(before, after));

    free(corrupt); minisnn_worlds_domain_snapshot_destroy(after); minisnn_worlds_domain_snapshot_destroy(before);
    minisnn_worlds_domain_snapshot_destroy(snapshot); minisnn_worlds_kernel_snapshot_destroy(kernel_snapshot);
    minisnn_worlds_domain_destroy(restored); minisnn_worlds_kernel_destroy(restored_kernel);
    minisnn_worlds_domain_destroy(domain); minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_v1_load_defaults_alive_and_future_death(void)
{
    MiniSNNWorldsKernel *kernel = NULL, *clone_kernel = NULL, *future_kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL, *clone = NULL, *future = NULL;
    MiniSNNWorldsKernelSnapshot *kernel_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *v2 = NULL, *v1 = NULL, *before_death = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainAction action = {0};
    MiniSNNWorldsDomainActionResult a, b;
    MiniSNNWorldsDomainOrganismInfo info;
    uint8_t *v1_bytes = NULL;
    size_t v1_size = 0U;
    uint64_t hash_a, hash_b;

    WD2_CHECK(make_domain(&kernel, &domain, &actor, 1U) &&
              minisnn_worlds_domain_snapshot_capture(domain, &v2) == 0 &&
              convert_v2_to_v1(v2, &v1_bytes, &v1_size) &&
              minisnn_worlds_domain_snapshot_from_bytes(v1_bytes, v1_size, &v1) == 0 &&
              minisnn_worlds_domain_snapshot_format_version(v1) == 1U &&
              minisnn_worlds_kernel_snapshot_capture(kernel, &kernel_snapshot) == 0 &&
              minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &clone_kernel) == 0);
    clone = minisnn_worlds_domain_create(clone_kernel, NULL);
    WD2_CHECK(clone != NULL && minisnn_worlds_domain_snapshot_restore(clone, v1) == 0 &&
              minisnn_worlds_domain_organism_at(clone, 0U, &info) == 0 &&
              info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE && info.death_cause == 0U &&
              info.death_tick == 0U);

    WD2_CHECK(minisnn_worlds_domain_snapshot_capture(domain, &before_death) == 0 &&
              minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &future_kernel) == 0);
    future = minisnn_worlds_domain_create(future_kernel, NULL);
    action.actor = actor; action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(future != NULL && minisnn_worlds_domain_snapshot_restore(future, before_death) == 0 &&
              minisnn_worlds_domain_step(domain, &action, 1U, &a) == 0 &&
              minisnn_worlds_domain_step(future, &action, 1U, &b) == 0 &&
              minisnn_worlds_domain_organism_at(domain, 0U, &info) == 0 &&
              info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD &&
              minisnn_worlds_domain_state_hash(domain, &hash_a) == 0 &&
              minisnn_worlds_domain_state_hash(future, &hash_b) == 0 && hash_a == hash_b);

    free(v1_bytes); minisnn_worlds_domain_snapshot_destroy(before_death); minisnn_worlds_domain_snapshot_destroy(v1);
    minisnn_worlds_domain_snapshot_destroy(v2); minisnn_worlds_kernel_snapshot_destroy(kernel_snapshot);
    minisnn_worlds_domain_destroy(future); minisnn_worlds_kernel_destroy(future_kernel);
    minisnn_worlds_domain_destroy(clone); minisnn_worlds_kernel_destroy(clone_kernel);
    minisnn_worlds_domain_destroy(domain); minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

int main(void)
{
    WD2_CHECK(test_dead_snapshot_restore_and_invalid_lifecycle() == 0);
    WD2_CHECK(test_v1_load_defaults_alive_and_future_death() == 0);
    puts("WD2 lifecycle snapshot V1/V2 persistence and restore OK");
    return 0;
}