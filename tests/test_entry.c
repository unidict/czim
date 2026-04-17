//
//  test_entry.c
//  czim tests
//
//  Entry parsing and access tests
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

// --- Tests ---

static void test_entry_get_by_index_first(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_NOT_NULL(czim_entry_get_path(e));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_get_by_index_last(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_entry_count(a);
    czim_entry *e = czim_archive_get_entry_by_index(a, count - 1);
    TEST_ASSERT_NOT_NULL(e);
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_get_by_index_out_of_range(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_entry_count(a);
    czim_entry *e = czim_archive_get_entry_by_index(a, count);
    TEST_ASSERT_NULL(e);
    czim_archive_close(a);
}

static void test_entry_namespace(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    char ns = czim_entry_get_namespace(e);
    TEST_ASSERT_TRUE(ns == 'C' || ns == 'M' || ns == 'W' || ns == 'X' ||
                     ns == '-' || ns == 'A' || ns == 'I' || ns == 'J');
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_path_not_null(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t count = czim_archive_entry_count(a);
    for (uint32_t i = 0; i < count && i < 10; i++) {
        czim_entry *e = czim_archive_get_entry_by_index(a, i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_NOT_NULL(czim_entry_get_path(e));
        czim_entry_free(e);
    }
    czim_archive_close(a);
}

static void test_entry_title_fallback(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    const char *title = czim_entry_get_title(e);
    TEST_ASSERT_NOT_NULL(title);
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_long_path(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_get_entry_by_index(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    const char *long_path = czim_entry_get_long_path(e);
    TEST_ASSERT_NOT_NULL(long_path);
    TEST_ASSERT_EQUAL_CHAR(czim_entry_get_namespace(e), long_path[0]);
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_find_by_path_content(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_entry_by_path(a, 'C', "African_Americans", &index);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_CHAR('C', czim_entry_get_namespace(e));
    TEST_ASSERT_EQUAL_STRING("African_Americans", czim_entry_get_path(e));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_find_by_path_not_found(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_entry_by_path(a, 'C', "NonExistentPage12345", &index);
    TEST_ASSERT_NULL(e);
    czim_archive_close(a);
}

static void test_entry_find_by_path_metadata(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_entry_by_path(a, 'M', "Counter", &index);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_CHAR('M', czim_entry_get_namespace(e));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_is_redirect(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    uint32_t index;
    czim_entry *e = czim_archive_find_entry_by_path(a, 'W', "mainPage", &index);
    if (e) {
        bool is_redir = czim_entry_is_redirect(e);
        if (is_redir) {
            TEST_ASSERT_TRUE(e->redirect_index < czim_archive_entry_count(a));
        }
        czim_entry_free(e);
    }
    czim_archive_close(a);
}

static void test_entry_is_article(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_find_entry_by_path(a, 'C', "African_Americans", NULL);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(czim_entry_is_article(e));
    czim_entry_free(e);
    czim_archive_close(a);
}

static void test_entry_content_has_cluster_blob(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    czim_entry *e = czim_archive_find_entry_by_path(a, 'C', "African_Americans", NULL);
    TEST_ASSERT_NOT_NULL(e);
    if (!czim_entry_is_redirect(e)) {
        TEST_ASSERT_TRUE(e->cluster_number < czim_archive_cluster_count(a));
    }
    czim_entry_free(e);
    czim_archive_close(a);
}

void run_entry_tests(void) {
    RUN_TEST(test_entry_get_by_index_first);
    RUN_TEST(test_entry_get_by_index_last);
    RUN_TEST(test_entry_get_by_index_out_of_range);
    RUN_TEST(test_entry_namespace);
    RUN_TEST(test_entry_path_not_null);
    RUN_TEST(test_entry_title_fallback);
    RUN_TEST(test_entry_long_path);
    RUN_TEST(test_entry_find_by_path_content);
    RUN_TEST(test_entry_find_by_path_not_found);
    RUN_TEST(test_entry_find_by_path_metadata);
    RUN_TEST(test_entry_is_redirect);
    RUN_TEST(test_entry_is_article);
    RUN_TEST(test_entry_content_has_cluster_blob);
}
