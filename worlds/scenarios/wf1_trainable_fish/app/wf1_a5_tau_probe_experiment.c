#include "wf1_a5_tau_probe.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define WF1_A5_RESULTS "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a5"

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
    const char *directory = argc == 2 ? argv[1] : WF1_A5_RESULTS;
    WF1A5TauProbeSummary summary;

    if (argc > 2 || !ensure_directory("build") || !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish/results") ||
        !ensure_directory(directory) || !wf1_a5_tau_probe_run(directory, &summary))
    {
        fputs("WF1-A.5 eligibility tau probe failed.\n", stderr);
        return 1;
    }
    printf("WF1-A.5 eligibility tau probe complete\npre_reward_equivalence=%s\n"
           "CLAMPS=%s\nDIAGNOSIS=%s\n",
        summary.pre_reward_equivalent ? "PASS" : "FAIL",
        summary.clamps_zero ? "ZERO" : "OBSERVED",
        wf1_a5_tau_probe_diagnosis_name(summary.diagnosis));
    return 0;
}