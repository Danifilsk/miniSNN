# WF1-A.1 Exploration Bootstrap

WF1-A.1 is a headless follow-up to WF1-A. It measures neural action selection before learning and evaluates whether generic exploratory conditions let the existing R-STDP path improve food consumption.

The bootstrap changes only generic properties: random recurrent topology, neuron count, uniform action-population size, connection magnitudes, decision steps, map size, food density, energy budget, and episode duration. It never maps food sensors to actions, overrides a neural decision, or rewards movement.

Run `mingw32-make experiment-wf1-a1`. Results are isolated in `build/worlds/scenarios/wf1_trainable_fish/results/wf1_a1/` and retain the original WF1-A output schema separately. `summary.txt` records calibration seeds, non-overlapping training/evaluation seed ranges, activity distributions, reward events, frozen evaluation signatures, and the honest result.

Positive reward remains exactly one event: a real `EAT` action whose Domain result is `APPLIED` and increases energy. Terminal starvation feedback remains `-1`. The control brain has plasticity disabled and evaluation never changes weights.
Reporting correction: an early A1 summary mixed training totals with trained-evaluation totals. The regenerated summary and seeds.csv now keep 	raining and 	rained_evaluation separate; held-out TRAINED means are derived only from TRAINED rows in evaluation.csv.
