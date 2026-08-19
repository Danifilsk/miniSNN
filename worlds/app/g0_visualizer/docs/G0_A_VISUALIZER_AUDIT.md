# G0-A Audit Guide

Run from the repository root:

    mingw32-make test-g0-a
    mingw32-make test-g0-a-determinism
    mingw32-make test-g0-a-optimization-determinism
    mingw32-make test-g0-a-sanitize
    mingw32-make test-g0-a-posix-smoke
    mingw32-make demo-g0-a
    mingw32-make check-g0-a
    mingw32-make audit-g0-a

test-g0-a checks PNG loading, the missing-asset path, layer order,
world-to-tile alignment, pause/step/reset/toggles, normal WATER/LAND/ROCK
mapping, and the important integration smoke: the real neural decision selects
MOVE +X, a WT0 ROCK rejects that move, and the rendered fish remains in its
original tile.

The deterministic and O0/O2 gates compare the headless trace and summary, not
framebuffer bytes. Sanitizer and POSIX gates report UNAVAILABLE only when the
local toolchain cannot provide that environment.

## G0-B

    mingw32-make test-g0-b
    mingw32-make test-g0-b-runtime
    mingw32-make test-g0-b-facing
    mingw32-make demo-g0-b
    mingw32-make check-g0-b
    mingw32-make test-g0-b-optimization-determinism
    mingw32-make test-g0-b-sanitize
    mingw32-make test-g0-b-posix-smoke
    mingw32-make audit-g0-b

G0-B adds portable scheduler and facing checks while preserving the G0-A
headless trace determinism gate. Visual inspection of the Win32 application
remains a human validation.