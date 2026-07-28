# Release checklist: miniSNN Core 1.0.0-rc.1

| Gate | Status | Evidence |
| --- | --- | --- |
| D1-A architecture and public API | PASS | `audit-d1-api` |
| D1-B robustness and determinism | PASS | `audit-d1-b` |
| Version and semantic API baseline v2 | PASS | `test-version`, `test-api-baseline` |
| Release build and symbols | PASS | `test-release-build` |
| External consumer | PASS | `test-external-consumer` |
| Core and Studio package integrity, safe extraction and non-visual smoke | PASS | `test-release-integrity`, `test-release-packages` |
| Historical determinism and persistence | PASS | D1-B and C7 tests |
| Local sanitizers | UNAVAILABLE | standalone MinGW probe evidence |
| POSIX compiler smoke | UNAVAILABLE | Windows host evidence |
| Studio visual checklist | PASS | `D1_C_STUDIO_MANUAL_CHECKLIST.md` |

D1-A, D1-B, automated D1-C, and the recorded manual Studio validation establish
`D1: CONCLUÍDO` for `miniSNN Core 1.0.0-rc.1`. The result authorizes K0 as the
next planning block; it does not declare Core 1.0, a public release, Worlds
implementation, or permanent freezing of the Core-Brain Bridge API.

The automated Studio smoke verifies identity and a minimal headless Core
initialization only. The recorded visual checklist remains separate evidence.
