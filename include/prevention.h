#ifndef PREVENTION_H
#define PREVENTION_H

#include <stdbool.h>
#include <stddef.h>

/*
 * DeadlockDoctor - Deadlock Prevention Module
 *
 * All matrices use row-major layout:
 * matrix[process * resource_count + resource]
 */

/* Check whether resource requests follow a global ordering rule. */
bool prevention_follows_order(
    const int *allocation,
    size_t resource_count,
    size_t process_id,
    size_t requested_resource
);

/* Check whether the system is in a safe state using Banker's safety test. */
bool prevention_is_safe(
    size_t process_count,
    size_t resource_count,
    const int *available,
    const int *allocation,
    const int *maximum
);

/* Determine whether a resource request can be safely granted. */
bool prevention_can_request(
    size_t process_count,
    size_t resource_count,
    size_t process_id,
    const int *request,
    const int *available,
    const int *allocation,
    const int *maximum
);

#endif /* PREVENTION_H */
