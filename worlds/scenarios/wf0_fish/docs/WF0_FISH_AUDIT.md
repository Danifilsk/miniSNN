# WF0 Fish Audit

## Focused gates

Run `mingw32-make test-wf0`, `test-wf0-neural-causality`, `test-wf0-neural-continuity`, `test-wf0-determinism`, `test-wf0-optimization-determinism`, `test-wf0-sanitize`, `test-wf0-posix-smoke`, `demo-wf0`, `check-wf0`, or `audit-wf0` from the repository root.

`test-wf0` proves:

- Food at `+X` produces neural `MOVE +X`.
- Food at `-X` produces neural `MOVE -X`.
- A colocated Food is consumed by neural `EAT`.
- move cost, nutrition, and per-tick metabolism are accounted by Domain.
- two equivalent Fish brains have separate Core state.
- two worlds at the same post-move Domain state diverge in their second decision scores when only one receives the explicit reset; normal ticks therefore retain neural state.

`check-wf0` verifies the canonical small summary golden, trace content, hashes, actual Core advancement, and the source contract that prohibits direct perception-to-action conversion or an automatic transient reset in the action loop.

## Determinism and portability

The determinism gate compares two independent demo output directories byte-for-byte. The optimization gate recompiles WF0 at `-O0` and `-O2` and compares both canonical artifacts. POSIX link commands append `-lm` after the Core archive.

Sanitizer and POSIX gates report `PASS`, `FAIL`, or `UNAVAILABLE`. `UNAVAILABLE` is limited to a nonfunctional sanitizer probe or an unavailable POSIX environment; once a sanitizer probe works, WF0 build or execution failure is `FAIL`.