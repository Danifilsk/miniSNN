# K0-D artifact contract

The headless runner publishes exactly `trace.csv`, `events.csv`,
`manifest.ini`, and `report.txt`. These are technical deterministic artifacts,
not a future player log or replay format. They use ASCII, `\n` newlines, fixed
headers, decimal integers, and fixed uppercase hexadecimal `0x` state hashes.
They contain no timestamps, host data, absolute paths, pointers, or durations.

`trace.csv` begins at tick 0 after initial commands are queued, is strictly
increasing, follows `trace_interval`, and always includes the final completed
tick. It records hash, entity, command, event, PRNG, and version counters.
`events.csv` records every consumed Kernel event in emission order using numeric
enums and monotonic event IDs/ticks.

`manifest.ini` records normalized scenario identity, Kernel versions, seed,
initial/final hashes, requested/completed ticks, counts, FNV-1a file signatures,
and `status=OK`. `report.txt` is a deterministic human summary of the same
facts. File signatures are non-cryptographic integrity checks.

All four artifacts are validated semantically as one set. The manifest and
report require their exact fixed title or key set, one occurrence of every
required key, canonical values, a final `\n`, and no blank, extra, duplicate,
or unknown lines. The validator cross-checks report fields against the manifest,
the trace initial/final hashes and final diagnostics, event ordering, and the
FNV-1a signatures calculated from the CSV bytes. A changed decimal signature
must therefore differ numerically, not merely in formatting.

The runner writes `.tmp` files in the selected output directory, closes them,
validates headers, row counts, final tick/hash, monotonic event ordering, and
file signatures, then publishes fixed final names. Without `--overwrite`, an
existing final artifact rejects the run before execution. With `--overwrite`,
existing finals are held as `.bak` during best-effort replacement and restored
when publishing fails. Directory checks use portable `S_ISDIR`. Multi-file replacement cannot be fully transactional on
all filesystems; known temporaries are removed and prior finals are preserved
where the filesystem permits rename restoration.
