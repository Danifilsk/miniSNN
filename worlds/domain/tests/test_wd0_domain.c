#include "wd0_test_support.h"

int main(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsDomainOrganismInfo organism_info;
    MiniSNNWorldsDomainFoodInfo food_info;
    MiniSNNWorldsDomainPerception perception;
    uint64_t hash_a;
    uint64_t hash_b;

    WD0_CHECK(kernel != NULL);
    WD0_CHECK(wd0_make_entity(kernel, 0, 0, &organism));
    WD0_CHECK(wd0_make_entity(kernel, 3000, 0, &food));
    domain = wd0_domain_with_species(kernel);
    WD0_CHECK(domain != NULL);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism, 1U, 101U) ==
              MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, food, 0U) ==
              MINISNN_WORLDS_DOMAIN_ERROR_INVALID_ARGUMENT);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism, 1U, 80U) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, food, 12U) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD0_CHECK(minisnn_worlds_domain_organism_count(domain) == 1U);
    WD0_CHECK(minisnn_worlds_domain_food_count(domain) == 1U);
    WD0_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &organism_info) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD0_CHECK(organism_info.energy == 80U && organism_info.max_energy == 100U &&
              organism_info.hunger == 20U);
    WD0_CHECK(minisnn_worlds_domain_food_at(domain, 0U, &food_info) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE && food_info.nutrition == 12U);
    WD0_CHECK(minisnn_worlds_domain_perceive(domain, organism, &perception) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD0_CHECK(perception.nearest_food_present && perception.nearest_food_entity.value == food.value &&
              perception.nearest_food_distance == 3000U);
    WD0_CHECK(minisnn_worlds_domain_state_hash(domain, &hash_a) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD0_CHECK(minisnn_worlds_domain_state_hash(domain, &hash_b) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE && hash_a == hash_b);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    puts("WD0 domain registration, energy, hunger and hash validation OK");
    return 0;
}
