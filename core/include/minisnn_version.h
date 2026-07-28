#ifndef MINISNN_VERSION_H
#define MINISNN_VERSION_H

#include <stdint.h>

#define MINISNN_VERSION_MAJOR UINT32_C(1)
#define MINISNN_VERSION_MINOR UINT32_C(0)
#define MINISNN_VERSION_PATCH UINT32_C(0)
#define MINISNN_VERSION_PRERELEASE "rc.1"
#define MINISNN_VERSION_STRING "1.0.0-rc.1"

uint32_t minisnn_version_major(void);
uint32_t minisnn_version_minor(void);
uint32_t minisnn_version_patch(void);
const char *minisnn_version_prerelease(void);
const char *minisnn_version_string(void);

#endif
