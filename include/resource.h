#ifndef DEADLOCKDOCTOR_RESOURCE_H
#define DEADLOCKDOCTOR_RESOURCE_H

#include "common.h"

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int total_instances;
    int available_instances;
} Resource;

typedef struct {
    Resource *resources;           /* dynamically resized array of Resource structs */
    int count;
    int capacity;
} ResourceManager;

void resource_manager_init(ResourceManager *rm);
void resource_manager_cleanup(ResourceManager *rm);
Resource *resource_create(ResourceManager *rm, int id, const char *name, int total_instances);
bool resource_remove(ResourceManager *rm, int id);
Resource *resource_find_by_id(ResourceManager *rm, int id);
Resource *resource_find_by_name(ResourceManager *rm, const char *name);
Resource *resource_find_by_id_or_name(ResourceManager *rm, const char *ident);
const Resource *resource_find_by_id_const(const ResourceManager *rm, int id);
const Resource *resource_find_by_id_or_name_const(const ResourceManager *rm, const char *ident);
int resource_get_index_by_id(const ResourceManager *rm, int id);
void resource_list(const ResourceManager *rm);

#endif /* DEADLOCKDOCTOR_RESOURCE_H */

