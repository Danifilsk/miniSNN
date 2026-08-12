# WB0 Brain Bridge Audit

## Evidence

WB0 is physically separated in worlds/brain_bridge/ with public include,
source, app, tests, scripts, documentation, and Makefile. The architecture gate
scans production Core, Kernel, Domain, and Bridge sources. It rejects Core or
Domain knowledge of the Bridge and direct Kernel includes or Kernel internals
in the Bridge library.

## Tests

- Encoder tests cover full energy, hunger, east/west/vertical signed deltas,
  clamping, and the no-food zero frame.
- Decoder tests cover all six channels, all-zero fallback, stable ties, and
  EAT without a target.
- Binding tests cover duplicate binding, unknown actor, known food instead of
  organism, nonexistent unbind, and invalid configuration.
- Core integration uses a real 12-neuron MiniSNN brain. Its first sensor neuron
  is also mapped to MOVE_POS_X; high fixed input deterministically yields an
  audited move without any hand-written food-direction controller.
- Cache tests compare minisnn_current_step before and after a repeated same-tick
  decision.
- Multi-organism tests bind two independent brains and verify independent neural
  step counts and caches.
- Failure/retry compiles with the existing Kernel test failure hook. A
  domain_step allocation failure preserves the Domain tick; both retry
  decisions are cache hits and do not advance either brain. The successful
  retry matches a clean run's actions, Core step counts, Domain state hash, and
  Kernel state hash.
- The demo checker validates rows, channel scores, bounded sensor fields,
  action names, tick progression, hashes, and cache evidence.
- The determinism harness compares every demo artifact byte-for-byte across two
  independent runs. The optimization harness rebuilds the WB0 integration
  around the same Core library in -O0 and -O2 and compares the same files.

Sanitizer and POSIX smoke targets use the same PASS, FAIL, or UNAVAILABLE
classification as WD0. An unavailable toolchain is explicit; a valid probe
followed by a failing build or run is a failure.

## Boundary result

The Bridge is an adapter only:

    DomainPerception -> encode -> Core inputs -> Core spikes -> decode -> DomainAction

It never calls the Kernel directly and never applies its action. This preserves
the separation between Domain semantics and neural state.

## Status

The WB0 audit does not declare the organism checkpointable. The next official
milestone is WD1 - persistencia do Domain, followed by WF0 - organismo neural
headless.