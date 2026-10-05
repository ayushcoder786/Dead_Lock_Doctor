#include "graph.h"

void wfg_init(WaitForGraph *g) {
    if (!g) return;
    memset(g, 0, sizeof(WaitForGraph));
}

void wfg_cleanup(WaitForGraph *g) {
    if (!g) return;

    if (g->node_names) {
        for (int i = 0; i < g->capacity; i++) {
            if (g->node_names[i]) free(g->node_names[i]);
        }
        free(g->node_names);
        g->node_names = NULL;
    }

    if (g->adj) {
        for (int i = 0; i < g->capacity; i++) {
            if (g->adj[i]) free(g->adj[i]);
        }
        free(g->adj);
        g->adj = NULL;
    }

    if (g->node_ids) {
        free(g->node_ids);
        g->node_ids = NULL;
    }

    g->num_nodes = 0;
    g->capacity = 0;
}

int wfg_add_node(WaitForGraph *g, int id, const char *name) {
    if (!g || !name) return -1;

    /* Check if node already exists */
    for (int i = 0; i < g->num_nodes; i++) {
        if (g->node_ids[i] == id) {
            return i;
        }
    }

    /* Grow dynamic arrays if needed */
    if (g->num_nodes >= g->capacity) {
        int old_cap = g->capacity;
        int new_cap = old_cap == 0 ? 4 : old_cap * 2;

        int *new_ids = (int *)realloc(g->node_ids, new_cap * sizeof(int));
        if (!new_ids) return -1;
        g->node_ids = new_ids;

        char **new_names = (char **)realloc(g->node_names, new_cap * sizeof(char *));
        if (!new_names) return -1;
        g->node_names = new_names;
        for (int i = old_cap; i < new_cap; i++) {
            g->node_names[i] = (char *)calloc(MAX_NAME_LEN, sizeof(char));
        }

        bool **new_adj = (bool **)realloc(g->adj, new_cap * sizeof(bool *));
        if (!new_adj) return -1;
        g->adj = new_adj;

        /* Resize existing rows */
        for (int i = 0; i < old_cap; i++) {
            bool *row = (bool *)realloc(g->adj[i], new_cap * sizeof(bool));
            if (!row) return -1;
            g->adj[i] = row;
            memset(g->adj[i] + old_cap, 0, (new_cap - old_cap) * sizeof(bool));
        }

        /* Allocate new rows */
        for (int i = old_cap; i < new_cap; i++) {
            g->adj[i] = (bool *)calloc(new_cap, sizeof(bool));
        }

        g->capacity = new_cap;
    }

    int idx = g->num_nodes++;
    g->node_ids[idx] = id;
    strncpy(g->node_names[idx], name, MAX_NAME_LEN - 1);
    g->node_names[idx][MAX_NAME_LEN - 1] = '\0';

    for (int j = 0; j < g->capacity; j++) {
        g->adj[idx][j] = false;
        g->adj[j][idx] = false;
    }

    return idx;
}

int wfg_get_node_index(const WaitForGraph *g, int id) {
    if (!g) return -1;
    for (int i = 0; i < g->num_nodes; i++) {
        if (g->node_ids[i] == id) {
            return i;
        }
    }
    return -1;
}

void wfg_add_edge(WaitForGraph *g, int from_idx, int to_idx) {
    if (!g) return;
    if (from_idx >= 0 && from_idx < g->num_nodes &&
        to_idx >= 0 && to_idx < g->num_nodes) {
        g->adj[from_idx][to_idx] = true;
    }
}

bool wfg_has_edge(const WaitForGraph *g, int from_idx, int to_idx) {
    if (!g) return false;
    if (from_idx >= 0 && from_idx < g->num_nodes &&
        to_idx >= 0 && to_idx < g->num_nodes) {
        return g->adj[from_idx][to_idx];
    }
    return false;
}

/* DFS cycle detection helper */
static bool dfs_cycle(const WaitForGraph *g, int u, int color[], int parent[],
                      int *cycle, int *cycle_len) {
    color[u] = 1; /* GRAY - currently visiting */

    for (int v = 0; v < g->num_nodes; v++) {
        if (g->adj[u][v]) {
            if (color[v] == 1) {
                /* Back-edge (u -> v) detected: construct cycle */
                int *temp = (int *)malloc(g->num_nodes * sizeof(int));
                int count = 0;
                int curr = u;
                temp[count++] = curr;
                while (curr != v && curr != -1) {
                    curr = parent[curr];
                    if (count < g->num_nodes) {
                        temp[count++] = curr;
                    } else {
                        break;
                    }
                }

                *cycle_len = 0;
                for (int i = count - 1; i >= 0; i--) {
                    cycle[(*cycle_len)++] = temp[i];
                }
                cycle[(*cycle_len)++] = v;
                free(temp);
                return true;
            } else if (color[v] == 0) {
                parent[v] = u;
                if (dfs_cycle(g, v, color, parent, cycle, cycle_len)) {
                    return true;
                }
            }
        }
    }

    color[u] = 2; /* BLACK - finished visiting */
    return false;
}

bool wfg_find_cycle_dfs(const WaitForGraph *g, int *cycle, int *cycle_len) {
    if (!g || !cycle || !cycle_len) return false;

    *cycle_len = 0;
    if (g->num_nodes == 0) {
        return false;
    }

    int *color = (int *)calloc(g->num_nodes, sizeof(int));
    int *parent = (int *)malloc(g->num_nodes * sizeof(int));
    if (!color || !parent) {
        if (color) free(color);
        if (parent) free(parent);
        return false;
    }

    for (int i = 0; i < g->num_nodes; i++) {
        parent[i] = -1;
    }

    bool has_cycle = false;
    for (int i = 0; i < g->num_nodes; i++) {
        if (color[i] == 0) {
            if (dfs_cycle(g, i, color, parent, cycle, cycle_len)) {
                has_cycle = true;
                break;
            }
        }
    }

    free(color);
    free(parent);
    return has_cycle;
}

void wfg_print(const WaitForGraph *g) {
    if (!g || g->num_nodes == 0) {
        printf("  [Empty Wait-For Graph: No active nodes]\n");
        return;
    }

    printf("+---------------------------------------------------+\n");
    printf("| WAIT-FOR GRAPH ADJACENCY MATRIX                   |\n");
    printf("+---------------------------------------------------+\n");
    printf("Nodes (%d): ", g->num_nodes);
    for (int i = 0; i < g->num_nodes; i++) {
        printf("%s%s", g->node_names[i], (i == g->num_nodes - 1) ? "\n" : ", ");
    }
    printf("\n    ");
    for (int j = 0; j < g->num_nodes; j++) {
        printf("%-5s", g->node_names[j]);
    }
    printf("\n");

    for (int i = 0; i < g->num_nodes; i++) {
        printf("%-4s", g->node_names[i]);
        for (int j = 0; j < g->num_nodes; j++) {
            printf("%-5s", g->adj[i][j] ? "1" : "0");
        }
        printf("\n");
    }
    printf("+---------------------------------------------------+\n");
}

void wfg_build_from_system(WaitForGraph *wfg, const ProcessManager *pm, const ResourceManager *rm) {
    if (!wfg || !pm || !rm) return;
    wfg_init(wfg);

    /* 1. Add all active processes as nodes */
    for (int i = 0; i < pm->count; i++) {
        const Process *p = pm->processes[i];
        if (!p) continue;
        if (p->state == PROCESS_STATE_COMPLETED || p->state == PROCESS_STATE_ABORTED) {
            continue;
        }
        wfg_add_node(wfg, p->id, p->name);
    }

    /* 2. Build wait-for edges: waiting process -> holding process */
    for (int i = 0; i < pm->count; i++) {
        const Process *p_u = pm->processes[i];
        if (!p_u) continue;
        if (p_u->state == PROCESS_STATE_COMPLETED || p_u->state == PROCESS_STATE_ABORTED) {
            continue;
        }

        int u_idx = wfg_get_node_index(wfg, p_u->id);
        if (u_idx < 0) continue;

        for (int r = 0; r < rm->count; r++) {
            int req_val = (r < p_u->res_capacity) ? p_u->requested[r] : 0;
            if (req_val > 0) {
                for (int j = 0; j < pm->count; j++) {
                    if (j == i) continue;
                    const Process *p_v = pm->processes[j];
                    if (!p_v) continue;
                    if (p_v->state == PROCESS_STATE_COMPLETED || p_v->state == PROCESS_STATE_ABORTED) {
                        continue;
                    }

                    int alloc_val = (r < p_v->res_capacity) ? p_v->allocated[r] : 0;
                    if (alloc_val > 0) {
                        int v_idx = wfg_get_node_index(wfg, p_v->id);
                        if (v_idx >= 0) {
                            wfg_add_edge(wfg, u_idx, v_idx);
                        }
                    }
                }
            }
        }
    }
}

void graph_visualize(const ProcessManager *pm, const ResourceManager *rm) {
    if (!pm || !rm) return;

    printf("\n==============================================================\n");
    printf("           RESOURCE ALLOCATION & WAIT-FOR GRAPH VIEWER        \n");
    printf("==============================================================\n\n");

    /* 1. Summary of Graph Components */
    printf("[GRAPH NODES]\n");
    printf("  Processes (%d): ", pm->count);
    if (pm->count == 0) {
        printf("(none)\n");
    } else {
        for (int i = 0; i < pm->count; i++) {
            if (pm->processes[i]) {
                printf("%s (PID %d)%s", pm->processes[i]->name, pm->processes[i]->id,
                       (i == pm->count - 1) ? "\n" : ", ");
            }
        }
    }

    printf("  Resources (%d): ", rm->count);
    if (rm->count == 0) {
        printf("(none)\n");
    } else {
        for (int i = 0; i < rm->count; i++) {
            printf("%s (Total: %d, Avail: %d)%s",
                   rm->resources[i].name,
                   rm->resources[i].total_instances,
                   rm->resources[i].available_instances,
                   (i == rm->count - 1) ? "\n" : ", ");
        }
    }
    printf("\n");

    /* 2. Resource Allocation Graph (RAG) */
    printf("RESOURCE ALLOCATION GRAPH\n\n");

    bool has_rag_edges = false;

    /* Print Allocation Edges: Process --holds--> Resource */
    for (int i = 0; i < pm->count; i++) {
        const Process *proc = pm->processes[i];
        if (!proc) continue;
        if (proc->state == PROCESS_STATE_COMPLETED || proc->state == PROCESS_STATE_ABORTED) {
            continue;
        }

        for (int r = 0; r < rm->count; r++) {
            int alloc_val = (r < proc->res_capacity) ? proc->allocated[r] : 0;
            if (alloc_val > 0) {
                has_rag_edges = true;
                if (alloc_val == 1) {
                    printf("%s --holds--> %s\n", proc->name, rm->resources[r].name);
                } else {
                    printf("%s --holds (%d units)--> %s\n", proc->name, alloc_val, rm->resources[r].name);
                }
            }
        }
    }

    /* Print Request Edges: Process --waits--> Resource */
    for (int i = 0; i < pm->count; i++) {
        const Process *proc = pm->processes[i];
        if (!proc) continue;
        if (proc->state == PROCESS_STATE_COMPLETED || proc->state == PROCESS_STATE_ABORTED) {
            continue;
        }

        for (int r = 0; r < rm->count; r++) {
            int req_val = (r < proc->res_capacity) ? proc->requested[r] : 0;
            if (req_val > 0) {
                has_rag_edges = true;
                if (req_val == 1) {
                    printf("%s --waits--> %s\n", proc->name, rm->resources[r].name);
                } else {
                    printf("%s --waits (%d units)--> %s\n", proc->name, req_val, rm->resources[r].name);
                }
            }
        }
    }

    if (!has_rag_edges) {
        printf("(No active allocation or request edges)\n");
    }
    printf("\n");

    /* 3. Wait-For Graph (WFG) */
    printf("WAIT-FOR GRAPH\n\n");

    WaitForGraph wfg;
    wfg_init(&wfg);
    wfg_build_from_system(&wfg, pm, rm);

    bool has_wfg_edges = false;
    for (int i = 0; i < wfg.num_nodes; i++) {
        for (int j = 0; j < wfg.num_nodes; j++) {
            if (wfg.adj[i][j]) {
                has_wfg_edges = true;
                printf("%s -> %s\n", wfg.node_names[i], wfg.node_names[j]);
            }
        }
    }

    if (!has_wfg_edges) {
        printf("(No wait-for dependencies in system)\n");
    }
    printf("\n");

    /* 4. Viva / Demo Reference */
    printf("[GRAPH SEMANTICS (VIVA REFERENCE)]\n");
    printf("  * Resource Allocation Graph (RAG) is bipartite: Processes <-> Resources.\n");
    printf("    - P --holds--> R  : Allocation edge (Resource assigned to Process)\n");
    printf("    - P --waits--> R  : Request edge (Process blocked awaiting Resource)\n");
    printf("  * Wait-For Graph (WFG) is process-only (collapsed from RAG).\n");
    printf("    - Pi -> Pj        : Pi is waiting on a resource held by Pj\n");
    printf("    - Directed Cycle  : Indicates circular wait (Deadlock condition)\n");
    printf("==============================================================\n\n");

    wfg_cleanup(&wfg);
}
