# miniSNN Worlds Brain Bridge

worlds/brain_bridge/ is the WB0 C11 integration layer. It is the only library
layer that translates between the public miniSNN Core API and the public Worlds
Domain API.

The Bridge owns its binding table and same-tick decision cache. The caller owns
the MiniSNNWorldsDomain and each caller-supplied MiniSNN brain; a bound brain
must outlive its binding. The Bridge never destroys either object. One Bridge
accepts bindings from one Domain at a time: the first successful bind establishes
that Domain, mismatched Domain pointers fail without touching cache or brains,
and unbinding the final actor releases the association.

## Boundary

    DomainPerception
      -> WB0 Sensor Contract V1
      -> Core public external inputs
      -> minisnn_step decision window
      -> Core spike outputs
      -> WB0 action decoder
      -> DomainAction

The Bridge library includes minisnn.h and minisnn_worlds_domain.h only. It does
not include Kernel headers directly and never queues Kernel commands, moves
entities, consumes food, or calls domain_step. The top-level demo is a
composition root: it creates a Core brain and a Domain world, asks the Bridge
for an action, then asks the caller-owned Domain to apply that action.

## Build and audit

    mingw32-make test-wb0-core-integration
    mingw32-make test-wb0-failure-retry
    mingw32-make demo-wb0
    mingw32-make check-wb0
    mingw32-make audit-wb0

The deterministic demo writes only below build/worlds/brain_bridge/results/wb0_demo:

- summary.txt
- sensor_frames.csv
- neural_outputs.csv
- decisions.csv
- world_hashes.csv

## Scope and limits

WB0 implements no concrete species, learning, reward, evolution, reproduction,
health, damage, death, renderer, or integrated persistence. Kernel and Domain persistence exist independently, but Bridge runtime and Core neural
persistence are not integrated. A brain-controlled organism is therefore not
checkpointable as a complete world/brain unit after WD1.

The next official milestone is WF0 - organismo neural headless.

See docs/WB0_BRAIN_BRIDGE_CONTRACT.md and docs/WB0_BRAIN_BRIDGE_AUDIT.md.