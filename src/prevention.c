#include "prevention.h"

#include <stdlib.h>
#include <stdint.h>

/*
 * Rule 1: Circular-wait prevention.
 *
 * A process holding resources must request only resources
 * whose IDs are greater than the IDs of resources it holds.
 *
 * Resource IDs range from 0 to resource_count - 1.
 */
bool prevention_follows_order(
    const int *allocation,
    size_t resource_count,
    size_t process_id,
    size_t requested_resource)
{
    if (allocation == NULL ||
        resource_count == 0 ||
        requested_resource >= resource_count ||
        process_id > SIZE_MAX / resource_count) {
        return false;
    }

    size_t offset = process_id * resource_count;

    for (size_t r = requested_resource + 1;
         r < resource_count; ++r) {
        if (allocation[offset + r] > 0) {
            return false;
        }
    }

    return true;
}

/*
 * Rule 2: Banker's safety test.
 *
 * Returns true only if all processes can finish in some order,
 * assuming the supplied maximum claims are valid.
 */
bool prevention_is_safe(
    size_t process_count,
    size_t resource_count,
    const int *available,
    const int *allocation,
    const int *maximum)
{
    if (process_count == 0 ||
        resource_count == 0 ||
        available == NULL ||
        allocation == NULL ||
        maximum == NULL ||
        process_count > SIZE_MAX / resource_count) {
        return false;
    }

    size_t cells = process_count * resource_count;

    int *work = malloc(resource_count * sizeof(int));
    int *finish = calloc(process_count, sizeof(bool));
    int *need = malloc(cells * sizeof(int));

    if (work == NULL || finish == NULL || need == NULL) {
        free(work);
        free(finish);
        free(need);
        return false;
    }

    bool valid = true;

    for (size_t r = 0; r < resource_count; ++r) {
        if (available[r] < 0) {
            valid = false;
            break;
        }
        work[r] = available[r];
    }

    for (size_t i = 0; valid && i < cells; ++i) {
        if (allocation[i] < 0 ||
            maximum[i] < allocation[i]) {
            valid = false;
            break;
        }

        need[i] = maximum[i] - allocation[i];
    }

    if (!valid) {
        free(work);
        free(finish);
        free(need);
        return false;
    }

    size_t completed = 0;
    bool progress = true;

    while (completed < process_count && progress) {
        progress = false;

        for (size_t p = 0; p < process_count; ++p) {
            if (finish[p]) {
                continue;
            }

            bool can_finish = true;
            size_t offset = p * resource_count;

            for (size_t r = 0; r < resource_count; ++r) {
                if (need[offset + r] > work[r]) {
                    can_finish = false;
                    break;
                }
            }

            if (can_finish) {
                for (size_t r = 0; r < resource_count; ++r) {
                    /* Reject overflow in the simulated release. */
                    if (allocation[offset + r] >
                        INT32_MAX - work[r]) {
                        valid = false;
                        break;
                    }
                }

                if (!valid) {
                    break;
                }

                for (size_t r = 0; r < resource_count; ++r) {
                    work[r] += allocation[offset + r];
                }

                finish[p] = true;
                ++completed;
                progress = true;
            }
        }

        if (!valid) {
            break;
        }
    }

    bool safe = valid && completed == process_count;

    free(work);
    free(finish);
    free(need);

    return safe;
}

/*
 * Rule 3: Request validation and safe allocation.
 *
 * A request is approved only when:
 *  1. The request is valid and does not exceed the process's need.
 *  2. The requested resources are currently available.
 *  3. Temporarily granting the request leaves the system safe.
 *
 * This function checks the request; it does not modify the
 * caller's resource-allocation matrices.
 */
bool prevention_can_request(
    size_t process_count,
    size_t resource_count,
    size_t process_id,
    const int *request,
    const int *available,
    const int *allocation,
    const int *maximum)
{
    if (process_count == 0 ||
        resource_count == 0 ||
        process_id >= process_count ||
        request == NULL ||
        available == NULL ||
        allocation == NULL ||
        maximum == NULL ||
        process_count > SIZE_MAX / resource_count) {
        return false;
    }

    size_t cells = process_count * resource_count;
    size_t offset = process_id * resource_count;

    int *new_available = malloc(resource_count * sizeof(int));
    int *new_allocation = malloc(cells * sizeof(int));

    if (new_available == NULL || new_allocation == NULL) {
        free(new_available);
        free(new_allocation);
        return false;
    }

    for (size_t r = 0; r < resource_count; ++r) {
        if (available[r] < 0 ||
            allocation[offset + r] < 0 ||
            maximum[offset + r] < allocation[offset + r] ||
            request[r] < 0 ||
            request[r] > maximum[offset + r] -
                         allocation[offset + r] ||
            request[r] > available[r]) {
            free(new_available);
            free(new_allocation);
            return false;
        }

        new_available[r] = available[r] - request[r];
    }

    for (size_t i = 0; i < cells; ++i) {
        new_allocation[i] = allocation[i];
    }

    for (size_t r = 0; r < resource_count; ++r) {
        if (new_allocation[offset + r] >
            INT32_MAX - request[r]) {
            free(new_available);
            free(new_allocation);
            return false;
        }

        new_allocation[offset + r] += request[r];
    }

    bool safe = prevention_is_safe(
        process_count,
        resource_count,
        new_available,
        new_allocation,
        maximum
    );

    free(new_available);
    free(new_allocation);

    return safe;
}
