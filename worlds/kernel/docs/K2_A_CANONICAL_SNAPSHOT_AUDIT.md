# K2-A Canonical Snapshot Audit

## Result

K2-A adds a public immutable, canonical snapshot blob for the Worlds Kernel.
It captures state but deliberately does not restore it.

## Evidence

- Empty-kernel capture has a valid V1 header and empty sections.
- A populated scenario captures alive and dead entities, optional transforms
  and occupancy, a spatial link, future commands at different ticks and
  priorities, observable events, nonzero counters, and two partially consumed
  PRNG streams.
- Capture is non-mutating: diagnostics and state hash are unchanged before and
  after capture. Allocation injection returns `ALLOCATION`, leaves the output
  NULL, and leaves the Kernel unchanged.
- Two captures and capacity-reservation histories produce byte-identical
  snapshots. O0/O2 and the deterministic long run compare raw bytes.
- Long-run captures occur at ticks 0, 100, 500, and 1000 in two independent
  executions.
- The snapshot inspector rejects bad magic, unknown version, truncated payload,
  extra/mismatched payload length, and impossible count arithmetic.

## Sanitizer And POSIX

The K2-A harness uses the shared probe contract. An unavailable ASan/UBSan or
POSIX toolchain is reported as `UNAVAILABLE`; once a supported sanitizer probe
passes, a project compile or runtime failure is `FAIL`.

## Golden V1

The golden file records format version, blob size, FNV-1a byte digest, and the
captured official state hash. It is independent of K0/K1 state-hash V1-V5 and
does not introduce V6.

## Boundary

K2-A has no filesystem dependency, no load path, no restore path, no replay,
and no pointer serialization. K2-B now provides restore plus app-layer
save/load of this canonical snapshot format; K2-C deterministic replay is next.

## Historical Boundary

K2-A itself intentionally had no restore or filesystem path. K2-B now consumes
the same frozen V1 bytes for import, transactional restore and app-layer file
persistence; it does not change the K2-A golden.
