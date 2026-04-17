//
//  czim_archive.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_archive.h"
#include "czim_entry.h"
#include "czim_cluster.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "czim_types.h"

// ============================================================
// Internal types
// ============================================================

typedef struct {
    bool found;
    uint32_t index;
} czim_findx_result;

// ============================================================
// Configuration
// ============================================================

#define CZIM_NARROWDOWN_INDEX_COUNT   1024   // NarrowDown index node count
#define CZIM_ENTRY_CACHE_MAX_ENTRIES 512    // Entry cache max entries
#define CZIM_CLUSTER_CACHE_MAX_BYTES  (128 * 1024 * 1024)  // Cluster cache max memory (128MB)

// ============================================================
// Internal function declarations
// ============================================================

static int load_mime_types(czim_archive *archive);
static int load_path_ptrs(czim_archive *archive);
static int load_title_ptrs(czim_archive *archive);
static int load_cluster_ptrs(czim_archive *archive);

static int build_path_narrowdown(czim_archive *archive);
static int build_title_narrowdown(czim_archive *archive);

static int compare_path_ns(czim_archive *archive, uint32_t index, char ns, const char *path);
static int compare_title_ns(czim_archive *archive, uint32_t index, char ns, const char *title);

static czim_findx_result findx_path(czim_archive *archive, char ns, const char *path);
static czim_findx_result findx_title(czim_archive *archive, char ns, const char *title);

static czim_cluster *czim_archive_get_cluster(czim_archive *archive, uint32_t index);

// ============================================================
// Internal helpers
// ============================================================

// Helper: build composite key "ns/path" or "ns/title" into buffer
static size_t make_ns_key(char *buf, size_t buf_size, char ns, const char *key) {
    size_t key_len = strlen(key);
    size_t total = 1 + 1 + key_len;  // ns + '/' + key
    if (total >= buf_size) {
        total = buf_size - 1;
    }
    buf[0] = ns;
    buf[1] = '/';
    size_t copy_len = total - 2;
    if (copy_len > key_len) copy_len = key_len;
    memcpy(buf + 2, key, copy_len);
    buf[total] = '\0';
    return total;
}

// ============================================================
// Internal: data loading
// ============================================================

static int load_mime_types(czim_archive *archive) {
    czim_offset_t offset = archive->header.mime_list_pos;
    size_t capacity = 16;
    size_t count = 0;

    archive->mime_types = malloc(capacity * sizeof(char *));
    if (!archive->mime_types) {
        return CZIM_ERROR_MEMORY;
    }

    // Read MIME type list (terminated by empty string)
    while (1) {
        // Read string
        char buf[256];
        size_t len = 0;

        while (len < sizeof(buf) - 1) {
            uint8_t c;
            if (czim_file_read_byte(archive->file, offset + len, &c) != CZIM_OK) {
                goto error;
            }
            buf[len] = c;
            if (c == '\0') {
                break;
            }
            len++;
        }
        buf[len] = '\0';

        // Empty string marks end of list
        if (len == 0) {
            break;
        }

        // Expand array
        if (count >= capacity) {
            capacity *= 2;
            char **new_types = realloc(archive->mime_types, capacity * sizeof(char *));
            if (!new_types) {
                goto error;
            }
            archive->mime_types = new_types;
        }

        // Copy string
        archive->mime_types[count] = strdup(buf);
        if (!archive->mime_types[count]) {
            goto error;
        }
        count++;

        offset += len + 1;
    }

    archive->mime_type_count = count;
    return CZIM_OK;

error:
    for (size_t i = 0; i < count; i++) {
        free(archive->mime_types[i]);
    }
    free(archive->mime_types);
    archive->mime_types = NULL;
    return CZIM_ERROR_MEMORY;
}

static int load_path_ptrs(czim_archive *archive) {
    if (archive->path_ptrs) {
        return CZIM_OK;  // Already loaded
    }

    uint32_t count = archive->header.entry_count;
    archive->path_ptrs = malloc(count * sizeof(uint64_t));
    if (!archive->path_ptrs) {
        return CZIM_ERROR_MEMORY;
    }

    czim_offset_t offset = archive->header.path_ptr_pos;
    for (uint32_t i = 0; i < count; i++) {
        if (czim_file_read_u64(archive->file, offset + i * 8, &archive->path_ptrs[i]) != CZIM_OK) {
            free(archive->path_ptrs);
            archive->path_ptrs = NULL;
            return CZIM_ERROR_IO;
        }
    }

    return CZIM_OK;
}

static int load_title_ptrs(czim_archive *archive) {
    if (archive->title_ptrs) {
        return CZIM_OK;  // Already loaded
    }

    if (!czim_header_has_title_index(&archive->header)) {
        return CZIM_ERROR_NOT_FOUND;
    }

    uint32_t count = archive->header.entry_count;
    archive->title_ptrs = malloc(count * sizeof(uint64_t));
    if (!archive->title_ptrs) {
        return CZIM_ERROR_MEMORY;
    }

    czim_offset_t offset = archive->header.title_ptr_pos;

    // Title index stores entry indices (32-bit), not offsets
    for (uint32_t i = 0; i < count; i++) {
        uint32_t entry_index;
        if (czim_file_read_u32(archive->file, offset + i * 4, &entry_index) != CZIM_OK) {
            free(archive->title_ptrs);
            archive->title_ptrs = NULL;
            return CZIM_ERROR_IO;
        }
        archive->title_ptrs[i] = entry_index;
    }

    return CZIM_OK;
}

static int load_cluster_ptrs(czim_archive *archive) {
    if (archive->cluster_ptrs) {
        return CZIM_OK;  // Already loaded
    }

    uint32_t count = archive->header.cluster_count;
    archive->cluster_ptrs = malloc((count + 1) * sizeof(uint64_t));
    if (!archive->cluster_ptrs) {
        return CZIM_ERROR_MEMORY;
    }

    czim_offset_t offset = archive->header.cluster_ptr_pos;
    for (uint32_t i = 0; i < count; i++) {
        if (czim_file_read_u64(archive->file, offset + i * 8, &archive->cluster_ptrs[i]) != CZIM_OK) {
            free(archive->cluster_ptrs);
            archive->cluster_ptrs = NULL;
            return CZIM_ERROR_IO;
        }
    }

    // End position of the last cluster
    archive->cluster_ptrs[count] = archive->header.checksum_pos;

    return CZIM_OK;
}

// ============================================================
// Internal: NarrowDown index building
// ============================================================

/**
 * Build Path NarrowDown index
 * Algorithm: sample every step entries, generate pseudo-keys and store them
 * Step calculation: step = max(1, entry_count / target_index_count)
 */
static int build_path_narrowdown(czim_archive *archive) {
    if (archive->path_narrowdown) {
        return CZIM_OK;  // Already built
    }

    if (!archive->path_ptrs) {
        return CZIM_ERROR_NOT_FOUND;
    }

    uint32_t count = archive->header.entry_count;

    // Calculate sampling step: target ~CZIM_NARROWDOWN_INDEX_COUNT index nodes
    uint32_t step = (count > CZIM_NARROWDOWN_INDEX_COUNT)
                  ? (count / CZIM_NARROWDOWN_INDEX_COUNT)
                  : 1;

    if (count < step * 2) {
        // Too few entries, no NarrowDown needed
        return CZIM_OK;
    }

    uint32_t index_count = count / step + 1;
    archive->path_narrowdown = czim_narrowdown_create(index_count);
    if (!archive->path_narrowdown) {
        return CZIM_ERROR_MEMORY;
    }

    char pseudo_key[512];
    char curr_long_path[512];
    char prev_long_path_buf[512];
    bool first_entry = true;

    for (uint32_t i = 0; i < count; i += step) {
        czim_entry *entry = czim_archive_get_entry_by_index(archive, i);
        if (!entry) {
            continue;
        }
        // Copy long_path into local buffer (czim_entry_get_long_path uses static buffer)
        const char *lp = czim_entry_get_long_path(entry);
        strncpy(curr_long_path, lp ? lp : "", sizeof(curr_long_path) - 1);
        curr_long_path[sizeof(curr_long_path) - 1] = '\0';

        if (first_entry) {
            // First entry: store the actual long path (ns/path)
            czim_narrowdown_add(archive->path_narrowdown, curr_long_path, i);
            first_entry = false;
            czim_entry_free(entry);
            continue;
        }

        // Get long path of previous consecutive entry for pseudo-key generation (i-1, i)
        czim_entry *prev_entry = czim_archive_get_entry_by_index(archive, i - 1);
        if (!prev_entry) {
            czim_narrowdown_add(archive->path_narrowdown, curr_long_path, i);
            czim_entry_free(entry);
            continue;
        }
        const char *plp = czim_entry_get_long_path(prev_entry);
        strncpy(prev_long_path_buf, plp ? plp : "", sizeof(prev_long_path_buf) - 1);
        prev_long_path_buf[sizeof(prev_long_path_buf) - 1] = '\0';

        // Generate pseudo-key from (i-1, i): result <= long_path[i], similar to direct sampling
        shortest_string_in_between(prev_long_path_buf, curr_long_path, pseudo_key, sizeof(pseudo_key));

        czim_narrowdown_add(archive->path_narrowdown, pseudo_key, i);

        czim_entry_free(entry);
        czim_entry_free(prev_entry);
    }

    // Add the last entry if not already covered by sampling
    if ((count - 1) % step != 0) {
        czim_entry *last = czim_archive_get_entry_by_index(archive, count - 1);
        if (last) {
            const char *llp = czim_entry_get_long_path(last);
            czim_narrowdown_add(archive->path_narrowdown,
                                llp ? llp : "", count - 1);
            czim_entry_free(last);
        }
    }

    return CZIM_OK;
}

/**
 * Build Title NarrowDown index
 * Algorithm: sample every step entries, generate pseudo-keys and store them
 * Step calculation: step = max(1, entry_count / target_index_count)
 * Note: Title Index is indirect; must use title_ptrs to get actual entry indices
 */
static int build_title_narrowdown(czim_archive *archive) {
    if (archive->title_narrowdown) {
        return CZIM_OK;  // Already built
    }

    if (!archive->title_ptrs) {
        return CZIM_ERROR_NOT_FOUND;
    }

    uint32_t count = archive->header.entry_count;

    // Calculate sampling step: target ~CZIM_NARROWDOWN_INDEX_COUNT index nodes
    uint32_t step = (count > CZIM_NARROWDOWN_INDEX_COUNT)
                  ? (count / CZIM_NARROWDOWN_INDEX_COUNT)
                  : 1;

    if (count < step * 2) {
        // Too few entries, no NarrowDown needed
        return CZIM_OK;
    }

    uint32_t index_count = count / step + 1;
    archive->title_narrowdown = czim_narrowdown_create(index_count);
    if (!archive->title_narrowdown) {
        return CZIM_ERROR_MEMORY;
    }

    char composite_key[512];
    char prev_composite_key[512];
    char pseudo_key[512];
    bool first_entry = true;

    for (uint32_t i = 0; i < count; i += step) {
        uint32_t path_index = (uint32_t)archive->title_ptrs[i];

        czim_entry *entry = czim_archive_get_entry_by_index(archive, path_index);
        if (!entry) {
            continue;
        }
        const char *title = czim_entry_get_title(entry);
        make_ns_key(composite_key, sizeof(composite_key), entry->ns, title);

        if (first_entry) {
            // First entry: store the actual composite key (ns/title)
            czim_narrowdown_add(archive->title_narrowdown, composite_key, i);
            first_entry = false;
            czim_entry_free(entry);
            continue;
        }

        // Get title of previous consecutive entry for pseudo-key generation (i-1, i)
        uint32_t prev_path_index = (uint32_t)archive->title_ptrs[i - 1];
        czim_entry *prev_entry = czim_archive_get_entry_by_index(archive, prev_path_index);
        if (!prev_entry) {
            czim_narrowdown_add(archive->title_narrowdown, composite_key, i);
            czim_entry_free(entry);
            continue;
        }
        const char *prev_title = czim_entry_get_title(prev_entry);
        make_ns_key(prev_composite_key, sizeof(prev_composite_key), prev_entry->ns, prev_title);

        // Generate pseudo-key from (i-1, i): result <= composite_key[i], similar to direct sampling
        shortest_string_in_between(prev_composite_key, composite_key, pseudo_key, sizeof(pseudo_key));

        czim_narrowdown_add(archive->title_narrowdown, pseudo_key, i);

        czim_entry_free(entry);
        czim_entry_free(prev_entry);
    }

    // Add the last entry if not already covered by sampling
    if ((count - 1) % step != 0) {
        uint32_t last_path_index = (uint32_t)archive->title_ptrs[count - 1];
        czim_entry *last = czim_archive_get_entry_by_index(archive, last_path_index);
        if (last) {
            make_ns_key(composite_key, sizeof(composite_key), last->ns, czim_entry_get_title(last));
            czim_narrowdown_add(archive->title_narrowdown, composite_key, count - 1);
            czim_entry_free(last);
        }
    }

    return CZIM_OK;
}

// ============================================================
// Internal: comparison functions
// ============================================================

static int compare_path_ns(czim_archive *archive, uint32_t index, char ns, const char *path) {
    czim_entry *entry = czim_archive_get_entry_by_index(archive, index);
    if (!entry) {
        return 0;
    }

    int result;
    if (entry->ns < ns) {
        result = -1;
    } else if (entry->ns > ns) {
        result = 1;
    } else {
        result = strcmp(entry->path, path);
    }
    czim_entry_free(entry);
    return result;
}

static int compare_title_ns(czim_archive *archive, uint32_t index, char ns, const char *title) {
    if (!archive->title_ptrs) {
        if (load_title_ptrs(archive) != CZIM_OK) {
            return 0;
        }
    }

    uint32_t entry_index = (uint32_t)archive->title_ptrs[index];
    czim_entry *entry = czim_archive_get_entry_by_index(archive, entry_index);
    if (!entry) {
        return 0;
    }

    int result;
    if (entry->ns < ns) {
        result = -1;
    } else if (entry->ns > ns) {
        result = 1;
    } else {
        const char *entry_title = czim_entry_get_title(entry);
        result = strcmp(entry_title, title);
    }
    czim_entry_free(entry);
    return result;
}

// ============================================================
// Internal: findx (lower_bound + exact match check)
// ============================================================

/**
 * Find lower_bound of (ns, path) in the path-sorted entry table.
 * Returns {found, index} where:
 * - index is always the lower_bound position (first >= key)
 * - found is true if entry at index exactly matches (ns, path)
 */
static czim_findx_result findx_path(czim_archive *archive, char ns, const char *path) {
    czim_findx_result result = {false, 0};

    if (!archive || !path) {
        return result;
    }

    // Load URL pointers
    if (load_path_ptrs(archive) != CZIM_OK) {
        return result;
    }

    // Build Path NarrowDown index (if not yet built)
    if (!archive->path_narrowdown) {
        build_path_narrowdown(archive);
    }

    uint32_t count = archive->header.entry_count;
    uint32_t left = 0;
    uint32_t right = count;

    // Use NarrowDown to narrow search range
    // NarrowDown is built from all entries (mixed namespaces).
    // Only use it if the range is meaningful (>1 entry), otherwise fall back to full range.
    if (archive->path_narrowdown) {
        char ns_key[512];
        make_ns_key(ns_key, sizeof(ns_key), ns, path);
        czim_range range = czim_narrowdown_get_range(archive->path_narrowdown, ns_key);
        if (range.begin != range.end) {
            left = range.begin;
            right = range.end;
        }
    }

    // lower_bound: find first position where (ns, path) >= search key
    while (left < right) {
        uint32_t mid = left + (right - left) / 2;
        if (compare_path_ns(archive, mid, ns, path) < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    result.index = left;

    // Check if exact match
    if (left < count && compare_path_ns(archive, left, ns, path) == 0) {
        result.found = true;
    }

    return result;
}

/**
 * Find lower_bound of (ns, title) in the title-sorted entry table.
 * Same semantics as findx_path but for title index.
 */
static czim_findx_result findx_title(czim_archive *archive, char ns, const char *title) {
    czim_findx_result result = {false, 0};

    if (!archive || !title) {
        return result;
    }

    // Load title pointers
    if (load_title_ptrs(archive) != CZIM_OK) {
        return result;
    }

    // Build Title NarrowDown index (if not yet built)
    if (!archive->title_narrowdown) {
        build_title_narrowdown(archive);
    }

    uint32_t count = archive->header.entry_count;
    uint32_t left = 0;
    uint32_t right = count;

    // Use NarrowDown to narrow search range
    if (archive->title_narrowdown) {
        char ns_key[512];
        make_ns_key(ns_key, sizeof(ns_key), ns, title);
        czim_range range = czim_narrowdown_get_range(archive->title_narrowdown, ns_key);
        left = range.begin;
        right = range.end;
    }

    // lower_bound: find first position where (ns, title) >= search key
    while (left < right) {
        uint32_t mid = left + (right - left) / 2;
        if (compare_title_ns(archive, mid, ns, title) < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    result.index = left;

    // Check if exact match
    if (left < count && compare_title_ns(archive, left, ns, title) == 0) {
        result.found = true;
    }

    return result;
}

// ============================================================
// Internal: cluster access
// ============================================================

static czim_cluster *czim_archive_get_cluster(czim_archive *archive, uint32_t index) {
    if (!archive || index >= archive->header.cluster_count) {
        return NULL;
    }

    // Check cache
    uobject *cached_obj = NULL;
    if (ucache_get_retain(archive->cluster_cache, &index, sizeof(uint32_t), &cached_obj) == UCACHE_OK) {
        return uobject_cast(cached_obj, czim_cluster, obj);
    }

    // Cache miss: read from file
    if (load_cluster_ptrs(archive) != CZIM_OK) {
        return NULL;
    }

    czim_offset_t offset = archive->cluster_ptrs[index];
    czim_size_t size = archive->cluster_ptrs[index + 1] - offset;

    czim_cluster *cluster = czim_cluster_read(archive->file, offset, size);
    if (!cluster) {
        return NULL;
    }

    // If cluster is too large (exceeds cache limit), don't cache
    uint64_t cluster_mem = uobject_memory_size(&cluster->obj);
    if (cluster_mem > CZIM_CLUSTER_CACHE_MAX_BYTES) {
        return cluster;  // refcount=1 (caller's reference)
    }

    // Store in cache (ucache_set will retain, refcount becomes 2)
    ucache_set(archive->cluster_cache, &index, sizeof(uint32_t), &cluster->obj);

    // Return to caller (refcount=2: cache holds 1, caller holds 1)
    return cluster;
}

// ============================================================
// Archive lifecycle
// ============================================================

czim_archive *czim_archive_open(const char *path) {
    if (!path) {
        return NULL;
    }

    czim_archive *archive = calloc(1, sizeof(czim_archive));
    if (!archive) {
        return NULL;
    }

    // Open file
    archive->file = czim_file_open(path);
    if (!archive->file) {
        free(archive);
        return NULL;
    }

    // Read file header
    if (czim_header_read(archive->file, &archive->header) != CZIM_OK) {
        czim_file_close(archive->file);
        free(archive);
        return NULL;
    }

    // Validate header
    if (czim_header_validate(&archive->header) != CZIM_OK) {
        czim_file_close(archive->file);
        free(archive);
        return NULL;
    }

    // Load MIME type list
    if (load_mime_types(archive) != CZIM_OK) {
        czim_file_close(archive->file);
        free(archive);
        return NULL;
    }

    // Create entry cache (by entry count, max 512)
    ucache_config entry_cfg = {
        .max_items = CZIM_ENTRY_CACHE_MAX_ENTRIES,
        .max_memory = 0,
        .initial_capacity = 64,
        .thread_safe = false,
        .enable_stats = false,
    };
    archive->entry_cache = ucache_new(&entry_cfg);
    if (!archive->entry_cache) {
        czim_file_close(archive->file);
        free(archive);
        return NULL;
    }

    // Create cluster cache (by memory size, max 128MB)
    ucache_config cluster_cfg = {
        .max_items = 0,
        .max_memory = CZIM_CLUSTER_CACHE_MAX_BYTES,
        .initial_capacity = 64,
        .thread_safe = false,
        .enable_stats = false,
    };
    archive->cluster_cache = ucache_new(&cluster_cfg);
    if (!archive->cluster_cache) {
        ucache_free(archive->entry_cache);
        czim_file_close(archive->file);
        free(archive);
        return NULL;
    }

    return archive;
}

void czim_archive_close(czim_archive *archive) {
    if (!archive) {
        return;
    }

    // Free caches (before closing file, since caches may reference cluster internal data)
    if (archive->cluster_cache) {
        ucache_free(archive->cluster_cache);
    }
    if (archive->entry_cache) {
        ucache_free(archive->entry_cache);
    }

    // Free NarrowDown indices
    if (archive->path_narrowdown) {
        czim_narrowdown_destroy(archive->path_narrowdown);
    }
    if (archive->title_narrowdown) {
        czim_narrowdown_destroy(archive->title_narrowdown);
    }

    // Free MIME type list
    if (archive->mime_types) {
        for (size_t i = 0; i < archive->mime_type_count; i++) {
            free(archive->mime_types[i]);
        }
        free(archive->mime_types);
    }

    // Free index pointers
    free(archive->path_ptrs);
    free(archive->title_ptrs);
    free(archive->cluster_ptrs);

    // Close file
    if (archive->file) {
        czim_file_close(archive->file);
    }

    free(archive);
}

// ============================================================
// Archive properties
// ============================================================

const char *czim_archive_uuid(const czim_archive *archive) {
    static char uuid_str[37];
    if (!archive) {
        return NULL;
    }
    czim_uuid_to_string(&archive->header.uuid, uuid_str);
    return uuid_str;
}

const char *czim_archive_mime_type(const czim_archive *archive, uint16_t index) {
    if (!archive || index >= archive->mime_type_count) {
        return NULL;
    }
    return archive->mime_types[index];
}

bool czim_archive_has_main_entry(const czim_archive *archive) {
    return archive && czim_header_has_main_page(&archive->header);
}

bool czim_archive_has_title_index(const czim_archive *archive) {
    return archive && czim_header_has_title_index(&archive->header);
}

uint32_t czim_archive_entry_count(const czim_archive *archive) {
    return archive ? archive->header.entry_count : 0;
}

uint32_t czim_archive_article_count(czim_archive *archive) {
    if (!archive) return 0;

    // 1. Try X/listing/titleOrdered/v1 index (most accurate)
    uint32_t idx;
    czim_entry *d = czim_archive_find_entry_by_path(archive, 'X', "listing/titleOrdered/v1", &idx);
    if (d && !czim_entry_is_redirect(d)) {
        czim_blob blob = czim_archive_get_blob(archive, d);
        if (blob.data && blob.size >= sizeof(uint32_t)) {
            uint32_t count = (uint32_t)(blob.size / sizeof(uint32_t));
            czim_blob_free(&blob);
            czim_entry_free(d);
            return count;
        }
        czim_blob_free(&blob);
    }
    if (d) czim_entry_free(d);

    // 2. Try M/Counter metadata (parse "text/html=N")
    d = czim_archive_find_metadata(archive, "Counter", &idx);
    if (d) {
        czim_blob blob = czim_archive_get_blob(archive, d);
        if (blob.data && blob.size > 0) {
            // Parse "text/html=N" from counter string
            const char *p = strstr((const char *)blob.data, "text/html=");
            if (p) {
                p += 10; // skip "text/html="
                uint32_t count = 0;
                while (*p >= '0' && *p <= '9') {
                    count = count * 10 + (*p - '0');
                    p++;
                }
                czim_blob_free(&blob);
                czim_entry_free(d);
                return count;
            }
        }
        czim_blob_free(&blob);
        czim_entry_free(d);
    }

    // 3. Fallback: not available
    return 0;
}

uint32_t czim_archive_cluster_count(const czim_archive *archive) {
    return archive ? archive->header.cluster_count : 0;
}

// ============================================================
// Namespace utilities
// ============================================================

bool czim_archive_has_new_namespace_scheme(const czim_archive *archive) {
    return archive && czim_header_has_new_namespace_scheme(&archive->header);
}

// ============================================================
// Entry access
// ============================================================

czim_entry *czim_archive_get_entry_by_index(czim_archive *archive, uint32_t index) {
    if (!archive || index >= archive->header.entry_count) {
        return NULL;
    }

    // Check cache
    uobject *cached_obj = NULL;
    if (ucache_get_retain(archive->entry_cache, &index, sizeof(uint32_t), &cached_obj) == UCACHE_OK) {
        return uobject_cast(cached_obj, czim_entry, obj);
    }

    // Cache miss: read from file
    if (load_path_ptrs(archive) != CZIM_OK) {
        return NULL;
    }

    czim_offset_t offset = archive->path_ptrs[index];
    czim_entry *entry = czim_entry_read(archive->file, offset);
    if (!entry) {
        return NULL;
    }

    // Store in cache (ucache_set will retain, refcount becomes 2)
    ucache_set(archive->entry_cache, &index, sizeof(uint32_t), &entry->obj);

    // Return to caller (refcount=2: cache holds 1, caller holds 1)
    return entry;
}

czim_entry *czim_archive_find_entry_by_path(czim_archive *archive, char ns, const char *path, uint32_t *out_index) {
    czim_findx_result r = findx_path(archive, ns, path);
    if (!r.found) {
        return NULL;
    }
    if (out_index) {
        *out_index = r.index;
    }
    return czim_archive_get_entry_by_index(archive, r.index);
}

czim_entry *czim_archive_find_entry_by_title(czim_archive *archive, char ns, const char *title, uint32_t *out_index) {
    czim_findx_result r = findx_title(archive, ns, title);
    if (!r.found) {
        return NULL;
    }
    uint32_t entry_index = (uint32_t)archive->title_ptrs[r.index];
    if (out_index) {
        *out_index = entry_index;
    }
    return czim_archive_get_entry_by_index(archive, entry_index);
}

czim_entry *czim_archive_resolve_redirect(czim_archive *archive, czim_entry *entry, int max_redirects) {
    if (!archive || !entry) {
        return NULL;
    }

    // Follow redirect chain
    for (int i = 0; i < max_redirects; i++) {
        if (!czim_entry_is_redirect(entry)) {
            // Not a redirect, return final target
            return entry;
        }

        // Get redirect target
        uint32_t redirect_index = entry->redirect_index;
        czim_entry *next_entry = czim_archive_get_entry_by_index(archive, redirect_index);
        if (!next_entry) {
            czim_entry_free(entry);
            return NULL;
        }

        // Release current entry, use next
        czim_entry_free(entry);
        entry = next_entry;
    }

    // Exceeded max redirects
    czim_entry_free(entry);
    return NULL;
}

czim_entry *czim_archive_find_metadata(czim_archive *archive, const char *name, uint32_t *out_index) {
    czim_entry *d = czim_archive_find_entry_by_path(archive, 'M', name, out_index);
    if (d && czim_entry_is_redirect(d)) {
        d = czim_archive_resolve_redirect(archive, d, 50);
    }
    return d;
}

// ============================================================
// Prefix search
// ============================================================

int czim_archive_find_entry_by_path_prefix(czim_archive *archive, char ns, const char *prefix,
                                         uint32_t *start_index, uint32_t *end_index) {
    if (!archive || !prefix || !start_index || !end_index) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    // start = lower_bound of prefix
    czim_findx_result r1 = findx_path(archive, ns, prefix);
    *start_index = r1.index;

    // end = lower_bound of prefix with last char incremented (libzim's path.back()++ trick)
    size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        // Empty prefix: find lower_bound of next namespace
        czim_findx_result r2 = findx_path(archive, ns + 1, "");
        *end_index = r2.index;
    } else {
        char end_prefix[512];
        if (prefix_len + 1 >= sizeof(end_prefix)) {
            *end_index = archive->header.entry_count;
        } else {
            memcpy(end_prefix, prefix, prefix_len);
            end_prefix[prefix_len - 1]++;
            end_prefix[prefix_len] = '\0';
            czim_findx_result r2 = findx_path(archive, ns, end_prefix);
            *end_index = r2.index;
        }
    }

    return CZIM_OK;
}

int czim_archive_find_entry_by_title_prefix(czim_archive *archive, char ns, const char *prefix,
                                          uint32_t *start_index, uint32_t *end_index) {
    if (!archive || !prefix || !start_index || !end_index) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    // start = lower_bound of prefix
    czim_findx_result r1 = findx_title(archive, ns, prefix);
    *start_index = r1.index;

    // end = lower_bound of incremented prefix
    size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        czim_findx_result r2 = findx_title(archive, ns + 1, "");
        *end_index = r2.index;
    } else {
        char end_prefix[512];
        if (prefix_len + 1 >= sizeof(end_prefix)) {
            *end_index = archive->header.entry_count;
        } else {
            memcpy(end_prefix, prefix, prefix_len);
            end_prefix[prefix_len - 1]++;
            end_prefix[prefix_len] = '\0';
            czim_findx_result r2 = findx_title(archive, ns, end_prefix);
            *end_index = r2.index;
        }
    }

    return CZIM_OK;
}

// ============================================================
// Blob access
// ============================================================

czim_blob czim_archive_get_blob(czim_archive *archive, const czim_entry *entry) {
    czim_blob empty = {NULL, 0, NULL};

    if (!archive || !entry) {
        return empty;
    }

    if (czim_entry_is_redirect(entry)) {
        return empty;
    }

    czim_cluster *cluster = czim_archive_get_cluster(archive, entry->cluster_number);
    if (!cluster) {
        return empty;
    }

    czim_blob blob = czim_cluster_get_blob(cluster, entry->blob_number);
    czim_cluster_release(cluster);
    return blob;
}
