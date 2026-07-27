# Robustness and fault model

D1-B treats malformed text, truncated files, signature mismatches, invalid
dimensions, numeric overflow, allocation failure and counter exhaustion as
expected failure modes. Public loaders and constructors must reject them before
publishing partial state. A previously live object remains usable after a
failed load or creation attempt.

Internal checked arithmetic protects allocation-size multiplication and cycle
counters. The neural step rejects `INT_MAX` before advancing; the C7 cycle
rejects `UINT64_MAX` counters before publishing an action or changing a tick.
The allocation fault injector is compiled only with `MINISNN_TESTING`; the
normal Core library has no test hook or global failure mode.

The sanitizer harness first compiles and runs a standalone allocation probe.
Only a probe/toolchain failure is `UNAVAILABLE`; once the probe passes, a Core
compile, link, test or sanitizer diagnostic is `FAIL`. Execution-derived D1-B
CSV artifacts are atomically replaced only after the corresponding binary
returns its documented success evidence.

For strict POSIX builds, `app_filesystem.c` declares `_POSIX_C_SOURCE` before
its regular includes. D1-B passes the same feature definition on the compiler
command line for project sources in the POSIX smoke and sanitizer harnesses,
so declarations remain available even when a forced test header is included
first.

This audit does not claim protection against hostile-process attacks, arbitrary
memory corruption outside the API, or universal binary-ABI compatibility.
