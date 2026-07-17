#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "evolution_runner.h"
#include "structure.h"

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

static int file_contains_text(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    char line[1024];
    int found = 0;

    if (file == NULL)
        return 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, needle) != NULL)
        {
            found = 1;
            break;
        }
    }
    fclose(file);
    return found;
}

static int rewrite_structure_checkpoint_as_c4(
    const char *directory,
    uint64_t legacy_signature)
{
    char path[1024];
    char temporary_path[1024];
    char line[2048];
    FILE *input;
    FILE *output;
    int first_line = 1;
    int ok = 1;

    if (snprintf(path, sizeof(path), "%s/checkpoint_structure.txt", directory) >=
            (int)sizeof(path) ||
        snprintf(temporary_path, sizeof(temporary_path),
            "%s/checkpoint_structure.c4.tmp", directory) >=
            (int)sizeof(temporary_path))
        return 0;
    input = fopen(path, "rb");
    output = fopen(temporary_path, "wb");
    if (input == NULL || output == NULL)
    {
        if (input != NULL)
            fclose(input);
        if (output != NULL)
            fclose(output);
        return 0;
    }
    while (fgets(line, sizeof(line), input) != NULL)
    {
        if (first_line)
        {
            first_line = 0;
            if (fputs("MINISNN_STRUCTURE_CHECKPOINT_V1\n", output) == EOF)
                ok = 0;
        }
        else if (strncmp(line, "neuron_blueprint_signature=", 27) == 0)
        {
            if (fprintf(output, "neuron_blueprint_signature=%llu\n",
                    (unsigned long long)legacy_signature) < 0)
                ok = 0;
        }
        else if (strncmp(line, "neuron_model=", 13) != 0 &&
                 strncmp(line, "neuron_model_config_signature=", 30) != 0 &&
                 fputs(line, output) == EOF)
        {
            ok = 0;
        }
        if (!ok)
            break;
    }
    {
        int input_error = ferror(input);
        int input_close_error = fclose(input) != 0;
        int output_close_error = fclose(output) != 0;
        if (input_error || input_close_error || output_close_error)
            ok = 0;
    }
    if (!ok || !MoveFileExA(temporary_path, path,
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temporary_path);
        return 0;
    }
    return 1;
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
    EvolutionRunResult legacy_partial;
    EvolutionRunResult legacy_resumed;
    EvolutionRunResult incompatible_partial;
    EvolutionRunResult rejected;
    Neuron legacy_neurons[5] = {0};
    uint64_t legacy_signature;
    char error[512] = {0};
    char left[1024];
    char right[1024];
    char manifest_path[1024];
    int ok = 1;

    remove_tree(TEST_ROOT);
    evolution_runner_default_options(&options);
    options.output_root = TEST_ROOT "/full";
    ok = evolution_runner_execute(
        "configs/evolution_structure_target_demo.ini",
        &options, &full, error, sizeof(error));
    ok = ok && snprintf(manifest_path, sizeof(manifest_path),
        "%s/evolution_manifest.txt", full.output_directory) <
            (int)sizeof(manifest_path) &&
        file_contains_text(manifest_path, "neural_model=LIF") &&
        file_contains_text(manifest_path, "neuron_model=lif") &&
        file_contains_text(manifest_path,
            "checkpoint_format=text_v1+structure_sidecar_v2");
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

    legacy_signature = structure_neuron_blueprint_signature_legacy(
        legacy_neurons, sizeof(legacy_neurons) / sizeof(legacy_neurons[0]));
    options.output_root = TEST_ROOT "/legacy";
    options.stop_after_generations = 5;
    ok = ok && legacy_signature != 0U && evolution_runner_execute(
        "configs/evolution_structure_target_demo.ini",
        &options, &legacy_partial, error, sizeof(error)) &&
        !legacy_partial.completed && rewrite_structure_checkpoint_as_c4(
            legacy_partial.output_directory, legacy_signature);
    options.stop_after_generations = 0;
    ok = ok && evolution_runner_resume(
        legacy_partial.output_directory, &options,
        &legacy_resumed, error, sizeof(error)) && legacy_resumed.completed &&
        full.best_individual_id == legacy_resumed.best_individual_id &&
        fabs(full.best_fitness - legacy_resumed.best_fitness) <= 1e-15;

    options.output_root = TEST_ROOT "/incompatible";
    options.stop_after_generations = 5;
    ok = ok && evolution_runner_execute(
        "configs/evolution_structure_target_demo.ini",
        &options, &incompatible_partial, error, sizeof(error)) &&
        !incompatible_partial.completed && rewrite_structure_checkpoint_as_c4(
            incompatible_partial.output_directory, legacy_signature + 1U);
    options.stop_after_generations = 0;
    error[0] = '\0';
    ok = ok && !evolution_runner_resume(
        incompatible_partial.output_directory, &options,
        &rejected, error, sizeof(error)) && error[0] != '\0';

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
