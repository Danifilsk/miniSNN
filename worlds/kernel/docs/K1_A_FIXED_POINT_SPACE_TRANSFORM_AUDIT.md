# K1-A Fixed-Point Space And Transform Audit

## Scope

K1-A adds one immutable 2D fixed-point space, optional entity transforms,
command-only placement/removal, spatial events, diagnostics, and versioned
state hashing. It remains headless and independent from the miniSNN Core.

## Decisions

| ID | Severity | Area | Problem | Correction | Test | Status |
| --- | --- | --- | --- | --- | --- | --- |
| K1A-1 | high | coordinates | Floating state could diverge | signed int64 fixed point, scale 1000 | scalar/space suite | closed |
| K1A-2 | high | mutation | Immediate setters bypass ticks | placement/removal commands only | transform suite | closed |
| K1A-3 | high | compatibility | K0 hash must remain stable | explicit v1/v2 hash APIs | K0 and K1 hash tests | closed |
| K1A-4 | medium | atomicity | allocation could partially commit transforms | StepPlan copies entity records before apply | allocation fault test | closed |
| K1A-5 | low | performance | ranking hash was cubic in stress | monotonic ID canonical storage invariant | 20000 entity stress | closed |
| K1A-6 | high | config compatibility | larger V1 struct ignored known space_bounds | read the field when its full readable prefix exists | larger-config bounds/tail regression | closed |

## Evidence

The audit covers API headers, bounds compatibility, placement conflicts,
removal, positioned destruction, event payloads, canonical enumeration,
allocation fault atomicity, hash v1/v2, physically allocated legacy/partial/
larger config prefixes, scalar overflow, O0/O2 comparison, moderate stress,
and headless portability/sanitizer probes.

## Limitations And Next Work

K1-A does not model movement, occupancy, collision, barriers, or spatial
queries. At the K1-A milestone, K1-B was planned for minimal shapes, barriers, displacement, destination conflicts, and deterministic collision. K1-B1/B2 and K1-C1..C4 are now complete; K2-A through K2-D are complete and WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block.
