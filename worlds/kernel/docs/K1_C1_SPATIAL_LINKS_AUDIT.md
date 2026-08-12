# K1-C1 Spatial Links Audit

**Status:** PASS after the K1-C1 focused test, determinism, optimizer,
sanitizer-classification, POSIX smoke and regression gates.

## Scope

K1-C1 stores one canonical directed parent-to-child link type. It adds no
subtree propagation, causal extra events, parser, demo, stress integration,
reparenting or save/load.

## Contracts audited

- Public `SpatialLink` and endpoint value structs are aggregate-header visible.
- Commands use `target_entity = parent`; zero endpoints fail at submission.
- The planned link array is copied before command application and promoted only
  with the rest of `StepPlan`; this is the atomic structural-link commit boundary.
- Links remain sorted by `(parent, child)`; each child has at most one parent
  and cycle detection is iterative and bounded.
- Create derives checked offsets without moving endpoints. Remove preserves
  transforms, orientation and occupancy.
- Destroy/remove-from-space reject any incident link. C1 move guards reject a
  child before a root-with-children case.
- C1 emits exactly one event per consumed command. `affected_entity` is always
  zero but is encoded by V5 from its first ABI version.
- V1-V4 retain strict compatibility selection; C1 causal state selects V5.

## Evidence

`test_k1_c1_spatial_links.c` covers public payloads, queries, directed-forest
rejections, lifecycle, movement guards, counters, V5 selection and an injected
planned-link allocation failure, canonical insertion, zero/positive/negative offsets, both offset-overflow axes, and a bounded iterative chain. The optimizer gate compares the test output
under `-O0` and `-O2`. Sanitizer and POSIX probes report PASS only when their
toolchains are genuinely usable; otherwise they explicitly report UNAVAILABLE.

The fixed V5 vector for an active 1 -> 2 link between (0, 0) and (10, 0) is
0xF6C92E0389E076CD.

## Follow-up status

K1-C2 now provides rigid subtree motion and uses nonzero `affected_entity` for
canonical overflow and external-conflict rejections. K1-C3 e K1-C4 foram posteriormente concluidos; este documento preserva apenas o escopo C1.
unimplemented.