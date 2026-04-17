//
//  czim_types.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_types.h"
#include <stdio.h>
#include <string.h>

// Convert UUID to string (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
void czim_uuid_to_string(const czim_uuid_t *uuid, char *out) {
    if (!uuid || !out) {
        return;
    }

    snprintf(out, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
             uuid->bytes[4], uuid->bytes[5],
             uuid->bytes[6], uuid->bytes[7],
             uuid->bytes[8], uuid->bytes[9],
             uuid->bytes[10], uuid->bytes[11], uuid->bytes[12], uuid->bytes[13],
             uuid->bytes[14], uuid->bytes[15]);
}

// Compare two UUIDs
int czim_uuid_compare(const czim_uuid_t *a, const czim_uuid_t *b) {
    if (!a || !b) {
        return (a == b) ? 0 : (a ? 1 : -1);
    }
    return memcmp(a->bytes, b->bytes, 16);
}
