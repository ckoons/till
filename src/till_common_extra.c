/*
 * till_common_extra.c - Additional common utilities for Till
 * 
 * Extended utilities for error reporting, path operations, and more
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <limits.h>

#include "till_config.h"
#include "till_common.h"
#include "till_platform.h"
#include "cJSON.h"

#ifndef TILL_MAX_PATH
#define TILL_MAX_PATH 4096
#endif

/* External log function from till_common.c */
extern void till_log(int level, const char *format, ...);

/* Error reporting with combined stderr + log */
void till_error(const char *fmt, ...) {
    va_list args;
    char msg[2048];
    
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    fprintf(stderr, "Error: %s\n", msg);
    till_log(LOG_ERROR, "%s", msg);
}

void till_warn(const char *fmt, ...) {
    va_list args;
    char msg[2048];
    
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    fprintf(stderr, "Warning: %s\n", msg);
    till_log(LOG_WARN, "%s", msg);
}

void till_info(const char *fmt, ...) {
    va_list args;
    char msg[2048];
    
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    printf("%s\n", msg);
    till_log(LOG_INFO, "%s", msg);
}

void till_debug(const char *fmt, ...) {
    va_list args;
    char msg[2048];
    
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    till_log(LOG_DEBUG, "%s", msg);
    
    /* Only print to console if DEBUG is defined */
#ifdef DEBUG
    printf("[DEBUG] %s\n", msg);
#endif
}

int till_error_return(int code, const char *fmt, ...) {
    va_list args;
    char msg[2048];
    
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    fprintf(stderr, "Error: %s\n", msg);
    till_log(LOG_ERROR, "%s", msg);
    
    return code;
}

/* Path and file utilities */
int path_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

int is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int is_file(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

int is_executable(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && (st.st_mode & S_IXUSR));
}

int is_symlink(const char *path) {
    struct stat st;
    return (lstat(path, &st) == 0 && S_ISLNK(st.st_mode));
}

int path_join(char *dest, size_t size, const char *path1, const char *path2) {
    if (!dest || size == 0) return -1;
    
    /* Handle empty paths */
    if (!path1 || path1[0] == '\0') {
        strncpy(dest, path2 ? path2 : "", size - 1);
        dest[size - 1] = '\0';
        return 0;
    }
    if (!path2 || path2[0] == '\0') {
        strncpy(dest, path1, size - 1);
        dest[size - 1] = '\0';
        return 0;
    }
    
    /* Check if path1 ends with separator */
    size_t len1 = strlen(path1);
    int has_sep = (len1 > 0 && path1[len1 - 1] == '/');
    
    /* Check if path2 starts with separator */
    int path2_has_sep = (path2[0] == '/');
    
    /* Build the joined path */
    if (has_sep && path2_has_sep) {
        /* Both have separator, skip one */
        snprintf(dest, size, "%s%s", path1, path2 + 1);
    } else if (!has_sep && !path2_has_sep) {
        /* Neither has separator, add one */
        snprintf(dest, size, "%s/%s", path1, path2);
    } else {
        /* One has separator, just concatenate */
        snprintf(dest, size, "%s%s", path1, path2);
    }
    
    return 0;
}

int path_make_absolute(char *dest, size_t size, const char *path) {
    if (!dest || size == 0 || !path) return -1;
    
    /* Already absolute */
    if (path[0] == '/') {
        strncpy(dest, path, size - 1);
        dest[size - 1] = '\0';
        return 0;
    }
    
    /* Try realpath first */
    char resolved[TILL_MAX_PATH];
    if (realpath(path, resolved) != NULL) {
        strncpy(dest, resolved, size - 1);
        dest[size - 1] = '\0';
        return 0;
    }
    
    /* If realpath fails, construct manually */
    char cwd[TILL_MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return -1;
    }
    
    return path_join(dest, size, cwd, path);
}

/* Command execution utilities */
int run_command_logged(const char *fmt, ...) {
    va_list args;
    char cmd[2048];
    
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);
    
    till_log(LOG_INFO, "Executing: %s", cmd);
    
    int result = system(cmd);
    
    if (result == 0) {
        till_log(LOG_DEBUG, "Command succeeded: %s", cmd);
    } else {
        till_log(LOG_ERROR, "Command failed (%d): %s", result, cmd);
    }
    
    return result;
}

int run_command_capture(char *output, size_t size, const char *fmt, ...) {
    va_list args;
    char cmd[2048];
    
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);
    
    till_log(LOG_DEBUG, "Executing with capture: %s", cmd);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        till_log(LOG_ERROR, "Failed to execute: %s", cmd);
        return -1;
    }
    
    size_t total = 0;
    char buffer[1024];
    
    if (output && size > 0) {
        output[0] = '\0';
        
        while (fgets(buffer, sizeof(buffer), fp)) {
            size_t len = strlen(buffer);
            if (total + len < size - 1) {
                strcat(output, buffer);
                total += len;
            } else {
                break;
            }
        }
    }
    
    int status = pclose(fp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int run_command_foreach_line(line_processor_fn callback, void *context, const char *fmt, ...) {
    if (!callback) return -1;
    
    va_list args;
    char cmd[2048];
    
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);
    
    till_log(LOG_DEBUG, "Executing with line processing: %s", cmd);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        till_log(LOG_ERROR, "Failed to execute: %s", cmd);
        return -1;
    }
    
    char line[1024];
    
    while (fgets(line, sizeof(line), fp)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        /* Call the callback */
        int result = callback(line, context);
        if (result != 0) {
            pclose(fp);
            return result;
        }
    }
    
    int status = pclose(fp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* JSON safe accessors */
const char* json_get_string(cJSON *obj, const char *key, const char *default_val) {
    if (!obj || !key) return default_val;
    
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    
    return default_val;
}

int json_get_int(cJSON *obj, const char *key, int default_val) {
    if (!obj || !key) return default_val;
    
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    
    return default_val;
}

int json_get_bool(cJSON *obj, const char *key, int default_val) {
    if (!obj || !key) return default_val;
    
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item) {
        if (cJSON_IsBool(item)) {
            return cJSON_IsTrue(item) ? 1 : 0;
        }
        if (cJSON_IsNumber(item)) {
            return item->valueint != 0;
        }
    }
    
    return default_val;
}

int json_set_string(cJSON *obj, const char *key, const char *value) {
    if (!obj || !key) return -1;
    
    /* Remove existing item if present */
    cJSON_DeleteItemFromObject(obj, key);
    
    /* Add new value */
    if (value) {
        cJSON_AddStringToObject(obj, key, value);
    } else {
        cJSON_AddNullToObject(obj, key);
    }
    
    return 0;
}

int json_set_int(cJSON *obj, const char *key, int value) {
    if (!obj || !key) return -1;
    
    /* Remove existing item if present */
    cJSON_DeleteItemFromObject(obj, key);
    
    /* Add new value */
    cJSON_AddNumberToObject(obj, key, value);
    
    return 0;
}

int json_set_bool(cJSON *obj, const char *key, int value) {
    if (!obj || !key) return -1;
    
    /* Remove existing item if present */
    cJSON_DeleteItemFromObject(obj, key);
    
    /* Add new value */
    cJSON_AddBoolToObject(obj, key, value ? cJSON_True : cJSON_False);
    
    return 0;
}

/* Process utilities - wrapper for platform abstraction */
int find_process_by_port(int port, process_info_t *info) {
    platform_process_info_t plat_info;
    int pid = platform_find_process_by_port(port, &plat_info);
    
    if (pid > 0 && info) {
        info->pid = plat_info.pid;
        strncpy(info->name, plat_info.name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        strncpy(info->cmd, plat_info.command, sizeof(info->cmd) - 1);
        info->cmd[sizeof(info->cmd) - 1] = '\0';
    }
    
    return pid;
}

int kill_process_graceful(int pid, int timeout_ms) {
    return platform_kill_process(pid, timeout_ms);
}

int is_port_available(int port) {
    return platform_is_port_available(port);
}

int find_available_port(int start, int end) {
    for (int port = start; port <= end; port++) {
        if (is_port_available(port)) {
            return port;
        }
    }
    return -1;
}

/* Directory operations */
int foreach_dir_entry(const char *path, dir_entry_fn callback, void *context) {
    if (!path || !callback) return -1;
    
    DIR *dir = opendir(path);
    if (!dir) {
        till_log(LOG_ERROR, "Cannot open directory: %s", path);
        return -1;
    }
    
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.' && 
            (entry->d_name[1] == '\0' || 
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        
        char full_path[TILL_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        int result = callback(full_path, entry->d_name, context);
        if (result != 0) {
            closedir(dir);
            return result;
        }
        
        count++;
    }
    
    closedir(dir);
    return count;
}

/* Symlink utilities */
int create_or_update_symlink(const char *target, const char *link_path) {
    if (!target || !link_path) return -1;
    
    /* Check if link already exists */
    struct stat st;
    if (lstat(link_path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            /* It's a symlink, check if it points to the right place */
            char current_target[TILL_MAX_PATH];
            ssize_t len = readlink(link_path, current_target, sizeof(current_target) - 1);
            
            if (len > 0) {
                current_target[len] = '\0';
                if (strcmp(current_target, target) == 0) {
                    /* Already correct */
                    return 0;
                }
            }
            
            /* Wrong target, remove and recreate */
            unlink(link_path);
        } else {
            /* Not a symlink, cannot proceed */
            till_log(LOG_ERROR, "%s exists but is not a symlink", link_path);
            return -1;
        }
    }
    
    /* Create the symlink */
    if (symlink(target, link_path) != 0) {
        till_log(LOG_ERROR, "Failed to create symlink %s -> %s: %s", 
                 link_path, target, strerror(errno));
        return -1;
    }
    
    till_log(LOG_INFO, "Created symlink %s -> %s", link_path, target);
    return 0;
}

int symlink_points_to(const char *link_path, const char *expected_target) {
    if (!link_path || !expected_target) return 0;
    
    struct stat st;
    if (lstat(link_path, &st) != 0 || !S_ISLNK(st.st_mode)) {
        return 0;
    }
    
    char actual_target[TILL_MAX_PATH];
    ssize_t len = readlink(link_path, actual_target, sizeof(actual_target) - 1);
    
    if (len <= 0) {
        return 0;
    }
    
    actual_target[len] = '\0';
    return (strcmp(actual_target, expected_target) == 0);
}

/* SSH command utilities */
int build_ssh_command(char *cmd, size_t size, const char *user, const char *host, 
                      int port, const char *remote_cmd) {
    /* Validate inputs */
    if (!cmd || size == 0 || !user || !host) {
        return -1;
    }
    
    /* Build base command */
    int len = snprintf(cmd, size, 
        "ssh -o ConnectTimeout=5 -o BatchMode=yes %s@%s -p %d",
        user, host, port);
    
    if (len < 0 || (size_t)len >= size) {
        return -1;
    }
    
    /* Add remote command if provided (properly quoted) */
    if (remote_cmd && remote_cmd[0]) {
        /* Simple quoting - for more complex cases use build_ssh_command_safe */
        int cmd_len = snprintf(cmd + len, size - len, " '%s' 2>/dev/null", remote_cmd);
        if (cmd_len < 0 || len + cmd_len >= (int)size) {
            return -1;
        }
        len += cmd_len;
    } else {
        int suffix_len = snprintf(cmd + len, size - len, " 2>/dev/null");
        if (suffix_len < 0 || len + suffix_len >= (int)size) {
            return -1;
        }
        len += suffix_len;
    }
    
    return len;
}

int run_ssh_command(const char *user, const char *host, int port, 
                    const char *remote_cmd, char *output, size_t output_size) {
    char ssh_cmd[2048];
    build_ssh_command(ssh_cmd, sizeof(ssh_cmd), user, host, port, remote_cmd);
    return run_command(ssh_cmd, output, output_size);
}

/* Run SSH command using host configuration from hosts file */
int run_ssh_host_command(const char *host_name, const char *remote_cmd,
                        char *output, size_t output_size) {
    /* Load host configuration */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_error("No hosts configured");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, host_name);
    if (!host) {
        till_error("Host '%s' not found", host_name);
        cJSON_Delete(json);
        return -1;
    }
    
    const char *user = json_get_string(host, "user", NULL);
    const char *hostname = json_get_string(host, "host", NULL);
    int port = json_get_int(host, "port", 22);
    
    if (!user || !hostname) {
        till_error("Invalid host configuration for '%s'", host_name);
        cJSON_Delete(json);
        return -1;
    }
    
    int result = run_ssh_command(user, hostname, port, remote_cmd, output, output_size);
    cJSON_Delete(json);
    return result;
}