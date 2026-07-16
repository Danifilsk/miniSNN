#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "network.h"

int topology_chain(Network *net);

int topology_ring(Network *net);

int topology_all_to_all(Network *net);

int topology_random(Network *net, double probability);

int topology_random_balanced(
    Network *net,
    double probability,
    double inhibitory_fraction);

#endif
