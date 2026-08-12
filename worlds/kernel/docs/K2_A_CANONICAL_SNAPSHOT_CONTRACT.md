# K2-A Canonical Snapshot Contract

## Scope

K2-A freezes the complete official Worlds Kernel state in an immutable,
memory-only snapshot. It does not itself load, restore, save, replay, compress, or
interpret a snapshot as a Kernel. K2-B consumes this frozen representation for
restore and app-layer persistence.

The central contract is:

```text
same official Kernel state -> identical snapshot bytes
```

The format is independent of pointers, allocator capacity, struct padding,
host endianness, filesystem state, wall clock, and test-only fault injection.

## Public API

`minisnn_worlds_kernel_snapshot_capture()` allocates one caller-owned,
immutable `MiniSNNWorldsKernelSnapshot`. `destroy()` releases it;
`format_version()`, `size()`, and `data()` expose only immutable metadata and
bytes. Capture never changes the Kernel. On failure `out_snapshot` is NULL and
the Kernel remains unchanged.

## Format V1

All integers are encoded little-endian with explicit `u8`, `u16`, `u32`,
`u64`, `i64`, and boolean writers. Signed fixed-point scalars use their
portable two's-complement `uint64_t` representation. No struct image is
serialized.

Header, exactly 40 bytes:

| Offset | Field | Encoding |
|---:|---|---|
| 0 | magic `MSWKSNP1` | 8 bytes |
| 8 | snapshot format version | u32, V1 |
| 12 | Kernel config schema version | u32 |
| 16 | captured state-hash version | u32 |
| 20 | reserved | two zero u16 values |
| 24 | payload size | u64 |
| 32 | captured state hash | u64 |

The payload sections are fixed in this order: semantic config and space,
tick/lifecycle/last error, next IDs, cardinalities, cumulative counters,
entities, spatial links, pending commands, last-tick events, and PRNG streams.
Optional transform, occupancy, command payload, and event payload data are
preceded by explicit booleans.

## State Audit

| State | Snapshot? | Reason | Encoding |
|---|---|---|---|
| config schema, master seed, bounds | yes | affects future validation/randomness/space | u32, u64, four i64 |
| tick, lifecycle, last error | yes | official observable state and future tick | u64, u32, u32 |
| next entity/command/event IDs | yes | future identity allocation | three u64 |
| entity records, alive/dead history | yes | lifecycle and future command results | canonical ID order, flags, ticks, optional payloads |
| transforms and occupancy | yes | future movement, occupancy and links | explicit presence plus i64/u32 fields |
| spatial links and offsets | yes | subtree movement and graph legality | parent/child canonical order plus offsets |
| pending commands | yes | changes future ticks and ordering | canonical target/priority/issuer/ID order and payload flags |
| last-tick events | yes | official observable state after capture | semantic event order and optional payloads |
| diagnostics counters | yes | official observable/persistent state | eighteen u64 counters and cardinalities |
| PRNG streams | yes | exact next random values | canonical key order, state, sequence, draw count |
| capacities, StepPlan, BFS buffers, pointers | no | allocator/temporary implementation detail | excluded |
| filesystem, wall time, Studio/Core state | no | outside Worlds Kernel semantics | excluded |

## Canonicalization And Limits

Entities are emitted by entity ID, links by `(parent, child)`, pending commands
by `(target_tick, priority, issuer, command_id)`, and streams by
`(namespace_id, stream_id)`. Last-tick events retain semantic emission order.
The encoder emits only counts, values, and presence flags, never vector
capacities or addresses.

All snapshot size additions and multiplications are checked before allocation
and while encoding. Allocation failure or overflow returns an error without a
partial snapshot and without changing the Kernel.

## Verification And Golden

The test-only inspector validates magic, V1, header/payload length, minimum
section lengths, and impossible counts. It does not restore a Kernel. The V1
golden is in `tests/golden/k2_a_snapshot_v1.txt`; it records a digest of bytes,
not a new Kernel state-hash version.

K2-A is memory-only. K2-B reuses this V1 byte format for validation and
restore rather than introducing a parallel representation.

## V1 Implicit Semantics

Snapshot Format V1 implicitly fixes `MINISNN_WORLDS_KERNEL_PRNG_VERSION = 1`
and `MINISNN_WORLDS_KERNEL_SCALAR_SCALE = 1000`. These values are guarded by
K2-B compile-time assertions. Changing either requires a new snapshot format;
V1 bytes must never be silently reinterpreted.
