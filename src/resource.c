#include "resource.h"

void resource_manager_init(ResourceManager *rm) {
    if (!rm) return;
    rm->resources = NULL;
    rm->count = 0;
    rm->capacity = 0;
}

void resource_manager_cleanup(ResourceManager *rm) {
    if (!rm) return;
    if (rm->resources) {
        free(rm->resources);
        rm->resources = NULL;
    }
    rm->count = 0;
    rm->capacity = 0;
}

Resource *resource_create(ResourceManager *rm, int id, const char *name, int total_instances) {
    if (!rm || !name) {
        fprintf(stderr, "[Resource Error] Invalid parameters.\n");
        return NULL;
    }

    if (id < 0) {
        fprintf(stderr, "[Resource Error] Resource ID must be non-negative.\n");
        return NULL;
    }

    if (total_instances <= 0) {
        fprintf(stderr, "[Resource Error] Total instances must be greater than zero.\n");
        return NULL;
    }

    if (resource_find_by_id(rm, id) != NULL) {
        fprintf(stderr, "[Resource Error] Resource with ID %d already exists.\n", id);
        return NULL;
    }

    if (resource_find_by_name(rm, name) != NULL) {
        fprintf(stderr, "[Resource Error] Resource with name '%s' already exists.\n", name);
        return NULL;
    }

    /* Dynamically grow resource array */
    if (rm->count >= rm->capacity) {
        int new_cap = rm->capacity == 0 ? 4 : rm->capacity * 2;
        Resource *new_res = (Resource *)realloc(rm->resources, new_cap * sizeof(Resource));
        if (!new_res) {
            fprintf(stderr, "[Resource Error] Memory reallocation failed for resources.\n");
            return NULL;
        }
        rm->resources = new_res;
        rm->capacity = new_cap;
    }

    Resource *res = &rm->resources[rm->count];
    res->id = id;
    strncpy(res->name, name, MAX_NAME_LEN - 1);
    res->name[MAX_NAME_LEN - 1] = '\0';
    res->total_instances = total_instances;
    res->available_instances = total_instances;

    rm->count++;
    return res;
}

bool resource_remove(ResourceManager *rm, int id) {
    if (!rm) return false;
    int idx = -1;
    for (int i = 0; i < rm->count; i++) {
        if (rm->resources[i].id == id) {
            idx = i;
            break;
        }
    }

    if (idx < 0) return false;

    for (int i = idx; i < rm->count - 1; i++) {
        rm->resources[i] = rm->resources[i + 1];
    }
    rm->count--;
    return true;
}

Resource *resource_find_by_id(ResourceManager *rm, int id) {
    if (!rm) return NULL;
    for (int i = 0; i < rm->count; i++) {
        if (rm->resources[i].id == id) {
            return &rm->resources[i];
        }
    }
    return NULL;
}

const Resource *resource_find_by_id_const(const ResourceManager *rm, int id) {
    if (!rm) return NULL;
    for (int i = 0; i < rm->count; i++) {
        if (rm->resources[i].id == id) {
            return &rm->resources[i];
        }
    }
    return NULL;
}

Resource *resource_find_by_name(ResourceManager *rm, const char *name) {
    if (!rm || !name) return NULL;
    for (int i = 0; i < rm->count; i++) {
        if (strcasecmp(rm->resources[i].name, name) == 0) {
            return &rm->resources[i];
        }
    }
    return NULL;
}

Resource *resource_find_by_id_or_name(ResourceManager *rm, const char *ident) {
    if (!rm || !ident) return NULL;

    /* 1. Try finding by exact or case-insensitive name */
    Resource *res = resource_find_by_name(rm, ident);
    if (res) return res;

    /* 2. Try prefix 'R' or 'r' (e.g. R1, r2) */
    if ((ident[0] == 'R' || ident[0] == 'r') && isdigit((unsigned char)ident[1])) {
        int id = atoi(&ident[1]);
        res = resource_find_by_id(rm, id);
        if (res) return res;
    }

    /* 3. Try pure numeric string */
    char *endptr = NULL;
    long val = strtol(ident, &endptr, 10);
    if (endptr != ident && *endptr == '\0') {
        res = resource_find_by_id(rm, (int)val);
        if (res) return res;
    }

    return NULL;
}

const Resource *resource_find_by_id_or_name_const(const ResourceManager *rm, const char *ident) {
    if (!rm || !ident) return NULL;

    for (int i = 0; i < rm->count; i++) {
        if (strcasecmp(rm->resources[i].name, ident) == 0) {
            return &rm->resources[i];
        }
    }

    if ((ident[0] == 'R' || ident[0] == 'r') && isdigit((unsigned char)ident[1])) {
        int id = atoi(&ident[1]);
        return resource_find_by_id_const(rm, id);
    }

    char *endptr = NULL;
    long val = strtol(ident, &endptr, 10);
    if (endptr != ident && *endptr == '\0') {
        return resource_find_by_id_const(rm, (int)val);
    }

    return NULL;
}

int resource_get_index_by_id(const ResourceManager *rm, int id) {
    if (!rm) return -1;
    for (int i = 0; i < rm->count; i++) {
        if (rm->resources[i].id == id) {
            return i;
        }
    }
    return -1;
}

void resource_list(const ResourceManager *rm) {
    if (!rm || rm->count == 0) {
        printf("  [No resources registered in system]\n");
        return;
    }

    printf("+-----+----------------------+-------+-----------+-----------+\n");
    printf("| ID  | Resource Name        | Total | Available | Allocated |\n");
    printf("+-----+----------------------+-------+-----------+-----------+\n");

    for (int i = 0; i < rm->count; i++) {
        const Resource *res = &rm->resources[i];
        int allocated = res->total_instances - res->available_instances;
        printf("| %-3d | %-20s | %-5d | %-9d | %-9d |\n",
               res->id, res->name, res->total_instances, res->available_instances, allocated);
    }
    printf("+-----+----------------------+-------+-----------+-----------+\n");
}
