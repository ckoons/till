/*
 * till_common.h - Common utilities for Till
 */

#ifndef TILL_COMMON_H
#define TILL_COMMON_H

#include <limits.h>
#include "cJSON.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Log levels */
#define LOG_ERROR   0
#define LOG_WARN    1
#define LOG_INFO    2
#define LOG_DEBUG   3

/* Logging functions */
int till_log_init(void);
void till_log_set_level(int level);
void till_log(int level, const char *format, ...);
void till_log_close(void);

/* Directory and path functions */
int get_till_dir(char *path, size_t size);
int build_till_path(char *dest, size_t size, const char *filename);
int ensure_directory(const char *path);

/* JSON file operations */
cJSON* load_json_file(const char *path);
int save_json_file(const char *path, cJSON *json);
cJSON* load_till_json(const char *filename);
int save_till_json(const char *filename, cJSON *json);

/* Command execution */
int run_command(const char *cmd, char *output, size_t output_size);
int run_command_timeout(const char *cmd, int timeout_seconds, char *output, size_t output_size);

/* SSH config management */
int add_ssh_config_entry(const char *name, const char *user, const char *host, int port);
int remove_ssh_config_entry(const char *name);

#endif /* TILL_COMMON_H */