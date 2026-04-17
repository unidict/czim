//
//  czim_compression.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_compression_h
#define czim_compression_h

#include "czim_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Compression query
// ============================================================

/**
 * Check if compression type is supported
 * @param type Compression type
 * @return true if supported
 */
bool czim_compression_supported(czim_compression_t type);

// ============================================================
// Streaming decompression context
// ============================================================

typedef struct czim_compression_ctx czim_compression_ctx;

/**
 * Create a streaming decompression context
 * @param type Compression type
 * @param src Compressed data
 * @param src_size Compressed data size
 * @return Compression context, NULL on failure
 *
 * The context maintains internal state to support incremental decompression.
 * Each call to czim_compression_ctx_read continues from where the last call stopped.
 */
czim_compression_ctx *czim_compression_ctx_create(
    czim_compression_t type,
    const uint8_t *src, size_t src_size
);

/**
 * Destroy a streaming decompression context
 * @param ctx Compression context
 */
void czim_compression_ctx_destroy(czim_compression_ctx *ctx);

/**
 * Read data from current position (incremental decompression)
 * @param ctx Compression context
 * @param output Output buffer
 * @param output_size Number of bytes to read
 * @return Actual bytes read, 0 if end of stream
 *
 * This function continues decompression from the current position,
 * reading up to output_size bytes into output.
 * The context state advances; the next call continues from the new position.
 *
 * This is the only read function, used for all scenarios:
 * - Reading offset tables during initialization
 * - Reading blob data
 *
 * Note: This is true incremental decompression; it never resets the decompressor state.
 * Consecutive calls sequentially read all data.
 */
size_t czim_compression_read(
    czim_compression_ctx *ctx,
    uint8_t *output,
    size_t output_size
);

#ifdef __cplusplus
}
#endif

#endif /* czim_compression_h */
