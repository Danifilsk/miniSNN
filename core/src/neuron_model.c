#include "neuron_model.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define NEURON_MODEL_SIGNATURE_VERSION 1U
#define ADEX_MAX_EXP_ARGUMENT 50.0
#define HH_GATE_TOLERANCE 1e-9

typedef struct
{
    double v;
    double m;
    double h;
    double n;
} HHVector;

#ifdef MINISNN_TESTING
static int test_fail_after_calls = -1;

void neuron_model_test_fail_after_calls(int successful_calls_before_failure)
{
    test_fail_after_calls = successful_calls_before_failure;
}
#endif

static int ascii_equal_ignore_case(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
        return 0;

    while (*left != '\0' && *right != '\0')
    {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= 'A' && a <= 'Z')
            a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (unsigned char)(b + ('a' - 'A'));
        if (a != b)
            return 0;
    }
    return *left == '\0' && *right == '\0';
}

int neuron_model_is_valid(MiniSNNNeuronModel model)
{
    return model == MINISNN_NEURON_MODEL_LIF ||
        model == MINISNN_NEURON_MODEL_ADEX ||
        model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
}

const char *neuron_model_name(MiniSNNNeuronModel model)
{
    switch (model)
    {
    case MINISNN_NEURON_MODEL_LIF:
        return "lif";
    case MINISNN_NEURON_MODEL_ADEX:
        return "adex";
    case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
        return "hodgkin_huxley";
    default:
        return "unknown";
    }
}

int neuron_model_from_name(const char *name, MiniSNNNeuronModel *out_model)
{
    if (name == NULL || out_model == NULL)
        return 0;
    if (ascii_equal_ignore_case(name, "lif"))
        *out_model = MINISNN_NEURON_MODEL_LIF;
    else if (ascii_equal_ignore_case(name, "adex"))
        *out_model = MINISNN_NEURON_MODEL_ADEX;
    else if (ascii_equal_ignore_case(name, "hh") ||
             ascii_equal_ignore_case(name, "hodgkin_huxley") ||
             ascii_equal_ignore_case(name, "hodgkin-huxley"))
        *out_model = MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
    else
        return 0;
    return 1;
}

void adex_parameters_default(AdExParameters *out_parameters)
{
    if (out_parameters == NULL)
        return;
    out_parameters->capacitance = 200.0;
    out_parameters->g_leak = 10.0;
    out_parameters->e_leak = -70.0;
    out_parameters->delta_t = 2.0;
    out_parameters->v_threshold = -50.0;
    out_parameters->tau_w = 100.0;
    out_parameters->a = 2.0;
    out_parameters->b = 60.0;
    out_parameters->v_reset = -58.0;
    out_parameters->v_peak = 20.0;
    out_parameters->dt = 0.1;
}

void hodgkin_huxley_parameters_default(
    HodgkinHuxleyParameters *out_parameters)
{
    if (out_parameters == NULL)
        return;
    out_parameters->capacitance = 1.0;
    out_parameters->g_na = 120.0;
    out_parameters->g_k = 36.0;
    out_parameters->g_leak = 0.3;
    out_parameters->e_na = 50.0;
    out_parameters->e_k = -77.0;
    out_parameters->e_leak = -54.387;
    out_parameters->v_init = -65.0;
    out_parameters->spike_threshold = 0.0;
    out_parameters->dt = 0.01;
}

static int adex_parameters_are_valid(const AdExParameters *p)
{
    return p != NULL && isfinite(p->capacitance) && p->capacitance > 0.0 &&
        isfinite(p->g_leak) && p->g_leak > 0.0 && isfinite(p->e_leak) &&
        isfinite(p->delta_t) && p->delta_t > 0.0 &&
        isfinite(p->v_threshold) && isfinite(p->tau_w) && p->tau_w > 0.0 &&
        isfinite(p->a) && isfinite(p->b) && isfinite(p->v_reset) &&
        isfinite(p->v_peak) && p->v_peak > p->v_threshold &&
        isfinite(p->dt) && p->dt > 0.0;
}

static int hh_parameters_are_valid(const HodgkinHuxleyParameters *p)
{
    return p != NULL && isfinite(p->capacitance) && p->capacitance > 0.0 &&
        isfinite(p->g_na) && p->g_na >= 0.0 &&
        isfinite(p->g_k) && p->g_k >= 0.0 &&
        isfinite(p->g_leak) && p->g_leak >= 0.0 &&
        isfinite(p->e_na) && isfinite(p->e_k) && isfinite(p->e_leak) &&
        isfinite(p->v_init) && isfinite(p->spike_threshold) &&
        isfinite(p->dt) && p->dt > 0.0;
}

int neuron_model_validate_config(const NeuronModelConfig *config)
{
    if (config == NULL)
        return 0;
    switch (config->model)
    {
    case MINISNN_NEURON_MODEL_LIF:
        return lif_parameters_are_valid(&config->data.lif);
    case MINISNN_NEURON_MODEL_ADEX:
        return adex_parameters_are_valid(&config->data.adex);
    case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
        return hh_parameters_are_valid(&config->data.hh);
    default:
        return 0;
    }
}

void neuron_model_config_lif(NeuronModelConfig *config, const LIFParameters *lif)
{
    if (config == NULL || lif == NULL)
        return;
    memset(config, 0, sizeof(*config));
    config->model = MINISNN_NEURON_MODEL_LIF;
    config->data.lif = *lif;
}

void neuron_model_config_adex(NeuronModelConfig *config, const AdExParameters *adex)
{
    if (config == NULL || adex == NULL)
        return;
    memset(config, 0, sizeof(*config));
    config->model = MINISNN_NEURON_MODEL_ADEX;
    config->data.adex = *adex;
}

void neuron_model_config_hodgkin_huxley(
    NeuronModelConfig *config,
    const HodgkinHuxleyParameters *hh)
{
    if (config == NULL || hh == NULL)
        return;
    memset(config, 0, sizeof(*config));
    config->model = MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
    config->data.hh = *hh;
}

double hodgkin_huxley_alpha_m(double v)
{
    double x = v + 40.0;
    if (fabs(x) < 1e-8)
        return 1.0;
    return 0.1 * x / (-expm1(-x / 10.0));
}

double hodgkin_huxley_beta_m(double v) { return 4.0 * exp(-(v + 65.0) / 18.0); }
double hodgkin_huxley_alpha_h(double v) { return 0.07 * exp(-(v + 65.0) / 20.0); }
double hodgkin_huxley_beta_h(double v) { return 1.0 / (1.0 + exp(-(v + 35.0) / 10.0)); }

double hodgkin_huxley_alpha_n(double v)
{
    double x = v + 55.0;
    if (fabs(x) < 1e-8)
        return 0.1;
    return 0.01 * x / (-expm1(-x / 10.0));
}

double hodgkin_huxley_beta_n(double v) { return 0.125 * exp(-(v + 65.0) / 80.0); }

static int hh_rates_are_finite(double v)
{
    return isfinite(hodgkin_huxley_alpha_m(v)) &&
        isfinite(hodgkin_huxley_beta_m(v)) &&
        isfinite(hodgkin_huxley_alpha_h(v)) &&
        isfinite(hodgkin_huxley_beta_h(v)) &&
        isfinite(hodgkin_huxley_alpha_n(v)) &&
        isfinite(hodgkin_huxley_beta_n(v));
}

static int neuron_model_initialize_state(
    Neuron *neuron,
    const NeuronModelConfig *config,
    NeuronType type)
{
    memset(neuron, 0, sizeof(*neuron));
    neuron->type = type;
    neuron->model = config->model;
    switch (config->model)
    {
    case MINISNN_NEURON_MODEL_LIF:
        neuron->V = config->data.lif.v_rest;
        break;
    case MINISNN_NEURON_MODEL_ADEX:
        neuron->V = config->data.adex.e_leak;
        neuron->state.adex.w = 0.0;
        break;
    case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
    {
        double v = config->data.hh.v_init;
        double am, bm, ah, bh, an, bn;
        if (!hh_rates_are_finite(v))
            return 0;
        am = hodgkin_huxley_alpha_m(v); bm = hodgkin_huxley_beta_m(v);
        ah = hodgkin_huxley_alpha_h(v); bh = hodgkin_huxley_beta_h(v);
        an = hodgkin_huxley_alpha_n(v); bn = hodgkin_huxley_beta_n(v);
        neuron->V = v;
        neuron->state.hh.m = am / (am + bm);
        neuron->state.hh.h = ah / (ah + bh);
        neuron->state.hh.n = an / (an + bn);
        neuron->state.hh.previous_V = v;
        break;
    }
    default:
        return 0;
    }
    return 1;
}

int neuron_model_init(Neuron *neuron, const NeuronModelConfig *config)
{
    if (neuron == NULL || !neuron_model_validate_config(config))
        return 0;
    return neuron_model_initialize_state(neuron, config, NEURON_EXCITATORY);
}

int neuron_model_reset(Neuron *neuron, const NeuronModelConfig *config)
{
    Neuron candidate;
    if (neuron == NULL || !neuron_model_validate_config(config))
        return 0;
    if (!neuron_model_initialize_state(&candidate, config, neuron->type))
        return 0;
    *neuron = candidate;
    return 1;
}

static int step_lif(Neuron *candidate, const NeuronModelConfig *config,
                    const NeuronStepContext *context)
{
    double threshold = context->has_adaptive_threshold ?
        context->adaptive_threshold : config->data.lif.v_threshold;
    int spike = lif_update_with_threshold(
        candidate, context->current, &config->data.lif, threshold);
    return isfinite(candidate->V) && (spike == 0 || spike == 1) ? spike : -1;
}

static int step_adex(Neuron *candidate, const NeuronModelConfig *config,
                     const NeuronStepContext *context)
{
    const AdExParameters *p = &config->data.adex;
    double argument = (candidate->V - p->v_threshold) / p->delta_t;
    double exponential;
    double d_v;
    double d_w;

    if (!isfinite(candidate->V) || !isfinite(candidate->state.adex.w) ||
        !isfinite(context->current))
        return -1;
    if (argument > ADEX_MAX_EXP_ARGUMENT)
        argument = ADEX_MAX_EXP_ARGUMENT;
    exponential = exp(argument);
    d_v = (-p->g_leak * (candidate->V - p->e_leak) +
           p->g_leak * p->delta_t * exponential -
           candidate->state.adex.w + context->current) / p->capacitance;
    d_w = (p->a * (candidate->V - p->e_leak) -
           candidate->state.adex.w) / p->tau_w;
    candidate->V += p->dt * d_v;
    candidate->state.adex.w += p->dt * d_w;
    candidate->spike = 0;
    if (!isfinite(candidate->V) || !isfinite(candidate->state.adex.w))
        return -1;
    if (candidate->V >= p->v_peak)
    {
        candidate->V = p->v_reset;
        candidate->state.adex.w += p->b;
        candidate->spike = 1;
        if (!isfinite(candidate->state.adex.w))
            return -1;
    }
    return candidate->spike;
}

static int hh_derivative(const HHVector *s, double current,
                         const HodgkinHuxleyParameters *p, HHVector *out)
{
    double am, bm, ah, bh, an, bn;
    double i_na, i_k, i_l;
    if (s == NULL || out == NULL || !isfinite(current) ||
        !isfinite(s->v) || !isfinite(s->m) || !isfinite(s->h) ||
        !isfinite(s->n) || !hh_rates_are_finite(s->v))
        return 0;
    am = hodgkin_huxley_alpha_m(s->v); bm = hodgkin_huxley_beta_m(s->v);
    ah = hodgkin_huxley_alpha_h(s->v); bh = hodgkin_huxley_beta_h(s->v);
    an = hodgkin_huxley_alpha_n(s->v); bn = hodgkin_huxley_beta_n(s->v);
    i_na = p->g_na * s->m * s->m * s->m * s->h * (s->v - p->e_na);
    i_k = p->g_k * s->n * s->n * s->n * s->n * (s->v - p->e_k);
    i_l = p->g_leak * (s->v - p->e_leak);
    out->v = (current - i_na - i_k - i_l) / p->capacitance;
    out->m = am * (1.0 - s->m) - bm * s->m;
    out->h = ah * (1.0 - s->h) - bh * s->h;
    out->n = an * (1.0 - s->n) - bn * s->n;
    return isfinite(out->v) && isfinite(out->m) &&
        isfinite(out->h) && isfinite(out->n);
}

static HHVector hh_add_scaled(HHVector a, HHVector b, double scale)
{
    a.v += b.v * scale; a.m += b.m * scale;
    a.h += b.h * scale; a.n += b.n * scale;
    return a;
}

static int clamp_gate(double *gate)
{
    if (!isfinite(*gate) || *gate < -HH_GATE_TOLERANCE ||
        *gate > 1.0 + HH_GATE_TOLERANCE)
        return 0;
    if (*gate < 0.0) *gate = 0.0;
    if (*gate > 1.0) *gate = 1.0;
    return 1;
}

static int step_hh(Neuron *candidate, const NeuronModelConfig *config,
                   const NeuronStepContext *context)
{
    const HodgkinHuxleyParameters *p = &config->data.hh;
    HHVector s = {candidate->V, candidate->state.hh.m,
                  candidate->state.hh.h, candidate->state.hh.n};
    HHVector k1, k2, k3, k4, temp;
    double previous_v = candidate->V;
    if (!hh_derivative(&s, context->current, p, &k1)) return -1;
    temp = hh_add_scaled(s, k1, p->dt * 0.5);
    if (!hh_derivative(&temp, context->current, p, &k2)) return -1;
    temp = hh_add_scaled(s, k2, p->dt * 0.5);
    if (!hh_derivative(&temp, context->current, p, &k3)) return -1;
    temp = hh_add_scaled(s, k3, p->dt);
    if (!hh_derivative(&temp, context->current, p, &k4)) return -1;
    candidate->V = s.v + p->dt * (k1.v + 2*k2.v + 2*k3.v + k4.v) / 6.0;
    candidate->state.hh.m = s.m + p->dt * (k1.m + 2*k2.m + 2*k3.m + k4.m) / 6.0;
    candidate->state.hh.h = s.h + p->dt * (k1.h + 2*k2.h + 2*k3.h + k4.h) / 6.0;
    candidate->state.hh.n = s.n + p->dt * (k1.n + 2*k2.n + 2*k3.n + k4.n) / 6.0;
    if (!isfinite(candidate->V) ||
        !clamp_gate(&candidate->state.hh.m) ||
        !clamp_gate(&candidate->state.hh.h) ||
        !clamp_gate(&candidate->state.hh.n))
        return -1;
    candidate->state.hh.previous_V = previous_v;
    candidate->spike = previous_v < p->spike_threshold &&
        candidate->V >= p->spike_threshold;
    return candidate->spike;
}

int neuron_model_step(Neuron *neuron, const NeuronModelConfig *config,
                      const NeuronStepContext *context)
{
    Neuron candidate;
    int result;
#ifdef MINISNN_TESTING
    if (test_fail_after_calls == 0)
    {
        test_fail_after_calls = -1;
        return -1;
    }
    if (test_fail_after_calls > 0)
        test_fail_after_calls--;
#endif
    if (neuron == NULL || !neuron_model_validate_config(config) ||
        context == NULL || !isfinite(context->current) ||
        neuron->model != config->model)
        return -1;
    if (context->has_adaptive_threshold &&
        (!neuron_model_supports_adaptive_threshold(config->model) ||
         !isfinite(context->adaptive_threshold)))
        return -1;
    candidate = *neuron;
    switch (config->model)
    {
    case MINISNN_NEURON_MODEL_LIF: result = step_lif(&candidate, config, context); break;
    case MINISNN_NEURON_MODEL_ADEX: result = step_adex(&candidate, config, context); break;
    case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY: result = step_hh(&candidate, config, context); break;
    default: return -1;
    }
    if (result < 0)
        return -1;
    *neuron = candidate;
    return result;
}

double neuron_model_voltage(const Neuron *neuron) { return neuron != NULL ? neuron->V : NAN; }
int neuron_model_spike(const Neuron *neuron) { return neuron != NULL ? neuron->spike : -1; }

double neuron_model_dt(const NeuronModelConfig *config)
{
    if (!neuron_model_validate_config(config)) return 0.0;
    if (config->model == MINISNN_NEURON_MODEL_LIF) return config->data.lif.dt;
    if (config->model == MINISNN_NEURON_MODEL_ADEX) return config->data.adex.dt;
    return config->data.hh.dt;
}

double neuron_model_base_threshold(const NeuronModelConfig *config)
{
    if (!neuron_model_validate_config(config)) return 0.0;
    if (config->model == MINISNN_NEURON_MODEL_LIF) return config->data.lif.v_threshold;
    if (config->model == MINISNN_NEURON_MODEL_ADEX) return config->data.adex.v_threshold;
    return config->data.hh.spike_threshold;
}

MiniSNNNeuronModelCapabilities neuron_model_capabilities(MiniSNNNeuronModel model)
{
    MiniSNNNeuronModelCapabilities c = {0, 0, 0, 0, 0};
    if (!neuron_model_is_valid(model)) return c;
    c.supports_voltage = 1;
    c.supports_spike_event = 1;
    c.supports_homeostatic_threshold = model == MINISNN_NEURON_MODEL_LIF;
    c.supports_adaptation_state = model == MINISNN_NEURON_MODEL_ADEX;
    c.supports_hh_gates = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
    return c;
}

int neuron_model_supports_adaptive_threshold(MiniSNNNeuronModel model)
{
    return neuron_model_capabilities(model).supports_homeostatic_threshold;
}

const char *neuron_model_integration_method(MiniSNNNeuronModel model)
{
    return model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? "rk4" :
        (neuron_model_is_valid(model) ? "euler" : "unknown");
}

static void fnv_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= value;
    *hash *= UINT64_C(1099511628211);
}

static void fnv_u64(uint64_t *hash, uint64_t value)
{
    for (int i = 0; i < 8; i++) fnv_byte(hash, (unsigned char)(value >> (i * 8)));
}

static void fnv_double(uint64_t *hash, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    fnv_u64(hash, bits);
}

unsigned long long neuron_model_config_signature(const NeuronModelConfig *config)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!neuron_model_validate_config(config)) return 0ULL;
    fnv_u64(&hash, NEURON_MODEL_SIGNATURE_VERSION);
    fnv_u64(&hash, (uint64_t)config->model);
    if (config->model == MINISNN_NEURON_MODEL_LIF)
    {
        fnv_double(&hash, config->data.lif.dt); fnv_double(&hash, config->data.lif.tau);
        fnv_double(&hash, config->data.lif.v_rest); fnv_double(&hash, config->data.lif.v_reset);
        fnv_double(&hash, config->data.lif.v_threshold); fnv_double(&hash, config->data.lif.resistance);
    }
    else if (config->model == MINISNN_NEURON_MODEL_ADEX)
    {
        const AdExParameters *p = &config->data.adex;
        fnv_double(&hash,p->capacitance); fnv_double(&hash,p->g_leak); fnv_double(&hash,p->e_leak);
        fnv_double(&hash,p->delta_t); fnv_double(&hash,p->v_threshold); fnv_double(&hash,p->tau_w);
        fnv_double(&hash,p->a); fnv_double(&hash,p->b); fnv_double(&hash,p->v_reset);
        fnv_double(&hash,p->v_peak); fnv_double(&hash,p->dt);
    }
    else
    {
        const HodgkinHuxleyParameters *p = &config->data.hh;
        fnv_double(&hash,p->capacitance); fnv_double(&hash,p->g_na); fnv_double(&hash,p->g_k);
        fnv_double(&hash,p->g_leak); fnv_double(&hash,p->e_na); fnv_double(&hash,p->e_k);
        fnv_double(&hash,p->e_leak); fnv_double(&hash,p->v_init);
        fnv_double(&hash,p->spike_threshold); fnv_double(&hash,p->dt);
    }
    return (unsigned long long)hash;
}
