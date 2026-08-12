#ifndef MINISNN_WORLDS_DOMAIN_SNAPSHOT_FILE_H
#define MINISNN_WORLDS_DOMAIN_SNAPSHOT_FILE_H

#include "minisnn_worlds_domain.h"

/* App-layer binary persistence. The Domain library remains memory-only. */
MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_save_file(
    const char *path,
    const MiniSNNWorldsDomainSnapshot *snapshot);

MiniSNNWorldsDomainError minisnn_worlds_domain_snapshot_load_file(
    const char *path,
    MiniSNNWorldsDomainSnapshot **out_snapshot);

#endif