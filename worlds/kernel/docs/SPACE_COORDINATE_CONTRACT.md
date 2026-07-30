# Space Coordinate Contract

K1-A gives each Worlds Kernel instance one authoritative two-dimensional mathematical space.
The axes are +X east/right and +Y north/up. The origin has no domain meaning.
Screen coordinates, cameras, rendering, and floating point are outside the Kernel.

MiniSNNWorldsKernelScalar is signed int64 fixed point with scale 1000:
one internal unit is 0.001 conceptual units. Bounds and placement use inclusive
comparisons: min_x <= x <= max_x and min_y <= y <= max_y.

Bounds are immutable for the lifetime of an instance. The K0 configuration
prefix remains accepted and receives documented default bounds
[-1000000, 1000000] on both axes. A V1 struct that physically reaches the
complete `space_bounds` field supplies those bounds even when it has an
unknown trailing tail; only a physically truncated field receives defaults.
Arithmetic used by authoritative spatial state is checked before mutation; the
Kernel does not rely on floating point or compiler-specific extended integers.
