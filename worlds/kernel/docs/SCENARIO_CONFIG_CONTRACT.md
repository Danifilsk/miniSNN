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
