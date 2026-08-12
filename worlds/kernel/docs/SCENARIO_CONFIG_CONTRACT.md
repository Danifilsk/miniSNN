# K0-D scenario configuration contract

The authoritative K0-D INI parser lives in `worlds/kernel/app/`, never in the
static Kernel library. It accepts only `[kernel]` and `[scenario]`, format
version `1`, decimal unsigned integers, documented peripheral spaces, full-line
`#` or `;` comments, LF or CRLF, and an optional final newline.

Required fields are `config_version`, `master_seed`, `scenario_version`,
`scenario_id`, `ticks`, entity limits, create/destroy intervals and batches,
`command_delay`, `priority_base`, random namespace/stream/draw count, and
`trace_interval`. `scenario_id` is ASCII alphanumeric plus `_` and `-`, with a
maximum of 48 bytes. Files are limited to 65536 bytes, lines to 512 bytes,
ticks to 1000000, live entities to 4096, and batches/draw counts to 1024.

Unknown, duplicate, malformed, signed, overflowing, or incomplete fields are
errors. `minimum_alive_entities` cannot exceed `maximum_alive_entities`, and
initial entities cannot exceed the maximum. Intervals, batches, delay, and
trace interval are positive. Invalid configuration does not create a Kernel or
result artifacts.

`scenario_config_signature` is a separate FNV-1a 64-bit hash over an explicit,
fixed-order normalized encoding. It excludes source path, comments, newline
style, whitespace, and textual key order. It is not the Kernel state hash.

The workload is version 1 and generic: initial creates target tick 1; periodic
creates respect the maximum; periodic destroys select canonical live indices by
the configured public random stream, retain the minimum, and enqueue one
duplicate destroy as a deterministic command-conflict observation. Technical
draws are independent of the entity selection stream. No world-domain meaning
is attached to entities or events.

## K1-B2 demo configuration

`configs/k1_movement_demo.ini` is a small headless application configuration,
not a general command language. It accepts only the documented `[scenario]`
and `[space]` keys, rejects unknown or duplicate keys, and supplies the seed,
tick count and fixed-point bounds used by the deterministic compiled movement
sequence.

## K1-C4 spatial-links demo configuration

configs/k1_spatial_links_demo.ini is an intentionally limited, strict application-layer INI. It accepts only scenario, space, entities, links and commands sections; unknown or duplicate keys fail before the Kernel is created. It is not a general Worlds serialization format. The demo writes the effective canonical form as config_used.ini; save/load and replay remain K2 work.
