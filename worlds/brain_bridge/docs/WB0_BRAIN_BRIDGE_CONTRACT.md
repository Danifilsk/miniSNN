# WB0 Brain Bridge Contract

## Dependency and ownership boundary

The production Bridge is a C11 library with exactly these product dependencies:

    Brain Bridge -> miniSNN Core public API
    Brain Bridge -> Worlds Domain public API

It does not include a Kernel header directly. MiniSNNWorldsKernelEntityId and
fixed-point scalar types arrive transitively through the Domain public API, so
the Bridge can preserve the Domain action contract without accessing Kernel
internals.

    Core
      ^
    Brain Bridge
      v
    Domain
      v
    Kernel

Core does not know food, hunger, organisms, moves, or eating. Domain does not
know neurons, spikes, synapses, or numeric input channels. The Bridge returns
a MiniSNNWorldsDomainAction; the caller alone invokes
minisnn_worlds_domain_step.

MiniSNNWorldsBrainBridge owns dynamically allocated bindings and per-binding
same-tick cached reports. MiniSNNWorldsDomain and MiniSNN are non-owning. A
caller must unbind or destroy the Bridge before destroying a bound brain or
Domain.

A Bridge has one active Domain identity. The first successful bind establishes
that identity; later binds and decisions must receive that exact Domain pointer
or fail with DOMAIN_MISMATCH before a cache lookup, neural step, counter update,
or caller-output write. Unbinding the final actor clears the identity, so the
Bridge may then be reused with another Domain.

## Core C7/public API mapping

| WB0 need | Core public API | Ownership | Can fail | Neural state |
| --- | --- | --- | --- | --- |
| Validate mapping | minisnn_neuron_count | caller brain | no | no |
| Begin a window | minisnn_clear_inputs | caller brain | no | clears external inputs |
| Apply sensor value | minisnn_set_input | caller brain | yes | external input only |
| Advance window | minisnn_step | caller brain | yes, -1 | one Core step |
| Aggregate output | minisnn_get_spike | caller brain | yes | no |
| Verify cache test | minisnn_current_step | caller brain | no | no |

WB0 uses direct public Core numeric input/spike APIs rather than AgentIO because
the V1 contract is a fixed, local one-to-one mapping. AgentIO remains available
for future externally clocked adapters; no semantic Worlds concept is added to
Core either way. Plasticity, reward, and evolution are not enabled or added by
WB0.

## Domain mapping

| WB0 need | Domain public API |
| --- | --- |
| Current logical tick | minisnn_worlds_domain_tick |
| Validate generic organism | minisnn_worlds_domain_organism_count, minisnn_worlds_domain_organism_at |
| Differentiate known food | minisnn_worlds_domain_food_count, minisnn_worlds_domain_food_at |
| Read semantic state | minisnn_worlds_domain_perceive |
| Caller applies returned action | minisnn_worlds_domain_step |

## Sensor Contract V1

The frame has six channels in this fixed order:

1. self_energy: self_energy / max_energy, clamped to [0, 1].
2. self_hunger: self_hunger / max_energy, clamped to [0, 1].
3. nearest_food_present: 0 or 1.
4. nearest_food_delta_x: signed fixed-point delta divided by
   perception_scale, clamped to [-1, 1].
5. nearest_food_delta_y: same signed encoding.
6. nearest_food_distance: non-negative distance divided by
   perception_scale, clamped to [0, 1].

perception_scale is finite and strictly positive. The default is 1000.0. When
no food exists, presence, both deltas, and distance are all exactly zero; no
previous frame data is retained. The conversion produces only finite, bounded
values.

## Decision window and action decoding

Each Domain tick maps to at most one neural window. The default
core_steps_per_decision is 8; values must be between 1 and 10000. WB0 clears
external inputs, applies all six encoded currents using input_gain, runs exactly
the configured number of minisnn_step calls, and counts spikes only from that
window.

Output channels use this stable order:

    0 WAIT
    1 MOVE_POS_X
    2 MOVE_NEG_X
    3 MOVE_POS_Y
    4 MOVE_NEG_Y
    5 EAT

The greatest spike count wins. Equal positive scores choose the lower channel
index. All-zero output becomes WAIT with NO_OUTPUT fallback. An EAT winner
without a perceived food becomes WAIT with EAT_WITHOUT_FOOD fallback. Movement
is semantic translation only and uses configured positive fixed-point
move_step; collision, range, energy, and food validity remain the Domain
authority.

## Same-tick cache and retry

A binding caches the report and action after the first successful decision for
a Domain tick. A second decide call for that actor and tick returns the same
action and report with cache_hit=1, without advancing the Core. If Domain step
fails and its tick remains unchanged, retrying decide returns that cached
action. If the observed Domain tick regresses, WB0 returns TICK_REGRESSION and
never rewinds the brain.

## Failure boundary audit

WB0 performs binding lookup, actor validation, perception, encoding, and all
neuron index checks before minisnn_clear_inputs and before any Core step.
Because the frame is bounded and configuration gain is finite and positive,
input currents are finite after this preflight.

network_update, reached through minisnn_step, validates network, homeostasis,
and reward contracts before mutating the neural step. A neuron_model_step
failure restores the neuron snapshot and clears spikes before returning -1; it
neither counts nor transmits that failed spike. WB0 propagates this as
CORE_FAILURE, emits no Domain action and does not cache a result. No allocator
is used by the Bridge after Core evaluation starts. Thus ordinary public
failures are rejected before the window or reported without a Bridge action;
WB0 does not claim a generic Core snapshot or rollback facility beyond the
Core step contract it audits here.

## Persistence and non-goals

Kernel Snapshot V1, Command Log V1, and State Hash V5 are unchanged. WB0 adds
no persistence format. Kernel persistence exists; Domain persistence, Brain
runtime persistence, and an integrated brain/world checkpoint are not
supported. There is no fish, shark, learning, reward, evolution, reproduction,
health, damage, death, renderer, or World App in this block.