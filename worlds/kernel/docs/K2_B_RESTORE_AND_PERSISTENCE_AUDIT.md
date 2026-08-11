# K2-B Restore And Persistence Audit

## Result

K2-B is complete. The frozen V1 snapshot can be imported from external bytes,
validated, saved and loaded by the app layer, and restored into a new Kernel
without changing its deterministic future.

## Evidence

- Import copies caller bytes and does not retain external storage.
- Restore is transactional: candidate allocation, decode, invariant validation,
  state-hash verification and canonical re-capture finish before publication.
- The restored snapshot is byte-identical to the input and has the captured
  official state hash.
- Continuation tests compare independent future PRNG values and each post-tick
  snapshot after restore.
- Checkpoint tests validate V1 at ticks 0, 100, 500 and 1000.
- File round-trip proves `S1 == file == S2 == S3` through the app adapter.
- Malformed headers, version, booleans, counts, hash corruption and truncation
  are rejected without a partial public object; allocation fault injection
  leaves output pointers NULL.
- O0/O2, sanitizer classification and POSIX smoke targets use the same
  memory-only Kernel sources. A platform without a usable sanitizer or POSIX
  compiler reports `UNAVAILABLE`, never a false pass.

## Demo

`k2_snapshot_restore_demo` emits a binary V1 snapshot and `summary.txt` in its
caller-selected output directory. Its summary reports format, byte size,
digest, source/restored state hashes and a `PASSOU` round-trip result. The K2-B
checker verifies those bytes and fields rather than accepting only an exit
code.

## Scope Boundary

K2-B does not introduce replay or persistent command logs. The next official
block is K2-C, deterministic command replay.