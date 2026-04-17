//
//  czim_cluster.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_cluster.h"
#include "czim_compression.h"
#include "czim_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// Blob implementation
// ============================================================

static czim_blob czim_blob_create(const uint8_t *data, size_t size, czim_cluster *cluster) {
    czim_blob blob;
    blob.data = data;
    blob.size = size;
    blob.cluster = cluster;
    if (cluster) {
        czim_cluster_retain(cluster);
    }
    return blob;
}

void czim_blob_free(czim_blob *blob) {
    if (!blob) return;
    if (blob->cluster) {
        czim_cluster_release(blob->cluster);
        blob->cluster = NULL;
    }
    blob->data = NULL;
    blob->size = 0;
}

const uint8_t *czim_blob_data(const czim_blob *blob) {
    return blob ? blob->data : NULL;
}

size_t czim_blob_size(const czim_blob *blob) {
    return blob ? blob->size : 0;
}

// ============================================================
// uobject type definition
// ============================================================

static void czim_cluster_release_fn(uobject *obj) {
    czim_cluster *cluster = uobject_cast(obj, czim_cluster, obj);
    if (!cluster) return;

    // Free blob entries
    if (cluster->blob_slots) {
        for (uint32_t i = 0; i < cluster->blob_count; i++) {
            free(cluster->blob_slots[i].data);
        }
        free(cluster->blob_slots);
    }

    // Free decompression context
    if (cluster->decompress_ctx) {
        czim_compression_ctx_destroy(cluster->decompress_ctx);
    }

    // Free compressed data
    free(cluster->compressed_data);

    // Free offset array
    free(cluster->blob_offsets);

    free(cluster);
}

static uint64_t czim_cluster_memory_size_fn(uobject *obj) {
    czim_cluster *cluster = uobject_cast(obj, czim_cluster, obj);
    if (!cluster) return 0;

    size_t size = sizeof(czim_cluster) +
                  cluster->compressed_size +
                  (cluster->blob_count + 1) * sizeof(uint64_t) +
                  cluster->blob_count * sizeof(czim_blob_slot);

    // Add loaded blob data sizes
    for (uint32_t i = 0; i < cluster->blob_count; i++) {
        if (cluster->blob_slots[i].is_loaded) {
            size += (size_t)cluster->blob_slots[i].size;
        }
    }

    // Decompressor state size (~100KB)
    if (cluster->decompress_ctx) {
        size += 102400;
    }

    return (uint64_t)size;
}

static const uobject_type czim_cluster_type = {
    .name = "czim_cluster",
    .size = sizeof(czim_cluster),
    .release = czim_cluster_release_fn,
    .memory_size = czim_cluster_memory_size_fn,
};

// ============================================================
// Cluster parsing (streaming)
// ============================================================

czim_cluster *czim_cluster_read(czim_file *reader, czim_offset_t offset, czim_size_t size) {
    if (!reader) {
        return NULL;
    }

    // Read cluster info byte
    uint8_t info_byte;
    if (czim_file_read_byte(reader, offset, &info_byte) != CZIM_OK) {
        return NULL;
    }

    // Parse compression type and extended flag
    czim_compression_t compression = (czim_compression_t)(info_byte & 0x0F);
    bool is_extended = (info_byte & 0x10) != 0;

    // Check if compression type is supported
    if (!czim_compression_supported(compression)) {
        fprintf(stderr, "Unsupported compression type: %d\n", compression);
        return NULL;
    }

    // Calculate data offset
    czim_offset_t data_offset = offset + 1;

    // If size is 0, use remaining file size
    if (size == 0) {
        size = czim_file_size(reader) - offset;
    }
    size_t compressed_size = size - 1;

    // Read compressed data
    uint8_t *compressed_data = malloc(compressed_size);
    if (!compressed_data) {
        return NULL;
    }

    if (czim_file_read(reader, data_offset, compressed_size, compressed_data) != CZIM_OK) {
        free(compressed_data);
        return NULL;
    }

    // Create decompression context (streaming)
    czim_compression_ctx *decompress_ctx = NULL;
    if (compression != CZIM_COMPRESSION_NONE) {
        decompress_ctx = czim_compression_ctx_create(compression, compressed_data, compressed_size);
        if (!decompress_ctx) {
            free(compressed_data);
            return NULL;
        }
    }

    // Create Cluster object
    czim_cluster *cluster = calloc(1, sizeof(czim_cluster));
    if (!cluster) {
        if (decompress_ctx) {
            czim_compression_ctx_destroy(decompress_ctx);
        }
        free(compressed_data);
        return NULL;
    }

    // Initialize uobject (refcount=1)
    uobject_init(&cluster->obj, &czim_cluster_type, NULL);

    cluster->compression = compression;
    cluster->is_extended = is_extended;
    cluster->compressed_data = compressed_data;
    cluster->compressed_size = compressed_size;
    cluster->decompress_ctx = decompress_ctx;

    // Parse blob offset table
    // libzim style: read first offset to calculate actual offset table size
    size_t offset_size = is_extended ? 8 : 4;

    // Read first offset value (offset_size bytes)
    uint8_t first_offset_buf[8];
    if (compression == CZIM_COMPRESSION_NONE) {
        // No compression, direct copy
        memcpy(first_offset_buf, compressed_data, offset_size);
    } else {
        // Streaming decompress first offset
        size_t bytes_read = czim_compression_read(decompress_ctx, first_offset_buf, offset_size);
        if (bytes_read != offset_size) {
            fprintf(stderr, "Failed to read first offset\n");
            czim_cluster_release(cluster);
            return NULL;
        }
    }

    // Parse first offset
    uint64_t first_offset;
    if (is_extended) {
        first_offset = read_le64(first_offset_buf);
    } else {
        first_offset = read_le32(first_offset_buf);
    }

    // Calculate actual blob count and offset table size
    uint32_t blob_count = (uint32_t)(first_offset / offset_size) - 1;
    size_t offset_table_size = (blob_count + 1) * offset_size;

    // Allocate offset table buffer
    uint8_t *offset_table = malloc(offset_table_size);
    if (!offset_table) {
        czim_cluster_release(cluster);
        return NULL;
    }

    // Copy first offset
    memcpy(offset_table, first_offset_buf, offset_size);

    // Read remaining offsets
    if (compression == CZIM_COMPRESSION_NONE) {
        // No compression, direct copy remaining offsets
        memcpy(offset_table + offset_size, compressed_data + offset_size, offset_table_size - offset_size);
    } else {
        // Streaming decompress remaining offsets
        size_t remaining = offset_table_size - offset_size;
        size_t bytes_read = czim_compression_read(decompress_ctx,
                                                         offset_table + offset_size,
                                                         remaining);
        if (bytes_read != remaining) {
            fprintf(stderr, "Failed to read remaining offsets: expected=%zu, read=%zu\n",
                    remaining, bytes_read);
            free(offset_table);
            czim_cluster_release(cluster);
            return NULL;
        }
    }

    // Save blob count
    cluster->blob_count = blob_count;

    // Allocate offset array
    cluster->blob_offsets = malloc((cluster->blob_count + 1) * sizeof(uint64_t));
    if (!cluster->blob_offsets) {
        free(offset_table);
        czim_cluster_release(cluster);
        return NULL;
    }

    // Read all offsets
    for (uint32_t i = 0; i <= cluster->blob_count; i++) {
        if (is_extended) {
            cluster->blob_offsets[i] = read_le64(offset_table + i * 8);
        } else {
            cluster->blob_offsets[i] = read_le32(offset_table + i * 4);
        }
    }

    free(offset_table);

    // Create blob entries
    cluster->blob_slots = calloc(cluster->blob_count, sizeof(czim_blob_slot));
    if (!cluster->blob_slots) {
        czim_cluster_release(cluster);
        return NULL;
    }

    // Initialize blob entries (record offset and size only)
    for (uint32_t i = 0; i < cluster->blob_count; i++) {
        cluster->blob_slots[i].offset = cluster->blob_offsets[i];
        cluster->blob_slots[i].size = cluster->blob_offsets[i + 1] - cluster->blob_offsets[i];
        cluster->blob_slots[i].data = NULL;
        cluster->blob_slots[i].is_loaded = false;
    }

    // Initialize created counter
    cluster->blob_slots_created = 0;

    return cluster;
}

czim_cluster *czim_cluster_retain(czim_cluster *cluster) {
    if (!cluster) return NULL;
    uobject_retain(&cluster->obj);
    return cluster;
}

bool czim_cluster_release(czim_cluster *cluster) {
    if (!cluster) return false;
    return uobject_release(&cluster->obj);
}

uint32_t czim_cluster_blob_count(const czim_cluster *cluster) {
    return cluster ? cluster->blob_count : 0;
}

czim_blob czim_cluster_get_blob(czim_cluster *cluster, uint32_t index) {
    if (!cluster || index >= cluster->blob_count) {
        fprintf(stderr, "Invalid cluster or index: cluster=%p, index=%u, blob_count=%u\n",
                (void*)cluster, index, cluster ? cluster->blob_count : 0);
        return czim_blob_create(NULL, 0, NULL);
    }

    czim_blob_slot *entry = &cluster->blob_slots[index];

    // On-demand decompression if not loaded (libzim-style incremental decompression)
    if (!entry->is_loaded) {
        // Create blob entries from 0 to index
        for (uint32_t i = cluster->blob_slots_created; i <= index; i++) {
            czim_blob_slot *e = &cluster->blob_slots[i];

            // Allocate memory
            e->data = malloc(e->size);
            if (!e->data) {
                fprintf(stderr, "Failed to allocate blob data: index=%u, size=%llu\n",
                        i, (unsigned long long)e->size);
                return czim_blob_create(NULL, 0, NULL);
            }

            // Read next blob from incremental decompressor
            size_t bytes_read = 0;
            if (cluster->compression == CZIM_COMPRESSION_NONE) {
                // No compression, direct copy from compressed data
                memcpy(e->data, cluster->compressed_data + e->offset, e->size);
                bytes_read = e->size;
            } else {
                // Incremental decompression: continue from current position
                bytes_read = czim_compression_read(cluster->decompress_ctx,
                                                        e->data,
                                                        (size_t)e->size);
            }

            if (bytes_read != e->size) {
                fprintf(stderr, "Failed to read blob: index=%u, expected=%llu, read=%zu\n",
                        i, (unsigned long long)e->size, bytes_read);
                free(e->data);
                e->data = NULL;
                return czim_blob_create(NULL, 0, NULL);
            }

            e->is_loaded = true;
        }

        // Update created counter
        cluster->blob_slots_created = index + 1;
    }

    return czim_blob_create(entry->data, (size_t)entry->size, cluster);
}

size_t czim_cluster_get_blob_size(const czim_cluster *cluster, uint32_t index) {
    if (!cluster || index >= cluster->blob_count) {
        return 0;
    }

    return (size_t)cluster->blob_slots[index].size;
}

size_t czim_cluster_memory_size(const czim_cluster *cluster) {
    if (!cluster) return 0;
    return (size_t)uobject_memory_size((uobject *)&cluster->obj);
}
