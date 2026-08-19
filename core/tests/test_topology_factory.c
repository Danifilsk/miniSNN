#include <stdio.h>
#include <string.h>

#include "minisnn.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Topology factory test failed: %s at line %d\n", \
                #condition, __LINE__); \
        return 0; \
    } \
} while (0)

static int connections_are_legal(
    const MiniSNN *snn,
    uint32_t neuron_count,
    uint32_t inhibitory_count,
    int allow_self_connections,
    int allow_inhibitory_to_inhibitory)
{
    unsigned char seen[16U * 16U] = {0U};
    size_t index;
    uint32_t inhibitory_start = neuron_count - inhibitory_count;

    CHECK(neuron_count <= 16U);
    for (index = 0U; index < minisnn_connection_count(snn); ++index)
    {
        MiniSNNConnectionInfo connection;
        size_t key;
        int source_is_inhibitory;
        int target_is_inhibitory;

        CHECK(minisnn_get_connection(snn, index, &connection));
        CHECK(connection.source < neuron_count && connection.target < neuron_count);
        CHECK(allow_self_connections || connection.source != connection.target);
        key = (size_t)connection.source * neuron_count + connection.target;
        CHECK(seen[key] == 0U);
        seen[key] = 1U;
        source_is_inhibitory = connection.source >= inhibitory_start;
        target_is_inhibitory = connection.target >= inhibitory_start;
        CHECK(source_is_inhibitory ? connection.weight < 0.0 : connection.weight > 0.0);
        CHECK(allow_inhibitory_to_inhibitory ||
              !(source_is_inhibitory && target_is_inhibitory));
    }
    return 1;
}

static int test_rejects_odd_small_world_neighbors(void)
{
    MiniSNNTopologyFactoryConfig config = minisnn_topology_factory_default();
    MiniSNN *snn = minisnn_create(8);
    int result = 0;

    CHECK(snn != NULL);
    config.kind = MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD;
    config.small_world_neighbors = 3U;
    CHECK(!minisnn_topology_factory_build(snn, &config));
    CHECK(minisnn_connection_count(snn) == 0U);
    result = 1;
    minisnn_destroy(&snn);
    return result;
}

static int test_small_world_is_deterministic_and_legal(void)
{
    MiniSNNTopologyFactoryConfig config = minisnn_topology_factory_default();
    MiniSNN *first = minisnn_create(12);
    MiniSNN *second = minisnn_create(12);
    uint64_t first_signature = 0U;
    uint64_t second_signature = 0U;
    int result = 0;

    CHECK(first != NULL && second != NULL);
    config.kind = MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD;
    config.seed = UINT64_C(17);
    config.inhibitory_count = 3U;
    config.small_world_neighbors = 4U;
    config.small_world_rewire_probability = 0.35;
    config.allow_self_connections = 0;
    config.allow_inhibitory_to_inhibitory = 0;
    CHECK(minisnn_topology_factory_build(first, &config));
    CHECK(minisnn_topology_factory_build(second, &config));
    CHECK(minisnn_get_topology_signature(first, &first_signature));
    CHECK(minisnn_get_topology_signature(second, &second_signature));
    CHECK(first_signature == second_signature);
    CHECK(connections_are_legal(first, 12U, 3U, 0, 0));
    CHECK(connections_are_legal(second, 12U, 3U, 0, 0));
    result = 1;
    minisnn_destroy(&first);
    minisnn_destroy(&second);
    return result;
}

static int test_random_and_full_extremes(void)
{
    MiniSNNTopologyFactoryConfig config = minisnn_topology_factory_default();
    MiniSNN *random = minisnn_create(10);
    MiniSNN *full = minisnn_create(10);
    int result = 0;

    CHECK(random != NULL && full != NULL);
    config.kind = MINISNN_TOPOLOGY_FACTORY_RANDOM;
    config.seed = UINT64_C(1);
    config.connection_probability = 0.0;
    CHECK(minisnn_topology_factory_build(random, &config));
    CHECK(minisnn_connection_count(random) == 0U);

    config.kind = MINISNN_TOPOLOGY_FACTORY_FULLY_CONNECTED;
    config.inhibitory_count = 2U;
    config.allow_self_connections = 0;
    config.allow_inhibitory_to_inhibitory = 1;
    CHECK(minisnn_topology_factory_build(full, &config));
    CHECK(minisnn_connection_count(full) == 90U);
    CHECK(connections_are_legal(full, 10U, 2U, 0, 1));
    result = 1;
    minisnn_destroy(&random);
    minisnn_destroy(&full);
    return result;
}

int main(void)
{
    if (!test_rejects_odd_small_world_neighbors() ||
        !test_small_world_is_deterministic_and_legal() ||
        !test_random_and_full_extremes())
    {
        return 1;
    }
    puts("Public topology factory validation OK");
    return 0;
}