# K0-D integrated scenario and stress audit

| ID | Severity | Area | Problem | Correction | Test | Status |
| --- | --- | --- | --- | --- | --- | --- |
| K0D-001 | high | boundary | Kernel could absorb file concerns | INI and artifacts live in `app/` | external consumer and symbol audit | PASS |
| K0D-002 | high | configuration | textual variation could change execution | strict parser and normalized signature | config and determinism | PASS |
| K0D-003 | high | artifacts | partial output could appear authoritative | temp validation then publish | artifacts and stress | PASS |
| K0D-004 | high | determinism | process or optimizer changes could diverge | byte comparisons across processes and O0/O2 | determinism tests | PASS |
| K0D-005 | medium | robustness | malformed configuration could start work | reject corpus before Kernel/output | corruption test | PASS |
| K0D-006 | medium | lifecycle | long counters and queues needed evidence | 1000000-tick deterministic profile | long-run test | PASS |
| K0D-007 | medium | portability | toolchain sanitizer availability varies | PASS/FAIL/UNAVAILABLE probe classification | sanitizer target | PASS or UNAVAILABLE |
| K0D-008 | medium | portability | `S_IFDIR` is not portable under strict C11 | portable `S_ISDIR` directory check | POSIX C11 smoke | PASS or UNAVAILABLE |
| K0D-009 | high | integrity test | leading-zero signature mutation could preserve its decimal value | explicit different numeric signature mutation | artifact corruption test | PASS |
| K0D-010 | high | artifacts | report existed without semantic validation | strict report parser and manifest cross-check | artifact corruption test | PASS |
| K0D-011 | high | artifacts | manifest accepted duplicate or unknown keys | exact-key parser with duplicate rejection | artifact corruption test | PASS |

`k0_integrated_demo.ini` exercises initial and periodic create/destroy commands,
generic conflict rejection, empty ticks, streams, draws, trace, event CSV,
manifest, and report. `k0_stress.ini` is the 50000-tick automated stress
profile. `k0_long_run.ini` runs 1000000 ticks with a bounded live set and
sparse trace.

K0-D does not add transforms, space, snapshots, replay, Core integration,
Domain, agents, graphics, or world semantics.

The automated stress profile runs 50000 ticks and the long profile runs
1000000 ticks with intervalled trace. Their elapsed times are measured only by
the local test harness and never enter canonical artifacts. The POSIX smoke
compiles the Kernel source plus all K0-D app modules, runner, and artifact
validator from an empty temporary directory when a POSIX C11 compiler target is
available; a missing POSIX target is reported as `UNAVAILABLE`, while a project
compile failure is `FAIL`.
