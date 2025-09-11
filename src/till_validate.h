/*
 * till_validate.h - Input validation utilities for Till
 */

#ifndef TILL_VALIDATE_H
#define TILL_VALIDATE_H

#include <stdbool.h>
#include <stddef.h>

/* String validation */
bool validate_string_length(const char *str, size_t max_len);
bool validate_input_path(const char *path);
bool validate_input_hostname(const char *hostname);
bool validate_input_port(int port);
bool validate_port_string(const char *port_str, int *port);

/* Safe string operations */
int till_safe_strncpy(char *dest, const char *src, size_t dest_size);
int till_safe_strncat(char *dest, const char *src, size_t dest_size);
int till_safe_snprintf(char *dest, size_t dest_size, const char *format, ...);

/* Input sanitization */
void sanitize_string(char *str);
void sanitize_path(char *path);
void trim_newline(char *str);

/* Command validation */
bool validate_command(const char *cmd);
bool validate_ssh_command(const char *cmd);

#endif /* TILL_VALIDATE_H */