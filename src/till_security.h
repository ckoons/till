/*
 * till_security.h - Security utilities for Till
 * 
 * Provides functions for input validation, sanitization, and secure operations
 */

#ifndef TILL_SECURITY_H
#define TILL_SECURITY_H

#include <stddef.h>

/* Input validation */

/* Validate a file path (no traversal, within bounds) */
int validate_path(const char *path, const char *base_dir);

/* Check if path contains directory traversal attempts */
int has_path_traversal(const char *path);

/* Validate a hostname */
int validate_hostname(const char *hostname);

/* Validate a port number */
int validate_port(int port);

/* Validate an environment variable name */
int validate_env_name(const char *name);

/* Input sanitization */

/* Escape a string for safe shell usage */
char *shell_escape(const char *str);

/* Escape a string for safe shell usage (in-place with buffer) */
int shell_escape_buf(const char *str, char *buf, size_t buf_size);

/* Quote a string for shell command */
char *shell_quote(const char *str);

/* Quote a string for shell command (in-place with buffer) */
int shell_quote_buf(const char *str, char *buf, size_t buf_size);

/* Sanitize a filename (remove dangerous characters) */
int sanitize_filename(char *filename);

/* Command execution */

/* Execute command with argument array (safer than system()) */
int exec_command_argv(const char *cmd, char *const argv[]);

/* Execute command with sanitized arguments */
int exec_command_safe(const char *cmd, ...);

/* Build safe SSH command with arguments */
int build_ssh_command_safe(char *buf, size_t buf_size, 
                           const char *user, const char *host, int port,
                           int argc, char *argv[]);

/* File operations */

/* Safe file creation with proper permissions */
int create_file_safe(const char *path, mode_t mode);

/* Safe directory creation */
int create_dir_safe(const char *path, mode_t mode);

/* Atomic file write (write to temp, then rename) */
int write_file_atomic(const char *path, const char *content, size_t len);

/* Lock file operations */

/* Acquire lock file (with timeout) */
int acquire_lock_file(const char *path, int timeout_ms);

/* Release lock file */
int release_lock_file(int lock_fd);

/* Memory operations */

/* Safe string copy (always null-terminated) */
char *safe_strncpy(char *dest, const char *src, size_t n);

/* Safe string concatenation */
char *safe_strncat(char *dest, const char *src, size_t n);

/* Clear sensitive memory */
void secure_memzero(void *ptr, size_t len);

#endif /* TILL_SECURITY_H */