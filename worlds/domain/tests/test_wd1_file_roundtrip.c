#include "minisnn_worlds_domain.h"
#include "wd1_domain_snapshot_file.h"
#include "wd0_test_support.h"

#include <stdio.h>
#include <string.h>

#define WD1_FILE_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD1 file test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

int main(int argc, char **argv)
{
    const char *path = argc == 2 ? argv[1] : "build/worlds/domain/tests/wd1_snapshot_roundtrip.bin";
    char temporary_path[1024] = "";
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *loaded = NULL;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    FILE *temporary = NULL;
    int status = 1;

    if (kernel == NULL || !wd0_make_entity(kernel, 0, 0, &organism) ||
        !wd0_make_entity(kernel, 1000, 0, &food) ||
        (domain = wd0_domain_with_species(kernel)) == NULL ||
        minisnn_worlds_domain_register_organism(domain, organism, species.species_id, 70U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_food(domain, food, 20U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_save_file(path, snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_load_file(path, &loaded) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_size(snapshot) != minisnn_worlds_domain_snapshot_size(loaded) ||
        minisnn_worlds_domain_snapshot_digest(snapshot) != minisnn_worlds_domain_snapshot_digest(loaded) ||
        memcmp(minisnn_worlds_domain_snapshot_data(snapshot), minisnn_worlds_domain_snapshot_data(loaded),
               minisnn_worlds_domain_snapshot_size(snapshot)) != 0)
    {
        status = 0;
        goto cleanup;
    }
    if (snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path) < 0 ||
        (temporary = fopen(temporary_path, "rb")) != NULL)
    {
        if (temporary != NULL) fclose(temporary);
        status = 0;
    }
cleanup:
    minisnn_worlds_domain_snapshot_destroy(loaded);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    (void)remove(path);
    (void)remove(temporary_path);
    if (!status)
    {
        fputs("WD1 file round-trip test failed\n", stderr);
        return 1;
    }
    puts("WD1 Domain file round-trip OK");
    return 0;
}