//
//  czim_file.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_file.h"
#include "czim_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// Cross-platform support
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define fseeko _fseeki64
#define ftello _ftelli64
typedef __int64 off_t;
typedef SRWLOCK czim_mutex;
#define MUTEX_INIT(m)    (InitializeSRWLock(&(m)), true)
#define MUTEX_DESTROY(m) ((void)0)
#define MUTEX_LOCK(m)    AcquireSRWLockExclusive(&(m))
#define MUTEX_UNLOCK(m)  ReleaseSRWLockExclusive(&(m))
#else
#include <pthread.h>
#include <sys/types.h>
typedef pthread_mutex_t czim_mutex;
#define MUTEX_INIT(m)    (pthread_mutex_init(&(m), NULL) == 0)
#define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define MUTEX_LOCK(m)    pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m)  pthread_mutex_unlock(&(m))
#endif

// ============================================================
// File I/O structure
// ============================================================

struct czim_file {
    char *filename;              // Filename
    FILE *fp;                    // File pointer
    czim_size_t total_size;      // File size
    czim_mutex lock;             // Read lock
};

// ============================================================
// API implementation
// ============================================================

czim_file *czim_file_open(const char *path) {
    if (!path) {
        return NULL;
    }

    // Runtime little-endian platform check
    uint16_t endian_test = 0x0001;
    if (*(uint8_t *)&endian_test != 0x01) {
        return NULL;
    }

    // Open file
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    // Get file size
    if (fseeko(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    off_t file_size = ftello(fp);
    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }
    fseeko(fp, 0, SEEK_SET);

    czim_file *file = calloc(1, sizeof(czim_file));
    if (!file) {
        fclose(fp);
        return NULL;
    }

    // Save filename
    file->filename = strdup(path);
    if (!file->filename) {
        fclose(fp);
        free(file);
        return NULL;
    }

    if (!MUTEX_INIT(file->lock)) {
        fclose(fp);
        free(file->filename);
        free(file);
        return NULL;
    }

    file->fp = fp;
    file->total_size = (czim_size_t)file_size;

    return file;
}

void czim_file_close(czim_file *file) {
    if (!file) {
        return;
    }

    MUTEX_DESTROY(file->lock);
    fclose(file->fp);
    free(file->filename);
    free(file);
}

czim_size_t czim_file_size(const czim_file *file) {
    return file ? file->total_size : 0;
}

int czim_file_read(czim_file *file, czim_offset_t offset, size_t size, void *out_buf) {
    if (!file || !out_buf) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    // Validate offset
    if (offset >= file->total_size) {
        return CZIM_ERROR_IO;
    }

    // Clamp read size
    if (offset + size > file->total_size) {
        size = file->total_size - offset;
    }

    MUTEX_LOCK(file->lock);

    if (fseeko(file->fp, (off_t)offset, SEEK_SET) != 0) {
        MUTEX_UNLOCK(file->lock);
        return CZIM_ERROR_IO;
    }

    size_t n = fread(out_buf, 1, size, file->fp);

    MUTEX_UNLOCK(file->lock);
    return (n == size) ? CZIM_OK : CZIM_ERROR_IO;
}

int czim_file_read_byte(czim_file *file, czim_offset_t offset, uint8_t *out) {
    if (!file || !out) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    return czim_file_read(file, offset, 1, out);
}

int czim_file_read_u16(czim_file *file, czim_offset_t offset, uint16_t *out) {
    if (!file || !out) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    uint8_t buf[2];
    if (czim_file_read(file, offset, 2, buf) != CZIM_OK) {
        return CZIM_ERROR_IO;
    }

    *out = read_le16(buf);
    return CZIM_OK;
}

int czim_file_read_u32(czim_file *file, czim_offset_t offset, uint32_t *out) {
    if (!file || !out) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    uint8_t buf[4];
    if (czim_file_read(file, offset, 4, buf) != CZIM_OK) {
        return CZIM_ERROR_IO;
    }

    *out = read_le32(buf);
    return CZIM_OK;
}

int czim_file_read_u64(czim_file *file, czim_offset_t offset, uint64_t *out) {
    if (!file || !out) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    uint8_t buf[8];
    if (czim_file_read(file, offset, 8, buf) != CZIM_OK) {
        return CZIM_ERROR_IO;
    }

    *out = read_le64(buf);
    return CZIM_OK;
}

const char *czim_file_filename(const czim_file *file) {
    return file ? file->filename : NULL;
}
