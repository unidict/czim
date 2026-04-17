//
//  czim_archive.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_archive_h
#define czim_archive_h

#include "czim_types.h"
#include "czim_header.h"
#include "czim_file.h"
#include "czim_entry.h"
#include "czim_cluster.h"
#include "ucache.h"
#include "czim_narrowdown.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Archive structure
// ============================================================

typedef struct czim_archive {
    czim_file *file;             // Low-level file I/O
    czim_header header;           // File header

    // MIME type list
    char **mime_types;           // MIME type array
    size_t mime_type_count;      // Number of MIME types

    // Index pointers (lazy-loaded)
    uint64_t *path_ptrs;          // Path pointer array
    uint64_t *title_ptrs;        // Title pointer array
    uint64_t *cluster_ptrs;      // Cluster pointer array

    // Caches
    ucache *entry_cache;        // Entry LRU cache
    ucache *cluster_cache;       // Cluster LRU cache

    // NarrowDown indices
    czim_narrowdown *path_narrowdown;   // Path/URL index
    czim_narrowdown *title_narrowdown;  // Title index
} czim_archive;

// ============================================================
// Archive API
// ============================================================

/**
 * Open a ZIM archive
 * @param path File path
 * @return Archive object, NULL on failure
 */
czim_archive *czim_archive_open(const char *path);

/**
 * Close an archive
 * @param archive Archive object
 */
void czim_archive_close(czim_archive *archive);

// ============================================================
// Archive properties
// ============================================================

/**
 * Get UUID
 * @param archive Archive object
 * @return UUID string (static buffer)
 */
const char *czim_archive_uuid(const czim_archive *archive);

/**
 * Get MIME type by index
 * @param archive Archive object
 * @param index MIME type index
 * @return MIME type string, NULL on failure
 */
const char *czim_archive_mime_type(const czim_archive *archive, uint16_t index);

/**
 * Check if main page is set
 * @param archive Archive object
 * @return true if main page exists
 */
bool czim_archive_has_main_entry(const czim_archive *archive);

/**
 * Check if title index exists
 * @param archive Archive object
 * @return true if title index exists
 */
bool czim_archive_has_title_index(const czim_archive *archive);

/**
 * Get entry count
 * @param archive Archive object
 * @return Number of entries
 */
uint32_t czim_archive_entry_count(const czim_archive *archive);

/**
 * Get article count (front articles only, e.g. text/html entries)
 *
 * Tries in order:
 * 1. X/listing/titleOrdered/v1 index (most accurate)
 * 2. M/Counter metadata (parse text/html count)
 * 3. Fallback: count entries in 'C' namespace
 *
 * @param archive Archive object
 * @return Number of front articles, 0 on failure
 */
uint32_t czim_archive_article_count(czim_archive *archive);

/**
 * Get cluster count
 * @param archive Archive object
 * @return Number of clusters
 */
uint32_t czim_archive_cluster_count(const czim_archive *archive);

// ============================================================
// Namespace utilities
// ============================================================

/**
 * ZIM Namespace Overview
 *
 * Namespaces separate different types of directory entries in a ZIM archive.
 * The full key for lookup is "ns/path" (e.g. "C/African_Americans").
 *
 * New namespace scheme (minor_version >= 1):
 *   C  - User content entries (articles)
 *   M  - ZIM metadata (e.g. "Counter", "Title", "Illustration_48x48")
 *   W  - Well-known entries (e.g. "mainPage", "favicon")
 *   X  - Search indexes (e.g. "title/xapian", "listing/titleOrdered/v1")
 *
 * Old namespace scheme (minor_version < 1):
 *   -  - Layout (CSS, favicon, JavaScript, images not related to articles)
 *   A  - Articles (user content)
 *   B  - Article metadata
 *   I  - Images, files
 *   J  - Images, text
 *   M  - ZIM metadata
 *   U  - Categories, text
 *   V  - Categories, article list
 *   W  - Categories per article, category list
 *   X  - Search indexes
 *
 * Convenience functions (find_content_entry_by_path, etc.) automatically
 * use 'C' (new) or 'A' (old) based on the archive's minor_version.
 */

/**
 * Check if using new namespace scheme (minor_version >= 1)
 * @param archive Archive object
 * @return true if new namespace scheme
 */
bool czim_archive_has_new_namespace_scheme(const czim_archive *archive);

// ============================================================
// Entry access
// ============================================================

/**
 * Get entry by path index
 *
 * Entries are sorted by path (URL) order. The index is the position in this
 * path-sorted entry table (i.e. path index, range [0, entry_count)).
 *
 * @param archive Archive object
 * @param index Path index of the entry
 * @return Entry object, NULL on failure (caller must call czim_entry_free)
 */
czim_entry *czim_archive_get_entry_by_index(czim_archive *archive, uint32_t index);

/**
 * Find entry by path with explicit namespace
 * @param archive Archive object
 * @param ns Namespace character (e.g. 'C', 'M', 'W', 'X')
 * @param path Path string
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_entry_by_path(czim_archive *archive, char ns, const char *path, uint32_t *index);

/**
 * Find entry by title with explicit namespace
 * @param archive Archive object
 * @param ns Namespace character
 * @param title Title string
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_entry_by_title(czim_archive *archive, char ns, const char *title, uint32_t *index);

/**
 * Resolve redirects, returning the final target entry
 * @param archive Archive object
 * @param entry Starting entry (reference count will be incremented)
 * @param max_redirects Maximum number of redirects to follow
 * @return Final entry, NULL on failure or max redirects exceeded (caller must call czim_entry_free)
 */
czim_entry *czim_archive_resolve_redirect(czim_archive *archive, czim_entry *entry, int max_redirects);

/**
 * Find a metadata entry by name (searches in 'M' namespace, resolves redirects)
 * @param archive Archive object
 * @param name Metadata name (e.g. "Counter", "Illustration_48x48")
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_metadata(czim_archive *archive, const char *name, uint32_t *index);

// ============================================================
// Prefix search
// ============================================================

/**
 * Find entry range by path prefix with explicit namespace
 * @param archive Archive object
 * @param ns Namespace character
 * @param prefix Prefix string
 * @param start_index Output start index
 * @param end_index Output end index (exclusive)
 * @return CZIM_OK on success, error code on failure
 */
int czim_archive_find_entry_by_path_prefix(czim_archive *archive, char ns, const char *prefix,
                                         uint32_t *start_index, uint32_t *end_index);

/**
 * Find entry range by title prefix with explicit namespace
 * @param archive Archive object
 * @param ns Namespace character
 * @param prefix Prefix string
 * @param start_index Output start index (title index)
 * @param end_index Output end index (exclusive)
 * @return CZIM_OK on success, error code on failure
 */
int czim_archive_find_entry_by_title_prefix(czim_archive *archive, char ns, const char *prefix,
                                          uint32_t *start_index, uint32_t *end_index);

// ============================================================
// Blob access
// ============================================================

/**
 * Get blob data for an entry
 * @param archive Archive object
 * @param entry Entry (must not be a redirect)
 * @return Blob object, caller must call czim_blob_free when done
 */
czim_blob czim_archive_get_blob(czim_archive *archive, const czim_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* czim_archive_h */
