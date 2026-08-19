#include "wf1_trainable_fish.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define WF1_RESULT_DIRECTORY "build/worlds/scenarios/wf1_trainable_fish/results"

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
    const char *directory = argc > 1 ? argv[1] : WF1_RESULT_DIRECTORY;
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    WF1TrainableFishExperimentSummary summary;

    if (argc > 2 || !ensure_directory("build") ||
        !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory(directory))
    {
        fprintf(stderr, "WF1-A could not create result directory: %s\n", directory);
        return 1;
    }
    if (!wf1_trainable_fish_run_experiment(&config, directory, &summary))
    {
        fputs("WF1-A experiment failed.\n", stderr);
        return 1;
    }
    printf("WF1-A experiment complete\n"
           "untrained_mean_food=%.6g\n"
           "trained_mean_food=%.6g\n"
           "control_mean_food=%.6g\n"
           "result=%s\n"
           "artifacts=%s/{summary.txt,training.csv,evaluation.csv,seeds.csv}\n",
           summary.untrained_mean_food, summary.trained_mean_food,
           summary.control_mean_food,
           wf1_trainable_fish_result_name(summary.result), directory);
    return 0;
}