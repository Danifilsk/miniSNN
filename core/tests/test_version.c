#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"

int main(void)
{
    if (minisnn_version_major() != MINISNN_VERSION_MAJOR ||
        minisnn_version_minor() != MINISNN_VERSION_MINOR ||
        minisnn_version_patch() != MINISNN_VERSION_PATCH ||
        strcmp(minisnn_version_prerelease(), MINISNN_VERSION_PRERELEASE) != 0 ||
        strcmp(minisnn_version_string(), MINISNN_VERSION_STRING) != 0 ||
        minisnn_version_string() != minisnn_version_string())
    {
        fprintf(stderr, "Version API validation failed\n");
        return 1;
    }

    printf("miniSNN version validation OK: %s\n", minisnn_version_string());
    return 0;
}
