#include "wd0_test_support.h"

static int fresh_and_corrupt(MiniSNNWorldsDomainTestingCorruption corruption)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;

    WD0_CHECK(kernel != NULL);
    WD0_CHECK(wd0_make_entity(kernel, 0, 0, &organism));
    WD0_CHECK(wd0_make_entity(kernel, 1000, 0, &food));
    domain = wd0_domain_with_species(kernel);
    WD0_CHECK(domain != NULL);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism, 1U, 50U) == 0);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, food, 4U) == 0);
    if (corruption == MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_EVENT_ID ||
        corruption == MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_KIND ||
        corruption == MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_COUNTER_MISMATCH)
    {
        MiniSNNWorldsDomainAction action = { organism, MINISNN_WORLDS_DOMAIN_ACTION_WAIT, {0, 0}, {0} };
        MiniSNNWorldsDomainActionResult result;
        WD0_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) == 0);
        if (corruption == MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_EVENT_ID)
        {
            WD0_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) == 0);
        }
    }
    WD0_CHECK(minisnn_worlds_domain_testing_validate_invariants(domain) == 0);
    WD0_CHECK(minisnn_worlds_domain_testing_inject_corruption(domain, corruption) == 0);
    WD0_CHECK(minisnn_worlds_domain_testing_validate_invariants(domain) ==
              MINISNN_WORLDS_DOMAIN_ERROR_INVARIANT_VIOLATION);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

int main(void)
{
    MiniSNNWorldsDomainTestingCorruption corruption;
    for (corruption = MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_DUPLICATE_ENTITY;
         corruption <= MINISNN_WORLDS_DOMAIN_TESTING_CORRUPTION_INVALID_KIND; ++corruption)
    {
        WD0_CHECK(fresh_and_corrupt(corruption) == 0);
    }
    puts("WD0 invariant corruption validation OK");
    return 0;
}
