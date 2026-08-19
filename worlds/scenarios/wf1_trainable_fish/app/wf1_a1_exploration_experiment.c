#include "wf1_a1_exploration.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define WF1_A1_RESULT_DIRECTORY "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a1"

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
    const char *directory = argc > 1 ? argv[1] : WF1_A1_RESULT_DIRECTORY;
    WF1A1ExplorationSummary summary;

    if (argc > 2 || !ensure_directory("build") ||
        !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish/results") ||
        !ensure_directory(directory))
    {
        fprintf(stderr, "WF1-A.1 could not create result directory: %s\n", directory);
        return 1;
    }
    if (!wf1_a1_exploration_run_experiment(directory, &summary))
    {
        fputs("WF1-A.1 experiment failed.\n", stderr);
        return 1;
    }
    printf("WF1-A.1 experiment complete\n"
           "calibration_config=%s\n"
           "exploration_bootstrapped=%s\n"
           "positive_reward_reached=%s\n"
           "successful_eats=%llu\n"
           "positive_rewards=%llu\n"
           "result=%s\n"
           "artifacts=%s/{summary.txt,training.csv,evaluation.csv,seeds.csv}\n",
           summary.calibration_config,
           summary.exploration_bootstrapped ? "YES" : "NO",
           summary.positive_reward_reached ? "YES" : "NO",
           (unsigned long long)summary.training_successful_eats,
           (unsigned long long)summary.positive_rewards,
           wf1_trainable_fish_result_name(summary.result), directory);
    return 0;
}