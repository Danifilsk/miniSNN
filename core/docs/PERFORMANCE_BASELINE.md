# D1-B performance baseline

`mingw32-make benchmark-d1` executes three local repetitions for neural steps,
structural maintenance, C7 ticks, checkpoint save and checkpoint load. It
writes raw rows to `results/d1_b_robustness/d1_b_performance.csv` and a local
minimum/median/maximum summary.

Timings are machine, compiler, runtime and power-state dependent. D1-B fails
only for objective execution faults such as no completion, zero throughput or
an invalid benchmark artifact. It does not impose a universal milliseconds or
steps-per-second threshold. Existing C3 evolution benchmarking remains the
evidence for generation-level evolutionary cost.
