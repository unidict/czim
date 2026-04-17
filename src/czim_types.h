//
//  czim_types.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_types_h
#define czim_types_h

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// ZIM file format constants
// ============================================================

// ZIM magic number: "ZIM\x04" in little-endian = 0x044D495A
#define CZIM_MAGIC 0x044D495A

// File header size in bytes
#define CZIM_HEADER_SIZE 80

// Special MIME type values
#define CZIM_MIME_REDIRECT     0xFFFF
#define CZIM_MIME_LINKTARGET   0xFFFE
#define CZIM_MIME_DELETED      0xFFFD

// Invalid index sentinel value
#define CZIM_INVALID_INDEX     0xFFFFFFFF

// ============================================================
// Basic type definitions
// ============================================================

// Entry index type
typedef uint32_t czim_entry_index_t;

// Cluster index type
typedef uint32_t czim_cluster_index_t;

// Blob index type
typedef uint32_t czim_blob_index_t;

// File offset type
typedef uint64_t czim_offset_t;

// Size type
typedef uint64_t czim_size_t;

// ============================================================
// Compression types
// ============================================================

typedef enum {
    CZIM_COMPRESSION_NONE = 1,    // No compression
    CZIM_COMPRESSION_ZIP  = 2,    // ZIP (deprecated)
    CZIM_COMPRESSION_BZIP2 = 3,   // BZIP2 (deprecated)
    CZIM_COMPRESSION_LZMA = 4,    // LZMA
    CZIM_COMPRESSION_ZSTD = 5     // ZSTD
} czim_compression_t;

// ============================================================
// Error codes
// ============================================================

typedef enum {
    CZIM_OK = 0,                  // Success
    CZIM_ERROR_INVALID_PARAM = -1,// Invalid parameter
    CZIM_ERROR_IO = -2,           // I/O error
    CZIM_ERROR_FORMAT = -3,       // File format error
    CZIM_ERROR_NOT_FOUND = -4,    // Not found
    CZIM_ERROR_MEMORY = -5,       // Memory allocation failure
    CZIM_ERROR_COMPRESSION = -6,  // Decompression error
    CZIM_ERROR_REDIRECT = -7,     // Redirect error
    CZIM_ERROR_UNSUPPORTED = -8   // Unsupported feature
} czim_error_t;

// ============================================================
// UUID type
// ============================================================

typedef struct {
    uint8_t bytes[16];
} czim_uuid_t;

// Convert UUID to string (output buffer must be at least 37 bytes)
void czim_uuid_to_string(const czim_uuid_t *uuid, char *out);

// Compare two UUIDs
int czim_uuid_compare(const czim_uuid_t *a, const czim_uuid_t *b);

#ifdef __cplusplus
}
#endif

#endif /* czim_types_h */
