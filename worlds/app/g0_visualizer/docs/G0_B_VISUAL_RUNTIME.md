# G0-B Visual Runtime

G0-B is a presentation layer over the real WF0, Terrain, Domain, Brain Bridge,
Kernel, and Core state. It adds no simulation rule and never feeds renderer
state back into the world.

## Runtime and layout

`minisnn_worlds.exe` advances the runtime from a Win32 timer and
`QueryPerformanceCounter`. The configured simulation rate is four ticks per
second; painting only reads the current state. The window uses a dark header,
main world viewport, optional debug panel, and footer controls. The map stays
centered in the main viewport at 64 pixels per 1000-unit logical tile.

The header shows `RUNNING` or `PAUSED` and the current tick. The debug panel
shows simulation tick/state/rate, fish tile and world coordinates, energy,
facing, Core step, last action, remaining food, and Terrain/Kernel/Domain
hashes. Water remains the final world overlay at alpha 128.

## Controls

- `ESC`: close the independent Worlds process.
- `SPACE`: pause or resume automatic ticks.
- `N`: execute exactly one WF0 tick while paused.
- `G`: toggle the map grid.
- `D`: toggle the debug panel.
- `R`: reset the deterministic scenario and restore the initial SOUTH facing.

The fish facing is derived only from the last successfully applied cardinal
move. Rejected movement leaves both world position and facing unchanged. The
four display orientations are created in memory from the existing fish PNG;
no extra sprite asset is stored.

## Studio entry and build

Run `mingw32-make worlds` to build these sibling products:

```text
build/studio/bin/minisnn_studio.exe
build/studio/bin/minisnn_worlds.exe
```

The Studio main panel provides `ABRIR MINISNN WORLDS`. It resolves
`minisnn_worlds.exe` beside the running Studio executable and starts a separate
process. The Studio does not link against Terrain, WF0, Domain, or Brain
Bridge. If the executable is absent, the Studio reports the missing product and
asks for the `worlds` target to be rebuilt.

## Automated and manual validation

`test-g0-b` covers the portable scheduler, the pause contract, controls, and
facing transitions. `test-g0-b-runtime` and `test-g0-b-facing` are focused
aliases. The Windows source contract confirms the timer and renderer wiring.
Visual validation remains manual because it requires opening the Win32 window.

## Manual checklist

- [ ] Open `minisnn_studio.exe`.
- [ ] Confirm that `Abrir miniSNN Worlds` is visible.
- [ ] Click it and confirm that `minisnn_worlds.exe` opens.
- [ ] Confirm that the Studio remains open.
- [ ] Close Worlds and confirm that the Studio remains open.
- [ ] Confirm the dark, readable viewport and visibly advancing tick.
- [ ] Confirm SPACE, N, G, D, and R in the Worlds window.