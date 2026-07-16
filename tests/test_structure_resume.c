#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "evolution_runner.h"

#define TEST_ROOT "build/test_structure_resume"

static int remove_tree(const char *path)
{
    char pattern[512];
    WIN32_FIND_DATAA data;
    HANDLE find;
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) >= (int)sizeof(pattern))
        return 0;
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ||
               GetLastError() == ERROR_PATH_NOT_FOUND;
    do
    {
        char child[512];
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0)
            continue;
        if (snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName) >=
            (int)sizeof(child))
        {
            FindClose(find);
            return 0;
        }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!remove_tree(child))
            {
                FindClose(find);
                return 0;
            }
        }
        else if (!DeleteFileA(child))
        {
            FindClose(find);
            return 0;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return RemoveDirectoryA(path) || GetLastError() == ERROR_PATH_NOT_FOUND;
}

static int files_equal(const char *left_path, const char *right_path)
{
    FILE *left = fopen(left_path, "rb");
    FILE *right = fopen(right_path, "rb");
    int equal = 1;
    if (left == NULL || right == NULL)
    {
        if (left != NULL)
            fclose(left);
        if (right != NULL)
            fclose(right);
        return 0;
    }
    for (;;)
    {
        unsigned char left_buffer[4096];
        unsigned char right_buffer[4096];
        size_t left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (left_count != right_count ||
            memcmp(left_buffer, right_buffer, left_count) != 0)
        {
            equal = 0;
            break;
        }
        if (left_count == 0)
            break;
    }
    fclose(left);
    fclose(right);
    return equal;
}

int main(void)
{
    static const char *files[] = {
        "best_genome.csv",
        "best_network_initial.csv",
        "best_topology.csv",
        "best_topology_initial.csv",
        "best_topology_lifetime_final.csv",
        "checkpoint.txt",
        "checkpoint_structure.txt",
        "fitness_terms.csv",
        "genomes.csv",
        "lineage.csv",
        "structures.csv",
        "structural_events.csv"
    };
    EvolutionRunnerOptions options;
    EvolutionRunResult full;
    EvolutionRunResult partial;
    EvolutionRunResult resumed;
    char error[512] = {0};
    char left[1024];
    char right[1024];
    int ok = 1;

    remove_tree(TEST_ROOT);
    evolution_runner_default_options(&options);
    options.output_root = TEST_ROOT "/full";
    ok = evolution_runner_execute(
        "configs/evolution_structure_target_demo.ini",
        &options, &full, error, sizeof(error));
    options.output_root = TEST_ROOT "/resume";
    options.stop_after_generations = 5;
    ok = ok && evolution_runner_execute(
        "configs/evolution_structure_target_demo.ini",
        &options, &partial, error, sizeof(error)) && !partial.completed;
    options.stop_after_generations = 0;
    ok = ok && evolution_runner_resume(
        partial.output_directory, &options,
        &resumed, error, sizeof(error));
    ok = ok && full.completed && resumed.completed &&
         full.generations_completed == resumed.generations_completed &&
         full.best_individual_id == resumed.best_individual_id &&
         fabs(full.best_fitness - resumed.best_fitness) <= 1e-15;

    for (size_t i = 0; ok && i < sizeof(files) / sizeof(files[0]); i++)
    {
        snprintf(left, sizeof(left), "%s/%s", full.output_directory, files[i]);
        snprintf(right, sizeof(right), "%s/%s", resumed.output_directory, files[i]);
        ok = files_equal(left, right);
    }
    if (!remove_tree(TEST_ROOT))
        ok = 0;
    if (!ok)
    {
        fprintf(stderr, "Structural resume validation FAILED: %s\n", error);
        return 1;
    }
    printf("Structural resume validation OK\n");
    return 0;
}
