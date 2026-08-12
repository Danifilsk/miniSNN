#include "wd0_test_support.h"

int main(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId first_food;
    MiniSNNWorldsKernelEntityId second_food;
    MiniSNNWorldsDomainPerception perception;

    WD0_CHECK(kernel != NULL);
    WD0_CHECK(wd0_make_entity(kernel, 0, 0, &organism));
    WD0_CHECK(wd0_make_entity(kernel, -1000, 0, &first_food));
    WD0_CHECK(wd0_make_entity(kernel, 1000, 0, &second_food));
    domain = wd0_domain_with_species(kernel);
    WD0_CHECK(domain != NULL);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism, 1U, 90U) == 0);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, second_food, 3U) == 0);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, first_food, 3U) == 0);
    WD0_CHECK(minisnn_worlds_domain_perceive(domain, organism, &perception) == 0);
    WD0_CHECK(perception.nearest_food_entity.value == first_food.value);
    WD0_CHECK(perception.nearest_food_distance == 1000U &&
              perception.nearest_food_delta_x == -1000 && perception.nearest_food_delta_y == 0);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    puts("WD0 nearest-food Manhattan and EntityId tie-break validation OK");
    return 0;
}
