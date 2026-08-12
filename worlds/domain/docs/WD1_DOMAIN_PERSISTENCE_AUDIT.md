# WD1 Domain Persistence Audit

## Scope

WD1 adds deterministic persistence for Domain semantic state only. Kernel
Snapshot V1, Command Log V1, and Kernel State Hash V5 remain unchanged.
Brain Bridge and miniSNN Core state are intentionally not persisted.

## Evidence

- `test-wd1-snapshot`: canonical capture, transactional restore, corruption,
  unknown Kernel entity rejection, and cross-Kernel binding rejection.
- `test-wd1-file-roundtrip`: atomic save/load and temporary-file cleanup.
- `test-wd1-determinism`: two independent demos produce identical snapshot bytes.
- `test-wd1-optimization-determinism`: `-O0` and `-O2` produce identical summary
  and snapshot bytes.
- `test-wd1-sanitize` and `test-wd1-posix-smoke`: focused availability-classified
  memory-safety and portability checks.
- `check-wd1`: validates the single representative golden summary and the stored
  FNV-1a digest.

## Binding and safety

A Domain Snapshot stores its expected Kernel tick and State Hash V5. Restore
rejects a different Kernel before Domain allocation or mutation, including a
Kernel that happens to use the same EntityIds and tick. Organism and food IDs are
resolved only through public Kernel APIs during candidate validation.

## Remaining limitation

WD1 supports a persistent Kernel + Domain checkpoint. It is not an integrated
organism/brain checkpoint: Brain Bridge bindings/cache and Core neural state are
outside this format. The next milestone is `WF0 - primeiro organismo neural
headless`.