//
//  czim_header.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_header_h
#define czim_header_h

#include "czim_types.h"
#include "czim_file.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// ZIM file header structure
// ============================================================

typedef struct czim_header {
    uint32_t magic;              // Magic number (0x044D495A)
    uint16_t major_version;      // Major version
    uint16_t minor_version;      // Minor version
    czim_uuid_t uuid;            // UUID
    uint32_t entry_count;        // Total number of entries
    uint32_t cluster_count;      // Total number of clusters
    uint64_t path_ptr_pos;       // Path pointer list position
    uint64_t title_ptr_pos;      // Title pointer list position
    uint64_t cluster_ptr_pos;    // Cluster pointer list position
    uint64_t mime_list_pos;      // MIME type list position
    uint32_t main_page;          // Main page index
    uint32_t layout_page;        // Layout page index (deprecated)
    uint64_t checksum_pos;       // Checksum position
} czim_header;

// ============================================================
// Header API
// ============================================================

/**
 * Read and parse file header
 * @param file File object
 * @param header Output header structure
 * @return CZIM_OK on success, error code on failure
 */
int czim_header_read(czim_file *file, czim_header *header);

/**
 * Validate header fields
 * @param header Header structure
 * @return CZIM_OK if valid, error code if invalid
 */
int czim_header_validate(const czim_header *header);

/**
 * Check if using new namespace scheme
 * @param header Header structure
 * @return true if new scheme (minor_version >= 1)
 */
bool czim_header_has_new_namespace_scheme(const czim_header *header);

/**
 * Check if checksum is present
 * @param header Header structure
 * @return true if checksum is present
 */
bool czim_header_has_checksum(const czim_header *header);

/**
 * Check if main page is set
 * @param header Header structure
 * @return true if main page is set
 */
bool czim_header_has_main_page(const czim_header *header);

/**
 * Check if title index is present
 * @param header Header structure
 * @return true if title index is present
 */
bool czim_header_has_title_index(const czim_header *header);

#ifdef __cplusplus
}
#endif

#endif /* czim_header_h */
