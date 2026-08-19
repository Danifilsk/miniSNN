# WF0 Fish

WF0 is the first concrete, headless neural organism in miniSNN Worlds. It composes only public Core, Kernel, Domain, and Brain Bridge APIs; none of those generic layers knows the Fish type.

## Build and run

From the repository root, run `mingw32-make demo-wf0`, then `mingw32-make check-wf0` or `mingw32-make audit-wf0`.

The deterministic outputs are written to `build/worlds/scenarios/wf0_fish/results/wf0_fish_demo/`: `wf0_summary.txt` and `wf0_fish_trace.csv`.

## Demo story

A Fish begins at `(0, 0)` with Food at `(1000, 0)`.

1. The Bridge encodes perception and the fixed LIF network selects `MOVE +X`.
2. Domain applies movement and metabolism.
3. A new neural decision selects `EAT`.
4. Domain consumes Food, adds nutrition, then applies metabolism.
5. With no Food remaining, the neural output resolves to `WAIT`.

The brain remains continuous across world ticks; only an explicit scenario reset clears transient neural state. The scenario is deterministic and has no plasticity, learning, reward, terrain, rendering, predators, death, reproduction, genetics, or evolution.

See [the full contract](docs/WF0_FISH_CONTRACT.md) and [audit guide](docs/WF0_FISH_AUDIT.md).