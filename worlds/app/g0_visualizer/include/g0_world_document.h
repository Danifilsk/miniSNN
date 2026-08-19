#ifndef G0_WORLD_DOCUMENT_H
#define G0_WORLD_DOCUMENT_H

#include <stddef.h>

#include "g0_app_settings.h"
#include "g0_world_config.h"

#define G0_WORLD_DOCUMENT_FORMAT "miniSNN Worlds World Config V2"
#define G0_WORLD_DOCUMENT_FORMAT_V1 "miniSNN Worlds World Config V1"
#define G0_WORLD_DOCUMENT_NAME_MAX 64U

typedef enum
{
    G0_WORLD_DOCUMENT_ERROR_NONE = 0,
    G0_WORLD_DOCUMENT_ERROR_INVALID_ARGUMENT,
    G0_WORLD_DOCUMENT_ERROR_IO,
    G0_WORLD_DOCUMENT_ERROR_FORMAT,
    G0_WORLD_DOCUMENT_ERROR_VERSION,
    G0_WORLD_DOCUMENT_ERROR_VALIDATION,
    G0_WORLD_DOCUMENT_ERROR_ALLOCATION
} G0WorldDocumentError;

/*
 * Blueprint-only persistence: no Kernel/Domain snapshots, Core state, Win32
 * handles, sprites, or live runtime pointers belong to this format.
 */
typedef struct
{
    char name[G0_WORLD_DOCUMENT_NAME_MAX];
    G0WorldConfig world;
    G0AppSettings settings;
} G0WorldDocument;

void g0_world_document_init(G0WorldDocument *document);
void g0_world_document_destroy(G0WorldDocument *document);
int g0_world_document_set_name(G0WorldDocument *document, const char *name);
int g0_world_document_create(
    G0WorldDocument *document,
    const char *name,
    const G0WorldConfig *world,
    const G0AppSettings *settings);
int g0_world_document_clone(const G0WorldDocument *source, G0WorldDocument *out_copy);
int g0_world_document_validate(const G0WorldDocument *document);
int g0_world_document_equal(const G0WorldDocument *left, const G0WorldDocument *right);

int g0_world_document_save(
    const char *filename,
    const G0WorldDocument *document,
    G0WorldDocumentError *out_error);
int g0_world_document_load(
    const char *filename,
    G0WorldDocument *out_document,
    G0WorldDocumentError *out_error);
const char *g0_world_document_error_name(G0WorldDocumentError error);

#endif