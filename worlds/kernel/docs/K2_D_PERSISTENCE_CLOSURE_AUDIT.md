# K2-D Persistence And Replay Closure Audit

## Status

K1: CONCLUIDO
K2-A: CONCLUIDO
K2-B: CONCLUIDO
K2-C: CONCLUIDO
K2-D: CONCLUIDO
K2: CONCLUIDO

Proximo marco oficial: Domain minimo do miniSNN Worlds.

## Frozen Formats

K2-D does not alter either canonical persistence format.

Snapshot Format V1: MSWKSNP1
Command Log Format V1: MSWKLOG1
Command Log V1 header: 40 bytes
Command Log V1 record: 136 bytes

Both formats remain little-endian, canonical and memory-only in the Kernel.
Filesystem access remains in worlds/kernel/app.

## State-Bound Replay

MiniSNNWorldsKernelReplaySession borrows an immutable command log and stores
an expected_state_hash, a cursor and a binding_validated flag.

The cursor is the number of command-log records already reflected in the
fresh or restored Kernel state. Before it accepts the first record, validate
compares the current canonical V5 state hash against the expected hash.
Mismatch returns REPLAY_DIVERGENCE before queue mutation. The cursor advances
only after one successful replayed submission.

The session does not own the log and is not thread-safe. Callers keep the log
alive and serialize access to one session.

Negative coverage verifies mismatch for seed, bounds, supplied initial hash,
checkpoint/run pairing, changed tick, changed next CommandId, changed PRNG
state, malformed record bytes and malformed cursor. The mismatch path preserves
the canonical state hash, queue count and counters; only last_error reports
the divergence.

## Integrated A/B/C Proof

The K2-D demo exercises entities, transforms, blocker occupancy, a spatial
link, subtree movement, lifecycle rejection while linked, occupancy conflict,
overflow movement rejection, unlink/removal, future commands and PRNG draws.

Run A: continuous source execution while capturing Command Log V1.
Run B: fresh equivalent Kernel plus full state-bound replay.
Run C: replay to checkpoint, snapshot save, destroy, load, restore and
checkpoint-bound replay of the remaining records.

Final snapshots A/B/C compare byte-for-byte. This transitively compares
entities, transforms, occupancy, links, pending commands, event history,
diagnostics, counters, IDs and PRNG state.

## Long Run

test_k2_d_persistence.c uses 1000 ticks and 1011 canonical command records.
It captures checkpoints at 0, 100, 250, 500, 750 and 1000, restores each to a
clean Kernel, validates the state-bound cursor and replays the remainder. It
also performs application-level snapshot save/load interruptions at 250, 500
and 750. Every path ends with the same canonical final snapshot.

## Verification

tests/golden/k2_d_persistence_replay_v1.txt freezes only the K2-D demo
summary. It does not replace K2-A, K2-B or K2-C goldens. The checker validates
the historical V1 layouts, demo artifacts, equal final snapshots, equal final
hashes, zero divergences and the frozen summary.

The O0/O2 harness compares all K2-D demo artifacts. Sanitizer and POSIX
harnesses classify unsupported toolchains as UNAVAILABLE; a valid probe
followed by any failure is FAIL.