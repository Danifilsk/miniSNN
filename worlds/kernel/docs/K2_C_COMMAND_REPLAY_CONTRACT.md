# K2-C Command Replay Contract

## Scope

K2-C records accepted external Kernel command submissions and replays those
submissions through the same public queue APIs. It is not a snapshot, event
log, diagnostics log, state hash, PRNG trace, network protocol or filesystem
format owned by the library.

The defining property is:

```text
same initial state + same canonical command log = same deterministic future
```

K2-C does not change Snapshot V1 and does not implement K2-D.

## Command Log V1

`MiniSNNWorldsKernelCommandLog` is opaque and caller-owned. Its V1 binary
encoding starts with magic `MSWKLOG1`, uses explicit little-endian fields, and
has a 40-byte header:

```text
magic[8], format_version u32, reserved u32, record_count u64,
payload_size u64, digest u64
```

Each record is exactly 136 bytes. It includes the logical `submission_tick`,
assigned `CommandId`, target tick, priority, issuer, command type, target
entity, presence flags and the complete canonical payload for transform,
displacement, occupancy and spatial-link endpoints. It has no pointers,
padding, capacity, allocator history, filesystem path, event, diagnostic,
derived AABB or PRNG output.

The digest is FNV-1a 64 over canonical header bytes excluding the digest field
and over the complete payload. Imports reconstruct canonical bytes and require
byte-for-byte equality before publishing an immutable log.

## Submission And Application Order

Log records retain real submission order. Records are never sorted after
capture. The Kernel still applies queued commands by:

```text
target_tick -> priority -> issuer EntityId -> CommandId
```

Thus submission order and application order are deliberately distinct.
`submission_tick` is logical time and permits a command for a future target
tick to be replayed at the same point where it was originally accepted.

## Capture And Replay

`command_log_capture_submission()` copies one already accepted pending command
by its assigned CommandId. Logging is optional and does not modify command
IDs, ordering, events, counters, PRNG state or state hash.

`command_log_replay_next()` is incremental. It first requires a ready Kernel,
a matching current tick and `next_command_id` equal to the recorded ID. It then
routes the record through the existing public queue function for every K0/K1
command type. The queue-generated ID must match the record. A timing or ID
mismatch returns `MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE`; no command
is queued in that mismatch case. A whole-log replay is not globally
transactional: successful earlier records remain applied if a later record
diverges.

Semantic rejections occur later during normal tick application and are not
rewritten by replay. Therefore their ordering, EventIds, causal groups,
diagnostics, counters and PRNG evolution are consequences of the same history.

## Snapshot Interaction

Snapshot V1 stores no replay cursor. A caller records the log index that is
already represented by a checkpoint. After restore, replay starts at that
external cursor. K2-C verifies this with a saved checkpoint at tick 2 and a
1000-tick run with a checkpoint at tick 500.

## Malformed And Allocation Handling

Malformed magic, version, reserved bytes, sizes, truncation, extra bytes,
enums, flags, zero IDs, noncanonical payloads and non-monotonic submission
records are rejected before publication. Create, append and import preserve
out-pointer and existing-log ownership on allocation failure. The library has
no file I/O; `app/k2_command_log_file.c` owns atomic save/load and removes a
failed temporary file.

## Golden And Verification

`tests/golden/k2_c_command_replay_v1.txt` freezes the K2-C demo's canonical
format, command-log size and digest, checkpoint cursor and hashes. The checker
compares the demo artifact, not only its exit status. POSIX, sanitizer and
-O0/-O2 gates use temporary directories and compare real canonical artifacts.

K2-C is complete. K2-D closes state-bound replay and the K2 persistence audit.