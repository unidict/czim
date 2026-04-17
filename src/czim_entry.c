//
//  czim_entry.c
//  czim
//
//  Created by kejinlu on 2026/4/22.
//

#include "czim_entry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// uobject type definition
// ============================================================

static void czim_entry_release_fn(uobject *obj) {
    czim_entry *d = uobject_cast(obj, czim_entry, obj);
    if (!d) return;

    free(d->path);
    free(d->title);
    free(d->parameter);
    free(d);
}

static const uobject_type czim_entry_type = {
    .name = "czim_entry",
    .size = sizeof(czim_entry),
    .release = czim_entry_release_fn,
};

// ============================================================
// Helper functions
// ============================================================

// Read a null-terminated string
static char *read_null_terminated_string(czim_file *reader, czim_offset_t *offset) {
    size_t capacity = 256;  // Initial capacity
    size_t len = 0;
    char *str = malloc(capacity);
    if (!str) {
        return NULL;
    }

    while (1) {
        if (len >= capacity) {
            // Expand buffer
            capacity *= 2;
            char *new_str = realloc(str, capacity);
            if (!new_str) {
                free(str);
                return NULL;
            }
            str = new_str;
        }

        uint8_t ch;
        if (czim_file_read_byte(reader, (*offset)++, &ch) != CZIM_OK) {
            free(str);
            return NULL;
        }

        str[len++] = (char)ch;

        if (ch == 0) {
            break;  // End of string
        }
    }

    return str;
}

// ============================================================
// Entry parsing
// ============================================================

czim_entry *czim_entry_read(czim_file *reader, czim_offset_t offset) {
    if (!reader) {
        return NULL;
    }

    czim_entry *entry = calloc(1, sizeof(czim_entry));
    if (!entry) {
        return NULL;
    }

    // Initialize uobject (refcount=1)
    uobject_init(&entry->obj, &czim_entry_type, NULL);

    czim_offset_t current = offset;

    // Read MIME type (2 bytes)
    if (czim_file_read_u16(reader, current, &entry->mime_type) != CZIM_OK) {
        goto error;
    }
    current += 2;

    // Read parameter length (1 byte)
    uint8_t parameter_len;
    if (czim_file_read_byte(reader, current, &parameter_len) != CZIM_OK) {
        goto error;
    }
    current += 1;

    // Read namespace (1 byte)
    uint8_t ns_byte;
    if (czim_file_read_byte(reader, current, &ns_byte) != CZIM_OK) {
        goto error;
    }
    entry->ns = (char)ns_byte;
    current += 1;

    // Read revision (4 bytes)
    if (czim_file_read_u32(reader, current, &entry->revision) != CZIM_OK) {
        goto error;
    }
    current += 4;

    // Read different fields based on MIME type
    if (entry->mime_type == CZIM_MIME_REDIRECT) {
        // Redirect entry: read redirect index (4 bytes)
        if (czim_file_read_u32(reader, current, &entry->redirect_index) != CZIM_OK) {
            goto error;
        }
        current += 4;
    } else {
        // Regular entry: read cluster and blob indices
        if (czim_file_read_u32(reader, current, &entry->cluster_number) != CZIM_OK) {
            goto error;
        }
        current += 4;

        if (czim_file_read_u32(reader, current, &entry->blob_number) != CZIM_OK) {
            goto error;
        }
        current += 4;
    }

    // Read path (null-terminated string)
    entry->path = read_null_terminated_string(reader, &current);
    if (!entry->path) {
        goto error;
    }

    // Read title (null-terminated string)
    entry->title = read_null_terminated_string(reader, &current);
    if (!entry->title) {
        goto error;
    }

    // Read parameters (if any)
    if (parameter_len > 0) {
        char *param_buf = malloc(parameter_len + 1);
        if (!param_buf) {
            goto error;
        }
        if (czim_file_read(reader, current, parameter_len, param_buf) != CZIM_OK) {
            free(param_buf);
            goto error;
        }
        param_buf[parameter_len] = '\0';
        entry->parameter = param_buf;
    }

    return entry;

error:
    czim_entry_free(entry);
    return NULL;
}

void czim_entry_free(czim_entry *entry) {
    if (entry) {
        uobject_release(&entry->obj);
    }
}

bool czim_entry_is_redirect(const czim_entry *entry) {
    return entry && entry->mime_type == CZIM_MIME_REDIRECT;
}

bool czim_entry_is_article(const czim_entry *entry) {
    return entry && !czim_entry_is_redirect(entry) &&
           entry->mime_type != CZIM_MIME_LINKTARGET &&
           entry->mime_type != CZIM_MIME_DELETED;
}

bool czim_entry_is_deleted(const czim_entry *entry) {
    return entry && entry->mime_type == CZIM_MIME_DELETED;
}

const char *czim_entry_get_title(const czim_entry *entry) {
    if (!entry) {
        return NULL;
    }
    return (entry->title && entry->title[0]) ? entry->title : entry->path;
}

const char *czim_entry_get_path(const czim_entry *entry) {
    return entry ? entry->path : NULL;
}

char czim_entry_get_namespace(const czim_entry *entry) {
    return entry ? entry->ns : '\0';
}

const char *czim_entry_get_long_path(const czim_entry *entry) {
    static char buf[512];
    if (!entry || !entry->path) {
        return NULL;
    }
    size_t path_len = strlen(entry->path);
    if (path_len + 3 > sizeof(buf)) {
        return entry->path;  // Fallback: return short path if too long
    }
    buf[0] = entry->ns;
    buf[1] = '/';
    memcpy(buf + 2, entry->path, path_len + 1);
    return buf;
}
