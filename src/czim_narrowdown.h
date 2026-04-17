//
//  czim_narrowdown.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_narrowdown_h
#define czim_narrowdown_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Data structures
// ============================================================

/**
 * NarrowDown: in-memory sparse index for accelerated lookup in sorted external data.
 *
 * Given a sorted sequence of entries (e.g. entries sorted by path), NarrowDown
 * samples every N-th entry and builds an in-memory index to narrow the binary
 * search range before hitting the disk.
 *
 * Instead of storing the full sampled paths as index keys, we generate shorter
 * pseudo-keys via shortest_string_in_between(path[i-1], path[i]), which takes a
 * prefix of path[i] up to the first differing character. The pseudo-key <= path[i],
 * so it is "attached to" position i — conceptually similar to direct sampling
 * where path[i] itself would be stored, but with a shorter key to save memory.
 *
 * Lookup: upper_bound among pseudo-keys yields a range [prev_lindex, cur_lindex).
 * A second binary search is then performed within this narrowed range on disk.
 */
typedef struct czim_narrowdown {
    char **keys;             // Key pointer array (each pointer points to a string)
    uint32_t *indices;       // Corresponding index positions
    uint32_t count;          // Number of index entries
    uint32_t capacity;       // Capacity
} czim_narrowdown;

/**
 * Lookup range
 */
typedef struct czim_range {
    uint32_t begin;  // Start index (inclusive)
    uint32_t end;    // End index (exclusive)
} czim_range;

// ============================================================
// Internal helpers
// ============================================================

/**
 * Generate a pseudo-key between two keys (libzim algorithm)
 * Takes a prefix of key2 up to the first differing character.
 * Precondition: key1 <= key2
 * @param key1 First key (must be <= key2)
 * @param key2 Second key
 * @param result Output buffer
 * @param max_len Buffer size
 */
void shortest_string_in_between(const char *key1, const char *key2, char *result, size_t max_len);

// ============================================================
// NarrowDown API
// ============================================================

/**
 * Create a NarrowDown index
 * @param capacity Initial capacity
 * @return NarrowDown object
 */
czim_narrowdown *czim_narrowdown_create(uint32_t capacity);

/**
 * Destroy a NarrowDown index
 * @param nd NarrowDown object
 */
void czim_narrowdown_destroy(czim_narrowdown *nd);

/**
 * Add an index entry
 * @param nd NarrowDown object
 * @param key Key (will be copied)
 * @param index Corresponding index position
 * @return 0 on success, -1 on failure
 */
int czim_narrowdown_add(czim_narrowdown *nd, const char *key, uint32_t index);

/**
 * Get lookup range
 * @param nd NarrowDown object
 * @param key Lookup key
 * @return Range [begin, end)
 */
czim_range czim_narrowdown_get_range(const czim_narrowdown *nd, const char *key);

/**
 * Get number of index entries
 */
size_t czim_narrowdown_size(const czim_narrowdown *nd);

/**
 * Get memory usage
 */
size_t czim_narrowdown_memory_size(const czim_narrowdown *nd);

#ifdef __cplusplus
}
#endif

#endif /* czim_narrowdown_h */
