#include <stdio.h>

#include "k0_scenario_artifacts.h"

int main(int argc, char **argv)
{
    char error[256];

    if (argc != 2 || !k0_scenario_artifacts_validate_directory(argv[1], error, sizeof(error)))
    {
        return 1;
    }
    printf("K0-D artifact directory validation OK\n");
    return 0;
}
