#ifndef AGENT_CYCLE_CHECKPOINT_INTERNAL_H
#define AGENT_CYCLE_CHECKPOINT_INTERNAL_H

#include <stdio.h>
#include <stdint.h>

#include "minisnn.h"

/* Internal component snapshots used exclusively by the C7.5-A cycle
 * checkpoint. They deliberately avoid exposing mutable implementation state
 * through the public API. */
int minisnn_agent_io_checkpoint_write(
    const MiniSNNAgentIOContext *context, FILE *file);
int minisnn_agent_io_checkpoint_load(
    MiniSNNAgentIOContext *context, FILE *file);
/* Returns a stable boundary code: 0 for READY, 1 for ACTION_PENDING, or -1
 * when the context contains a partial tick and cannot be checkpointed. */
int minisnn_agent_io_checkpoint_boundary(
    const MiniSNNAgentIOContext *context, uint64_t next_global_tick);

int minisnn_sensor_encoder_checkpoint_write(
    const MiniSNNSensorEncoder *encoder, FILE *file);
int minisnn_sensor_encoder_checkpoint_load(
    MiniSNNSensorEncoder *encoder, FILE *file);

int minisnn_action_decoder_checkpoint_write(
    const MiniSNNActionDecoder *decoder, FILE *file);
int minisnn_action_decoder_checkpoint_load(
    MiniSNNActionDecoder *decoder, FILE *file);

int minisnn_network_checkpoint_write(const MiniSNN *network, FILE *file);
int minisnn_network_checkpoint_load(MiniSNN *network, FILE *file);
uint64_t minisnn_network_checkpoint_contract_signature(const MiniSNN *network);

#endif
