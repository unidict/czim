//
//  czim_archive_ext.h
//  czim
//
//  Extension APIs for czim_archive.
//  Include czim_archive.h for core functionality.
//
//  Created by kejinlu on 2026/4/24.
//

#ifndef czim_archive_ext_h
#define czim_archive_ext_h

#include "czim_archive.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Convenience search (uses content namespace 'C' or 'A')
// ============================================================

/**
 * Find entry by path (convenience wrapper, uses content namespace 'C' or 'A')
 * @param archive Archive object
 * @param path Path string
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_content_entry_by_path(czim_archive *archive, const char *path, uint32_t *index);

/**
 * Find entry by title (convenience wrapper, uses content namespace 'C' or 'A')
 * @param archive Archive object
 * @param title Title string
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_content_entry_by_title(czim_archive *archive, const char *title, uint32_t *index);

/**
 * Find entry range by path prefix (convenience wrapper, uses content namespace 'C' or 'A')
 * @param archive Archive object
 * @param prefix Prefix string
 * @param start_index Output start index
 * @param end_index Output end index (exclusive)
 * @return CZIM_OK on success, error code on failure
 */
int czim_archive_find_content_entry_by_path_prefix(czim_archive *archive, const char *prefix,
                                    uint32_t *start_index, uint32_t *end_index);

/**
 * Find entry range by title prefix (convenience wrapper, uses content namespace 'C' or 'A')
 * @param archive Archive object
 * @param prefix Prefix string
 * @param start_index Output start index (title index)
 * @param end_index Output end index (exclusive)
 * @return CZIM_OK on success, error code on failure
 */
int czim_archive_find_content_entry_by_title_prefix(czim_archive *archive, const char *prefix,
                                      uint32_t *start_index, uint32_t *end_index);

// ============================================================
// Namespace utilities
// ============================================================

/**
 * Find favicon entry
 *
 * Searches in order: -/favicon, -/favicon.png, I/favicon, I/favicon.png
 * This is only applicable to old namespace scheme (minor_version < 1) ZIM files.
 * New namespace scheme (minor_version >= 1) replaces favicon with M/Illustration_48x48@1.
 *
 * @param archive Archive object
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_find_favicon(czim_archive *archive, uint32_t *index);

// ============================================================
// Illustration
// ============================================================

/**
 * Illustration info (parsed from metadata path "Illustration_<W>x<H>@<scale>")
 */
typedef struct {
    uint32_t width;     // CSS pixels
    uint32_t height;    // CSS pixels
    float scale;        // devicePixelRatio
} czim_illustration_info;

/**
 * Illustration info list (dynamically allocated, call czim_illustration_infos_free when done)
 */
typedef struct {
    czim_illustration_info *items;
    uint32_t count;
} czim_illustration_infos;

/**
 * Get illustration entry by size
 *
 * Looks for M/Illustration_<size>x<size>@1.
 * If not found and size is 48, falls back to find_favicon for old format ZIM files.
 *
 * @param archive Archive object
 * @param size Illustration size in pixels (default 48)
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_get_illustration(czim_archive *archive, uint32_t size, uint32_t *index);

/**
 * Check if illustration exists for the given size
 * @param archive Archive object
 * @param size Illustration size in pixels (default 48)
 * @return true if illustration exists
 */
bool czim_archive_has_illustration(czim_archive *archive, uint32_t size);

/**
 * Get all illustration infos from the archive
 *
 * Enumerates M/Illustration_* entries and parses width/height/scale.
 * If no Illustration entries found but a favicon exists (old format), adds {48, 48, 1.0}.
 *
 * @param archive Archive object
 * @return Illustration infos (caller must call czim_illustration_infos_free)
 */
czim_illustration_infos czim_archive_get_illustration_infos(czim_archive *archive);

/**
 * Free illustration infos
 * @param infos Illustration infos to free
 */
void czim_illustration_infos_free(czim_illustration_infos *infos);

// ============================================================
// Main entry
// ============================================================

/**
 * Get main entry (tries 'W' namespace, then falls back to header's main_page)
 * @param archive Archive object
 * @param index Output entry index (can be NULL)
 * @return Entry object, NULL if not found (caller must call czim_entry_free)
 */
czim_entry *czim_archive_get_main_entry(czim_archive *archive, uint32_t *index);

#ifdef __cplusplus
}
#endif

#endif /* czim_archive_ext_h */
