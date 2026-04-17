//
//  czim_narrowdown.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_narrowdown.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// Helper functions
// ============================================================

/**
 * Generate a pseudo-key between two keys (libzim algorithm)
 * Takes a prefix of key2 up to (and including) the first differing character.
 * Precondition: key1 <= key2
 *
 * Examples:
 *   "Cherry" and "Grape"   -> "Gr"
 *   "Apple"  and "Banana"  -> "Ba"
 *   "abc"    and "abd"     -> "abd"
 *   "apple"  and "apples"  -> "apples"
 */
void shortest_string_in_between(const char *key1, const char *key2, char *result, size_t max_len) {
    size_t len1 = strlen(key1);
    size_t len2 = strlen(key2);
    size_t minlen = len1 < len2 ? len1 : len2;

    // Find first differing position
    size_t diff_pos = 0;
    while (diff_pos < minlen && key1[diff_pos] == key2[diff_pos]) {
        diff_pos++;
    }

    // Take prefix of key2 up to and including the first differing character
    size_t result_len = diff_pos + 1;
    if (result_len > len2) {
        result_len = len2;
    }

    // Clamp to buffer size
    if (result_len >= max_len) {
        result_len = max_len - 1;
    }

    memcpy(result, key2, result_len);
    result[result_len] = '\0';
}

// ============================================================
// NarrowDown API implementation
// ============================================================

czim_narrowdown *czim_narrowdown_create(uint32_t capacity) {
    czim_narrowdown *nd = (czim_narrowdown *)calloc(1, sizeof(czim_narrowdown));
    if (!nd) {
        return NULL;
    }

    nd->capacity = capacity;
    nd->count = 0;

    // Pre-allocate key pointer array
    nd->keys = (char **)calloc(capacity, sizeof(char *));
    if (!nd->keys) {
        free(nd);
        return NULL;
    }

    // Pre-allocate index array
    nd->indices = (uint32_t *)calloc(capacity, sizeof(uint32_t));
    if (!nd->indices) {
        free(nd->keys);
        free(nd);
        return NULL;
    }

    return nd;
}

void czim_narrowdown_destroy(czim_narrowdown *nd) {
    if (!nd) {
        return;
    }

    // Free each key string
    if (nd->keys) {
        for (uint32_t i = 0; i < nd->count; i++) {
            if (nd->keys[i]) {
                free(nd->keys[i]);
            }
        }
        free(nd->keys);
    }

    // Free index array
    if (nd->indices) {
        free(nd->indices);
    }

    free(nd);
}

int czim_narrowdown_add(czim_narrowdown *nd, const char *key, uint32_t index) {
    if (!nd || !key) {
        return -1;
    }

    // Check capacity
    if (nd->count >= nd->capacity) {
        // Double the capacity
        uint32_t new_capacity = nd->capacity * 2;

        // Expand key pointer array
        char **new_keys = (char **)realloc(nd->keys, new_capacity * sizeof(char *));
        if (!new_keys) {
            return -1;
        }
        nd->keys = new_keys;

        // Expand index array
        uint32_t *new_indices = (uint32_t *)realloc(nd->indices, new_capacity * sizeof(uint32_t));
        if (!new_indices) {
            return -1;
        }
        nd->indices = new_indices;

        // Initialize new key pointers to NULL
        for (uint32_t i = nd->count; i < new_capacity; i++) {
            nd->keys[i] = NULL;
        }

        nd->capacity = new_capacity;
    }

    // Copy key string
    nd->keys[nd->count] = strdup(key);
    if (!nd->keys[nd->count]) {
        return -1;
    }

    // Add index
    nd->indices[nd->count] = index;
    nd->count++;

    return 0;
}

/**
 * Binary search in the NarrowDown index, returning a narrowed range
 * Algorithm:
 * 1. Binary search in the pseudo-key array to find the insertion position
 * 2. Return [prev_index, next_index) as the narrowed range
 */
czim_range czim_narrowdown_get_range(const czim_narrowdown *nd, const char *key) {
    czim_range range = {0, 0};

    if (!nd || !key || nd->count == 0) {
        return range;
    }

    // Find the first pseudo-key strictly greater than (upper bound)
    // the search key. pos is the insertion point — the index where the search
    // key would be inserted to maintain sorted order, placed AFTER any equal
    // elements. This means:
    //   pos == 0       → key < all pseudo-keys (no possible match)
    //   pos == count   → key >= all pseudo-keys (only last entry may match)
    //   0 < pos < count → entries[pos-1].key <= key < entries[pos].key
    int32_t left = 0;
    int32_t right = (int32_t)nd->count;

    while (left < right) {
        int32_t mid = left + (right - left) / 2;
        int cmp = strcmp(key, nd->keys[mid]);
        if (cmp >= 0) {
            left = mid + 1;   // key >= mid_key, mid is not the upper bound
        } else {
            right = mid;       // key < mid_key, mid is a candidate
        }
    }

    int32_t pos = left;

    // Determine range boundaries from pos
    // With (i-1, i) pseudo-key generation: pseudo_key <= path[i],
    // so the pseudo-key is "attached to" position i, similar to direct sampling.
    // Since key < entries[pos].key <= path[indices[pos]], path[indices[pos]] cannot
    // equal the search key, so we don't need +1 on the upper bound.
    uint32_t begin, end;

    if (pos == 0) {
        // key < first pseudo-key (= path[0]): no possible match
        begin = 0;
        end = 0;
    } else if (pos >= (int32_t)nd->count) {
        // key >= last entry's pseudo-key: search from last sampled position to end
        begin = nd->indices[nd->count - 1];
        end = nd->indices[nd->count - 1] + 1;
    } else {
        // entries[pos-1].key <= key < entries[pos].key
        // key must be in [indices[pos-1], indices[pos])
        begin = nd->indices[pos - 1];
        end = nd->indices[pos];
    }

    range.begin = begin;
    range.end = end;

    return range;
}

size_t czim_narrowdown_size(const czim_narrowdown *nd) {
    if (!nd) {
        return 0;
    }
    return nd->count;
}

size_t czim_narrowdown_memory_size(const czim_narrowdown *nd) {
    if (!nd) {
        return 0;
    }

    size_t total = sizeof(czim_narrowdown);

    // Key pointer array
    if (nd->keys) {
        total += nd->capacity * sizeof(char *);
        // Actual size of each key string
        for (uint32_t i = 0; i < nd->count; i++) {
            if (nd->keys[i]) {
                total += strlen(nd->keys[i]) + 1;
            }
        }
    }

    // Index array
    if (nd->indices) {
        total += nd->capacity * sizeof(uint32_t);
    }

    return total;
}
