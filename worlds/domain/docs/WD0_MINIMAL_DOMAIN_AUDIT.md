# WD0 Minimal Domain Audit

## Evidence

- `test-wd0-domain`: species, organism, food, derived hunger, and Domain Hash V1.
- `test-wd0-actions`: accepted move, Kernel-rejected move, EAT, unavailable EAT,
  deterministic food competition, movement cost, and metabolism.
- `test-wd0-perception`: Manhattan nearest-food query and EntityId tie-break.
- `test-wd0-invariants`: duplicate semantic entity, unknown species, excess
  energy, invalid nutrition, missing Kernel entity, tick divergence, duplicate
  event id, counter mismatch, and invalid event kind.
- `test-wd0-atomicity`: injected Kernel-step failure, provisional rollback, no ghost commands, and retry equivalence.
- `test-wd0-stress`: 256 organisms, 512 foods, 200 deterministic ticks, with forced occupancy rejection and forced same-food competition.
- `test-wd0-determinism`: compares every demo artifact byte for byte.
- `test-wd0-optimization-determinism`: compares O0 and O2 artifacts.
- `test-wd0-sanitize`: after a real probe covers registration, actions, perception, invariants, atomicity, reduced stress, sanitized demo and checker.
- `test-wd0-posix-smoke`: strict C11 smoke over the same representative cross-layer paths where POSIX is available.

The demo writes `summary.txt`, `domain_entities.csv`, `domain_actions.csv`,
`domain_events.csv`, `perceptions.csv`, `kernel_hashes.csv`, and
`domain_hashes.csv`. `check-wd0` parses these files and checks semantic
outcomes, rather than accepting only a process exit code.

## Limits and next step

WD0 proves `Kernel Entity + Domain semantics = organism or food` without
teaching the Kernel those words. It does not prove a neural agent or a game.

Kernel persistence exists; Domain persistence is intentionally absent. The next
milestone is `WB0 - Brain Bridge minimo`, followed later by `WF0 - organismo
headless`. WD0 does not implement fish, predators, rendering, or an SNN.
## Closure hardening

The final WD0 stress workload records at least one real occupancy-rejected MOVE
and one deterministic food-competition rejection. Its closure hash is
`0x55BA5DE093CC61A4` for 256 organisms, 512 foods and 200 ticks. The canonical
demo remains unchanged: Kernel `0xFC81CCDD0490E6FC`, Domain
`0xD485AD8521E2DC3D`.
