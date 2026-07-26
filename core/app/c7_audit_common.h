#ifndef C7_AUDIT_COMMON_H
#define C7_AUDIT_COMMON_H

#include <stdint.h>

#include "minisnn.h"

#define C7_AUDIT_SENSOR_COUNT 3U
#define C7_AUDIT_ACTION_COUNT 4U
#define C7_AUDIT_MIN_NEURONS 12U

/* Internal test/demo support for the domain-neutral C7 audit. It owns every
 * object it creates and deliberately keeps channel meaning out of the Core. */
typedef struct
{
    MiniSNN *network;
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNActionSchema *action_schema;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *encoder;
    MiniSNNActionDecoder *decoder;
    MiniSNNAgentCycle *cycle;
    MiniSNNNeuronModel model;
    uint32_t brain_steps_per_tick;
    double input_drive;
} C7AuditFixture;

typedef struct
{
    uint64_t value;
    uint64_t neural_steps;
    uint64_t actions;
    uint64_t rewards;
    uint64_t resets;
} C7AuditFingerprint;

/* The public integrated-audit configuration can calibrate current amplitude
 * per model without changing the C7 encoder or any neuron implementation. */
int c7_audit_fixture_create_calibrated(
    C7AuditFixture *fixture,
    MiniSNNNeuronModel model,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    double input_drive,
    int enable_plasticity,
    int enable_reward,
    int enable_homeostasis,
    int enable_structural);

int c7_audit_fixture_create(
    C7AuditFixture *fixture,
    MiniSNNNeuronModel model,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    int enable_plasticity,
    int enable_reward,
    int enable_homeostasis,
    int enable_structural);

void c7_audit_fixture_destroy(C7AuditFixture *fixture);

int c7_audit_submit_sensor(
    C7AuditFixture *fixture,
    uint64_t tick,
    const double values[C7_AUDIT_SENSOR_COUNT]);

int c7_audit_run_pending(
    C7AuditFixture *fixture,
    MiniSNNAgentCycleDiagnostics *out_diagnostics);

int c7_audit_consume_action(
    C7AuditFixture *fixture,
    uint64_t expected_tick,
    double out_values[C7_AUDIT_ACTION_COUNT]);

int c7_audit_run_tick(
    C7AuditFixture *fixture,
    uint64_t tick,
    const double sensor_values[C7_AUDIT_SENSOR_COUNT],
    double out_actions[C7_AUDIT_ACTION_COUNT],
    MiniSNNAgentCycleDiagnostics *out_diagnostics);

int c7_audit_submit_feedback(
    C7AuditFixture *fixture,
    uint64_t source_tick,
    uint64_t delivery_tick,
    double reward,
    int episode_terminal);

int c7_audit_fixture_all_finite(const C7AuditFixture *fixture);

uint64_t c7_audit_fixture_state_signature(const C7AuditFixture *fixture);

/* Headless C7 fixtures must work on Windows and POSIX toolchains. */
int c7_audit_ensure_directory(const char *directory);
void c7_audit_remove_checkpoint_directory(const char *directory);

void c7_audit_fingerprint_init(
    C7AuditFingerprint *fingerprint,
    const C7AuditFixture *fixture,
    uint64_t seed);

void c7_audit_fingerprint_tick(
    C7AuditFingerprint *fingerprint,
    uint64_t tick,
    const double sensor_values[C7_AUDIT_SENSOR_COUNT],
    const double action_values[C7_AUDIT_ACTION_COUNT],
    const MiniSNNAgentCycleDiagnostics *diagnostics,
    const C7AuditFixture *fixture);

uint64_t c7_audit_fingerprint_value(const C7AuditFingerprint *fingerprint);

#endif
