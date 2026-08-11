#include "k1_c4_config.h"

#include <stdio.h>
#include <string.h>

static int write_text_file(const char *filename, const char *text)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        return 0;
    }
    if (fputs(text, file) == EOF || fclose(file) != 0)
    {
        return 0;
    }
    return 1;
}

static int rejects(
    const char *filename,
    const char *text,
    const char *expected_error_fragment)
{
    K1C4Config config;
    char error_message[256];

    if (!write_text_file(filename, text))
    {
        fprintf(stderr, "cannot write temporary config\n");
        return 0;
    }
    if (k1_c4_config_load_file(
            filename, &config, error_message, sizeof(error_message)))
    {
        fprintf(stderr, "invalid config accepted\n");
        return 0;
    }
    if (strstr(error_message, expected_error_fragment) == NULL)
    {
        fprintf(stderr, "unexpected parser error (expected %s): %s\n", expected_error_fragment, error_message);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char unknown_key_config[] =
        "[scenario]\n"
        "scenario_version=1\n"
        "scenario_id=bad\n"
        "master_seed=1\n"
        "ticks=15\n"
        "unknown_key=1\n"
        "[space]\n"
        "min_x=0\nmin_y=0\nmax_x=1\nmax_y=1\n";
    static const char duplicate_key_config[] =
        "[scenario]\n"
        "scenario_version=1\n"
        "scenario_version=1\n"
        "scenario_id=bad\n"
        "master_seed=1\n"
        "ticks=15\n"
        "[space]\n"
        "min_x=0\nmin_y=0\nmax_x=1\nmax_y=1\n";
    static const char duplicate_section_config[] =
        "[scenario]\n"
        "scenario_version=1\n"
        "[scenario]\n";
    static const char impossible_reference_config[] =
        "[scenario]\n"
        "scenario_version=1\n"
        "scenario_id=bad_reference\n"
        "master_seed=1\n"
        "ticks=15\n"
        "[space]\n"
        "min_x=0\nmin_y=0\nmax_x=100\nmax_y=100\n"
        "[entities]\n"
        "entity_1=1,0,0,0,0,0,0,0,0\n"
        "[links]\n"
        "link_1=1,2\n"
        "[commands]\n";
    const char *temporary =
        "../../build/worlds/kernel/tests/k1_c4_config_temporary.ini";
    K1C4Config first;
    K1C4Config second;
    char error_message[256];
    uint64_t first_signature;

    if (!k1_c4_config_load_file(
            "configs/k1_spatial_links_demo.ini",
            &first,
            error_message,
            sizeof(error_message)) ||
        !k1_c4_config_validate(&first, error_message, sizeof(error_message)))
    {
        fprintf(stderr, "valid config rejected: %s\n", error_message);
        return 1;
    }
    first_signature = k1_c4_config_signature(&first);
    if (!k1_c4_config_load_file(
            "configs/k1_spatial_links_demo.ini",
            &second,
            error_message,
            sizeof(error_message)) ||
        first_signature != k1_c4_config_signature(&second))
    {
        fprintf(stderr, "valid config is not deterministic\n");
        return 1;
    }

    if (!rejects(temporary, unknown_key_config, "unknown") ||
        !rejects(temporary, duplicate_key_config, "duplicate") ||
        !rejects(temporary, duplicate_section_config, "duplicate section") ||
        !rejects(temporary, impossible_reference_config, "invalid"))
    {
        (void)remove(temporary);
        return 1;
    }
    (void)remove(temporary);
    printf("K1-C4 configuration parser validation OK\n");
    return 0;
}