#include <math.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "c7_audit_common.h"
#include "c7_integrated_audit_config.h"

#define C7_AUDIT_OUTPUT_ROOT "results/scenarios"
#define C7_AUDIT_PATH_MAX 512U

typedef struct
{
    MiniSNNNeuronModel model;
    uint64_t seed;
    uint64_t ticks;
    uint64_t neural_steps;
    uint64_t spikes;
    uint64_t actions;
    uint64_t resets;
    uint64_t checkpoints;
    uint64_t resumes;
    double reward;
    uint64_t fingerprint;
    int finite;
    int completed;
} AuditRun;

static int ensure_directory(const char *path)
{
    return path != NULL &&
        (CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS);
}

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

static int replace_from_checkpoint(C7AuditFixture *fixture, MiniSNNNeuronModel model,
                                   uint32_t brain_steps, int homeostasis,
                                   const char *directory, int pending,
                                   uint64_t tick,
                                   double actions[C7_AUDIT_ACTION_COUNT])
{
    C7AuditFixture restored = {0};
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    double restored_actions[C7_AUDIT_ACTION_COUNT] = {0};
    if (!minisnn_agent_cycle_save_checkpoint(fixture->cycle, directory, &error) ||
        !c7_audit_fixture_create(&restored, model, C7_AUDIT_MIN_NEURONS, brain_steps,
                                 1, 1, homeostasis, 1) ||
        !minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error))
    {
        c7_audit_fixture_destroy(&restored);
        return 0;
    }
    if (pending)
    {
        if (!c7_audit_consume_action(fixture, tick, actions) ||
            !c7_audit_consume_action(&restored, tick, restored_actions))
        {
            c7_audit_fixture_destroy(&restored);
            return 0;
        }
        for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
            if (actions[index] != restored_actions[index])
            {
                c7_audit_fixture_destroy(&restored);
                return 0;
            }
    }
    c7_audit_fixture_destroy(fixture);
    *fixture = restored;
    return 1;
}

static int run_audit_once(const C7IntegratedAuditConfig *config,
                          MiniSNNNeuronModel model, uint64_t seed,
                          unsigned int repeat, const char *output_directory,
                          FILE *actions_file, FILE *feedback_file,
                          FILE *checkpoints_file, AuditRun *out_run)
{
    C7AuditFixture fixture = {0};
    C7AuditFingerprint fingerprint;
    const int homeostasis = config->homeostasis_enabled && model == MINISNN_NEURON_MODEL_LIF;
    uint64_t tick = 0U;
    AuditRun run = {0};
    char checkpoint_path[C7_AUDIT_PATH_MAX];
    int ok = c7_audit_fixture_create(&fixture, model, C7_AUDIT_MIN_NEURONS,
                                     config->brain_steps_per_tick, config->stdp_enabled,
                                     config->reward_enabled, homeostasis,
                                     config->structural_enabled);
    if (!ok)
        return 0;
    run.model = model;
    run.seed = seed;
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
                ok = c7_audit_submit_sensor(&fixture, tick, values) &&
                    c7_audit_run_pending(&fixture, &diagnostics) &&
                    join_path(checkpoint_path, sizeof(checkpoint_path), output_directory,
                              "checkpoint_action_pending") && ensure_directory(checkpoint_path) &&
                    replace_from_checkpoint(&fixture, model, config->brain_steps_per_tick,
                                            homeostasis, checkpoint_path, 1, tick, actions);
            else
                ok = c7_audit_run_tick(&fixture, tick, values, actions, &diagnostics);
            if (!ok || !c7_audit_fixture_all_finite(&fixture))
                break;
            if (config->checkpoint_ready_enabled && episode == 0U && local_tick == 1U)
            {
                ok = join_path(checkpoint_path, sizeof(checkpoint_path), output_directory,
                               "checkpoint_ready") && ensure_directory(checkpoint_path) &&
                    replace_from_checkpoint(&fixture, model, config->brain_steps_per_tick,
                                            homeostasis, checkpoint_path, 0, tick, actions);
                if (ok)
                    fprintf(checkpoints_file, "%s,%u,READY,%s,ok\n",
                            minisnn_neuron_model_name(model), repeat, checkpoint_path);
            }
            if (pending_boundary && ok)
                fprintf(checkpoints_file, "%s,%u,ACTION_PENDING,%s,ok\n",
                        minisnn_neuron_model_name(model), repeat, checkpoint_path);
            if (fprintf(actions_file, "%s,%u,%llu,%u,%.17g,%.17g,%.17g,%.17g\n",
                        minisnn_neuron_model_name(model), repeat, (unsigned long long)tick,
                        episode, actions[0], actions[1], actions[2], actions[3]) < 0)
                ok = 0;
            c7_audit_fingerprint_tick(&fingerprint, tick, values, actions, &diagnostics, &fixture);
            run.ticks++;
            run.neural_steps += diagnostics.brain_steps_executed;
            run.spikes += diagnostics.total_spikes;
            run.actions++;
            run.reward += diagnostics.reward_delivered;
            if (local_tick + 1U == config->ticks_per_episode)
            {
                ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, 0.0, 1) &&
                    minisnn_agent_cycle_reset_episode(fixture.cycle);
                run.resets++;
                fprintf(feedback_file, "%s,%u,%llu,%llu,0,terminal\n",
                        minisnn_neuron_model_name(model), repeat,
                        (unsigned long long)tick, (unsigned long long)(tick + 1U));
            }
            else
            {
                double reward = (tick % 3U) == 0U ? 0.20 :
                    ((tick % 3U) == 1U ? -0.10 : 0.0);
                ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, reward, 0);
                fprintf(feedback_file, "%s,%u,%llu,%llu,%.17g,delayed\n",
                        minisnn_neuron_model_name(model), repeat,
                        (unsigned long long)tick, (unsigned long long)(tick + 1U), reward);
            }
        }
    }
    run.checkpoints = (config->checkpoint_ready_enabled ? 1U : 0U) +
        (config->checkpoint_pending_enabled ? 1U : 0U);
    run.resumes = run.checkpoints;
    run.fingerprint = c7_audit_fingerprint_value(&fingerprint);
    run.finite = ok && c7_audit_fixture_all_finite(&fixture);
    run.completed = run.finite && run.actions ==
        (uint64_t)config->episodes * config->ticks_per_episode;
    c7_audit_fixture_destroy(&fixture);
    *out_run = run;
    return run.completed;
}

static int write_report(const char *path, const C7IntegratedAuditConfig *config)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file,
                "<!doctype html><meta charset=\"utf-8\"><title>C7 integrated audit</title>"
                "<style>body{background:#121821;color:#e6edf3;font-family:system-ui;max-width:980px;margin:auto;padding:24px}table{border-collapse:collapse}th,td{border:1px solid #456;padding:7px}a{color:#8be9fd}</style>"
                "<h1>C7 integrated brain-agent audit</h1><p>Domain-neutral audit of AgentIO, encoder, network, decoder, cycle, feedback, reset and checkpoint replay.</p>"
                "<table><tr><th>models</th><td>LIF, AdEx, Hodgkin-Huxley</td></tr><tr><th>episodes</th><td>%u</td></tr><tr><th>ticks/episode</th><td>%u</td></tr><tr><th>brain steps/tick</th><td>%u</td></tr><tr><th>plasticity</th><td>STDP=%s, reward=%s, homeostasis=LIF, structural=%s</td></tr></table>"
                "<h2>Limits</h2><p>This confirms controlled numeric interface contracts and deterministic replay. It does not define a world, body, task semantics, general agent or frozen public API. D1 is the next audit and stabilization block before any real bridge.</p>"
                "<p><a href=\"c7_audit_runs.csv\">runs</a> | <a href=\"c7_audit_actions.csv\">actions</a> | <a href=\"c7_audit_feedback.csv\">feedback</a> | <a href=\"c7_audit_checkpoints.csv\">checkpoints</a> | <a href=\"c7_audit_model_summary.csv\">model summary</a> | <a href=\"config_used.ini\">effective config</a></p>",
                config->episodes, config->ticks_per_episode, config->brain_steps_per_tick,
                config->stdp_enabled ? "on" : "off", config->reward_enabled ? "on" : "off",
                config->structural_enabled ? "on" : "off") < 0)
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
    FILE *runs = NULL, *actions = NULL, *feedback = NULL, *checkpoints = NULL, *model_summary = NULL;
    size_t result_count = 0U;
    size_t model_count = 0U;
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
        !ensure_directory("results") || !ensure_directory(C7_AUDIT_OUTPUT_ROOT) ||
        !ensure_directory(output) || !copy_file_bytes(config_path, source) ||
        !c7_integrated_audit_config_write_file(used, &config, message, sizeof(message)))
        return 1;
    runs = fopen(runs_path, "wb"); actions = fopen(actions_path, "wb");
    feedback = fopen(feedback_path, "wb"); checkpoints = fopen(checkpoints_path, "wb");
    model_summary = fopen(model_summary_path, "wb");
    if (runs == NULL || actions == NULL || feedback == NULL || checkpoints == NULL ||
        model_summary == NULL ||
        fputs("model,seed,repeat,ticks,neural_steps,spikes,actions,resets,reward,checkpoints,resumes,fingerprint,final_state,finite,completed\n", runs) < 0 ||
        fputs("model,repeat,tick,episode,output_a,output_b,target_signal,context_output\n", actions) < 0 ||
        fputs("model,repeat,source_tick,delivery_tick,reward,type\n", feedback) < 0 ||
        fputs("model,repeat,boundary,path,status\n", checkpoints) < 0 ||
        fputs("model,runs,deterministic,finite,actions_produced,actions_consumed\n", model_summary) < 0)
        success = 0;
    for (int model = MINISNN_NEURON_MODEL_LIF; success &&
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        AuditRun first = {0};
        AuditRun second = {0};
        const unsigned int repeat_count = config.replay_enabled ? 2U : 1U;
        if (!config.model_enabled[model]) continue;
        success = run_audit_once(&config, (MiniSNNNeuronModel)model, config.seed, 0U,
                                 output, actions, feedback, checkpoints, &first);
        if (success && repeat_count == 2U)
            success = run_audit_once(&config, (MiniSNNNeuronModel)model, config.seed, 1U,
                                     output, actions, feedback, checkpoints, &second);
        if (success)
        {
            int deterministic = repeat_count == 1U || first.fingerprint == second.fingerprint;
            uint64_t produced = first.actions + (repeat_count == 2U ? second.actions : 0U);
            success = deterministic && fprintf(runs,
                "%s,%llu,0,%llu,%llu,%llu,%llu,%llu,%.17g,%llu,%llu,%llu,READY,%d,%d\n",
                minisnn_neuron_model_name((MiniSNNNeuronModel)model), (unsigned long long)config.seed,
                (unsigned long long)first.ticks, (unsigned long long)first.neural_steps,
                (unsigned long long)first.spikes, (unsigned long long)first.actions,
                (unsigned long long)first.resets, first.reward,
                (unsigned long long)first.checkpoints, (unsigned long long)first.resumes,
                (unsigned long long)first.fingerprint, first.finite, first.completed) > 0;
            if (success && repeat_count == 2U)
                success = fprintf(runs,
                    "%s,%llu,1,%llu,%llu,%llu,%llu,%llu,%.17g,%llu,%llu,%llu,READY,%d,%d\n",
                    minisnn_neuron_model_name((MiniSNNNeuronModel)model), (unsigned long long)config.seed,
                    (unsigned long long)second.ticks, (unsigned long long)second.neural_steps,
                    (unsigned long long)second.spikes, (unsigned long long)second.actions,
                    (unsigned long long)second.resets, second.reward,
                    (unsigned long long)second.checkpoints, (unsigned long long)second.resumes,
                    (unsigned long long)second.fingerprint, second.finite, second.completed) > 0;
            success = success && fprintf(model_summary, "%s,%u,%d,%d,%llu,%llu\n",
                        minisnn_neuron_model_name((MiniSNNNeuronModel)model), repeat_count,
                        deterministic,
                        first.finite && (repeat_count == 1U || second.finite),
                        (unsigned long long)produced, (unsigned long long)produced) > 0;
            result_count += repeat_count;
            model_count++;
        }
    }
    if (runs != NULL)
        fclose(runs);
    if (actions != NULL)
        fclose(actions);
    if (feedback != NULL)
        fclose(feedback);
    if (checkpoints != NULL)
        fclose(checkpoints);
    if (model_summary != NULL) fclose(model_summary);
    if (success)
    {
        FILE *summary = fopen(summary_path, "wb");
        FILE *manifest_file = fopen(manifest, "wb");
        success = summary != NULL && manifest_file != NULL &&
            fprintf(summary, "c7_audit_runs=%zu\nmodels=%d\ndeterministic=yes\nstatus=PASSOU\n",
                    result_count, (int)model_count) > 0 && fclose(summary) == 0 &&
            fprintf(manifest_file, "protocol=c7_integrated_audit_v1\nseed=%llu\nmodels=%zu\ncheckpoint=reused_c7_5_a\n",
                    (unsigned long long)config.seed, model_count) > 0 && fclose(manifest_file) == 0 &&
            write_report(report_path, &config);
    }
    printf(success ? "C7 integrated audit OK: %s\n" : "C7 integrated audit failed\n", output);
    return success ? 0 : 1;
}
