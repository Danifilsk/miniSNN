#include "wf1_a4_pilot.h"

#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define WF1_A4P_RESULTS "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a4_pilot"
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
    const char *directory = argc == 2 ? argv[1] : WF1_A4P_RESULTS;
    WF1A4PilotSummary summary;
    if (argc > 2 || !ensure_directory("build") || !ensure_directory("build/worlds") ||
        !ensure_directory("build/worlds/scenarios") || !ensure_directory("build/worlds/scenarios/wf1_trainable_fish") ||
        !ensure_directory("build/worlds/scenarios/wf1_trainable_fish/results") || !ensure_directory(directory) ||
        !wf1_a4_pilot_run(directory, &summary))
    { fputs("WF1-A.4P pilot failed.\n", stderr); return 1; }
    printf("WF1-A.4P complete\npositive_rewards=%llu\nclamp_events=%llu\nRESULT=%s\n",
        (unsigned long long)summary.positive_rewards, (unsigned long long)summary.clamp_events,
        summary.improvement ? "PILOT_IMPROVEMENT" : "PILOT_NO_IMPROVEMENT");
    return 0;
}