//
//  czim_header.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_header.h"
#include "czim_utils.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// Header parsing
// ============================================================

int czim_header_read(czim_file *file, czim_header *header) {
    if (!file || !header) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    // Read 80-byte header
    uint8_t buf[CZIM_HEADER_SIZE];
    if (czim_file_read(file, 0, CZIM_HEADER_SIZE, buf) != CZIM_OK) {
        return CZIM_ERROR_IO;
    }

    // Parse fields
    size_t offset = 0;

    // Magic number (4 bytes)
    header->magic = read_le32(buf + offset);
    offset += 4;

    // Major version (2 bytes)
    header->major_version = read_le16(buf + offset);
    offset += 2;

    // Minor version (2 bytes)
    header->minor_version = read_le16(buf + offset);
    offset += 2;

    // UUID (16 bytes)
    memcpy(header->uuid.bytes, buf + offset, 16);
    offset += 16;

    // Entry count (4 bytes)
    header->entry_count = read_le32(buf + offset);
    offset += 4;

    // Cluster count (4 bytes)
    header->cluster_count = read_le32(buf + offset);
    offset += 4;

    // Path pointer list position (8 bytes)
    header->path_ptr_pos = read_le64(buf + offset);
    offset += 8;

    // Title pointer list position (8 bytes)
    header->title_ptr_pos = read_le64(buf + offset);
    offset += 8;

    // Cluster pointer list position (8 bytes)
    header->cluster_ptr_pos = read_le64(buf + offset);
    offset += 8;

    // MIME list position (8 bytes)
    header->mime_list_pos = read_le64(buf + offset);
    offset += 8;

    // Main page (4 bytes)
    header->main_page = read_le32(buf + offset);
    offset += 4;

    // Layout page (4 bytes)
    header->layout_page = read_le32(buf + offset);
    offset += 4;

    // Checksum position (8 bytes)
    header->checksum_pos = read_le64(buf + offset);
    offset += 8;

    return CZIM_OK;
}

int czim_header_validate(const czim_header *header) {
    if (!header) {
        return CZIM_ERROR_INVALID_PARAM;
    }

    // Validate magic number
    if (header->magic != CZIM_MAGIC) {
        fprintf(stderr, "Invalid ZIM magic: 0x%08X (expected 0x%08X)\n",
                header->magic, CZIM_MAGIC);
        return CZIM_ERROR_FORMAT;
    }

    // Validate version (supported: 5.x, 6.x)
    if (header->major_version < 5 || header->major_version > 6) {
        fprintf(stderr, "Unsupported ZIM version: %u.%u\n",
                header->major_version, header->minor_version);
        return CZIM_ERROR_UNSUPPORTED;
    }

    // Validate basic fields
    if (header->entry_count == 0) {
        fprintf(stderr, "Invalid entry count: 0\n");
        return CZIM_ERROR_FORMAT;
    }

    if (header->path_ptr_pos < CZIM_HEADER_SIZE) {
        fprintf(stderr, "Invalid path pointer position: %llu\n",
                (unsigned long long)header->path_ptr_pos);
        return CZIM_ERROR_FORMAT;
    }

    if (header->cluster_ptr_pos < CZIM_HEADER_SIZE) {
        fprintf(stderr, "Invalid cluster pointer position: %llu\n",
                (unsigned long long)header->cluster_ptr_pos);
        return CZIM_ERROR_FORMAT;
    }

    if (header->mime_list_pos < CZIM_HEADER_SIZE) {
        fprintf(stderr, "Invalid MIME list position: %llu\n",
                (unsigned long long)header->mime_list_pos);
        return CZIM_ERROR_FORMAT;
    }

    return CZIM_OK;
}

bool czim_header_has_new_namespace_scheme(const czim_header *header) {
    return header && header->minor_version >= 1;
}

bool czim_header_has_checksum(const czim_header *header) {
    // If mime_list_pos >= 80, the header includes checksum_pos field
    return header && header->mime_list_pos >= 80;
}

bool czim_header_has_main_page(const czim_header *header) {
    return header && header->main_page != CZIM_INVALID_INDEX;
}

bool czim_header_has_title_index(const czim_header *header) {
    return header && header->title_ptr_pos != 0;
}
