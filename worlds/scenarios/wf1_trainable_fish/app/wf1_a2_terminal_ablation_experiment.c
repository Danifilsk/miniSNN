#include "wf1_a2_terminal_ablation.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define WF1_A2_RESULTS "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a2"

static int ensure_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : WF1_A2_RESULTS;
    WF1A2TerminalAblationSummary summary;
    if (argc > 2 || !ensure_directory("build") || !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") || !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish/results") || !ensure_directory(directory) ||
        !wf1_a2_terminal_ablation_run_experiment(directory, &summary))
    {
        fputs("WF1-A.2 terminal reward ablation failed.\n", stderr);
        return 1;
    }
    printf("WF1-A.2 terminal reward ablation complete\nterminal_reward=0\npositive_rewards=%llu\ntraining_starvations=%llu\nPOLICY_COLLAPSE=%s\nresult=%s\n",
        (unsigned long long)summary.positive_rewards, (unsigned long long)summary.training_starvations,
        summary.policy_collapse ? "YES" : "NO", wf1_trainable_fish_result_name(summary.result));
    return 0;
}