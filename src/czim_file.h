//
//  czim_file.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_file_h
#define czim_file_h

#include "czim_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// File I/O structure
// ============================================================

typedef struct czim_file czim_file;

// ============================================================
// File I/O API
// ============================================================

/**
 * Open a file
 * @param path File path
 * @return File object, NULL on failure
 */
czim_file *czim_file_open(const char *path);

/**
 * Close a file
 * @param file File object
 */
void czim_file_close(czim_file *file);

/**
 * Get total file size
 * @param file File object
 * @return File size in bytes
 */
czim_size_t czim_file_size(const czim_file *file);

/**
 * Read data from specified offset
 * @param file File object
 * @param offset File offset
 * @param size Number of bytes to read
 * @param out_buf Output buffer
 * @return CZIM_OK on success, error code on failure
 */
int czim_file_read(czim_file *file, czim_offset_t offset, size_t size, void *out_buf);

/**
 * Read a single byte
 * @param file File object
 * @param offset File offset
 * @param out Output byte
 * @return CZIM_OK on success, error code on failure
 */
int czim_file_read_byte(czim_file *file, czim_offset_t offset, uint8_t *out);

/**
 * Read a 16-bit little-endian integer
 * @param file File object
 * @param offset File offset
 * @param out Output value
 * @return CZIM_OK on success, error code on failure
 */
int czim_file_read_u16(czim_file *file, czim_offset_t offset, uint16_t *out);

/**
 * Read a 32-bit little-endian integer
 * @param file File object
 * @param offset File offset
 * @param out Output value
 * @return CZIM_OK on success, error code on failure
 */
int czim_file_read_u32(czim_file *file, czim_offset_t offset, uint32_t *out);

/**
 * Read a 64-bit little-endian integer
 * @param file File object
 * @param offset File offset
 * @param out Output value
 * @return CZIM_OK on success, error code on failure
 */
int czim_file_read_u64(czim_file *file, czim_offset_t offset, uint64_t *out);

/**
 * Get filename
 * @param file File object
 * @return Filename string
 */
const char *czim_file_filename(const czim_file *file);

#ifdef __cplusplus
}
#endif

#endif /* czim_file_h */
