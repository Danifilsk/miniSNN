# K2-C Command Replay Audit

## Result

K2-C completes canonical command logging and deterministic replay on top of
K2-A snapshots and K2-B restore/persistence. Command records cover every
public K0/K1 queue operation: lifecycle, placement/removal from space,
occupancy set/clear, movement and spatial-link create/remove.

## Proof Surface

- Functional replay records submissions in a deliberately different order from
  application ordering and reproduces accepted commands and semantic
  rejections through the public API.
- The functional workload includes occupancy conflict, movement, lifecycle
  interaction with a link, link removal and command ordering variation.
- The same logical PRNG draw is consumed between submissions in source and
  replay. Final canonical snapshots compare byte-for-byte, so state hash,
  events, diagnostics, counters, IDs and PRNG state are equal.
- A checkpoint stores an external record cursor. Snapshot import, restore and
  replay of only later records produce the same final snapshot as the
  uninterrupted source run.
- The long run records 1000 submissions, compares snapshots at ticks 100, 500
  and 1000, and restores from the tick-500 checkpoint before replaying the
  remaining records.
- Bad timing or CommandId produces `REPLAY_DIVERGENCE` before a command is
  queued. Malformed bytes and allocation failures do not partially publish a
  log.

## Canonical Boundary

The in-memory command-log implementation has no filesystem calls. The app
adapter writes and loads the exact canonical bytes with a temporary file and
atomic replacement. `check_k2_c.py` verifies that boundary, V1 artifact fields
and the frozen golden.

## K2-B Residual Hardening Closed Here

K2-B restore now rejects a duplicate pending CommandId globally, not only among
adjacent ordered commands. Restore invariants enforce the exact
relationships between lifecycle counters, live counts, submitted/pending/
resolved commands, spatial-link counts and next IDs. The file round-trip test
releases reused output ownership; the demo releases final snapshots; file save
cleans `.tmp` after failed replacement; and POSIX/sanitizer harnesses use their
own temporary directories while including demo/checker coverage.

## Status

```text
K1:   CONCLUIDO
K2-A: CONCLUIDO
K2-B: CONCLUIDO
K2-C: CONCLUIDO

Proximo bloco oficial:
K2-D - auditoria final de persistencia e fechamento de K2
```