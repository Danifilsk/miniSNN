# WF0 Fish Contract

## Scope

WF0 is a composition-root scenario. `core/`, `worlds/kernel/`, `worlds/domain/`, and `worlds/brain_bridge/` remain generic. The scenario uses only their public headers.

## Fish Species V1

| Field | Value |
|---|---:|
| species_id | 1 |
| max_energy | 100 |
| initial_energy | 50 |
| metabolism_per_tick | 1 |
| move_energy_cost | 2 |
| eat_range | 0 |
| food nutrition | 15 |

The demo starts Fish at `(0, 0)` and Food at `(1000, 0)`. Movement uses the Bridge action step `(1000, 0)`.

Domain owns action legality and energy semantics. A valid move costs `2`, every Domain tick costs `1`, and a valid eat adds `15` before that tick metabolism.

## Fish Brain V1

The brain is a fixed, deterministic 12-neuron LIF network. It has no plasticity, learning, reward, or hidden controller. It remains continuous between world ticks.

- Sensor neurons: `0..5`, using the six WB0 sensor channels.
- Output neurons: `6..11`, matching `WAIT`, `MOVE +X`, `MOVE -X`, `MOVE +Y`, `MOVE -Y`, `EAT`.
- Decision window: 8 Core steps.
- Input gain: 5000.
- Perception scale: 1000.

Fixed outgoing connections:

| Source sensor | Output | Weight | Purpose |
|---|---|---:|---|
| food_present | MOVE -X | +200 | gives a left fallback when Food is present |
| food_present | EAT | +1000 | dominates residual movement activity once Food is colocated |
| food_dx | MOVE +X | +400 | positive `food_dx` selects right movement |
| food_distance | EAT | -1000 | inhibits eating while Food is distant |

The distance sensor neuron is inhibitory. The public Brain Bridge encodes Domain perception, advances the real Core, counts output spikes, decodes the selected output, and creates the Domain action. The scenario does not inspect a perception to select or rewrite an action.

The normal world loop never calls `minisnn_reset_transient_state`. `wf0_fish_world_reset_brain` remains an explicit manual scenario operation for tests or an intentional scenario reset; it preserves the Core step counter but clears transient neural dynamics.

The deliberately small V1 topology demonstrates opposite X directions plus `MOVE -> EAT`; it does not attempt complete navigation intelligence.

## Observability

`wf0_fish_trace.csv` records pre-decision perception, the actual Bridge sensor frame, output scores, selected neural channel, action result, energy accounting, Core step range, Food count, and Kernel/Domain hashes. Direct perception is isolated in `src/wf0_fish_observation.c`; the action loop in `src/wf0_fish.c` is `BrainBridge.decide -> Domain.step`.

## Persistence boundary

WD1 can persist Kernel plus Domain. WF0 does not yet persist a Core brain state or Brain Bridge cache, so neural organisms do not support an integrated full checkpoint in WF0.

## Intentional limits

WF0 has no terrain, WATER/LAND/ROCK semantics, sprites, renderer, window, predators, damage/death, reproduction, genetics, learning, reward, communication, or evolution. It is an abstract empty fixed-point space containing Food.