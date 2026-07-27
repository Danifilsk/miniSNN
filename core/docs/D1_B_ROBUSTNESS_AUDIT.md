# D1-B robustness audit

D1-B closes the robustness, determinism, stress and performance pass after the
automated targets complete. It does not complete D1 and does not declare
`miniSNN Core v1.0-rc`.

| ID | Severity | Area | Problem | Correction | Test | Status |
| --- | --- | --- | --- | --- | --- | --- |
| D1B-001 | high | allocation sizes | multiplication could overflow before allocation | checked size arithmetic in network setup | lifecycle stress | corrected |
| D1B-002 | high | cycle counters | tick counters could wrap | preflight checked increments and observable error | lifecycle stress | corrected |
| D1B-003 | medium | neural step | `INT_MAX` could wrap the step counter | reject before update | lifecycle stress | corrected |
| D1B-004 | medium | parser/output | audit config needed strict close/error paths | strict fixed-buffer parser and explicit close paths | corruption and analyzer | corrected |
| D1B-005 | medium | sanitizers | current MinGW may not link ASan/UBSan | standalone probe distinguishes environmental UNAVAILABLE from project FAIL and records each test | sanitizer runner | D1-C follow-up when unavailable |
| D1B-006 | medium | POSIX headless | Windows-only assumptions could hide strict C failures | `localtime_r`, `popen` and `-lm` are checked by a POSIX-only smoke | `test-d1-posix-headless` | environment dependent |
| D1B-007 | medium | audit evidence | fixed PASS tables could outlive a failed executable | CSV artifacts are written only after their binaries return known success evidence | portability harness | corrected |

## Evidence

- `test-d1-determinism`: repeated models, plasticity, C7 isolation and seeded
  topology.
- `test-d1-optimization-determinism`: clean `-O0`/`-O2` executables, including
  nonzero C7 spikes and action variation for LIF, AdEx and HH.
- `test-d1-corruption`: authoritative INI and AgentIO schema mutations.
- `test-d1-lifecycle-stress`: repeated lifecycle, checkpoint, allocation fault
  and near-limit counters.
- `test-d1-long-run`: 1,000,000 LIF, 250,000 AdEx, 100,000 HH, 250,000
  structural steps and 10,000 C7 ticks per model.

The generated local HTML report links raw CSV/TXT evidence. The manifest uses
`audit_format_version=d1_b_v2`, a canonical config signature and explicit
statuses for optimization, POSIX and sanitizer evidence. It is ignored by Git
and contains no absolute paths. D1-C must decide the release gate, resolve an
unavailable sanitizer environment when present, and perform the remaining
human review before any release candidate claim.

`app_filesystem.c` declares `_POSIX_C_SOURCE` before ordinary headers for a
normal POSIX compile. D1-B also passes `-D_POSIX_C_SOURCE=200809L` in the
headless smoke and sanitizer harnesses: a forced header such as
`-include tests/test_allocation.h` can include system declarations before the
source file is reached.
