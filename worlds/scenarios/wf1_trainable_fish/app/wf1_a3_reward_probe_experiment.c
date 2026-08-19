#include "wf1_a3_reward_probe.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define WF1_A3_RESULTS "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a3"

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
    const char *directory = argc == 2 ? argv[1] : WF1_A3_RESULTS;
    WF1A3RewardProbeSummary summary;

    if (argc > 2 || !ensure_directory("build") || !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish/results") ||
        !ensure_directory(directory) || !wf1_a3_reward_probe_run(directory, &summary))
    {
        fputs("WF1-A.3 reward probe failed.\n", stderr);
        return 1;
    }
    printf("WF1-A.3 reward probe complete\nrewards_observed=%u\nepisodes_run=%u\n"
           "mean_modified_connections=%.17g\n",
           summary.rewards_observed, summary.episodes_run,
           summary.mean_modified_connection_count);
    return 0;
}