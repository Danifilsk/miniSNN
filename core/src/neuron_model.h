#ifndef NEURON_MODEL_H
#define NEURON_MODEL_H

#include "neuron.h"

int neuron_model_is_valid(MiniSNNNeuronModel model);
const char *neuron_model_name(MiniSNNNeuronModel model);
int neuron_model_from_name(const char *name, MiniSNNNeuronModel *out_model);
int neuron_model_validate_config(const NeuronModelConfig *config);
void neuron_model_config_lif(NeuronModelConfig *config, const LIFParameters *lif);
void neuron_model_config_adex(NeuronModelConfig *config, const AdExParameters *adex);
void neuron_model_config_hodgkin_huxley(
    NeuronModelConfig *config,
    const HodgkinHuxleyParameters *hh);
void adex_parameters_default(AdExParameters *out_parameters);
void hodgkin_huxley_parameters_default(
    HodgkinHuxleyParameters *out_parameters);
int neuron_model_init(Neuron *neuron, const NeuronModelConfig *config);
int neuron_model_reset(Neuron *neuron, const NeuronModelConfig *config);
int neuron_model_step(Neuron *neuron, const NeuronModelConfig *config,
                      const NeuronStepContext *context);
double neuron_model_voltage(const Neuron *neuron);
int neuron_model_spike(const Neuron *neuron);
double neuron_model_dt(const NeuronModelConfig *config);
double neuron_model_base_threshold(const NeuronModelConfig *config);
int neuron_model_supports_adaptive_threshold(MiniSNNNeuronModel model);
MiniSNNNeuronModelCapabilities neuron_model_capabilities(
    MiniSNNNeuronModel model);
const char *neuron_model_integration_method(MiniSNNNeuronModel model);
unsigned long long neuron_model_config_signature(
    const NeuronModelConfig *config);

double hodgkin_huxley_alpha_m(double voltage);
double hodgkin_huxley_beta_m(double voltage);
double hodgkin_huxley_alpha_h(double voltage);
double hodgkin_huxley_beta_h(double voltage);
double hodgkin_huxley_alpha_n(double voltage);
double hodgkin_huxley_beta_n(double voltage);

#ifdef MINISNN_TESTING
void neuron_model_test_fail_after_calls(int successful_calls_before_failure);
#endif

#endif
