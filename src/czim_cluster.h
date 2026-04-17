//
//  czim_cluster.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_cluster_h
#define czim_cluster_h

#include "czim_types.h"
#include "czim_file.h"
#include "czim_compression.h"
#include "uobject.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct czim_compression_ctx czim_compression_ctx;

// ============================================================
// Blob slot (internal to cluster, tracks loading state)
// ============================================================

/**
 * struct czim_blob_slot - A blob slot within a cluster
 * @offset: Start offset in decompressed data
 * @size: Blob size
 * @data: Decompressed data (NULL = not yet loaded)
 * @is_loaded: Whether data has been loaded into memory
 */
typedef struct czim_blob_slot {
    uint64_t offset;          // Start offset in decompressed data
    uint64_t size;            // Blob size
    uint8_t *data;            // Decompressed data (NULL = not loaded)
    bool is_loaded;           // Whether data is loaded
} czim_blob_slot;

// ============================================================
// Cluster structure
// ============================================================

typedef struct czim_cluster {
    uobject obj;                    // Reference counting

    czim_compression_t compression; // Compression type
    bool is_extended;               // Whether using 64-bit offsets

    uint32_t blob_count;            // Number of blobs
    uint64_t *blob_offsets;         // Blob offset array (blob_count + 1 elements)

    // Streaming decompression support
    uint8_t *compressed_data;       // Compressed data (NULL = no compression)
    size_t compressed_size;         // Compressed data size

    czim_compression_ctx *decompress_ctx;  // Streaming decompression context
    czim_blob_slot *blob_slots;               // Blob slot array (blob_count elements)
    uint32_t blob_slots_created;              // Number of created blob slots (for incremental decompression)
} czim_cluster;

// ============================================================
// Blob (read-only data handle returned by cluster)
// ============================================================

typedef struct czim_blob {
    const uint8_t *data;         // Data pointer
    size_t size;                 // Data size
    czim_cluster *cluster;       // Owning cluster (retained reference)
} czim_blob;

/**
 * Free a blob (releases the underlying cluster reference)
 */
void czim_blob_free(czim_blob *blob);

/**
 * Get data pointer
 */
const uint8_t *czim_blob_data(const czim_blob *blob);

/**
 * Get data size
 */
size_t czim_blob_size(const czim_blob *blob);

// ============================================================
// Cluster API
// ============================================================

/**
 * Read a cluster from file
 * @param reader File reader
 * @param offset Cluster offset in the file
 * @param size Cluster size (0 = read to end of file)
 * @return Cluster object, NULL on failure
 */
czim_cluster *czim_cluster_read(czim_file *reader, czim_offset_t offset, czim_size_t size);

/**
 * Retain a cluster (increment reference count)
 * @param cluster Cluster object
 */
czim_cluster *czim_cluster_retain(czim_cluster *cluster);

/**
 * Release a cluster (decrement reference count)
 * @param cluster Cluster object
 */
bool czim_cluster_release(czim_cluster *cluster);

/**
 * Get blob count
 * @param cluster Cluster object
 * @return Number of blobs
 */
uint32_t czim_cluster_blob_count(const czim_cluster *cluster);

/**
 * Get a blob by index
 * @param cluster Cluster object
 * @param index Blob index
 * @return Blob object
 */
czim_blob czim_cluster_get_blob(czim_cluster *cluster, uint32_t index);

/**
 * Get blob size by index
 * @param cluster Cluster object
 * @param index Blob index
 * @return Blob size
 */
size_t czim_cluster_get_blob_size(const czim_cluster *cluster, uint32_t index);

/**
 * Get cluster memory size (for cache management)
 * @param cluster Cluster object
 * @return Memory size
 */
size_t czim_cluster_memory_size(const czim_cluster *cluster);

#ifdef __cplusplus
}
#endif

#endif /* czim_cluster_h */
