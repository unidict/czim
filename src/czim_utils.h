//
//  czim_utils.h
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#ifndef czim_utils_h
#define czim_utils_h

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Read little-endian integers from buffer (memcpy-based, handles unaligned access)
// ============================================================

static inline uint16_t read_le16(const void *ptr) {
    uint16_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline uint32_t read_le32(const void *ptr) {
    uint32_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline uint64_t read_le64(const void *ptr) {
    uint64_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

#ifdef __cplusplus
}
#endif

#endif /* czim_utils_h */
