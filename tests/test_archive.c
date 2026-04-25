//
//  test_archive.c
//  czim tests
//
//  Archive integration tests
//

#include "unity.h"
#include "czim_archive.h"
#include "czim_archive_ext.h"
#include "test_platform.h"
#include <string.h>

static const char *get_test_zim_path(void) {
    static char path[1024];
    static int initialized = 0;
    if (!initialized) {
        const char *file = __FILE__;
        const char *last_sep = NULL;
        for (const char *p = file; *p; p++) {
            if (*p == '/' || *p == '\\') last_sep = p;
        }
        if (last_sep) {
            size_t dir_len = (size_t)(last_sep - file + 1);
            memcpy(path, file, dir_len);
            strcpy(path + dir_len, "data/wikipedia_en_100_mini_2026-01.zim");
        } else {
            strcpy(path, "data/wikipedia_en_100_mini_2026-01.zim");
        }
        initialized = 1;
    }
    return path;
}

// --- Properties ---

static void test_archive_uuid(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    const char *uuid = czim_archive_uuid(a);
    TEST_ASSERT_NOT_NULL(uuid);
    TEST_ASSERT_TRUE(strlen(uuid) > 0);
    czim_archive_close(a);
}

static void test_archive_entry_count(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_entry_count(a);
    TEST_ASSERT_TRUE(count > 0);
    czim_archive_close(a);
}

static void test_archive_article_count(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_article_count(a);
    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_TRUE(count <= czim_archive_entry_count(a));
    czim_archive_close(a);
}

static void test_archive_cluster_count(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_cluster_count(a);
    TEST_ASSERT_TRUE(count > 0);
    czim_archive_close(a);
}

static void test_archive_has_main_entry(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    bool has = czim_archive_has_main_entry(a);
    TEST_ASSERT_TRUE(has);
    czim_archive_close(a);
}

static void test_archive_has_title_index(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    bool has = czim_archive_has_title_index(a);
    TEST_ASSERT_TRUE(has);
    czim_archive_close(a);
}

static void test_archive_has_new_namespace_scheme(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    bool has = czim_archive_has_new_namespace_scheme(a);
    TEST_ASSERT_TRUE(has);
    czim_archive_close(a);
}

static void test_archive_mime_type(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    const char *mime = czim_archive_mime_type(a, 0);
    TEST_ASSERT_NOT_NULL(mime);
    czim_archive_close(a);
}

// --- Redirect resolution ---

static void test_redirect_resolve_non_redirect(void) {
    // Non-redirect entry should be returned as-is
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_find_entry_by_path(a, 'C', "African_Americans", NULL);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(czim_entry_is_redirect(e));

    czim_entry *resolved = czim_archive_resolve_redirect(a, e, 10);
    TEST_ASSERT_EQUAL_PTR(e, resolved);  // Same pointer, no redirect followed
    czim_entry_free(resolved);
    czim_archive_close(a);
}

static void test_redirect_resolve_single(void) {
    // Entry[0] is a redirect to C/Elvis_Presley (index 1494)
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(czim_entry_is_redirect(e));
    TEST_ASSERT_EQUAL_UINT32(1494, e->redirect_index);

    czim_entry *resolved = czim_archive_resolve_redirect(a, e, 10);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_FALSE(czim_entry_is_redirect(resolved));
    TEST_ASSERT_EQUAL_STRING("Elvis_Presley", czim_entry_get_path(resolved));
    czim_entry_free(resolved);
    czim_archive_close(a);
}

static void test_redirect_resolve_max_exceeded(void) {
    // With max_redirects=0, redirect should not be followed
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(czim_entry_is_redirect(e));

    czim_entry *resolved = czim_archive_resolve_redirect(a, e, 0);
    TEST_ASSERT_NULL(resolved);  // Max redirects exceeded immediately
    czim_archive_close(a);
}

static void test_redirect_resolve_null_archive(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);

    czim_entry *resolved = czim_archive_resolve_redirect(NULL, e, 10);
    TEST_ASSERT_NULL(resolved);

    // After resolve_redirect returns NULL, the input entry's refcount
    // was NOT consumed because the NULL check happens before any free.
    // The caller still owns the entry and must free it.
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_redirect_resolve_null_entry(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());

    czim_entry *resolved = czim_archive_resolve_redirect(a, NULL, 10);
    TEST_ASSERT_NULL(resolved);
    czim_archive_close(a);
}

static void test_redirect_resolve_then_read_blob(void) {
    // Resolve a redirect, then read the blob from the resolved entry
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(czim_entry_is_redirect(e));

    czim_entry *resolved = czim_archive_resolve_redirect(a, e, 10);
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_FALSE(czim_entry_is_redirect(resolved));

    // Read blob from resolved entry
    czim_blob blob = czim_archive_get_blob(a, resolved);
    TEST_ASSERT_NOT_NULL(blob.data);
    TEST_ASSERT_TRUE(blob.size > 0);
    czim_blob_free(&blob);

    czim_entry_free(resolved);
    czim_archive_close(a);
}

// --- Metadata ---

static void test_archive_find_metadata_counter(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_metadata(a, "Counter", &index);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_CHAR('M', czim_entry_get_namespace(e));

    if (!czim_entry_is_redirect(e)) {
        czim_blob blob = czim_archive_get_blob(a, e);
        TEST_ASSERT_NOT_NULL(blob.data);
        TEST_ASSERT_TRUE(blob.size > 0);
        czim_blob_free(&blob);
    }
    czim_entry_free(e);
    czim_archive_close(a);
}

// --- Blob access ---

static void test_archive_get_blob_content(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_find_entry_by_path(a, 'M', "Counter", NULL);
    TEST_ASSERT_NOT_NULL(e);

    if (!czim_entry_is_redirect(e)) {
        czim_blob blob = czim_archive_get_blob(a, e);
        TEST_ASSERT_NOT_NULL(blob.data);
        TEST_ASSERT_TRUE(blob.size > 0);
        czim_blob_free(&blob);
    }
    czim_entry_free(e);
    czim_archive_close(a);
}

// --- Prefix search ---

static void test_archive_path_prefix_search(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t start, end;
    int rc = czim_archive_find_entry_by_path_prefix(a, 'C', "African", &start, &end);
    TEST_ASSERT_EQUAL(CZIM_OK, rc);
    TEST_ASSERT_TRUE(end > start);

    czim_entry *e = czim_archive_get_entry_by_index(a, start);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_CHAR('C', czim_entry_get_namespace(e));
    const char *path = czim_entry_get_path(e);
    TEST_ASSERT_EQUAL_INT(0, strncmp(path, "African", 7));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_archive_path_prefix_no_match(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t start, end;
    int rc = czim_archive_find_entry_by_path_prefix(a, 'C', "ZZZZZnonexistent", &start, &end);
    TEST_ASSERT_EQUAL(CZIM_OK, rc);
    TEST_ASSERT_TRUE(end <= start);
    czim_archive_close(a);
}

// --- Extension API ---

static void test_archive_get_main_entry(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_get_main_entry(a, &index);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_NOT_NULL(czim_entry_get_path(e));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_archive_find_favicon(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_favicon(a, &index);
    if (e) {
        czim_entry_free(e);
    }
    czim_archive_close(a);
}

static void test_archive_illustration_info(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_illustration_infos infos = czim_archive_get_illustration_infos(a);
    czim_illustration_infos_free(&infos);
    czim_archive_close(a);
}

void run_archive_tests(void) {
    // Properties
    RUN_TEST(test_archive_uuid);
    RUN_TEST(test_archive_entry_count);
    RUN_TEST(test_archive_article_count);
    RUN_TEST(test_archive_cluster_count);
    RUN_TEST(test_archive_has_main_entry);
    RUN_TEST(test_archive_has_title_index);
    RUN_TEST(test_archive_has_new_namespace_scheme);
    RUN_TEST(test_archive_mime_type);

    // Redirect resolution
    RUN_TEST(test_redirect_resolve_non_redirect);
    RUN_TEST(test_redirect_resolve_single);
    RUN_TEST(test_redirect_resolve_max_exceeded);
    RUN_TEST(test_redirect_resolve_null_archive);
    RUN_TEST(test_redirect_resolve_null_entry);
    RUN_TEST(test_redirect_resolve_then_read_blob);

    // Metadata
    RUN_TEST(test_archive_find_metadata_counter);

    // Blob access
    RUN_TEST(test_archive_get_blob_content);

    // Prefix search
    RUN_TEST(test_archive_path_prefix_search);
    RUN_TEST(test_archive_path_prefix_no_match);

    // Extension API
    RUN_TEST(test_archive_get_main_entry);
    RUN_TEST(test_archive_find_favicon);
    RUN_TEST(test_archive_illustration_info);
}
