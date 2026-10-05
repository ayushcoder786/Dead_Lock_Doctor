#ifndef DEADLOCKDOCTOR_GRAPH_H
#define DEADLOCKDOCTOR_GRAPH_H

#include "common.h"
#include "process.h"
#include "resource.h"

typedef struct {
    int num_nodes;
    int capacity;
    int *node_ids;
    char **node_names;
    bool **adj;                    /* adj[u][v] == true means u waits for v (u -> v) */
} WaitForGraph;

void wfg_init(WaitForGraph *g);
void wfg_cleanup(WaitForGraph *g);
int wfg_add_node(WaitForGraph *g, int id, const char *name);
int wfg_get_node_index(const WaitForGraph *g, int id);
void wfg_add_edge(WaitForGraph *g, int from_idx, int to_idx);
bool wfg_has_edge(const WaitForGraph *g, int from_idx, int to_idx);
bool wfg_find_cycle_dfs(const WaitForGraph *g, int *cycle, int *cycle_len);
void wfg_print(const WaitForGraph *g);
void wfg_build_from_system(WaitForGraph *wfg, const ProcessManager *pm, const ResourceManager *rm);
void graph_visualize(const ProcessManager *pm, const ResourceManager *rm);

#endif /* DEADLOCKDOCTOR_GRAPH_H */

