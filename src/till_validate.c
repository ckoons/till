/*
 * till_validate.c - Input validation utilities for Till
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "till_config.h"
#include "till_validate.h"
#include "till_common.h"

/* Validate string length */
bool validate_string_length(const char *str, size_t max_len) {
    if (!str) return false;
    return strlen(str) <= max_len;
}

/* Validate path - check for directory traversal and invalid characters */
bool validate_input_path(const char *path) {
    if (!path || strlen(path) == 0 || strlen(path) >= TILL_MAX_PATH) {
        return false;
    }
    
    /* Check for directory traversal */
    if (strstr(path, "../") || strstr(path, "..\\")) {
        till_log(LOG_WARN, "Path validation failed: directory traversal detected");
        return false;
    }
    
    /* Check for null bytes */
    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\0' && i < len - 1) {
            till_log(LOG_WARN, "Path validation failed: embedded null byte");
            return false;
        }
    }
    
    return true;
}

/* Validate hostname */
bool validate_input_hostname(const char *hostname) {
    if (!hostname || strlen(hostname) == 0 || strlen(hostname) > 255) {
        return false;
    }
    
    /* Check each character */
    for (const char *p = hostname; *p; p++) {
        if (!isalnum(*p) && *p != '.' && *p != '-' && *p != '_') {
            till_log(LOG_WARN, "Invalid hostname character: %c", *p);
            return false;
        }
    }
    
    /* Check for consecutive dots */
    if (strstr(hostname, "..")) {
        return false;
    }
    
    return true;
}

/* Validate port number */
bool validate_input_port(int port) {
    return port >= 1 && port <= 65535;
}

/* Validate and parse port string */
bool validate_port_string(const char *port_str, int *port) {
    if (!port_str || !port) {
        return false;
    }
    
    /* Check if all characters are digits */
    for (const char *p = port_str; *p; p++) {
        if (!isdigit(*p)) {
            return false;
        }
    }
    
    char *endptr;
    long val = strtol(port_str, &endptr, 10);
    
    if (*endptr != '\0' || val < 1 || val > 65535) {
        return false;
    }
    
    *port = (int)val;
    return true;
}

/* Safe string copy */
int till_safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    
    /* Check if truncation occurred */
    if (strlen(src) >= dest_size) {
        till_log(LOG_WARN, "String truncated during copy");
        return 1;  /* Truncation occurred */
    }
    
    return 0;
}

/* Safe string concatenation */
int till_safe_strncat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return -1;  /* No space to append */
    }
    
    size_t space_left = dest_size - dest_len - 1;
    strncat(dest, src, space_left);
    
    /* Check if truncation occurred */
    if (strlen(src) > space_left) {
        till_log(LOG_WARN, "String truncated during concatenation");
        return 1;  /* Truncation occurred */
    }
    
    return 0;
}

/* Safe formatted print */
int till_safe_snprintf(char *dest, size_t dest_size, const char *format, ...) {
    if (!dest || !format || dest_size == 0) {
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, dest_size, format, args);
    va_end(args);
    
    if (result < 0) {
        return -1;  /* Encoding error */
    }
    
    if ((size_t)result >= dest_size) {
        till_log(LOG_WARN, "String truncated during formatting");
        return 1;  /* Truncation occurred */
    }
    
    return 0;
}

/* Sanitize string - remove dangerous characters */
void sanitize_string(char *str) {
    if (!str) return;
    
    char *read = str;
    char *write = str;
    
    while (*read) {
        /* Remove control characters except newline and tab */
        if (iscntrl(*read) && *read != '\n' && *read != '\t') {
            read++;
            continue;
        }
        
        /* Remove shell metacharacters for safety */
        if (strchr(";|&<>`$", *read)) {
            read++;
            continue;
        }
        
        *write++ = *read++;
    }
    
    *write = '\0';
}

/* Sanitize path - remove dangerous elements */
void sanitize_path(char *path) {
    if (!path) return;
    
    /* First do basic string sanitization */
    sanitize_string(path);
    
    /* Remove any remaining directory traversal attempts */
    char *p;
    while ((p = strstr(path, "../"))) {
        memmove(p, p + 3, strlen(p + 3) + 1);
    }
    
    while ((p = strstr(path, "..\\"))) {
        memmove(p, p + 3, strlen(p + 3) + 1);
    }
}

/* Trim newline from end of string */
void trim_newline(char *str) {
    if (!str) return;
    
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

/* Validate command for execution */
bool validate_command(const char *cmd) {
    if (!cmd || strlen(cmd) == 0 || strlen(cmd) > 4096) {
        return false;
    }
    
    /* Check for shell injection attempts */
    const char *dangerous[] = {
        "rm -rf /",
        ":(){ :|:& };:",  /* Fork bomb */
        "> /dev/sda",
        "dd if=/dev/zero",
        NULL
    };
    
    for (int i = 0; dangerous[i]; i++) {
        if (strstr(cmd, dangerous[i])) {
            till_log(LOG_ERROR, "Dangerous command pattern detected");
            return false;
        }
    }
    
    return true;
}

/* Validate SSH command */
bool validate_ssh_command(const char *cmd) {
    if (!validate_command(cmd)) {
        return false;
    }
    
    /* Additional SSH-specific checks */
    if (strstr(cmd, "StrictHostKeyChecking=no") && 
        strstr(cmd, "UserKnownHostsFile=/dev/null")) {
        till_log(LOG_WARN, "Insecure SSH options detected");
    }
    
    return true;
}