#ifndef MINISNN_STUDIO_PRODUCT_LAUNCHER_H
#define MINISNN_STUDIO_PRODUCT_LAUNCHER_H

#include <stddef.h>

#define STUDIO_PRODUCT_LAUNCHER_ERROR_SIZE 512

int studio_product_launcher_resolve(
    const char *studio_executable_path,
    char *out_worlds_executable_path,
    size_t out_worlds_executable_path_size,
    char *error_message,
    size_t error_message_size);

int studio_product_launcher_launch(
    const char *worlds_executable_path,
    char *error_message,
    size_t error_message_size);

#endif
