# G0-A Visualizer Contract

G0-A lives in worlds/app/g0_visualizer/ and is an application consumer of the
public Terrain, WF0, Kernel, Domain, Brain Bridge, and Core APIs. None of those
layers include or depend on the renderer.

## Scene and layers

Logical world units stay in WT0/WF0. One 1000-unit tile is rendered as
64 x 64 pixels, and ROCK, FOOD, and FISH use centered 32 x 32 pixel sprites.
The Win32 renderer decodes PNGs as premultiplied BGRA and composes every sprite with AlphaBlend using AC_SRC_ALPHA. Nearest-neighbor scaling is used by the Win32 renderer.

The draw order is fixed:

1. base terrain: WATER uses assets/sprites/terrain/terra.png; LAND uses
   assets/sprites/terrain/gramado.png;
2. terrain objects: ROCK uses assets/sprites/objects/pedra.png;
3. entities: FOOD uses assets/sprites/objects/comida.png; FISH uses
   assets/sprites/creatures/peixe.png;
4. WATER overlay: assets/sprites/terrain/agua.png at alpha 0.50;
5. grid and debug/UI.

Thus submerged ROCK, FOOD, and FISH are below the water overlay. Collision and
movement come only from the WT0-materialized Kernel and WF0 Domain action.

## Controls

- ESC: close the viewer.
- SPACE: pause or resume.
- N: execute one real WF0 tick while paused.
- G: toggle the tile grid.
- D: toggle the debug overlay.
- R: rebuild the deterministic scenario.

The overlay displays tick, paused/running state, fish position and energy, last
action, remaining food, Kernel hash, Domain hash, and Core step.

## Limits

G0-A deliberately has no smooth interpolation, swim animation, audio,
particles, shaders, editor, mouse tools, inventory, complex interactions,
visual perception, or additional terrain classes. The fish still moves one
logical tile per WF0 action, so its display is discrete.

## Executable and assets

`mingw32-make worlds` builds `build/studio/bin/minisnn_worlds.exe`, next to the clickable Core Studio executable. The Worlds executable resolves repository assets from its own path; it never silently falls back to the current working directory. `--repository-root` is an explicit developer override.

## G0-B visual runtime

G0-B keeps the G0-A rendering contract and adds a dark, readable world-first
layout, automatic four-ticks-per-second scheduling, and in-memory cardinal fish
orientations. See `G0_B_VISUAL_RUNTIME.md` for the G0-B controls, Studio
launcher behavior, automated gates, and the manual checklist.