# WD1 Domain Snapshot Contract

Domain Snapshot V1 persists the semantic state owned by `worlds/domain/`. It is
separate from Kernel Snapshot V1. A complete world checkpoint without a brain is:

```text
Kernel Snapshot V1 + Domain Snapshot V1
```

Both snapshots must come from the same logical checkpoint. Domain restore checks
the current public Kernel tick and State Hash V5 before it allocates or replaces
any Domain collection.

## Format V1

The immutable memory format starts with the eight-byte ASCII magic `MSWDOMS1`.
All numeric fields are explicit little-endian values. The header records format
version, header size, total size, Kernel State Hash version, expected Kernel tick,
expected Kernel hash, FNV-1a 64-bit digest, Domain tick, next event ID, collection
counts, and Domain diagnostics.

Payload order is canonical:

1. species by increasing SpeciesId;
2. organisms by increasing Kernel EntityId;
3. food by increasing Kernel EntityId;
4. semantic events by increasing event ID.

The snapshot contains species parameters, organisms, food, events, next event ID,
and diagnostics. It never contains capacities, pointers, addresses, temporary
buffers, test hooks, paths, wall-clock values, Brain Bridge state, or Core state.

## Restore

Restore is transactional:

```text
validate bytes and digest
-> validate Kernel tick/hash
-> build temporary Domain state
-> validate referenced public Kernel entities and WD0 invariants
-> swap state
```

Any failure leaves the logical Domain state unchanged. A successful
capture -> restore -> capture cycle is byte-identical when the Kernel remains at
the bound checkpoint.

## File adapter

`app/wd1_domain_snapshot_file.c` is the only WD1 filesystem adapter. It writes
`<path>.tmp`, flushes it, replaces the destination atomically, and removes the
temporary file after failure. The Domain library itself remains memory-only.

The WD1 demo writes `domain_snapshot_v1.bin` and `wd1_summary.txt` below
`build/worlds/domain/results/wd1_demo/`.

## Hardening boundaries

Event IDs must leave a representable next ID. An event ID of `UINT64_MAX` is
invalid; `UINT64_MAX - 1` followed by next ID `UINT64_MAX` is still
representable. Capture and restore reject `INVALID_STATE` while the bound Kernel
has an active provisional command batch, so Domain and Kernel checkpoints remain
compatible.

`wd1_summary.txt` is written in binary mode with canonical LF line endings on
all platforms. The textual golden therefore remains portable; the binary Domain
Snapshot V1 remains a byte-for-byte artifact.
