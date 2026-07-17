#include <stdio.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define SOURCE_PATH "build/c5 source config.ini"
#define COPIED_PATH "results/scenarios/c5_source_roundtrip/config_source.ini"

static int read_bytes(const char *path, unsigned char *buffer,
                      size_t capacity, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    size_t size;
    if (file == NULL) return 0;
    size = fread(buffer, 1, capacity, file);
    {
        int read_failed = ferror(file) != 0 || !feof(file);
        int close_failed = fclose(file) != 0;
        if (read_failed || close_failed) return 0;
    }
    *out_size = size;
    return 1;
}

static int file_contains(const char *path, const char *needle)
{
    char buffer[16384];
    FILE *file = fopen(path, "rb");
    size_t count;
    int read_failed;
    int close_failed;
    if (file == NULL)
        return 0;
    count = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[count] = '\0';
    read_failed = ferror(file) != 0;
    close_failed = fclose(file) != 0;
    if (read_failed || close_failed)
        return 0;
    return strstr(buffer, needle) != NULL;
}

int main(void)
{
    static const char source[] =
        "# comentario preservado\r\n"
        "[run]\r\nrun_name   =   c5_source_roundtrip\r\n"
        "[network]\r\ntopology = chain\r\nneurons = 1\r\n"
        "[neuron]\r\nmodel = LiF\r\n"
        "[input]\r\nsource_count = 1\r\ninput_current = 20\r\n"
        "[simulation]\r\nsteps = 2\r\n"
        "[recording]\r\nrecord_neuron = 0\r\n"
        "[diagnostics]\r\nlevel = basic\r\n";
    unsigned char original[2048], copied[2048];
    size_t original_size, copied_size;
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    FILE *file = fopen(SOURCE_PATH, "wb");
    if (file == NULL || fwrite(source, 1, sizeof(source) - 1, file) !=
            sizeof(source) - 1 || fclose(file) != 0)
        return 1;
    if (!scenario_config_load_file(SOURCE_PATH, &config, error, sizeof(error)) ||
        config.neuron_model != MINISNN_NEURON_MODEL_LIF ||
        !scenario_runner_execute(&config, SOURCE_PATH, &result,
                                 error, sizeof(error)) ||
        !read_bytes(SOURCE_PATH, original, sizeof(original), &original_size) ||
        !read_bytes(COPIED_PATH, copied, sizeof(copied), &copied_size) ||
        original_size != copied_size ||
        memcmp(original, copied, original_size) != 0 ||
        !file_contains("results/scenarios/c5_source_roundtrip/config_used.ini",
                       "model = lif") ||
        !file_contains("results/scenarios/c5_source_roundtrip/summary.txt",
                       "state_nonfinite_count=0") ||
        !file_contains("results/scenarios/c5_source_roundtrip/summary.txt",
                       "voltage_mean=") ||
        !file_contains("results/scenarios/c5_source_roundtrip/summary.txt",
                       "adex_w_initial=NA") ||
        !file_contains("results/scenarios/c5_source_roundtrip/metrics.csv",
                       "neuron_model,model_config_signature,integration_method"))
    {
        remove(SOURCE_PATH);
        return 1;
    }
    remove(SOURCE_PATH);
    printf("Neuron model provenance validation OK\n");
    return 0;
}
