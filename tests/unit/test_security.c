/*
 * test_security.c - Unit tests for till_security.c
 * 
 * Tests input validation, sanitization, and secure operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../../src/till_security.h"
#include "../../src/till_config.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_START(name) do { \
    printf("Testing %s... ", name); \
    tests_run++; \
} while(0)

#define TEST_PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

#define ASSERT(condition, msg) do { \
    if (!(condition)) { \
        TEST_FAIL(msg); \
        return; \
    } \
} while(0)

/* Test path traversal detection */
void test_path_traversal() {
    TEST_START("has_path_traversal");
    
    /* Should detect .. sequences */
    ASSERT(has_path_traversal("../etc/passwd") == 1, "Should detect ../");
    ASSERT(has_path_traversal("foo/../bar") == 1, "Should detect /../");
    ASSERT(has_path_traversal("foo/..") == 1, "Should detect trailing ..");
    
    /* Should detect ~ expansion */
    ASSERT(has_path_traversal("~/secret") == 1, "Should detect ~/");
    ASSERT(has_path_traversal("~root/secret") == 1, "Should detect ~user/");
    
    /* Should allow normal paths */
    ASSERT(has_path_traversal("foo/bar") == 0, "Should allow normal path");
    ASSERT(has_path_traversal("./foo") == 0, "Should allow ./");
    ASSERT(has_path_traversal("/absolute/path") == 0, "Should allow absolute path");
    
    /* Edge cases */
    ASSERT(has_path_traversal(NULL) == 1, "Should reject NULL");
    ASSERT(has_path_traversal("") == 0, "Should allow empty string");
    
    TEST_PASS();
}

/* Test hostname validation */
void test_hostname_validation() {
    TEST_START("validate_hostname");
    
    /* Valid hostnames */
    ASSERT(validate_hostname("example.com") == 1, "Should allow example.com");
    ASSERT(validate_hostname("sub.example.com") == 1, "Should allow subdomains");
    ASSERT(validate_hostname("localhost") == 1, "Should allow localhost");
    ASSERT(validate_hostname("server-01") == 1, "Should allow hyphens");
    ASSERT(validate_hostname("server_01") == 1, "Should allow underscores");
    ASSERT(validate_hostname("192.168.1.1") == 1, "Should allow IP addresses");
    
    /* Invalid hostnames */
    ASSERT(validate_hostname("-server") == 0, "Should reject leading hyphen");
    ASSERT(validate_hostname("server-") == 0, "Should reject trailing hyphen");
    ASSERT(validate_hostname(".server") == 0, "Should reject leading dot");
    ASSERT(validate_hostname("server.") == 0, "Should reject trailing dot");
    ASSERT(validate_hostname("server..com") == 0, "Should reject double dots");
    ASSERT(validate_hostname("server@com") == 0, "Should reject special chars");
    ASSERT(validate_hostname("server com") == 0, "Should reject spaces");
    
    /* Edge cases */
    ASSERT(validate_hostname(NULL) == 0, "Should reject NULL");
    ASSERT(validate_hostname("") == 0, "Should reject empty string");
    
    /* Length limits */
    char long_hostname[300];
    memset(long_hostname, 'a', 299);
    long_hostname[299] = '\0';
    ASSERT(validate_hostname(long_hostname) == 0, "Should reject > 255 chars");
    
    TEST_PASS();
}

/* Test port validation */
void test_port_validation() {
    TEST_START("validate_port");
    
    /* Valid ports */
    ASSERT(validate_port(22) == 1, "Should allow port 22");
    ASSERT(validate_port(80) == 1, "Should allow port 80");
    ASSERT(validate_port(443) == 1, "Should allow port 443");
    ASSERT(validate_port(8080) == 1, "Should allow port 8080");
    ASSERT(validate_port(1) == 1, "Should allow port 1");
    ASSERT(validate_port(65535) == 1, "Should allow port 65535");
    
    /* Invalid ports */
    ASSERT(validate_port(0) == 0, "Should reject port 0");
    ASSERT(validate_port(-1) == 0, "Should reject negative port");
    ASSERT(validate_port(65536) == 0, "Should reject port > 65535");
    ASSERT(validate_port(100000) == 0, "Should reject very large port");
    
    TEST_PASS();
}

/* Test filename sanitization */
void test_filename_sanitization() {
    TEST_START("sanitize_filename");
    
    char buffer[256];
    
    /* Should remove dangerous characters */
    strncpy(buffer, "file;name.txt", sizeof(buffer));
    int result = sanitize_filename(buffer);
    ASSERT(result == 0, "Should sanitize successfully");
    ASSERT(strstr(buffer, ";") == NULL, "Should remove semicolon");
    
    strncpy(buffer, "file|name.txt", sizeof(buffer));
    sanitize_filename(buffer);
    ASSERT(strstr(buffer, "|") == NULL, "Should remove pipe");
    
    strncpy(buffer, "file&name.txt", sizeof(buffer));
    sanitize_filename(buffer);
    ASSERT(strstr(buffer, "&") == NULL, "Should remove ampersand");
    
    /* Should allow normal characters */
    strncpy(buffer, "file_name-123.txt", sizeof(buffer));
    char original[256];
    strncpy(original, buffer, sizeof(original));
    sanitize_filename(buffer);
    ASSERT(strcmp(buffer, original) == 0, "Should not change safe filename");
    
    /* Edge cases */
    buffer[0] = '\0';
    result = sanitize_filename(buffer);
    ASSERT(buffer[0] == '\0', "Should handle empty string");
    
    TEST_PASS();
}

/* Test safe string copy */
void test_safe_strncpy() {
    TEST_START("safe_strncpy");
    
    char dest[10];
    
    /* Normal copy */
    memset(dest, 'X', sizeof(dest));
    safe_strncpy(dest, "hello", sizeof(dest));
    ASSERT(strcmp(dest, "hello") == 0, "Should copy string");
    ASSERT(dest[9] == '\0', "Should null-terminate");
    
    /* Truncation */
    memset(dest, 'X', sizeof(dest));
    safe_strncpy(dest, "this is too long", sizeof(dest));
    ASSERT(strlen(dest) == 9, "Should truncate to size-1");
    ASSERT(dest[9] == '\0', "Should null-terminate after truncation");
    
    /* Empty source */
    memset(dest, 'X', sizeof(dest));
    safe_strncpy(dest, "", sizeof(dest));
    ASSERT(dest[0] == '\0', "Should handle empty string");
    
    /* NULL source */
    memset(dest, 'X', sizeof(dest));
    dest[0] = '\0';
    safe_strncpy(dest, NULL, sizeof(dest));
    ASSERT(dest[0] == '\0', "Should handle NULL source");
    
    TEST_PASS();
}

/* Test safe string concatenation */
void test_safe_strncat() {
    TEST_START("safe_strncat");
    
    char dest[20];
    
    /* Normal concatenation */
    strncpy(dest, "hello", sizeof(dest));
    safe_strncat(dest, " world", sizeof(dest));
    ASSERT(strcmp(dest, "hello world") == 0, "Should concatenate");
    
    /* Truncation */
    strncpy(dest, "hello", sizeof(dest));
    safe_strncat(dest, " this is too long to fit", sizeof(dest));
    ASSERT(strlen(dest) == 19, "Should truncate to size-1");
    ASSERT(dest[19] == '\0', "Should null-terminate");
    
    /* Empty source */
    strncpy(dest, "hello", sizeof(dest));
    safe_strncat(dest, "", sizeof(dest));
    ASSERT(strcmp(dest, "hello") == 0, "Should handle empty source");
    
    /* NULL source */
    strncpy(dest, "hello", sizeof(dest));
    safe_strncat(dest, NULL, sizeof(dest));
    ASSERT(strcmp(dest, "hello") == 0, "Should handle NULL source");
    
    TEST_PASS();
}

/* Test lock file operations */
void test_lock_file() {
    TEST_START("lock_file operations");
    
    const char *test_lock = "/tmp/test_till_lock.lock";
    
    /* Clean up any existing lock */
    unlink(test_lock);
    
    /* Should acquire lock with timeout */
    int fd = acquire_lock_file(test_lock, 1000);  /* 1 second timeout */
    ASSERT(fd >= 0, "Should acquire lock");
    
    /* Should not acquire already held lock */
    int fd2 = acquire_lock_file(test_lock, 100);  /* 100ms timeout */
    ASSERT(fd2 < 0, "Should not acquire held lock");
    
    /* Should release lock */
    release_lock_file(fd);
    
    /* Should be able to acquire after release */
    fd = acquire_lock_file(test_lock, 1000);
    ASSERT(fd >= 0, "Should acquire after release");
    release_lock_file(fd);
    
    /* Clean up */
    unlink(test_lock);
    
    TEST_PASS();
}

/* Test atomic file write */
void test_atomic_write() {
    TEST_START("write_file_atomic");
    
    const char *test_file = "/tmp/test_till_atomic.txt";
    const char *content = "test content\n";
    
    /* Clean up any existing file */
    unlink(test_file);
    
    /* Should write file atomically */
    int result = write_file_atomic(test_file, content, strlen(content));
    ASSERT(result == 0, "Should write successfully");
    
    /* Verify content */
    FILE *fp = fopen(test_file, "r");
    ASSERT(fp != NULL, "File should exist");
    
    char buffer[100];
    fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
    ASSERT(strcmp(buffer, content) == 0, "Content should match");
    
    /* Verify permissions */
    struct stat st;
    stat(test_file, &st);
    ASSERT((st.st_mode & 0777) == TILL_FILE_PERMS, "Should have correct permissions");
    
    /* Clean up */
    unlink(test_file);
    
    TEST_PASS();
}

/* Test safe directory creation */
void test_create_dir_safe() {
    TEST_START("create_dir_safe");
    
    const char *test_dir = "/tmp/test_till_safe_dir";
    
    /* Clean up any existing directory */
    rmdir(test_dir);
    
    /* Should create directory */
    int result = create_dir_safe(test_dir, TILL_DIR_PERMS);
    ASSERT(result == 0, "Should create directory");
    
    /* Verify it exists */
    struct stat st;
    ASSERT(stat(test_dir, &st) == 0, "Directory should exist");
    ASSERT(S_ISDIR(st.st_mode), "Should be a directory");
    
    /* Clean up */
    rmdir(test_dir);
    
    TEST_PASS();
}

/* Main test runner */
int main() {
    printf("\n=== Till Security Tests ===\n\n");
    
    /* Run all tests */
    test_path_traversal();
    test_hostname_validation();
    test_port_validation();
    test_filename_sanitization();
    test_safe_strncpy();
    test_safe_strncat();
    test_lock_file();
    test_atomic_write();
    test_create_dir_safe();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll tests passed!\n");
        return 0;
    } else {
        printf("\nSome tests failed.\n");
        return 1;
    }
}