# K2-B Restore And Persistence Contract

## Scope

K2-B consumes only the frozen canonical Snapshot Format V1. It adds byte
import, full validation and transactional construction of a new Worlds Kernel.
It does not add replay, migration, Snapshot V2, compression or a command log.
K2-C is the next block and owns deterministic command replay.

## Public Memory APIs

`minisnn_worlds_kernel_snapshot_from_bytes(data, size, out_snapshot)` copies
caller bytes only after a complete V1 validation. On failure it leaves
`*out_snapshot` NULL and retains no caller pointer. Imported snapshots remain
opaque and immutable.

`minisnn_worlds_kernel_create_from_snapshot(snapshot, out_kernel)` decodes into
an unpublished candidate. It allocates the required canonical records,
rebuilds only derived capacities and indices, validates invariants and the
stored state hash, and re-captures the candidate to require byte-for-byte V1
canonical equality before publishing it. Failure leaves `*out_kernel` NULL.

The Kernel library remains memory-only. `app/k2_snapshot_file.c` is the only
K2-B file I/O adapter: it writes exactly `snapshot_data()` bytes to a temporary
file and atomically replaces the destination after a successful close; loading
reads bytes and delegates import to the Kernel. The file adapter adds no
container, timestamp, path metadata or newline.

## V1 Compatibility

Only V1 is accepted. A malformed V1 blob returns
`MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT`; an unsupported version
returns `MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_UNSUPPORTED_VERSION`; a decoded
state whose recomputed official hash differs from the stored hash returns
`MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_STATE_HASH_MISMATCH`.

V1 implicitly fixes PRNG version 1 and fixed-point scalar scale 1000. Compile
time assertions bind the decoder to those values. Changing either requires a
new snapshot format version.

## Validation

The decoder performs bounds-checked little-endian reads and rejects truncated
headers or sections, wrong magic, unknown format, mismatched payload length,
extra bytes, nonzero reserved fields, invalid booleans/enums/optional flags,
invalid configuration and space bounds, oversized counts and allocation
failure.

It validates canonical ordering and semantic consistency for entities,
transforms, occupancy, links, pending commands, last-tick events and PRNG
streams. It verifies IDs, monotonic visible event IDs, next IDs, counters,
stream ordering and the equality between per-stream draw counts and the global
PRNG draw counter. The internal invariant checker additionally validates links,
transforms, occupancy, queue ordering and cross references. No invalid value is
normalized or repaired.

## Deterministic Continuation

Immediately after a restore, snapshot bytes and state hash are identical to the
captured Kernel. Subsequent identical inputs preserve PRNG outputs, command
ordering (`target_tick`, priority, issuer, command ID), IDs, events,
diagnostics, counters and final canonical snapshot bytes. The K2-B tests cover
import ownership, malformed and truncated blobs, allocation failure,
checkpoints at ticks 0/100/500/1000, file round-trip and O0/O2 agreement.

## Golden

K2-B preserves the K2-A V1 golden without modification:

```text
format=1
size=1443
digest=0xC12509D783A5C6C0
state_hash=0xAA73D6A793A66B5B
```

## Boundaries

```text
Core <- no Worlds dependency
Worlds Kernel <- no Core, Domain or filesystem dependency
app/tool layer -> file persistence only
```