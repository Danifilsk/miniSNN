# Determinism contract

## Level 1: required

Within the same executable, code revision, configuration, initial state and
seed, miniSNN Core requires exact reproducibility for the currently versioned
formats: spikes, actions, delivered rewards, weights, topology, structural
events, checkpoints, canonical hashes and scientific outputs. The D1-B matrix
repeats each representative case three times and compares the complete trace.

## Level 2: equivalent builds

For this audited GCC toolchain, the D1-B runner compares independent `-O0` and
`-O2` executables. LIF, AdEx, Hodgkin-Huxley, STDP, R-STDP, structural updates
and the C7 cycle must yield the same canonical trace. Every C7 model must also
emit at least one spike and vary at least one decoded action; equality of two
silent traces is not accepted. No rounding is used to hide a divergence.

## Level 3: toolchains and platforms

Headless compilation and safe rejection of incompatible formats are the
portable goal. A POSIX-only smoke compiles the Core library and D1-B headless
tools with strict C11 and `-lm`; Windows reports `UNAVAILABLE` when no POSIX
toolchain exists. Bit-for-bit equality between GCC, Clang, MinGW and POSIX is
not guaranteed until demonstrated for a specific toolchain matrix. The
persisted formats are versioned; the C binary ABI is not promised across
compilers, architectures or C runtimes.

The contract excludes wall-clock time, file timestamps and machine-dependent
benchmark timing.
