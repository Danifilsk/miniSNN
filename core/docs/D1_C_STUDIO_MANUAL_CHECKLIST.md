# D1-C Studio manual checklist

Validator: project user
System: not recorded  Date: not recorded

Version: `miniSNN Core 1.0.0-rc.1`  Result: PASS

- [x] Double-clicking `Abrir miniSNN Studio.cmd` opens the GUI.
- [x] The repository launcher opens the debug Studio.
- [x] The debug Studio opens directly from `build/studio/bin/`.
- [x] The release Studio opens directly from `build/release/studio/bin/`.
- [x] The extracted Studio package opens without a compiler.
- [x] The title identifies miniSNN Studio and miniSNN Core 1.0.0-rc.1.
- [x] Historical panels appear exactly once.
- [x] A normal scenario loads and executes.
- [ N/A ] Pause during execution -- it does not exist in the current batch Studio.
- [ N/A ] Speed control -- it does not exist in the current batch Studio.
- [ N/A ] Live neural one-step -- it does not exist in the current batch Studio.
- [x] Supported save/load works.
- [x] Existing reports open.
- [x] `NEUROEVOLUCAO` opens without a configuration error.
- [x] The default `evolution_weight_target_demo.ini` and its base scenario load.
- [x] Choosing, cancelling, and saving evolution configurations preserve working paths.
- [x] A short evolution executes.
- [x] No duplicate panel or evident visual regression is observed.
- [x] Closing and reopening works.

`RODAR SIMULACAO` executes one complete batch and returns when it finishes.
Its duration is determined by the scenario cost and the machine. Pause, speed,
and live one-step were never implemented in this Studio and do not block the
D1 manual validation.

Observations: not recorded.
