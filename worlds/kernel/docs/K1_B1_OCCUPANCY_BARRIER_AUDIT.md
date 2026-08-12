# K1-B1 Occupancy And Barrier Audit

| ID | Severity | Area | Problem | Correction | Test | Status |
|---|---|---|---|---|---|---|
| K1B1-001 | high | fixed-point envelope | overflow or out-of-bounds envelope | checked add/subtract and envelope validation | occupancy suite | closed |
| K1B1-002 | high | overlap | edge contact treated as penetration | strict positive-overlap AABB predicate | occupancy suite | closed |
| K1B1-003 | high | barriers | asymmetric masks could be ignored | either-side mask/category rule | occupancy suite | closed |
| K1B1-004 | high | determinism | conflict subject depended on storage | ascending Entity ID linear scan | determinism and stress | closed |
| K1B1-005 | high | atomicity | allocation failure could partially apply commands | planned entity/event preflight | occupancy suite | closed |
| K1B1-006 | high | sanitizer gate | project compilation failure could be classified as UNAVAILABLE | independent ASan/UBSan probe followed by authoritative project compilation | sanitizer classification regression | closed |
| K1B1-007 | medium | stress | spaced placements did not produce real occupancy conflicts | dedicated positive-overlap blocking phase | repeated conflict stress | closed |
| K1B1-008 | medium | observability | occupancy changes were opaque | structured events, counters, v3 hash | demo and checker | closed |

K1-B1 covers optional AABB occupancy, category bits, generic blocking masks,
command-only SET/CLEAR, placement integration, removal and destruction
lifecycle, canonical related_entity, diagnostics, and state hash v3.

Published vectors:

- K0 v1: 0xE30082CBF59AFE21;
- K1-A v2: 0xDAA66E4FFBC693C9;
- K1-B1 v3 demo: 0x9D8B5539282DF587.

Sanitizer UNAVAILABLE is emitted only when the independent probe proves that
ASan/UBSan compilation or runtime is unavailable. Once that probe succeeds,
project compilation, execution, sanitizer reports, and leaks are authoritative
FAIL conditions. The stress suite now records 1000 deterministic, positive-area
blocking conflicts before continuing with CLEAR, REMOVE, DESTROY, enumeration,
and final hash checks.

The implementation deliberately retains a linear conflict scan for
auditability. K1-B2 closes displacement and movement; K1-B
as a whole remains in progress.