//
//  czim_entry.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_entry_h
#define czim_entry_h

#include "czim_types.h"
#include "czim_file.h"
#include "uobject.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Entry structure (Directory Entry in ZIM specification)
// ============================================================

typedef struct czim_entry {
    uobject obj;                   // Reference counting

    uint16_t mime_type;           // MIME type index
    char ns;                      // Namespace
    uint32_t revision;            // Revision number

    // Non-redirect entries
    uint32_t cluster_number;     // Cluster index
    uint32_t blob_number;        // Blob index

    // Redirect entries
    uint32_t redirect_index;     // Redirect target index

    char *path;                   // Path (URL)
    char *title;                 // Title
    char *parameter;             // Parameters
} czim_entry;

// ============================================================
// Entry API
// ============================================================

/**
 * Read a directory entry from file
 * @param reader File reader
 * @param offset Offset of the directory entry in the file
 * @return Entry object, NULL on failure
 */
czim_entry *czim_entry_read(czim_file *reader, czim_offset_t offset);

/**
 * Free a directory entry (wrapper for uobject_release)
 * @param entry Entry object
 */
void czim_entry_free(czim_entry *entry);

/**
 * Check if the entry is a redirect
 * @param entry Entry object
 * @return true if redirect
 */
bool czim_entry_is_redirect(const czim_entry *entry);

/**
 * Check if the entry is an article
 * @param entry Entry object
 * @return true if article
 */
bool czim_entry_is_article(const czim_entry *entry);

/**
 * Check if the entry is deleted
 * @param entry Entry object
 * @return true if deleted
 */
bool czim_entry_is_deleted(const czim_entry *entry);

/**
 * Get title (returns path if title is empty)
 * @param entry Entry object
 * @return Title string
 */
const char *czim_entry_get_title(const czim_entry *entry);

/**
 * Get path
 * @param entry Entry object
 * @return Path string
 */
const char *czim_entry_get_path(const czim_entry *entry);

/**
 * Get namespace character
 * @param entry Entry object
 * @return Namespace character (e.g. 'C', 'M', 'W', 'X')
 */
char czim_entry_get_namespace(const czim_entry *entry);

/**
 * Get long path (namespace + '/' + path)
 * Note: uses an internal static buffer, not thread-safe
 * @param entry Entry object
 * @return Long path string (e.g. "C/African_Americans")
 */
const char *czim_entry_get_long_path(const czim_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* czim_entry_h */
