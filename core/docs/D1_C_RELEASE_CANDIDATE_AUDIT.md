# D1-C release-candidate audit

Status: **D1-C: CONCLUÍDO** for `miniSNN Core 1.0.0-rc.1`.

- D1-C automated validation: PASS
- D1-C manual Studio validation: PASS

Automated validation and the documented manual Studio validation both passed.
This is not a public release declaration.

| ID | Severity | Area | Problem | Correction | Test | State |
| --- | --- | --- | --- | --- | --- | --- |
| D1C-001 | high | identity | version needed one canonical source | public version header/source and test | `test-version` | PASS |
| D1C-002 | high | public API | candidate surface needed a reviewable baseline | semantic v2 baseline with complete declarations | `test-api-baseline` | PASS |
| D1C-003 | high | delivery | consumers needed release-only inputs | isolated release tree and external example | `test-external-consumer` | PASS |
| D1C-004 | medium | packages | Core and Studio need separate artifacts | deterministic ZIPs, crossed checksums/manifest, safe extraction and Studio smoke | `test-release-packages` | PASS |
| D1C-005 | medium | environment | local sanitizer/POSIX support may be absent | explicit evidence, never a synthetic pass | D1-B harnesses | UNAVAILABLE when unsupported |
| D1C-006 | high | presentation | GUI remains a human usability gate | documented manual checklist | manual | PASS |

The Core package contains headers, static library, external example, and
release documentation. The Studio package contains a relative launcher and its
documented runtime dependencies. Build metadata does not affect scientific
configuration signatures, checkpoints, or public version values.

`PUBLIC_API_BASELINE_1_0_RC.txt` uses `baseline_format=minisnn_public_api_v2`.
It records complete semantic declarations, excluding formatting, comments,
includes, include guards, and `MINISNN_TESTING` content. Package validation
recalculates the outer ZIP hashes and requires exact agreement between the
payload, internal checksums, and manifest before safe extraction. The Studio
`--smoke-test` is non-visual and automatic; it does not replace the human GUI
checklist.

D1 differs from D2: D1 provides a provisionally stable candidate Core-Brain
Bridge API; D2 audits it after real integration and may freeze Bridge v1
definitively.
