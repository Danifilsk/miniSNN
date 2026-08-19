# G0-C Interactive World Sandbox

G0-C turns the Worlds product into an application-level sandbox. `G0WorldConfig`
stores map dimensions, base terrain, rocks, food placements, and the WF0 fish
spawn. It belongs to `worlds/app/g0_visualizer`, not to Kernel, Domain, Terrain,
or Core.

## Edit and run

The product opens with a 20 x 15 sandbox. Click `EDIT WORLD` to pause the
simulation and edit the blueprint. The editor exposes `NEW WORLD`, dimension
controls from 5 x 5 through 64 x 64, and the tools `WATER`, `LAND`, `ROCK`,
`FOOD`, `ERASE`, and `FISH SPAWN`. The map preview is drawn from the blueprint
before a runtime exists for it.

`APPLY / RUN` validates the full blueprint and builds a candidate Terrain,
Kernel, Domain, WF0 fish, and Brain Bridge. The active runtime is replaced only
after every construction step succeeds. A failed apply retains the prior runtime
and the editable blueprint.

Food is not inserted live into Kernel in G0-C. To change food during an
experiment, enter edit mode, place food, then use `APPLY / RUN`. This is an
explicit deterministic rebuild of the sandbox without closing the executable.

## Camera and pacing

- Mouse wheel: nearest-neighbor zoom from 25% through 400%.
- Arrow keys or WASD: pan the visual camera, clamped to retain the map.
- `F`: fit the map.
- `SPACE`: pause/resume.
- `N`: exactly one real WF0 tick while paused.
- `+` and `-`: select 0.25x, 0.5x, 1x, 2x, 4x, or 8x pacing.

One times speed is four simulation ticks per second. Speed changes only the
wall-clock scheduler; it cannot modify Core, Domain, Kernel, or fish state.
The sandbox runtime tests compare identical tick counts at distinct speeds.

## Rendering

The tile-to-screen geometry uses the active tile size at every zoom level: 25%,
75%, 100%, 200%, and 400% correspond to strides of 16, 48, 64, 128, and 256
pixels. Rendering and hit testing share that conversion, so clicking a visible
tile addresses the same tile in the editor.

Water preserves the medium contract: `WATER` first renders dirt, then rocks,
food, and fish, and finally the water sprite at `SourceConstantAlpha = 128`.
`LAND` renders grass with no water overlay. The debug panel reports the Terrain
API tile index, the fixed-point world position, and the actual action status and
reason returned by Domain.

`WM_PAINT` renders a complete frame into a compatible memory DC and copies it
to the screen with one `BitBlt`. `WM_ERASEBKGND` is handled without erasing the
window background, which avoids inter-layer flashes. Existing assets remain
nearest-neighbor pixel art; no sprite was added.

## Scope and validation

`test-g0-c` covers configuration, editor operations, atomic apply, camera
conversion, pause/single-step, and speed-independent state. The focused Win32
source contract checks double-buffer lifecycle and editor/runtime wiring.
`demo-g0-c` builds a 10 x 10 blueprint and writes a summary below `build/`.

G0-C is a controlled environment tool, not a change to fish behavior or the
neural model. Human validation of rendering, controls, and the Studio launcher
remains required.

## Manual checklist

- [ ] Open `build/studio/bin/minisnn_studio.exe` and see `ABRIR MINISNN WORLDS`.
- [ ] Launch Worlds and confirm Studio stays open.
- [ ] Confirm stable rendering without flicker and an uncropped viewport/debug panel.
- [ ] Create a world, change dimensions, and use every editor tool.
- [ ] Apply the blueprint and confirm the fish, food count, pause, and single-step.
- [ ] Confirm 25%, 75%, 100%, 200%, and 400% zoom have no tile gaps or overlaps.
- [ ] Confirm editor clicks address exactly the rendered tile.
- [ ] Confirm WATER shows dirt under a 50% water overlay after rocks, food, and fish.
- [ ] Confirm debug Tile is the Terrain tile index and rejected actions show Result and Reason.
- [ ] Confirm wheel zoom, camera pan, and `F` fit retain crisp tiles.
- [ ] Change speed with `+` and `-` without changing simulation logic.
- [ ] After food is consumed, enter edit mode, add food, apply, and continue without closing Worlds.