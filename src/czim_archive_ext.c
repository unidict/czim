//
//  czim_archive_ext.c
//  czim
//
//  Extension API implementations for czim_archive.
//
//  Created by kejinlu on 2026/4/24.
//

#include "czim_archive_ext.h"
#include "czim_entry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// Internal helper
// ============================================================

static char content_ns(const czim_archive *archive) {
    return czim_archive_has_new_namespace_scheme(archive) ? 'C' : 'A';
}

// ============================================================
// Convenience search (uses content namespace 'C' or 'A')
// ============================================================

czim_entry *czim_archive_find_content_entry_by_path(czim_archive *archive, const char *path, uint32_t *out_index) {
    return czim_archive_find_entry_by_path(archive, content_ns(archive), path, out_index);
}

czim_entry *czim_archive_find_content_entry_by_title(czim_archive *archive, const char *title, uint32_t *out_index) {
    return czim_archive_find_entry_by_title(archive, content_ns(archive), title, out_index);
}

// ============================================================
// Prefix search (convenience wrappers)
// ============================================================

int czim_archive_find_content_entry_by_path_prefix(czim_archive *archive, const char *prefix,
                                    uint32_t *start_index, uint32_t *end_index) {
    return czim_archive_find_entry_by_path_prefix(archive, content_ns(archive), prefix, start_index, end_index);
}

int czim_archive_find_content_entry_by_title_prefix(czim_archive *archive, const char *prefix,
                                      uint32_t *start_index, uint32_t *end_index) {
    return czim_archive_find_entry_by_title_prefix(archive, content_ns(archive), prefix, start_index, end_index);
}

// ============================================================
// Namespace utilities
// ============================================================

czim_entry *czim_archive_find_favicon(czim_archive *archive, uint32_t *out_index) {
    // Try namespaces in order: '-', 'I'
    static const char ns_list[] = {'-', 'I'};
    static const char *path_list[] = {"favicon", "favicon.png"};

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            czim_entry *d = czim_archive_find_entry_by_path(archive, ns_list[i], path_list[j], out_index);
            if (d) {
                return d;
            }
        }
    }
    return NULL;
}

// ============================================================
// Illustration
// ============================================================

static bool parse_illustration_path(const char *path, czim_illustration_info *info) {
    // Expected format: Illustration_<width>x<height>@<scale>
    // e.g. "Illustration_48x48@1"
    if (strncmp(path, "Illustration_", 13) != 0) {
        return false;
    }

    unsigned int width = 0, height = 0;
    float scale = 1.0f;
    char extra;

    // Try matching: <width>x<height>@<scale> with no trailing content
    if (sscanf(path + 13, "%ux%u@%f%c", &width, &height, &scale, &extra) == 3) {
        info->width = (uint32_t)width;
        info->height = (uint32_t)height;
        info->scale = scale;
        return true;
    }

    // Try matching without @scale (default scale=1.0)
    if (sscanf(path + 13, "%ux%u%c", &width, &height, &extra) == 2) {
        info->width = (uint32_t)width;
        info->height = (uint32_t)height;
        info->scale = 1.0f;
        return true;
    }

    return false;
}

czim_entry *czim_archive_get_illustration(czim_archive *archive, uint32_t size, uint32_t *out_index) {
    if (!archive) {
        return NULL;
    }

    // Build metadata name: "Illustration_<size>x<size>@1"
    char name[64];
    snprintf(name, sizeof(name), "Illustration_%ux%u@1", size, size);

    czim_entry *entry = czim_archive_find_entry_by_path(archive, 'M', name, out_index);
    if (entry) {
        return entry;
    }

    // Fallback for old format: use favicon for size=48
    if (size == 48) {
        return czim_archive_find_favicon(archive, out_index);
    }

    return NULL;
}

bool czim_archive_has_illustration(czim_archive *archive, uint32_t size) {
    if (!archive) {
        return false;
    }
    czim_entry *entry = czim_archive_get_illustration(archive, size, NULL);
    if (entry) {
        czim_entry_free(entry);
        return true;
    }
    return false;
}

czim_illustration_infos czim_archive_get_illustration_infos(czim_archive *archive) {
    czim_illustration_infos result = {NULL, 0};

    if (!archive) {
        return result;
    }

    uint32_t start, end;
    int rc = czim_archive_find_entry_by_path_prefix(archive, 'M', "Illustration_", &start, &end);
    if (rc == CZIM_OK && end > start) {
        uint32_t count = end - start;
        czim_illustration_info *items = calloc(count, sizeof(czim_illustration_info));
        if (!items) {
            return result;
        }

        uint32_t valid = 0;
        for (uint32_t i = 0; i < count; i++) {
            czim_entry *entry = czim_archive_get_entry_by_index(archive, start + i);
            if (!entry) continue;

            czim_illustration_info info;
            if (parse_illustration_path(czim_entry_get_path(entry), &info)) {
                items[valid++] = info;
            }
            czim_entry_free(entry);
        }

        if (valid > 0) {
            result.items = items;
            result.count = valid;
        } else {
            free(items);
        }
    }

    // If no Illustration entries found but favicon exists (old format), add default
    if (result.count == 0) {
        czim_entry *favicon = czim_archive_find_favicon(archive, NULL);
        if (favicon) {
            czim_entry_free(favicon);
            czim_illustration_info *items = malloc(sizeof(czim_illustration_info));
            if (items) {
                items[0].width = 48;
                items[0].height = 48;
                items[0].scale = 1.0f;
                result.items = items;
                result.count = 1;
            }
        }
    }

    return result;
}

void czim_illustration_infos_free(czim_illustration_infos *infos) {
    if (infos) {
        free(infos->items);
        infos->items = NULL;
        infos->count = 0;
    }
}

// ============================================================
// Main entry
// ============================================================

czim_entry *czim_archive_get_main_entry(czim_archive *archive, uint32_t *out_index) {
    if (!archive) {
        return NULL;
    }

    // First try the W namespace (modern ZIM files store mainPage redirect there)
    czim_entry *d = czim_archive_find_entry_by_path(archive, 'W', "mainPage", out_index);
    if (d) {
        d = czim_archive_resolve_redirect(archive, d, 10);
        if (d && out_index) {
            // Resolve redirect doesn't update out_index, find the actual index
        }
        return d;
    }

    // Fallback: use header's main_page index
    if (czim_archive_has_main_entry(archive)) {
        uint32_t main_idx = archive->header.main_page;
        d = czim_archive_get_entry_by_index(archive, main_idx);
        if (d) {
            if (out_index) {
                *out_index = main_idx;
            }
            d = czim_archive_resolve_redirect(archive, d, 10);
        }
        return d;
    }

    return NULL;
}
