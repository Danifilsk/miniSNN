#include <math.h>
#include <stdio.h>
#include <string.h>

#include "c7_audit_common.h"
#include "c7_integrated_audit_config.h"

#define C7_AUDIT_OUTPUT_ROOT "results/scenarios"
#define C7_AUDIT_PATH_MAX 512U
#define C7_AUDIT_MAX_RUNS 6U

typedef struct
{
    MiniSNNNeuronModel model;
    uint64_t seed;
    unsigned int repeat;
    uint64_t ticks;
    uint64_t neural_steps;
    uint64_t spikes;
    uint64_t actions;
    uint64_t resets;
    uint64_t checkpoints;
    uint64_t resumes;
    uint64_t replays;
    uint64_t ready_checkpoints;
    uint64_t pending_checkpoints;
    uint64_t active_readout_channels;
    uint64_t nondefault_action_count;
    uint64_t action_variation_count;
    uint64_t silent_ticks;
    double reward;
    double input_drive;
    uint64_t model_config_signature;
    uint64_t fingerprint;
    int finite;
    int completed;
} AuditRun;

typedef struct
{
    int save_completed;
    int load_completed;
    int resume_completed;
    int replay_completed;
    uint64_t state_signature;
} CheckpointOutcome;

static int join_path(char *out, size_t size, const char *directory, const char *name)
{
    int written;
    if (out == NULL || size == 0U || directory == NULL || name == NULL)
        return 0;
    written = snprintf(out, size, "%s/%s", directory, name);
    return written >= 0 && (size_t)written < size;
}

static int copy_file_bytes(const char *source_path, const char *destination_path)
{
    FILE *source = NULL;
    FILE *destination = NULL;
    int value;
    if (source_path == NULL || destination_path == NULL)
        return 0;
    source = fopen(source_path, "rb");
    if (source == NULL)
        return 0;
    destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        fclose(source);
        return 0;
    }
    while ((value = fgetc(source)) != EOF)
        if (fputc(value, destination) == EOF)
        {
            fclose(source);
            fclose(destination);
            return 0;
        }
    if (ferror(source))
    {
        fclose(source);
        fclose(destination);
        return 0;
    }
    if (fclose(source) != 0)
    {
        fclose(destination);
        return 0;
    }
    return fclose(destination) == 0;
}

static void values_for_tick(uint64_t seed, uint64_t tick,
                            double values[C7_AUDIT_SENSOR_COUNT])
{
    values[0] = ((seed + tick) % 3U) == 0U ? 1.0 : 0.5;
    values[1] = ((seed + tick) % 3U) == 1U ? -0.5 : 0.25;
    values[2] = ((seed ^ tick) & UINT64_C(1)) == 0U ? 1.0 : 0.0;
}

static int actions_equal(const double left[C7_AUDIT_ACTION_COUNT],
                         const double right[C7_AUDIT_ACTION_COUNT])
{
    for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
        if (left[index] != right[index])
            return 0;
    return 1;
}

static int write_checkpoint_metadata(const char *directory, MiniSNNNeuronModel model,
                                     unsigned int repeat, const char *boundary,
                                     uint64_t state_signature, uint64_t run_id)
{
    char path[C7_AUDIT_PATH_MAX];
    FILE *file;
    if (!join_path(path, sizeof(path), directory, "audit_metadata.txt"))
        return 0;
    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file, "model=%s\nrepeat=%u\nboundary=%s\nstate_signature=%llu\n"
                "run_id=%llu\n",
                minisnn_neuron_model_name(model), repeat, boundary,
                (unsigned long long)state_signature, (unsigned long long)run_id) < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int replace_from_checkpoint(C7AuditFixture *fixture,
                                   const C7IntegratedAuditConfig *config,
                                   MiniSNNNeuronModel model, unsigned int repeat,
                                   const char *directory, const char *boundary,
                                   int pending, int replay_enabled, uint64_t run_id,
                                   uint64_t tick,
                                   double actions[C7_AUDIT_ACTION_COUNT],
                                   CheckpointOutcome *outcome)
{
    C7AuditFixture restored = {0};
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    double original_actions[C7_AUDIT_ACTION_COUNT] = {0};
    double restored_actions[C7_AUDIT_ACTION_COUNT] = {0};
    CheckpointOutcome completed = {0};
    const int homeostasis = config->homeostasis_enabled &&
        model == MINISNN_NEURON_MODEL_LIF;
    uint64_t saved_signature;
    uint64_t loaded_signature;

    if (fixture == NULL || config == NULL || directory == NULL || boundary == NULL ||
        actions == NULL || outcome == NULL)
        return 0;
    memset(outcome, 0, sizeof(*outcome));
    if (!minisnn_agent_cycle_save_checkpoint(fixture->cycle, directory, &error))
        return 0;
    completed.save_completed = 1;
    saved_signature = c7_audit_fixture_state_signature(fixture);
    if (saved_signature == 0U || !write_checkpoint_metadata(directory, model, repeat,
                                                              boundary, saved_signature,
                                                              run_id) ||
        !c7_audit_fixture_create_calibrated(&restored, model, C7_AUDIT_MIN_NEURONS,
                                             config->brain_steps_per_tick,
                                             config->input_drive[model],
                                             config->stdp_enabled, config->reward_enabled,
                                             homeostasis, config->structural_enabled) ||
        !minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error))
    {
        c7_audit_fixture_destroy(&restored);
        return 0;
    }
    completed.load_completed = 1;
    loaded_signature = c7_audit_fixture_state_signature(&restored);
    if (loaded_signature == 0U || loaded_signature != saved_signature)
    {
        c7_audit_fixture_destroy(&restored);
        return 0;
    }
    completed.resume_completed = 1;
    completed.state_signature = saved_signature;
    if (pending)
    {
        if (!c7_audit_consume_action(&restored, tick, restored_actions))
        {
            c7_audit_fixture_destroy(&restored);
            return 0;
        }
        if (replay_enabled &&
            (!c7_audit_consume_action(fixture, tick, original_actions) ||
             !actions_equal(original_actions, restored_actions)))
        {
            c7_audit_fixture_destroy(&restored);
            return 0;
        }
        memcpy(actions, restored_actions, sizeof(restored_actions));
    }
    if (replay_enabled)
    {
        completed.replay_completed = 1;
    }
    c7_audit_fixture_destroy(fixture);
    *fixture = restored;
    *outcome = completed;
    return 1;
}

static int checkpoint_relative_name(char *out, size_t size, MiniSNNNeuronModel model,
                                    unsigned int repeat, const char *boundary)
{
    int written = snprintf(out, size, "checkpoint_%s_repeat_%u_%s",
                           minisnn_neuron_model_name(model), repeat, boundary);
    return written >= 0 && (size_t)written < size;
}

static int checkpoint_path(char *out, size_t size, const char *output,
                           MiniSNNNeuronModel model, unsigned int repeat,
                           const char *boundary)
{
    char name[160];
    return checkpoint_relative_name(name, sizeof(name), model, repeat, boundary) &&
        join_path(out, size, output, name);
}

static void cleanup_known_artifacts(const char *output)
{
    static const char *const files[] =
    {
        "config_source.ini", "config_used.ini", "c7_audit_manifest.txt",
        "c7_audit_runs.csv", "c7_audit_actions.csv", "c7_audit_feedback.csv",
        "c7_audit_checkpoints.csv", "c7_audit_model_summary.csv",
        "c7_audit_summary.txt", "c7_audit_report.html"
    };
    char path[C7_AUDIT_PATH_MAX];
    if (output == NULL)
        return;
    for (size_t index = 0U; index < sizeof(files) / sizeof(files[0]); index++)
        if (join_path(path, sizeof(path), output, files[index]))
            remove(path);
    for (int model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
        for (unsigned int repeat = 0U; repeat < 2U; repeat++)
        {
            if (checkpoint_path(path, sizeof(path), output,
                                (MiniSNNNeuronModel)model, repeat, "ready"))
                c7_audit_remove_checkpoint_directory(path);
            if (checkpoint_path(path, sizeof(path), output,
                                (MiniSNNNeuronModel)model, repeat, "action_pending"))
                c7_audit_remove_checkpoint_directory(path);
        }
}

static uint64_t run_id_for(const C7IntegratedAuditConfig *config)
{
    uint64_t value = UINT64_C(14695981039346656037);
    if (config == NULL)
        return 0U;
    for (size_t index = 0U; config->run_name[index] != '\0'; index++)
    {
        value ^= (unsigned char)config->run_name[index];
        value *= UINT64_C(1099511628211);
    }
    value ^= config->seed;
    value *= UINT64_C(1099511628211);
    value ^= ((uint64_t)config->episodes << 32U) | config->ticks_per_episode;
    return value;
}

static int record_checkpoint(FILE *file, MiniSNNNeuronModel model, unsigned int repeat,
                             const char *boundary, const char *relative_path,
                             const CheckpointOutcome *outcome)
{
    if (file == NULL || boundary == NULL || relative_path == NULL || outcome == NULL)
        return 0;
    return fprintf(file, "%s,%u,%s,%s,%d,%d,%d,%d,%llu\n",
                   minisnn_neuron_model_name(model), repeat, boundary, relative_path,
                   outcome->save_completed, outcome->load_completed,
                   outcome->resume_completed, outcome->replay_completed,
                   (unsigned long long)outcome->state_signature) > 0;
}

static void measure_action(AuditRun *run, uint32_t *active_mask,
                           const double previous[C7_AUDIT_ACTION_COUNT], int have_previous,
                           const double current[C7_AUDIT_ACTION_COUNT])
{
    int nondefault = 0;
    int variation = have_previous ? 0 : 1;
    if (run == NULL || active_mask == NULL || current == NULL)
        return;
    for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
    {
        if (fabs(current[index]) > 1e-12)
        {
            *active_mask |= UINT32_C(1) << index;
            nondefault = 1;
        }
        if (have_previous && current[index] != previous[index])
            variation = 1;
    }
    if (nondefault)
        run->nondefault_action_count++;
    if (variation && nondefault)
        run->action_variation_count++;
}

static unsigned int count_bits(uint32_t value)
{
    unsigned int count = 0U;
    while (value != 0U)
    {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

static int run_audit_once(const C7IntegratedAuditConfig *config,
                          MiniSNNNeuronModel model, uint64_t seed,
                          unsigned int repeat, const char *output_directory,
                          FILE *actions_file, FILE *feedback_file,
                          FILE *checkpoints_file, uint64_t run_id, AuditRun *out_run)
{
    C7AuditFixture fixture = {0};
    C7AuditFingerprint fingerprint;
    int homeostasis;
    uint64_t tick = 0U;
    uint32_t active_mask = 0U;
    double previous_action[C7_AUDIT_ACTION_COUNT] = {0};
    int have_previous = 0;
    AuditRun run = {0};
    int ok;

    if (config == NULL || out_run == NULL)
        return 0;
    homeostasis = config->homeostasis_enabled && model == MINISNN_NEURON_MODEL_LIF;
    ok = c7_audit_fixture_create_calibrated(&fixture, model, C7_AUDIT_MIN_NEURONS,
                                             config->brain_steps_per_tick,
                                             config->input_drive[model],
                                             config->stdp_enabled, config->reward_enabled,
                                             homeostasis, config->structural_enabled);
    if (!ok)
        return 0;
    run.model = model;
    run.seed = seed;
    run.repeat = repeat;
    run.input_drive = config->input_drive[model];
    run.model_config_signature = minisnn_neuron_model_config_signature(fixture.network);
    c7_audit_fingerprint_init(&fingerprint, &fixture, seed);
    for (uint32_t episode = 0U; ok && episode < config->episodes; episode++)
    {
        for (uint32_t local_tick = 0U; ok && local_tick < config->ticks_per_episode;
             local_tick++, tick++)
        {
            double values[C7_AUDIT_SENSOR_COUNT];
            double actions[C7_AUDIT_ACTION_COUNT] = {0};
            MiniSNNAgentCycleDiagnostics diagnostics = {0};
            const int pending_boundary = config->checkpoint_pending_enabled &&
                episode == 0U && local_tick == 2U;
            values_for_tick(seed, tick, values);
            if (pending_boundary)
            {
                char path[C7_AUDIT_PATH_MAX];
                char name[160];
                CheckpointOutcome outcome;
                ok = c7_audit_submit_sensor(&fixture, tick, values) &&
                    c7_audit_run_pending(&fixture, &diagnostics) &&
                    checkpoint_path(path, sizeof(path), output_directory, model, repeat,
                                    "action_pending") && c7_audit_ensure_directory(path) &&
                    replace_from_checkpoint(&fixture, config, model, repeat, path,
                                            "ACTION_PENDING", 1, config->replay_enabled,
                                            run_id, tick, actions, &outcome);
                if (outcome.save_completed) run.checkpoints++;
                if (outcome.resume_completed) run.resumes++;
                if (outcome.replay_completed) run.replays++;
                if (outcome.save_completed) run.pending_checkpoints++;
                if (ok)
                    ok = checkpoint_relative_name(name, sizeof(name), model, repeat,
                                                  "action_pending") &&
                        record_checkpoint(checkpoints_file, model, repeat, "ACTION_PENDING",
                                          name, &outcome);
            }
            else
                ok = c7_audit_run_tick(&fixture, tick, values, actions, &diagnostics);
            if (!ok || !c7_audit_fixture_all_finite(&fixture))
                break;
            if (config->checkpoint_ready_enabled && episode == 0U && local_tick == 1U)
            {
                char path[C7_AUDIT_PATH_MAX];
                char name[160];
                CheckpointOutcome outcome;
                ok = checkpoint_path(path, sizeof(path), output_directory, model, repeat, "ready") &&
                    c7_audit_ensure_directory(path) &&
                    replace_from_checkpoint(&fixture, config, model, repeat, path, "READY", 0,
                                            config->replay_enabled, run_id, tick, actions, &outcome);
                if (outcome.save_completed) run.checkpoints++;
                if (outcome.resume_completed) run.resumes++;
                if (outcome.replay_completed) run.replays++;
                if (outcome.save_completed) run.ready_checkpoints++;
                if (ok)
                    ok = checkpoint_relative_name(name, sizeof(name), model, repeat, "ready") &&
                        record_checkpoint(checkpoints_file, model, repeat, "READY", name,
                                          &outcome);
                if (!ok)
                    break;
            }
            if (fprintf(actions_file, "%s,%u,%llu,%u,%.17g,%.17g,%.17g,%.17g\n",
                        minisnn_neuron_model_name(model), repeat, (unsigned long long)tick,
                        episode, actions[0], actions[1], actions[2], actions[3]) < 0)
                break;
            c7_audit_fingerprint_tick(&fingerprint, tick, values, actions, &diagnostics, &fixture);
            run.ticks++;
            run.neural_steps += diagnostics.brain_steps_executed;
            run.spikes += diagnostics.total_spikes;
            run.actions++;
            run.reward += diagnostics.reward_delivered;
            if (diagnostics.total_spikes == 0U)
                run.silent_ticks++;
            measure_action(&run, &active_mask, previous_action, have_previous, actions);
            memcpy(previous_action, actions, sizeof(previous_action));
            have_previous = 1;
            if (local_tick + 1U == config->ticks_per_episode)
            {
                ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, 0.0, 1) &&
                    minisnn_agent_cycle_reset_episode(fixture.cycle);
                if (ok)
                {
                    run.resets++;
                    ok = fprintf(feedback_file, "%s,%u,%llu,%llu,0,terminal\n",
                                 minisnn_neuron_model_name(model), repeat,
                                 (unsigned long long)tick,
                                 (unsigned long long)(tick + 1U)) > 0;
                }
            }
            else
            {
                double reward = config->reward_enabled ?
                    ((tick % 3U) == 0U ? 0.20 : ((tick % 3U) == 1U ? -0.10 : 0.0)) :
                    0.0;
                ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, reward, 0);
                if (ok)
                    ok = fprintf(feedback_file, "%s,%u,%llu,%llu,%.17g,delayed\n",
                                 minisnn_neuron_model_name(model), repeat,
                                 (unsigned long long)tick,
                                 (unsigned long long)(tick + 1U), reward) > 0;
            }
        }
    }
    run.active_readout_channels = count_bits(active_mask);
    run.fingerprint = c7_audit_fingerprint_value(&fingerprint);
    run.finite = ok && c7_audit_fixture_all_finite(&fixture);
    run.completed = run.finite && run.actions ==
        (uint64_t)config->episodes * config->ticks_per_episode && run.spikes > 0U &&
        run.active_readout_channels > 0U && run.nondefault_action_count > 0U &&
        run.action_variation_count > 0U;
    c7_audit_fixture_destroy(&fixture);
    *out_run = run;
    return run.completed;
}

static int append_model_names(char *out, size_t size, const C7IntegratedAuditConfig *config)
{
    size_t used = 0U;
    if (out == NULL || size == 0U || config == NULL)
        return 0;
    out[0] = '\0';
    for (int model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
        if (config->model_enabled[model])
        {
            int written = snprintf(out + used, size - used, "%s%s", used == 0U ? "" : ", ",
                                   minisnn_neuron_model_name((MiniSNNNeuronModel)model));
            if (written < 0 || (size_t)written >= size - used)
                return 0;
            used += (size_t)written;
        }
    return used > 0U;
}

static const char *module_status(int enabled)
{
    return enabled ? "on" : "off";
}

static int operation_totals(const AuditRun *runs, size_t run_count,
                            uint64_t *out_ready_checkpoints,
                            uint64_t *out_pending_checkpoints,
                            uint64_t *out_resumes, uint64_t *out_replays)
{
    uint64_t checkpoints = 0U;
    uint64_t ready_checkpoints = 0U;
    uint64_t pending_checkpoints = 0U;
    uint64_t resumes = 0U;
    uint64_t replays = 0U;

    for (size_t index = 0U; index < run_count; index++)
    {
        checkpoints += runs[index].checkpoints;
        ready_checkpoints += runs[index].ready_checkpoints;
        pending_checkpoints += runs[index].pending_checkpoints;
        resumes += runs[index].resumes;
        replays += runs[index].replays;
    }
    *out_ready_checkpoints = ready_checkpoints;
    *out_pending_checkpoints = pending_checkpoints;
    *out_resumes = resumes;
    *out_replays = replays;
    return checkpoints == ready_checkpoints + pending_checkpoints;
}

static int write_report(const char *path, const C7IntegratedAuditConfig *config,
                        const AuditRun *runs, size_t run_count)
{
    FILE *file;
    char models[96];
    uint64_t ready_checkpoints, pending_checkpoints, resumes, replays;
    if (path == NULL || config == NULL || runs == NULL || !append_model_names(models, sizeof(models), config))
        return 0;
    if (!operation_totals(runs, run_count, &ready_checkpoints, &pending_checkpoints,
                          &resumes, &replays))
        return 0;
    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file,
                "<!doctype html><meta charset=\"utf-8\"><title>C7 integrated audit</title>"
                "<style>body{background:#121821;color:#e6edf3;font-family:system-ui;max-width:980px;margin:auto;padding:24px}table{border-collapse:collapse}th,td{border:1px solid #456;padding:7px}a{color:#8be9fd}</style>"
                "<h1>C7 integrated brain-agent audit</h1><p>Domain-neutral audit of AgentIO, encoder, neural activity, decoder, cycle, feedback, reset and checkpoint replay.</p>"
                "<table><tr><th>enabled models</th><td>%s</td></tr><tr><th>episodes</th><td>%u</td></tr><tr><th>ticks/episode</th><td>%u</td></tr><tr><th>brain steps/tick</th><td>%u</td></tr><tr><th>configured modules</th><td>STDP=%s, reward=%s, homeostasis=%s, structural=%s</td></tr></table>"
                "<h2>Executed matrix</h2><table><tr><th>model</th><th>repeat</th><th>input drive</th><th>model config signature</th><th>effective STDP</th><th>effective reward</th><th>effective homeostasis</th><th>effective structural plasticity</th><th>spikes</th><th>active readouts</th><th>nondefault actions</th><th>action variations</th><th>silent ticks</th><th>checkpoints/resumes/replays</th></tr>",
                models, config->episodes, config->ticks_per_episode, config->brain_steps_per_tick,
                module_status(config->stdp_enabled), module_status(config->reward_enabled),
                module_status(config->homeostasis_enabled),
                module_status(config->structural_enabled)) < 0)
    {
        fclose(file);
        return 0;
    }
    for (size_t index = 0U; index < run_count; index++)
        if (fprintf(file, "<tr><td>%s</td><td>%u</td><td>%.17g</td><td>%llu</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%llu</td><td>%llu</td><td>%llu</td><td>%llu</td><td>%llu</td><td>%llu/%llu/%llu</td></tr>",
                    minisnn_neuron_model_name(runs[index].model), runs[index].repeat,
                    runs[index].input_drive,
                    (unsigned long long)runs[index].model_config_signature,
                    module_status(config->stdp_enabled), module_status(config->reward_enabled),
                    module_status(config->homeostasis_enabled &&
                                  runs[index].model == MINISNN_NEURON_MODEL_LIF),
                    module_status(config->structural_enabled),
                    (unsigned long long)runs[index].spikes,
                    (unsigned long long)runs[index].active_readout_channels,
                    (unsigned long long)runs[index].nondefault_action_count,
                    (unsigned long long)runs[index].action_variation_count,
                    (unsigned long long)runs[index].silent_ticks,
                    (unsigned long long)runs[index].checkpoints,
                    (unsigned long long)runs[index].resumes,
                    (unsigned long long)runs[index].replays) < 0)
        {
            fclose(file);
            return 0;
        }
    if (fprintf(file,
                "</table><h2>Operations</h2><table><tr><th>operation</th><th>configured</th><th>attempted</th><th>completed</th><th>failed</th><th>not reached</th></tr>"
                "<tr data-operation=\"checkpoint-ready\"><td>checkpoint READY</td><td>%s</td><td>%llu</td><td>%llu</td><td>0</td><td>0</td></tr>"
                "<tr data-operation=\"checkpoint-action-pending\"><td>checkpoint ACTION_PENDING</td><td>%s</td><td>%llu</td><td>%llu</td><td>0</td><td>0</td></tr>"
                "<tr data-operation=\"resume\"><td>resume</td><td>%s</td><td>%llu</td><td>%llu</td><td>0</td><td>0</td></tr>"
                "<tr data-operation=\"replay\"><td>replay</td><td>%s</td><td>%llu</td><td>%llu</td><td>0</td><td>0</td></tr>"
                "<tr><td>long run</td><td>not configured</td><td>0</td><td>0</td><td>0</td><td>not reached</td></tr>"
                "<tr><td>evolution</td><td>not configured</td><td>0</td><td>0</td><td>0</td><td>not reached</td></tr></table>",
                config->checkpoint_ready_enabled ? "yes" : "no",
                (unsigned long long)ready_checkpoints,
                (unsigned long long)ready_checkpoints,
                config->checkpoint_pending_enabled ? "yes" : "no",
                (unsigned long long)pending_checkpoints,
                (unsigned long long)pending_checkpoints,
                (config->checkpoint_ready_enabled || config->checkpoint_pending_enabled) ? "yes" : "no",
                (unsigned long long)resumes,
                (unsigned long long)resumes,
                config->replay_enabled ? "yes" : "no",
                (unsigned long long)replays,
                (unsigned long long)replays) < 0)
    {
        fclose(file);
        return 0;
    }
    if (fprintf(file,
                "<h2>Limits</h2><p>This validates controlled numeric interface contracts. The calibrated currents come from the validated C5 demonstrations; they do not alter neuron equations. Silence is decoded as defaults, not as an active decision. This is not a world, body, task semantics, general agent or a frozen public API. D1 is the pre-Worlds stabilization audit.</p>"
                "<p><a href=\"c7_audit_runs.csv\">runs</a> | <a href=\"c7_audit_actions.csv\">actions</a> | <a href=\"c7_audit_feedback.csv\">feedback</a> | <a href=\"c7_audit_checkpoints.csv\">checkpoints</a> | <a href=\"c7_audit_model_summary.csv\">model summary</a> | <a href=\"config_used.ini\">effective config</a></p>") < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int main(int argc, char **argv)
{
    const char *config_path = argc == 2 ? argv[1] : "configs/c7_integrated_audit.ini";
    C7IntegratedAuditConfig config;
    char message[256] = "";
    char output[C7_AUDIT_PATH_MAX], source[C7_AUDIT_PATH_MAX], used[C7_AUDIT_PATH_MAX];
    char manifest[C7_AUDIT_PATH_MAX], runs_path[C7_AUDIT_PATH_MAX], actions_path[C7_AUDIT_PATH_MAX];
    char feedback_path[C7_AUDIT_PATH_MAX], checkpoints_path[C7_AUDIT_PATH_MAX];
    char summary_path[C7_AUDIT_PATH_MAX], report_path[C7_AUDIT_PATH_MAX];
    char model_summary_path[C7_AUDIT_PATH_MAX];
    AuditRun results[C7_AUDIT_MAX_RUNS];
    FILE *runs = NULL, *actions = NULL, *feedback = NULL, *checkpoints = NULL, *model_summary = NULL;
    size_t result_count = 0U;
    size_t model_count = 0U;
    uint64_t run_id;
    int success = 1;
    if (argc > 2 || !c7_integrated_audit_config_load_file(config_path, &config, message,
                                                            sizeof(message)))
    {
        printf("Erro ao carregar auditoria C7: %s\n", message);
        return 1;
    }
    if (!join_path(output, sizeof(output), C7_AUDIT_OUTPUT_ROOT, config.run_name) ||
        !join_path(source, sizeof(source), output, "config_source.ini") ||
        !join_path(used, sizeof(used), output, "config_used.ini") ||
        !join_path(manifest, sizeof(manifest), output, "c7_audit_manifest.txt") ||
        !join_path(runs_path, sizeof(runs_path), output, "c7_audit_runs.csv") ||
        !join_path(actions_path, sizeof(actions_path), output, "c7_audit_actions.csv") ||
        !join_path(feedback_path, sizeof(feedback_path), output, "c7_audit_feedback.csv") ||
        !join_path(checkpoints_path, sizeof(checkpoints_path), output, "c7_audit_checkpoints.csv") ||
        !join_path(model_summary_path, sizeof(model_summary_path), output, "c7_audit_model_summary.csv") ||
        !join_path(summary_path, sizeof(summary_path), output, "c7_audit_summary.txt") ||
        !join_path(report_path, sizeof(report_path), output, "c7_audit_report.html") ||
        !c7_audit_ensure_directory("results") ||
        !c7_audit_ensure_directory(C7_AUDIT_OUTPUT_ROOT) ||
        !c7_audit_ensure_directory(output))
        return 1;
    cleanup_known_artifacts(output);
    if (!copy_file_bytes(config_path, source) ||
        !c7_integrated_audit_config_write_file(used, &config, message, sizeof(message)))
        return 1;
    run_id = run_id_for(&config);
    runs = fopen(runs_path, "wb"); actions = fopen(actions_path, "wb");
    feedback = fopen(feedback_path, "wb"); checkpoints = fopen(checkpoints_path, "wb");
    model_summary = fopen(model_summary_path, "wb");
    if (runs == NULL || actions == NULL || feedback == NULL || checkpoints == NULL ||
        model_summary == NULL ||
        fputs("model,seed,repeat,ticks,neural_steps,total_spikes,actions,resets,reward,checkpoint_count,resume_count,replay_count,active_readout_channels,nondefault_action_count,action_variation_count,silent_ticks,input_drive,model_config_signature,fingerprint,final_state,finite,completed\n", runs) < 0 ||
        fputs("model,repeat,tick,episode,output_a,output_b,target_signal,context_output\n", actions) < 0 ||
        fputs("model,repeat,source_tick,delivery_tick,reward,type\n", feedback) < 0 ||
        fputs("model,repeat,boundary,path,save_completed,load_completed,resume_completed,replay_completed,state_signature\n", checkpoints) < 0 ||
        fputs("model,runs,deterministic,finite,active_readout_channels,nondefault_actions,action_variations,stdp,reward,homeostasis,structural,input_drive,model_config_signature\n", model_summary) < 0)
        success = 0;
    for (int model = MINISNN_NEURON_MODEL_LIF; success &&
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        AuditRun first = {0};
        AuditRun second = {0};
        const unsigned int repeat_count = config.replay_enabled ? 2U : 1U;
        const int homeostasis = config.homeostasis_enabled && model == MINISNN_NEURON_MODEL_LIF;
        if (!config.model_enabled[model])
            continue;
        success = run_audit_once(&config, (MiniSNNNeuronModel)model, config.seed, 0U,
                                 output, actions, feedback, checkpoints, run_id, &first);
        if (success && repeat_count == 2U)
            success = run_audit_once(&config, (MiniSNNNeuronModel)model, config.seed, 1U,
                                     output, actions, feedback, checkpoints, run_id, &second);
        if (success && result_count + repeat_count <= C7_AUDIT_MAX_RUNS)
        {
            results[result_count++] = first;
            if (repeat_count == 2U)
                results[result_count++] = second;
            for (unsigned int repeat = 0U; success && repeat < repeat_count; repeat++)
            {
                const AuditRun *run = repeat == 0U ? &first : &second;
                success = fprintf(runs,
                    "%s,%llu,%u,%llu,%llu,%llu,%llu,%llu,%.17g,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.17g,%llu,%llu,READY,%d,%d\n",
                    minisnn_neuron_model_name((MiniSNNNeuronModel)model),
                    (unsigned long long)config.seed, repeat,
                    (unsigned long long)run->ticks, (unsigned long long)run->neural_steps,
                    (unsigned long long)run->spikes, (unsigned long long)run->actions,
                    (unsigned long long)run->resets, run->reward,
                    (unsigned long long)run->checkpoints, (unsigned long long)run->resumes,
                    (unsigned long long)run->replays,
                    (unsigned long long)run->active_readout_channels,
                    (unsigned long long)run->nondefault_action_count,
                    (unsigned long long)run->action_variation_count,
                    (unsigned long long)run->silent_ticks, run->input_drive,
                    (unsigned long long)run->model_config_signature,
                    (unsigned long long)run->fingerprint, run->finite, run->completed) > 0;
            }
            if (success)
            {
                const int deterministic = repeat_count == 1U || first.fingerprint == second.fingerprint;
                success = deterministic && fprintf(model_summary,
                    "%s,%u,%d,%d,%llu,%llu,%llu,%d,%d,%d,%d,%.17g,%llu\n",
                    minisnn_neuron_model_name((MiniSNNNeuronModel)model), repeat_count,
                    deterministic, first.finite && (repeat_count == 1U || second.finite),
                    (unsigned long long)first.active_readout_channels,
                    (unsigned long long)first.nondefault_action_count,
                    (unsigned long long)first.action_variation_count,
                    config.stdp_enabled, config.reward_enabled, homeostasis,
                    config.structural_enabled, config.input_drive[model],
                    (unsigned long long)first.model_config_signature) > 0;
                model_count++;
            }
        }
        else if (success)
            success = 0;
    }
    if (runs != NULL && fclose(runs) != 0) success = 0;
    if (actions != NULL && fclose(actions) != 0) success = 0;
    if (feedback != NULL && fclose(feedback) != 0) success = 0;
    if (checkpoints != NULL && fclose(checkpoints) != 0) success = 0;
    if (model_summary != NULL && fclose(model_summary) != 0) success = 0;
    if (success)
    {
        FILE *summary = fopen(summary_path, "wb");
        FILE *manifest_file = fopen(manifest, "wb");
        success = summary != NULL && manifest_file != NULL &&
            fprintf(summary, "c7_audit_runs=%zu\nmodels=%zu\nrun_id=%llu\ndeterministic=yes\nstatus=PASSOU\n",
                    result_count, model_count, (unsigned long long)run_id) > 0 &&
            fclose(summary) == 0 &&
            fprintf(manifest_file, "protocol=c7_integrated_audit_v2\nrun_id=%llu\nseed=%llu\nmodels=%zu\ncheckpoint=agent_cycle_v1\n",
                    (unsigned long long)run_id, (unsigned long long)config.seed, model_count) > 0 &&
            fclose(manifest_file) == 0 && write_report(report_path, &config, results, result_count);
    }
    printf(success ? "C7 integrated audit OK: %s\n" : "C7 integrated audit failed\n", output);
    return success ? 0 : 1;
}
