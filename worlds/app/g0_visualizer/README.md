# miniSNN Worlds G0-A Visualizer

G0-A is a small Win32 edge application. It reads the real WT0 terrain and WF0
fish scenario state, advances the scenario one tick at a time, and renders that
state without adding visual physics.

Build the user-facing Worlds executable from the repository root:

    mingw32-make worlds
    .\minisnn_worlds.exe

`minisnn_worlds.exe` lives beside the Core's clickable Studio entry point,
`build/studio/bin/minisnn_studio.exe`. It resolves `assets/sprites/` relative
to its own executable path, so it does not depend on the current working
directory. `--repository-root <path>` remains an explicit development override.
Use `--runtime-smoke` to validate startup and WIC asset decoding without
opening a window.

The internal G0 build remains available for development:

    mingw32-make test-g0-a
    mingw32-make demo-g0-a
    mingw32-make -C worlds/app/g0_visualizer all

The required existing assets are resolved below assets/sprites. PNGs are loaded
as premultiplied BGRA and every sprite is composed with per-pixel source alpha.
WATER is terra plus agua at 50% alpha; FISH, FOOD, and ROCK are drawn below that
water overlay. LAND uses gramado.

See docs/G0_A_VISUALIZER_CONTRACT.md and docs/G0_A_VISUALIZER_AUDIT.md.
## G0-C interactive sandbox

The shipped Worlds application now starts with a configurable 20 x 15 sandbox.
Use `EDIT WORLD` to create a 5 x 5 through 64 x 64 map, paint WATER/LAND/ROCK,
place FOOD, choose the fish spawn, and apply the complete blueprint
transactionally. Mouse wheel zoom, WASD/arrow pan, `F` fit, `SPACE` pause, `N`
single-step, and `+`/`-` pacing remain visual/application controls only.

```text
mingw32-make test-g0-c
mingw32-make demo-g0-c
mingw32-make check-g0-c
```

The historical G0-A five-by-five fixture remains available only to its tests and
golden artifacts. See `docs/G0_C_INTERACTIVE_SANDBOX.md` for editor and rebuild
semantics, including the explicit rebuild flow for adding food during a run.
## G0-D product shell

G0-D adds a menu, dirty-world confirmation, blueprint-only world persistence,
persistent application preferences, `R` episode reset, and a bounded structured
event log. Worlds are saved under `worlds/saves/` by default and deliberately do
not serialize live Kernel, Domain, Core, or brain state. Use `mingw32-make
test-g0-d`, `mingw32-make demo-g0-d`, or `mingw32-make audit-g0-d`; see
`docs/G0_D_PRODUCT_SHELL.md` for the format and reset contract.

For a local side-by-side product entry point, use:

```text
mingw32-make publish-worlds
```

This publishes `minisnn_studio.exe` and `minisnn_worlds.exe` at the repository
root for local use while preserving the canonical build artifacts in
`build/studio/bin/`.

## G0-D-H presentation and safe resize

The Settings screen stores water opacity at 25%, 35%, 50%, or 65%; its default is
35%. Presentation Mode temporarily renders water at 25% opacity and scales
native pixel controls and labels for legibility without changing the saved
choice or the simulation. Worlds saved in the V2 format retain both preferences,
while V1 worlds load with 35% opacity and Presentation Mode off.

Changing W or H in the editor is a transactional blueprint resize, not a fresh
world. WATER/LAND/ROCK/FOOD are preserved in the old/new intersection, new
cells begin as WATER, and truncated cells are discarded. A shrink that would
put the fish spawn outside the map is rejected with the blueprint unchanged.
The explicit NEW WORLD action remains the way to start from a blank blueprint.

Validation commands: mingw32-make test-g0-d-settings, mingw32-make test-g0-d-h,
and mingw32-make audit-g0-d.

## Published product parity

The normal worlds target builds Studio plus Worlds and publishes both executables
at the repository root. worlds-app remains an internal build only. The explicit
publish-worlds target is an alias for the same product path. Run
test-worlds-published-product after a build to compare both products byte for
byte and execute the published Worlds runtime smoke.
