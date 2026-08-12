N?o ? poss?vel substituir a vari?vel Error porque ela ? somente leitura ou constante. N?o ? poss?vel substituir a vari?vel Error porque ela ? somente leitura ou constante.
For K1-B2, `DESTINATION_OVERFLOW` is a semantic command rejection: it consumes
one move command, emits one rejection event and preserves the official spatial
state. It is not an internal tick failure.
## K1-C1 structural rejection

Spatial-link semantic failures use explicit parent/child alive, placement,
self, duplicate, child-parent, cycle, offset-overflow and not-found rejection
reasons. Lifecycle with an incident link rejects as
`TARGET_HAS_SPATIAL_LINKS`; linked movement rejects as
`TARGET_HAS_SPATIAL_PARENT`; the historical children rejection is retained only for compatibility and is not emitted by K1-C2 root movement. Allocation,
identifier overflow and invariant failures remain atomic kernel errors rather
than command rejection.

## K1-C3 hardening

Overflow de identificadores, contadores de promocao, capacidade de plano ou alocacao interrompe o tick antes da promocao. O estado oficial, a fila pendente, eventos, diagnosticos e hash permanecem atomicos em falhas de preflight.

## K2-A snapshot

MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_SIZE_OVERFLOW indica que a aritmetica
do payload V1 excederia size_t. A captura falha sem alocar snapshot parcial e sem
modificar o Kernel. Falhas de alocacao retornam MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION.
## K2-B restore

`MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT` rejects malformed V1
bytes; `MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_UNSUPPORTED_VERSION` rejects a
non-V1 format; `MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_STATE_HASH_MISMATCH`
rejects a syntactically valid decoded state whose stored official state hash
does not match. Import and restore leave output pointers NULL on failure.

## K2-C command replay

`MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT` rejects malformed or
noncanonical command-log V1 bytes; `MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_UNSUPPORTED_VERSION`
rejects another format version. `MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE`
means the ready Kernel tick or expected CommandId did not match a record before
submission. Replay is incremental; earlier successfully replayed records are
not rolled back by a later divergence.