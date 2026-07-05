#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include "network.h"

void topology_chain(Network *net);

void topology_ring(Network *net);

void topology_all_to_all(Network *net);

void topology_random(Network *net, double probability);

#endif