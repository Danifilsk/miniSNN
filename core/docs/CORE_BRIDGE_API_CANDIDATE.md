# Core-Brain Bridge API candidate

Status: `CANDIDATE_V1`, provisional until D2 validates the Core against a real
Worlds consumer. The Core remains numeric and domain-neutral.

## Consumer lifecycle

1. Create `MiniSNN` with `minisnn_create_with_config`.
2. Create sensor and action schemas, then `MiniSNNAgentIOContext`.
3. Create a `MiniSNNSensorEncoder` and `MiniSNNActionDecoder` with matching
   schema signatures.
4. Create `MiniSNNAgentCycle`; the consumer keeps ownership of the network,
   AgentIO, encoder, and decoder.
5. Submit one sensor frame, run one tick, then consume exactly one action
   frame. The cycle publishes no domain interpretation for that action.

The Core owns copied schema data and transient cycle buffers. The caller owns
all opaque objects it creates and destroys each with the matching destroy
function.

## Reset, feedback, and checkpoints

`minisnn_agent_cycle_reset_episode` preserves topology, weights, neuron types,
and model configuration while clearing transient episode state. It requires no
pending external action. Generic feedback is submitted through
`minisnn_agent_cycle_submit_feedback`; it is neither a World reward policy nor
a domain model.

Checkpoints are allowed only at documented stable Agent Cycle boundaries and
preserve network configuration, schemas, mappings, weights, delays, types, and
validated signatures. Consumers must handle signature, format, I/O, and state
errors as recoverable failures. A reset does not substitute for a checkpoint.

## State and provisional contract

States are `READY`, `RUNNING`, `ACTION_PENDING`, and `FAULTED`. Sensor/action
frame ordering errors, schema mismatches, invalid feedback, and checkpoint
incompatibility are explicit recoverable errors. This candidate does not define
creatures, maps, movement, hunger, objectives, or Worlds semantics. D2 is the
only stage allowed to freeze the Core-Brain Bridge v1 contract definitively.
