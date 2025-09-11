/*
 * till_common.c - Common utilities for Till
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#include "till_config.h"
#include "till_common.h"
#include "cJSON.h"

static FILE *log_file = NULL;
static int log_level = LOG_INFO;

/* Initialize logging */
int till_log_init(void) {
    char log_path[TILL_MAX_PATH];
    char till_dir[TILL_MAX_PATH];
    
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) {
        return -1;
    }
    
    /* Create logs directory */
    char log_dir[TILL_MAX_PATH];
    snprintf(log_dir, sizeof(log_dir), "%s/logs", till_dir);
    mkdir(log_dir, TILL_DIR_PERMS);
    
    /* Open log file with timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(log_path, sizeof(log_path), "%s/till_%04d%02d%02d.log",
             log_dir, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    
    log_file = fopen(log_path, "a");
    if (!log_file) {
        fprintf(stderr, "Warning: Could not open log file %s\n", log_path);
        return -1;
    }
    
    return 0;
}

/* Set log level */
void till_log_set_level(int level) {
    log_level = level;
}

/* Log a message */
void till_log(int level, const char *format, ...) {
    if (level < log_level) return;
    
    va_list args;
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
    
    const char *level_str;
    FILE *output = stdout;
    
    switch (level) {
        case LOG_ERROR:
            level_str = "ERROR";
            output = stderr;
            break;
        case LOG_WARN:
            level_str = "WARN ";
            output = stderr;
            break;
        case LOG_INFO:
            level_str = "INFO ";
            break;
        case LOG_DEBUG:
            level_str = "DEBUG";
            break;
        default:
            level_str = "?????";
    }
    
    /* Log to file if available */
    if (log_file) {
        fprintf(log_file, "[%s] %s: ", timestamp, level_str);
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    
    /* Also output to screen for errors and warnings */
    if (level <= LOG_WARN) {
        fprintf(output, "%s: ", level_str);
        va_start(args, format);
        vfprintf(output, format, args);
        va_end(args);
        fprintf(output, "\n");
    }
}

/* Close logging */
void till_log_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

/* Get the Till configuration directory path */
int get_till_dir(char *path, size_t size) {
    char test_path[TILL_MAX_PATH];
    char real_path[TILL_MAX_PATH];
    
    /* Strategy: Find .till directory, preferring the one in till installation */
    
    /* 1. Check if .till exists in current directory (for Tekton dirs with symlinks) */
    if (path_exists(TILL_DIR_NAME)) {
        if (realpath(TILL_DIR_NAME, real_path)) {
            strncpy(path, real_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
    
    /* 2. Check parent directory for till/.till */
    char parent_till[TILL_MAX_PATH];
    snprintf(parent_till, sizeof(parent_till), "../till/%s", TILL_DIR_NAME);
    if (path_exists(parent_till)) {
        if (realpath(parent_till, real_path)) {
            strncpy(path, real_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
    
    /* 3. Check known locations */
    char *home = getenv("HOME");
    if (home) {
        /* Check ~/.till first (standard location) */
        snprintf(test_path, sizeof(test_path), "%s/%s", home, TILL_DIR_NAME);
        if (path_exists(test_path)) {
            strncpy(path, test_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
        
        /* Then check projects/github/till/.till */
        snprintf(test_path, sizeof(test_path), "%s/%s/till/%s", home, TILL_PROJECTS_BASE, TILL_DIR_NAME);
        if (path_exists(test_path)) {
            strncpy(path, test_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
    
    /* 4. Try to find via the till executable */
    FILE *fp = popen("which till 2>/dev/null", "r");
    if (fp) {
        char exe_path[TILL_MAX_PATH];
        if (fgets(exe_path, sizeof(exe_path), fp) != NULL) {
            exe_path[strcspn(exe_path, "\n")] = '\0';
            pclose(fp);
            
            /* Get directory of executable */
            char *dir_end = strrchr(exe_path, '/');
            if (dir_end) {
                *dir_end = '\0';
                
                /* Check for .till in parent of executable location */
                snprintf(test_path, sizeof(test_path), "%s/../%s", exe_path, TILL_DIR_NAME);
                if (path_exists(test_path)) {
                    if (realpath(test_path, real_path)) {
                        strncpy(path, real_path, size - 1);
                        path[size - 1] = '\0';
                        return 0;
                    }
                }
            }
        } else {
            pclose(fp);
        }
    }
    
    /* 5. Last resort - create .till in current directory */
    fprintf(stderr, "Warning: Cannot find till/.till directory, using current directory\n");
    if (getcwd(real_path, sizeof(real_path))) {
        snprintf(path, size, "%s/.till", real_path);
        return 0;
    }
    
    return -1;
}

/* Build a path within Till directory */
int build_till_path(char *dest, size_t size, const char *filename) {
    char till_dir[TILL_MAX_PATH];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) {
        return -1;
    }
    
    snprintf(dest, size, "%s/%s", till_dir, filename);
    return 0;
}

/* Ensure directory exists (with parents) */
int ensure_directory(const char *path) {
    char tmp[TILL_MAX_PATH];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, TILL_DIR_PERMS) != 0 && errno != EEXIST) {
                till_log(LOG_ERROR, "Cannot create directory %s: %s", tmp, strerror(errno));
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, TILL_DIR_PERMS) != 0 && errno != EEXIST) {
        till_log(LOG_ERROR, "Cannot create directory %s: %s", tmp, strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Load JSON from file */
cJSON* load_json_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        till_log(LOG_DEBUG, "Cannot open file %s: %s", path, strerror(errno));
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(fp);
        till_log(LOG_DEBUG, "File %s is empty", path);
        return NULL;
    }
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        till_log(LOG_ERROR, "Cannot allocate memory for file %s", path);
        return NULL;
    }
    
    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        const char *error = cJSON_GetErrorPtr();
        till_log(LOG_ERROR, "JSON parse error in %s: %s", path, error ? error : "unknown");
    }
    
    free(content);
    return json;
}

/* Save JSON to file */
int save_json_file(const char *path, cJSON *json) {
    char *output = cJSON_Print(json);
    if (!output) {
        till_log(LOG_ERROR, "Cannot serialize JSON");
        return -1;
    }
    
    /* Use atomic write with temp file */
    char temp_path[TILL_MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.XXXXXX", path);
    
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        free(output);
        till_error("Cannot create temp file for %s: %s", path, strerror(errno));
        return -1;
    }
    
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        unlink(temp_path);
        free(output);
        till_error("Cannot open temp file for writing: %s", strerror(errno));
        return -1;
    }
    
    if (fprintf(fp, "%s", output) < 0) {
        till_error("Failed to write to temp file");
        fclose(fp);
        unlink(temp_path);
        free(output);
        return -1;
    }
    
    if (fclose(fp) != 0) {
        till_error("Failed to close temp file: %s", strerror(errno));
        unlink(temp_path);
        free(output);
        return -1;
    }
    
    /* Atomic rename to final location */
    if (rename(temp_path, path) != 0) {
        till_error("Failed to rename %s to %s: %s", temp_path, path, strerror(errno));
        unlink(temp_path);
        free(output);
        return -1;
    }
    
    free(output);
    
    till_log(LOG_DEBUG, "Saved JSON to %s", path);
    return 0;
}

/* Load or create Till registry with installations object */
cJSON* load_or_create_registry(void) {
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) {
        registry = cJSON_CreateObject();
        if (!registry) return NULL;
    }
    
    /* Ensure installations object exists */
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations) {
        installations = cJSON_CreateObject();
        if (installations) {
            cJSON_AddItemToObject(registry, "installations", installations);
        }
    }
    
    return registry;
}

/* Load JSON file from Till directory */
cJSON* load_till_json(const char *filename) {
    char path[TILL_MAX_PATH];
    if (build_till_path(path, sizeof(path), filename) != 0) {
        return NULL;
    }
    return load_json_file(path);
}

/* Save JSON file to Till directory */
int save_till_json(const char *filename, cJSON *json) {
    char path[TILL_MAX_PATH];
    fprintf(stderr, "DEBUG: save_till_json called with filename: %s\n", filename);
    if (build_till_path(path, sizeof(path), filename) != 0) {
        till_error("Failed to build path for %s", filename);
        return -1;
    }
    fprintf(stderr, "DEBUG: Built path: %s\n", path);
    
    /* Ensure parent directory exists */
    char *last_slash = strrchr(path, '/');
    if (last_slash) {
        *last_slash = '\0';
        fprintf(stderr, "DEBUG: Creating directory: %s\n", path);
        if (ensure_directory(path) != 0) {
            till_error("Failed to create directory %s", path);
            *last_slash = '/';
            return -1;
        }
        *last_slash = '/';
    }
    
    fprintf(stderr, "DEBUG: Calling save_json_file with path: %s\n", path);
    till_debug("Saving JSON to path: %s", path);
    int result = save_json_file(path, json);
    fprintf(stderr, "DEBUG: save_json_file returned: %d\n", result);
    return result;
}

/* Run command and capture output */
int run_command(const char *cmd, char *output, size_t output_size) {
    till_log(LOG_DEBUG, "Running command: %s", cmd);
    
    if (output) {
        FILE *fp = popen(cmd, "r");
        if (!fp) {
            till_log(LOG_ERROR, "Cannot execute command: %s", cmd);
            return -1;
        }
        
        size_t total = 0;
        size_t n;
        while ((n = fread(output + total, 1, output_size - total - 1, fp)) > 0) {
            total += n;
        }
        output[total] = '\0';
        
        int ret = pclose(fp);
        if (ret != 0) {
            till_log(LOG_DEBUG, "Command failed with status %d: %s", ret, cmd);
        }
        return (ret == 0) ? 0 : -1;
    } else {
        int ret = system(cmd);
        if (ret != 0) {
            till_log(LOG_DEBUG, "Command failed with status %d: %s", ret, cmd);
        }
        return (ret == 0) ? 0 : -1;
    }
}

/* Run command with timeout */
int run_command_timeout(const char *cmd, int timeout_seconds, char *output, size_t output_size) {
    char timeout_cmd[4096];
    
    /* Use timeout command if available */
    snprintf(timeout_cmd, sizeof(timeout_cmd), "timeout %d %s", timeout_seconds, cmd);
    
    /* Check if timeout command exists */
    if (system("which timeout >/dev/null 2>&1") == 0) {
        return run_command(timeout_cmd, output, output_size);
    } else {
        /* Fall back to regular execution with warning */
        till_log(LOG_WARN, "timeout command not available, running without timeout");
        return run_command(cmd, output, output_size);
    }
}

/* Add SSH config entry */
int add_ssh_config_entry(const char *name, const char *user, const char *host, int port) {
    char ssh_config[TILL_MAX_PATH];
    if (build_till_path(ssh_config, sizeof(ssh_config), "ssh/config") != 0) {
        return -1;
    }
    
    /* Ensure SSH directory exists */
    char ssh_dir[TILL_MAX_PATH];
    if (build_till_path(ssh_dir, sizeof(ssh_dir), "ssh") != 0) {
        return -1;
    }
    mkdir(ssh_dir, TILL_SECURE_DIR_PERMS);
    
    FILE *fp = fopen(ssh_config, "a");
    if (!fp) {
        till_log(LOG_ERROR, "Cannot open SSH config for writing: %s", strerror(errno));
        return -1;
    }
    
    fprintf(fp, "\n# Till managed host: %s\n", name);
    fprintf(fp, "Host %s\n", name);
    fprintf(fp, "    HostName %s\n", host);
    fprintf(fp, "    User %s\n", user);
    fprintf(fp, "    Port %d\n", port);
    fprintf(fp, "    StrictHostKeyChecking no\n");
    fprintf(fp, "    UserKnownHostsFile ~/.till/ssh/known_hosts\n");
    fprintf(fp, "\n");
    
    fclose(fp);
    till_log(LOG_INFO, "Added SSH config entry for %s", name);
    return 0;
}

/* Remove SSH config entry */
int remove_ssh_config_entry(const char *name) {
    char ssh_config[TILL_MAX_PATH];
    char ssh_config_tmp[TILL_MAX_PATH];
    
    if (build_till_path(ssh_config, sizeof(ssh_config), "ssh/config") != 0) {
        return -1;
    }
    
    FILE *fp_in = fopen(ssh_config, "r");
    if (!fp_in) {
        till_log(LOG_WARN, "No SSH config file to clean");
        return 0;
    }
    
    /* Create secure temporary file */
    if (create_temp_copy(ssh_config, ssh_config_tmp, sizeof(ssh_config_tmp)) != 0) {
        fclose(fp_in);
        return -1;
    }
    
    FILE *fp_out = fopen(ssh_config_tmp, "w");
    if (!fp_out) {
        fclose(fp_in);
        unlink(ssh_config_tmp);
        till_log(LOG_ERROR, "Cannot open temporary SSH config: %s", strerror(errno));
        return -1;
    }
    
    char line[1024];
    int skip_block = 0;
    char host_pattern[256];
    snprintf(host_pattern, sizeof(host_pattern), "Host %s", name);
    
    while (fgets(line, sizeof(line), fp_in)) {
        if (strstr(line, host_pattern) != NULL) {
            skip_block = 1;
            continue;
        }
        
        if (skip_block) {
            if (line[0] == '\n' || strncmp(line, "Host ", 5) == 0 || 
                strncmp(line, "#", 1) == 0) {
                skip_block = 0;
                if (line[0] == '\n') continue;
            } else {
                continue;
            }
        }
        
        if (!skip_block) {
            fputs(line, fp_out);
        }
    }
    
    fclose(fp_in);
    fclose(fp_out);
    
    if (rename(ssh_config_tmp, ssh_config) != 0) {
        till_log(LOG_ERROR, "Cannot update SSH config: %s", strerror(errno));
        unlink(ssh_config_tmp);  /* Clean up temp file on error */
        return -1;
    }
    
    till_log(LOG_INFO, "Removed SSH config entry for %s", name);
    return 0;
}

/* Create a secure temporary file using mkstemp */
int create_temp_file(char *template, FILE **fp) {
    int fd = mkstemp(template);
    if (fd == -1) {
        till_log(LOG_ERROR, "Failed to create temp file: %s", strerror(errno));
        return -1;
    }
    
    *fp = fdopen(fd, "w+");
    if (!*fp) {
        till_log(LOG_ERROR, "Failed to open temp file: %s", strerror(errno));
        close(fd);
        unlink(template);
        return -1;
    }
    
    return 0;
}

/* Create a temporary copy of a file for atomic operations */
int create_temp_copy(const char *original, char *temp_path, size_t temp_size) {
    /* Build temp file template */
    snprintf(temp_path, temp_size, "%s.XXXXXX", original);
    
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        till_log(LOG_ERROR, "Failed to create temp file: %s", strerror(errno));
        return -1;
    }
    
    /* Set secure permissions */
    if (fchmod(fd, TILL_FILE_PERMS) != 0) {
        till_log(LOG_WARN, "Failed to set temp file permissions: %s", strerror(errno));
    }
    
    close(fd);
    return 0;
}