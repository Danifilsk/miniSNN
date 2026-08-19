# G0-D Product Shell

G0-D turns the interactive G0-C sandbox into a small deterministic product
shell. It does not add training, persistence of live Core state, or a new
simulation model.

## Application states

`minisnn_worlds.exe` opens on a main menu with **New World**, **Load World**,
**Open Sandbox**, **Settings**, and **Exit**. The sandbox remains a separate
state, with its existing edit/run substate. Returning to the menu, loading a
world, creating a world, and exiting ask whether a dirty blueprint should be
saved, discarded, or left unchanged. Opening the editor preserves that blueprint until an explicit edit and APPLY / RUN.

## Saved worlds

World files use the textual `miniSNN Worlds World Config V2` format and belong
by default in `worlds/saves/`. They contain only a reproducible blueprint:

- world name and dimensions;
- WATER/LAND terrain;
- ROCK, FOOD, and fish-spawn placement;
- presentation/scheduler settings: default speed, start-paused, grid, debug,
  initial zoom, event-log enablement, water opacity, and Presentation Mode.

They deliberately do not contain pointers, Win32 handles, snapshots, Kernel or
Domain dynamic state, Core state, neural traces, or a live brain. Loading first
parses and validates a candidate document and constructs a new runtime; the
active runtime is replaced only after that succeeds.

## Episode reset

`R` resets the current episode from its immutable blueprint. The runtime is
rebuilt with the same terrain, objects, food, spawn, WF0 brain configuration,
and saved preferences. Dynamic state returns to the runtime's initial state:
the product-level episode tick is 0, so the next simulation tick is 1.
The technical Domain tick remains visible separately because setup commands may
advance it before the first episode update. Reset is not a save operation and
does not change the world blueprint.

## Event log

The in-memory bounded log records real `SYSTEM`, `ACTION`, `FOOD`, `ENERGY`,
and `ERROR` observations. It has a collapsible UI panel, clear action, and CSV
export. Exported columns are:

`sequence,tick,category,actor,event,result,reason,text`

No speculative neural, reward, training, or death events are fabricated. The
displayed WF0 identity is `WF0 Fixed Brain V1`, with 12 neurons and plasticity
off.

## Build and validation

From the repository root:

```text
mingw32-make worlds-app
mingw32-make test-g0-d
mingw32-make demo-g0-d
mingw32-make check-g0-d
mingw32-make audit-g0-d
mingw32-make publish-worlds
```

`worlds-app` creates `build/studio/bin/minisnn_worlds.exe`. `publish-worlds`
copies the Studio and Worlds executables side by side to the repository root
for the development product entry point. The executable resolves assets from
the repository root next to it, or from the packaged layout, without relying
on the process working directory.

Manual validation remains necessary for the native menu, dialog focus, dirty
prompt choices, scroll behavior, and rendering.

## Tick semantics

The product exposes an episode tick separately from the technical Domain tick.
A clean or reset episode has episode tick = 0; its next simulation tick is 1.
Event-log tick values are episode ticks, while the header continues to show the
technical Domain tick for debugging.

## Presentation and editor resize

Water opacity is a presentation preference with the stored choices 25%, 35%,
50%, and 65%; 35% is the default. Presentation Mode uses an effective 25%
water opacity and larger terminal-style controls, but never changes the stored
opacity, saved blueprint, or deterministic simulation state. V1 world files
remain loadable and receive the historical-compatible defaults: 35% opacity and
Presentation Mode off.

The editor W/H steppers perform a transactional resize of the editable
blueprint. The intersection keeps terrain, ROCK, and FOOD exactly; newly added
cells are WATER; cells outside a shrink are removed. Resize validates the fish
spawn before committing, so a shrink that would remove the spawn fails and
leaves the complete blueprint unchanged. The explicit NEW WORLD command is the
separate blank-blueprint action.

The header geometry is computed from one shared layout contract. MENU, SAVE,
LOAD, LOG, and EDITING are verified disjoint in normal and Presentation Mode.

## Published executable parity

worlds-app creates only the internal Worlds executable. The normal worlds target
builds Studio plus Worlds and publishes both executables at the repository root;
publish-worlds is an explicit alias. test-worlds-published-product verifies byte
identity and SHA-256 for internal and published Studio/Worlds executables, then
runs the published Worlds smoke. The smoke reports the V2 world format, water
opacity setting, Presentation Mode, and transactional resize support.
