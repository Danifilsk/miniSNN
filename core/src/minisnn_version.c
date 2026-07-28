#include "minisnn_version.h"

uint32_t minisnn_version_major(void)
{
    return MINISNN_VERSION_MAJOR;
}

uint32_t minisnn_version_minor(void)
{
    return MINISNN_VERSION_MINOR;
}

uint32_t minisnn_version_patch(void)
{
    return MINISNN_VERSION_PATCH;
}

const char *minisnn_version_prerelease(void)
{
    return MINISNN_VERSION_PRERELEASE;
}

const char *minisnn_version_string(void)
{
    return MINISNN_VERSION_STRING;
}
