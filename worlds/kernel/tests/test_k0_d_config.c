#include <stdio.h>
#include <string.h>

#include "k0_scenario_config.h"

static int write_text(const char *filename, const char *text)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        return 0;
    }
    return fputs(text, file) != EOF && fclose(file) == 0;
}

int main(void)
{
    static const char equivalent[] =
        "; normalized textual variation\r\n"
        "[scenario]\r\nrandom_stream=1\r\ntrace_interval=1\r\nrandom_namespace=1\r\n"
        "priority_base=100\r\ncommand_delay=1\r\ndestroy_batch=1\r\ndestroy_interval=7\r\n"
        "create_batch=2\r\ncreate_interval=5\r\nmaximum_alive_entities=32\r\n"
        "minimum_alive_entities=4\r\ninitial_entities=10\r\nticks=73\r\n"
        "scenario_id=k0_integrated_demo\r\nscenario_version=1\r\nrandom_draws_per_tick=1\r\n\r\n"
        "[kernel]\r\nmaster_seed=12345\r\nconfig_version=1";
    static const char invalid[] =
        "[kernel]\nconfig_version=1\nmaster_seed=1\n\n[scenario]\n"
        "scenario_version=1\nscenario_id=bad/path\nticks=1\ninitial_entities=0\n"
        "minimum_alive_entities=0\nmaximum_alive_entities=1\ncreate_interval=1\ncreate_batch=1\n"
        "destroy_interval=1\ndestroy_batch=1\ncommand_delay=1\npriority_base=0\n"
        "random_namespace=1\nrandom_stream=1\nrandom_draws_per_tick=0\ntrace_interval=1\n";
    const char *equivalent_file = "../../build/worlds/kernel/tests/k0_d_equivalent.ini";
    const char *invalid_file = "../../build/worlds/kernel/tests/k0_d_invalid.ini";
    K0ScenarioConfig defaults;
    K0ScenarioConfig loaded;
    K0ScenarioConfig equivalent_config;
    char error[256];
    uint64_t signature;

    k0_scenario_config_default(&defaults);
    if (!k0_scenario_config_validate(&defaults, error, sizeof(error)))
    {
        return 1;
    }
    if (!k0_scenario_config_load_file("configs/k0_integrated_demo.ini", &loaded,
                                      error, sizeof(error)) ||
        loaded.master_seed != UINT64_C(12345) || loaded.ticks != UINT64_C(73) ||
        strcmp(loaded.scenario_id, "k0_integrated_demo") != 0)
    {
        return 1;
    }
    signature = k0_scenario_config_signature(&loaded);
    if (signature == 0U || !write_text(equivalent_file, equivalent) ||
        !k0_scenario_config_load_file(equivalent_file, &equivalent_config, error, sizeof(error)) ||
        k0_scenario_config_signature(&equivalent_config) != signature)
    {
        return 1;
    }
    if (!write_text(invalid_file, invalid) ||
        k0_scenario_config_load_file(invalid_file, &equivalent_config, error, sizeof(error)) ||
        error[0] == '\0')
    {
        return 1;
    }
    (void)remove(equivalent_file);
    (void)remove(invalid_file);
    printf("K0-D scenario config validation OK\n");
    return 0;
}
