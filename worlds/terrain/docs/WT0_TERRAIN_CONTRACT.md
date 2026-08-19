# WT0 Terrain Contract

Terrain Grid V1 is finite and fixed-point. A configuration has width, height, tile_size, origin_x, and origin_y; tile size is strictly positive.

WATER is the default navigable base tile. LAND is a blocking base tile. ROCK is a one-tile blocking object stored separately from base tile state. A rock can therefore sit over WATER or LAND. During materialization, a LAND with a rock receives a single ROCK blocker to avoid overlapping generic Kernel blockers. Terrain blockers use a generic Terrain category with `blocking_mask = UINT32_MAX`, so every valid Kernel occupancy category is blocked. Materialization is published transactionally: a failure leaves the supplied Kernel unchanged and the Terrain reusable for a clean retry.

Out-of-bounds queries return OUT_OF_BOUNDS. The map bounds returned by Terrain must be used for the Kernel so a move ending outside the map is rejected by the ordinary Kernel spatial contract.

No Core, Domain, or Brain Bridge production code knows Terrain. The WF0 smoke is test-layer composition only: the real fish brain chooses MOVE and the generic Kernel rejects that movement against a Terrain ROCK.
