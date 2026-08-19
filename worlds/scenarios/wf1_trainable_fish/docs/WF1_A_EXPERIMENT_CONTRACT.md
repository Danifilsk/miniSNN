# WF1-A Experimental Contract

WF1-A measures whether the same generic WB1 brain performs better after
R-STDP experience than before it. It is not a claim of general animal
intelligence or navigation.

## Experimental phases

- **UNTRAINED**: fresh brain in evaluation mode; plasticity and rewards off.
- **TRAINING**: same initial brain in training mode; fresh worlds per episode;
  learned weights survive only the transient episode reset.
- **TRAINED**: the trained brain in evaluation mode on held-out world seeds.
- **CONTROL**: the same initial brain exposed to the training-world schedule
  with plasticity disabled.
- **LOADED**: the saved learned brain reloaded and evaluated again.

Evaluation uses no overlapping world seeds with training and asserts its weight
signature is unchanged before and after all held-out episodes.

## World

The scenario contains a single organism in open water, no terrain, rocks, or
obstacles, and a deterministic seed-driven set of food entities on a reachable
fixed-point grid. The species begins with energy 36, has maximum energy 60,
metabolizes one unit per Domain tick, pays one unit for a successful move, and
receives nutrition 12 from a real eat. Starvation remains terminal.

## Causal reward

Only a true Domain consequence can reward the brain:

- EAT + APPLIED + increased energy: +1.0
- terminal starvation after that action consequence was consumed: -1.0
- a truncation is not a starvation event and receives no penalty.

The scenario never examines food_dx or food_dy to choose or alter an action. It
invokes WB1 decide, applies precisely that result to Domain, then passes the
Domain action result back to WB1 exactly once.