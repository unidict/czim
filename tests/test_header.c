//
//  test_header.c
//  czim tests
//
//  Header parsing and validation tests
//

#include "unity.h"
#include "czim_archive.h"
#include "test_platform.h"
#include <string.h>

// Helper: get test ZIM file path using __FILE__
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

static void test_archive_open_valid(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    czim_archive_close(a);
}

static void test_archive_open_null_path(void) {
    czim_archive *a = czim_archive_open(NULL);
    TEST_ASSERT_NULL(a);
}

static void test_archive_open_invalid_path(void) {
    czim_archive *a = czim_archive_open("/nonexistent/path/test.zim");
    TEST_ASSERT_NULL(a);
}

static void test_header_magic(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_UINT32(CZIM_MAGIC, a->header.magic);
    czim_archive_close(a);
}

static void test_header_version(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->header.major_version == 5 ||
                     a->header.major_version == 6);
    czim_archive_close(a);
}

static void test_header_entry_count(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->header.entry_count > 0);
    czim_archive_close(a);
}

static void test_header_cluster_count(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->header.cluster_count > 0);
    czim_archive_close(a);
}

static void test_header_positions_valid(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->header.path_ptr_pos >= CZIM_HEADER_SIZE);
    TEST_ASSERT_TRUE(a->header.cluster_ptr_pos >= CZIM_HEADER_SIZE);
    TEST_ASSERT_TRUE(a->header.mime_list_pos >= CZIM_HEADER_SIZE);
    czim_archive_close(a);
}

static void test_header_new_namespace_scheme(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    bool has_new = czim_header_has_new_namespace_scheme(&a->header);
    TEST_ASSERT_TRUE(has_new);
    czim_archive_close(a);
}

static void test_header_has_title_index(void) {
    czim_archive *a = czim_archive_open(get_test_zim_path());
    TEST_ASSERT_NOT_NULL(a);
    bool has_title = czim_header_has_title_index(&a->header);
    TEST_ASSERT_TRUE(has_title);
    czim_archive_close(a);
}

void run_header_tests(void) {
    RUN_TEST(test_archive_open_valid);
    RUN_TEST(test_archive_open_null_path);
    RUN_TEST(test_archive_open_invalid_path);
    RUN_TEST(test_header_magic);
    RUN_TEST(test_header_version);
    RUN_TEST(test_header_entry_count);
    RUN_TEST(test_header_cluster_count);
    RUN_TEST(test_header_positions_valid);
    RUN_TEST(test_header_new_namespace_scheme);
    RUN_TEST(test_header_has_title_index);
}
