# Known limitations: 1.0.0-rc.1

- The public API is a candidate, not frozen definitively until D2.
- The Studio is a Win32 frontend. Its D1-C visual validation was recorded
  manually; interactive usability remains a human concern, not a continuous
  automated guarantee.
- The package Studio `--smoke-test` is deliberately non-visual. The separate
  `--runtime-smoke-test` also validates the repository/package layout, default
  evolution resources, runners, scripts, and a writable results directory; it
  still does not validate usability or interactive workflows.
- The current Studio executes complete scenario batches. It has no live pause,
  speed control, or neural one-step interaction.
- Local MinGW ASan/UBSan support is unavailable when its standalone probe lacks
  the sanitizer runtimes. POSIX evidence is recorded separately when available.
- No real Worlds integration, multithreading, installer, digital signature, or
  public release workflow is included.
- LIF, AdEx, and Hodgkin-Huxley need distinct calibration for comparable
  activity. Experimental plasticity, cognition protocols, and evolution do not
  prove general intelligence or guaranteed task learning.
- Legacy evolution remains supported. Historical formats are accepted only when
  their documented compatibility path exists; no universal migration is promised.
- No license file is present in this repository. A licensing decision is a
  blocker for public distribution, though not for local evaluation packages.
- Release ZIP integrity is checked by crossed outer and internal SHA-256 data,
  but those local packages are still evaluation artifacts, not a signed public
  distribution channel.
