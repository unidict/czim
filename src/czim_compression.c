//
//  czim_compression.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_compression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ZSTD
#include <zstd.h>

// LZMA
#include <lzma.h>

// ============================================================
// Streaming decompression context definition (libzim style)
// ============================================================

struct czim_compression_ctx {
    czim_compression_t type;
    const uint8_t *src;
    size_t src_size;
    size_t decompressed_size;  // Total decompressed size

    // Current decompression position (key to incremental decompression)
    size_t decompressed_pos;   // Bytes already decompressed

    // ZSTD context
    ZSTD_DCtx *zstd_dctx;
    size_t zstd_input_pos;     // ZSTD input position already processed

    // LZMA context
    lzma_stream lzma_stream;
    bool lzma_initialized;
};

// ============================================================
// ZSTD incremental streaming decompression
// ============================================================

static int zstd_ctx_init(czim_compression_ctx *ctx) {
    ctx->zstd_dctx = ZSTD_createDCtx();
    if (!ctx->zstd_dctx) {
        fprintf(stderr, "ZSTD: Failed to create decompression context\n");
        return CZIM_ERROR_MEMORY;
    }

    // Try to get decompressed size
    unsigned long long content_size = ZSTD_getFrameContentSize(ctx->src, ctx->src_size);

    if (content_size == ZSTD_CONTENTSIZE_ERROR) {
        fprintf(stderr, "ZSTD: Invalid compressed data\n");
        ZSTD_freeDCtx(ctx->zstd_dctx);
        ctx->zstd_dctx = NULL;
        return CZIM_ERROR_COMPRESSION;
    }

    if (content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        // Unknown size, need streaming decompression to determine size
        ZSTD_inBuffer input = { ctx->src, ctx->src_size, 0 };
        size_t ret = 0;

        // Use a small buffer to determine decompressed size
        uint8_t temp_buf[4096];
        ZSTD_outBuffer out = { temp_buf, sizeof(temp_buf), 0 };

        size_t total_size = 0;
        while (input.pos < input.size) {
            ret = ZSTD_decompressStream(ctx->zstd_dctx, &out, &input);
            if (ZSTD_isError(ret)) {
                fprintf(stderr, "ZSTD decompression error: %s\n", ZSTD_getErrorName(ret));
                ZSTD_freeDCtx(ctx->zstd_dctx);
                ctx->zstd_dctx = NULL;
                return CZIM_ERROR_COMPRESSION;
            }
            total_size += out.pos;
            out.pos = 0;
        }

        ctx->decompressed_size = total_size;

        // Reset stream for subsequent use (recreate decompressor context)
        ZSTD_freeDCtx(ctx->zstd_dctx);
        ctx->zstd_dctx = ZSTD_createDCtx();
        if (!ctx->zstd_dctx) {
            return CZIM_ERROR_MEMORY;
        }
    } else {
        ctx->decompressed_size = (size_t)content_size;
    }

    ctx->zstd_input_pos = 0;
    ctx->decompressed_pos = 0;

    return CZIM_OK;
}

static size_t zstd_ctx_read_next(czim_compression_ctx *ctx, uint8_t *output, size_t output_size) {
    if (ctx->decompressed_pos >= ctx->decompressed_size) {
        return 0;  // End of stream
    }

    // Set output buffer
    ZSTD_outBuffer out = { output, output_size, 0 };

    // Set input buffer (continue from last position)
    ZSTD_inBuffer input = { ctx->src, ctx->src_size, ctx->zstd_input_pos };

    // Incremental decompression (do not reset decompressor state)
    while (out.pos < out.size && input.pos < input.size) {
        size_t ret = ZSTD_decompressStream(ctx->zstd_dctx, &out, &input);
        if (ZSTD_isError(ret) && ret != 0) {
            fprintf(stderr, "ZSTD decompression error: %s\n", ZSTD_getErrorName(ret));
            return 0;
        }
    }

    // Update positions
    ctx->zstd_input_pos = input.pos;
    ctx->decompressed_pos += out.pos;

    return out.pos;
}

static void zstd_ctx_cleanup(czim_compression_ctx *ctx) {
    if (ctx->zstd_dctx) {
        ZSTD_freeDCtx(ctx->zstd_dctx);
        ctx->zstd_dctx = NULL;
    }
}

// ============================================================
// LZMA incremental streaming decompression
// ============================================================

static int lzma_ctx_init(czim_compression_ctx *ctx) {
    memset(&ctx->lzma_stream, 0, sizeof(ctx->lzma_stream));

    lzma_ret ret = lzma_alone_decoder(&ctx->lzma_stream, UINT64_MAX);
    if (ret != LZMA_OK) {
        fprintf(stderr, "LZMA: Failed to initialize decoder\n");
        return CZIM_ERROR_COMPRESSION;
    }

    ctx->lzma_initialized = true;

    // Need streaming decompression to determine total size
    ctx->lzma_stream.next_in = (uint8_t *)ctx->src;
    ctx->lzma_stream.avail_in = ctx->src_size;

    size_t total_size = 0;
    uint8_t temp_buf[4096];
    ctx->lzma_stream.next_out = temp_buf;
    ctx->lzma_stream.avail_out = sizeof(temp_buf);

    while (true) {
        ret = lzma_code(&ctx->lzma_stream, LZMA_FINISH);

        if (ret == LZMA_STREAM_END) {
            total_size += sizeof(temp_buf) - ctx->lzma_stream.avail_out;
            break;
        }

        if (ret == LZMA_BUF_ERROR) {
            total_size += sizeof(temp_buf) - ctx->lzma_stream.avail_out;
            ctx->lzma_stream.next_out = temp_buf;
            ctx->lzma_stream.avail_out = sizeof(temp_buf);
            continue;
        }

        if (ret != LZMA_OK) {
            fprintf(stderr, "LZMA decompression error: %d\n", ret);
            lzma_end(&ctx->lzma_stream);
            ctx->lzma_initialized = false;
            return CZIM_ERROR_COMPRESSION;
        }
    }

    ctx->decompressed_size = total_size;

    // Reset stream for subsequent use
    lzma_end(&ctx->lzma_stream);
    memset(&ctx->lzma_stream, 0, sizeof(ctx->lzma_stream));
    ret = lzma_alone_decoder(&ctx->lzma_stream, UINT64_MAX);
    if (ret != LZMA_OK) {
        return CZIM_ERROR_COMPRESSION;
    }

    ctx->lzma_stream.next_in = (uint8_t *)ctx->src;
    ctx->lzma_stream.avail_in = ctx->src_size;
    ctx->decompressed_pos = 0;

    return CZIM_OK;
}

static size_t lzma_ctx_read_next(czim_compression_ctx *ctx, uint8_t *output, size_t output_size) {
    if (ctx->decompressed_pos >= ctx->decompressed_size) {
        return 0;  // End of stream
    }

    // Set output buffer
    ctx->lzma_stream.next_out = output;
    ctx->lzma_stream.avail_out = output_size;

    // Incremental decompression (do not reset decompressor state)
    lzma_ret ret;
    size_t total_out = 0;

    while (ctx->lzma_stream.avail_out > 0) {
        ret = lzma_code(&ctx->lzma_stream, LZMA_FINISH);

        if (ret == LZMA_STREAM_END) {
            total_out = output_size - ctx->lzma_stream.avail_out;
            break;
        }

        if (ret == LZMA_BUF_ERROR) {
            if (ctx->lzma_stream.avail_in == 0) {
                // No more input data
                total_out = output_size - ctx->lzma_stream.avail_out;
                break;
            }
            continue;
        }

        if (ret != LZMA_OK) {
            fprintf(stderr, "LZMA decompression error: %d\n", ret);
            return 0;
        }
    }

    ctx->decompressed_pos += total_out;

    return total_out;
}

static void lzma_ctx_cleanup(czim_compression_ctx *ctx) {
    if (ctx->lzma_initialized) {
        lzma_end(&ctx->lzma_stream);
        ctx->lzma_initialized = false;
    }
}

// ============================================================
// Compression query
// ============================================================

bool czim_compression_supported(czim_compression_t type) {
    switch (type) {
        case CZIM_COMPRESSION_NONE:
        case CZIM_COMPRESSION_LZMA:
        case CZIM_COMPRESSION_ZSTD:
            return true;
        default:
            return false;
    }
}

// ============================================================
// Streaming decompression context
// ============================================================

czim_compression_ctx *czim_compression_ctx_create(
    czim_compression_t type,
    const uint8_t *src, size_t src_size
) {
    if (!src || src_size == 0) {
        return NULL;
    }

    czim_compression_ctx *ctx = calloc(1, sizeof(czim_compression_ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->type = type;
    ctx->src = src;
    ctx->src_size = src_size;
    ctx->decompressed_pos = 0;
    ctx->zstd_input_pos = 0;
    ctx->lzma_initialized = false;

    int ret = CZIM_OK;

    switch (type) {
        case CZIM_COMPRESSION_NONE:
            ctx->decompressed_size = src_size;
            break;

        case CZIM_COMPRESSION_ZSTD:
            ret = zstd_ctx_init(ctx);
            break;

        case CZIM_COMPRESSION_LZMA:
            ret = lzma_ctx_init(ctx);
            break;

        default:
            fprintf(stderr, "Unsupported compression type: %d\n", type);
            ret = CZIM_ERROR_UNSUPPORTED;
            break;
    }

    if (ret != CZIM_OK) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void czim_compression_ctx_destroy(czim_compression_ctx *ctx) {
    if (!ctx) {
        return;
    }

    // Clean up compression context
    switch (ctx->type) {
        case CZIM_COMPRESSION_ZSTD:
            zstd_ctx_cleanup(ctx);
            break;

        case CZIM_COMPRESSION_LZMA:
            lzma_ctx_cleanup(ctx);
            break;

        default:
            break;
    }

    free(ctx);
}

size_t czim_compression_read(
    czim_compression_ctx *ctx,
    uint8_t *output,
    size_t output_size
) {
    if (!ctx || !output || output_size == 0) {
        return 0;
    }

    switch (ctx->type) {
        case CZIM_COMPRESSION_NONE: {
            // No compression, direct copy
            size_t available = ctx->src_size - ctx->decompressed_pos;
            size_t to_read = (output_size < available) ? output_size : available;
            memcpy(output, ctx->src + ctx->decompressed_pos, to_read);
            ctx->decompressed_pos += to_read;
            return to_read;
        }

        case CZIM_COMPRESSION_ZSTD:
            return zstd_ctx_read_next(ctx, output, output_size);

        case CZIM_COMPRESSION_LZMA:
            return lzma_ctx_read_next(ctx, output, output_size);

        default:
            return 0;
    }
}
