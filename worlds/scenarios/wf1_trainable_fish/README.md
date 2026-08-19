# WF1-A: First Trainable Fish Experiment

WF1-A is a headless, deterministic scientific scenario distinct from the frozen
WF0 fixture. It creates a fresh open-water Kernel and Domain for every episode
while preserving one WB1 trainable brain across training episodes.

The initial network is built only through WB1's deterministic generic Topology
Factory. This scenario never creates sensor-to-action connections, never maps
food direction to a movement, and never replaces the action decoded by the
trainable brain.

## Episode contract

1. The Domain supplies perception.
2. WB1 produces the action through the existing sensor/decoder contracts.
3. Domain applies that exact action.
4. WB1 consumes the resulting action result once.
5. Starvation applies one generic terminal feedback of -1.0.
6. reset_episode clears transient neural state, binding, and cycle state while
   preserving learned weights before the next fresh world is bound.

Positive reward is +1.0 only when an EAT action was applied and increased
energy. There is no reward for distance, direction, or survival. A live time
limit is truncation: it applies no starvation penalty.

Training seeds begin at 100000; held-out evaluation seeds begin at 900000.
The fixed pilot uses four brain seeds (101, 202, 303, 404), 32 training
episodes, and 12 held-out evaluation episodes per seed.

## Run

Use mingw32-make experiment-wf1-a, mingw32-make check-wf1-a, and
mingw32-make audit-wf1-a.

Results are written below
build/worlds/scenarios/wf1_trainable_fish/results/:

- summary.txt
- training.csv
- evaluation.csv
- seeds.csv
- one WB1 learned-brain manifest and checkpoint sidecar per brain seed

summary.txt reports LEARNING_DETECTED, NO_IMPROVEMENT, or INCONCLUSIVE without
treating any of them as a software PASS. The pilot criterion for detection is a
trained mean foods-per-episode above both the untrained mean and the
no-learning control, with a majority of brain seeds improving.