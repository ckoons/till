/*
 * till_security.c - Security utilities implementation
 * 
 * Implements input validation, sanitization, and secure operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include "till_security.h"
#include "till_common.h"
#include "till_config.h"

/* Validate a file path (no traversal, within bounds) */
int validate_path(const char *path, const char *base_dir) {
    if (!path) return 0;
    
    /* Check for path traversal */
    if (has_path_traversal(path)) {
        return 0;
    }
    
    /* If base_dir specified, ensure path is within it */
    if (base_dir) {
        char resolved_path[TILL_MAX_PATH];
        char resolved_base[TILL_MAX_PATH];
        
        if (!realpath(path, resolved_path) || !realpath(base_dir, resolved_base)) {
            return 0;
        }
        
        /* Check if resolved_path starts with resolved_base */
        size_t base_len = strlen(resolved_base);
        if (strncmp(resolved_path, resolved_base, base_len) != 0) {
            return 0;
        }
        
        /* Ensure it's a subdirectory (not just a prefix match) */
        if (resolved_path[base_len] != '\0' && resolved_path[base_len] != '/') {
            return 0;
        }
    }
    
    return 1;
}

/* Check if path contains directory traversal attempts */
int has_path_traversal(const char *path) {
    if (!path) return 1;
    
    /* Check for .. sequences */
    if (strstr(path, "..")) {
        return 1;
    }
    
    /* Check for absolute paths when not expected */
    if (path[0] == '/') {
        /* This might be valid, but flag it for caller to decide */
        return 0;  /* Allow absolute paths but validate them separately */
    }
    
    /* Check for ~/ expansion attempts */
    if (path[0] == '~') {
        return 1;
    }
    
    return 0;
}

/* Validate a hostname */
int validate_hostname(const char *hostname) {
    if (!hostname || strlen(hostname) == 0 || strlen(hostname) > 255) {
        return 0;
    }
    
    /* Check each character */
    for (const char *p = hostname; *p; p++) {
        if (!isalnum(*p) && *p != '.' && *p != '-' && *p != '_') {
            return 0;
        }
    }
    
    /* Can't start or end with . or - */
    if (hostname[0] == '.' || hostname[0] == '-' ||
        hostname[strlen(hostname)-1] == '.' || hostname[strlen(hostname)-1] == '-') {
        return 0;
    }
    
    /* Check for double dots */
    if (strstr(hostname, "..") != NULL) {
        return 0;
    }
    
    return 1;
}

/* Validate a port number */
int validate_port(int port) {
    return port > 0 && port <= 65535;
}

/* Validate an environment variable name */
int validate_env_name(const char *name) {
    if (!name || strlen(name) == 0) {
        return 0;
    }
    
    /* Must start with letter or underscore */
    if (!isalpha(name[0]) && name[0] != '_') {
        return 0;
    }
    
    /* Rest must be alphanumeric or underscore */
    for (const char *p = name + 1; *p; p++) {
        if (!isalnum(*p) && *p != '_') {
            return 0;
        }
    }
    
    return 1;
}

/* Escape a string for safe shell usage */
char *shell_escape(const char *str) {
    if (!str) return NULL;
    
    /* Count characters that need escaping */
    size_t len = 0;
    for (const char *p = str; *p; p++) {
        if (strchr("\"'\\$`\n", *p)) {
            len += 2;  /* Need backslash */
        } else {
            len++;
        }
    }
    
    char *escaped = malloc(len + 1);
    if (!escaped) return NULL;
    
    char *out = escaped;
    for (const char *p = str; *p; p++) {
        if (strchr("\"'\\$`\n", *p)) {
            *out++ = '\\';
        }
        *out++ = *p;
    }
    *out = '\0';
    
    return escaped;
}

/* Escape a string for safe shell usage (in-place with buffer) */
int shell_escape_buf(const char *str, char *buf, size_t buf_size) {
    if (!str || !buf || buf_size == 0) return -1;
    
    size_t out_pos = 0;
    for (const char *p = str; *p; p++) {
        if (strchr("\"'\\$`\n", *p)) {
            if (out_pos + 2 >= buf_size) {
                return -1;  /* Buffer too small */
            }
            buf[out_pos++] = '\\';
        } else if (out_pos + 1 >= buf_size) {
            return -1;  /* Buffer too small */
        }
        buf[out_pos++] = *p;
    }
    
    if (out_pos >= buf_size) {
        return -1;
    }
    buf[out_pos] = '\0';
    
    return 0;
}

/* Quote a string for shell command */
char *shell_quote(const char *str) {
    if (!str) return NULL;
    
    /* Use single quotes and escape any single quotes in the string */
    size_t len = strlen(str);
    size_t quote_count = 0;
    
    for (const char *p = str; *p; p++) {
        if (*p == '\'') quote_count++;
    }
    
    /* Need: 2 quotes + original length + (3 chars per quote: '\'' ) */
    size_t needed = 2 + len + (quote_count * 3);
    char *quoted = malloc(needed + 1);
    if (!quoted) return NULL;
    
    char *out = quoted;
    *out++ = '\'';
    
    for (const char *p = str; *p; p++) {
        if (*p == '\'') {
            /* Close quote, escape quote, reopen quote */
            *out++ = '\'';
            *out++ = '\\';
            *out++ = '\'';
            *out++ = '\'';
        } else {
            *out++ = *p;
        }
    }
    
    *out++ = '\'';
    *out = '\0';
    
    return quoted;
}

/* Quote a string for shell command (in-place with buffer) */
int shell_quote_buf(const char *str, char *buf, size_t buf_size) {
    if (!str || !buf || buf_size < 3) return -1;  /* Need at least 3 chars for '' */
    
    size_t out_pos = 0;
    buf[out_pos++] = '\'';
    
    for (const char *p = str; *p; p++) {
        if (*p == '\'') {
            if (out_pos + 4 >= buf_size) {
                return -1;  /* Buffer too small */
            }
            buf[out_pos++] = '\'';
            buf[out_pos++] = '\\';
            buf[out_pos++] = '\'';
            buf[out_pos++] = '\'';
        } else {
            if (out_pos + 1 >= buf_size - 1) {
                return -1;  /* Buffer too small (need room for closing quote) */
            }
            buf[out_pos++] = *p;
        }
    }
    
    if (out_pos + 1 >= buf_size) {
        return -1;
    }
    buf[out_pos++] = '\'';
    buf[out_pos] = '\0';
    
    return 0;
}

/* Sanitize a filename (remove dangerous characters) */
int sanitize_filename(char *filename) {
    if (!filename) return -1;
    
    /* Handle empty string */
    if (strlen(filename) == 0) {
        strcpy(filename, "unnamed");
        return 0;
    }
    
    char *out = filename;
    for (char *in = filename; *in; in++) {
        if (isalnum(*in) || strchr("-_.", *in)) {
            *out++ = *in;
        } else if (*in == ' ') {
            *out++ = '_';
        }
        /* Skip other characters */
    }
    *out = '\0';
    
    /* Don't allow empty result */
    if (strlen(filename) == 0) {
        strcpy(filename, "unnamed");
    }
    
    return 0;
}

/* Build safe SSH command with arguments */
int build_ssh_command_safe(char *buf, size_t buf_size, 
                           const char *user, const char *host, int port,
                           int argc, char *argv[]) {
    if (!buf || buf_size == 0 || !user || !host) {
        return -1;
    }
    
    /* Validate inputs */
    if (!validate_hostname(host) || !validate_port(port)) {
        return -1;
    }
    
    /* Validate username (alphanumeric, underscore, dash) */
    for (const char *p = user; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            return -1;
        }
    }
    
    /* Build base command */
    int len = snprintf(buf, buf_size, "ssh %s@%s -p %d", user, host, port);
    if (len < 0 || (size_t)len >= buf_size) {
        return -1;
    }
    
    /* Add additional arguments (properly quoted) */
    for (int i = 0; i < argc; i++) {
        char quoted[TILL_LARGE_BUFFER];
        if (shell_quote_buf(argv[i], quoted, sizeof(quoted)) != 0) {
            return -1;
        }
        
        int arg_len = snprintf(buf + len, buf_size - len, " %s", quoted);
        if (arg_len < 0 || len + arg_len >= (int)buf_size) {
            return -1;
        }
        len += arg_len;
    }
    
    return 0;
}

/* Safe file creation with proper permissions */
int create_file_safe(const char *path, mode_t mode) {
    if (!path) return -1;
    
    /* Use O_EXCL to prevent symlink attacks */
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (fd < 0) {
        return -1;
    }
    
    close(fd);
    return 0;
}

/* Safe directory creation */
int create_dir_safe(const char *path, mode_t mode) {
    if (!path) return -1;
    
    /* Check if it exists first */
    struct stat st;
    if (stat(path, &st) == 0) {
        /* Verify it's a directory */
        if (!S_ISDIR(st.st_mode)) {
            errno = ENOTDIR;
            return -1;
        }
        return 0;  /* Already exists */
    }
    
    return mkdir(path, mode);
}

/* Atomic file write (write to temp, then rename) */
int write_file_atomic(const char *path, const char *content, size_t len) {
    if (!path || !content) return -1;
    
    /* Create temp file in same directory */
    char temp_path[TILL_MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp.%d", path, getpid());
    
    int fd = open(temp_path, O_CREAT | O_EXCL | O_WRONLY, TILL_FILE_PERMS);
    if (fd < 0) {
        return -1;
    }
    
    /* Write content */
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, content + written, len - written);
        if (n < 0) {
            close(fd);
            unlink(temp_path);
            return -1;
        }
        written += n;
    }
    
    /* Sync to disk */
    if (fsync(fd) != 0) {
        close(fd);
        unlink(temp_path);
        return -1;
    }
    
    close(fd);
    
    /* Atomic rename */
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        return -1;
    }
    
    return 0;
}

/* Acquire lock file (with timeout) */
int acquire_lock_file(const char *path, int timeout_ms) {
    if (!path) return -1;
    
    int fd = open(path, O_CREAT | O_RDWR, TILL_FILE_PERMS);
    if (fd < 0) {
        return -1;
    }
    
    /* Try to acquire lock with timeout */
    int elapsed = 0;
    int interval = 100;  /* Check every 100ms */
    
    while (elapsed < timeout_ms) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            /* Got the lock */
            return fd;
        }
        
        if (errno != EWOULDBLOCK) {
            /* Unexpected error */
            close(fd);
            return -1;
        }
        
        usleep(interval * 1000);
        elapsed += interval;
    }
    
    /* Timeout */
    close(fd);
    errno = ETIMEDOUT;
    return -1;
}

/* Release lock file */
int release_lock_file(int lock_fd) {
    if (lock_fd < 0) return -1;
    
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    return 0;
}

/* Safe string copy (always null-terminated) */
char *safe_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
    
    return dest;
}

/* Safe string concatenation */
char *safe_strncat(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    
    size_t dest_len = strlen(dest);
    if (dest_len >= n - 1) {
        return dest;  /* Already full */
    }
    
    size_t remaining = n - dest_len - 1;
    strncat(dest, src, remaining);
    dest[n - 1] = '\0';
    
    return dest;
}

/* Clear sensitive memory */
void secure_memzero(void *ptr, size_t len) {
    if (!ptr || len == 0) return;
    
    volatile unsigned char *p = ptr;
    while (len--) {
        *p++ = 0;
    }
}