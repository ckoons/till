/*
 * till_common.h - Common utilities for Till
 */

#ifndef TILL_COMMON_H
#define TILL_COMMON_H

#include <stdio.h>
#include <limits.h>
#include "cJSON.h"
#include "till_constants.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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
cJSON* load_or_create_registry(void);

/* Command execution */
int run_command(const char *cmd, char *output, size_t output_size);
int run_command_timeout(const char *cmd, int timeout_seconds, char *output, size_t output_size);

/* SSH config management */
int add_ssh_config_entry(const char *name, const char *user, const char *host, int port);
int remove_ssh_config_entry(const char *name);

/* Error reporting with combined stderr + log */
void till_error(const char *fmt, ...);
void till_warn(const char *fmt, ...);
void till_info(const char *fmt, ...);
void till_debug(const char *fmt, ...);
int till_error_return(int code, const char *fmt, ...);

/* Path and file utilities */
int path_exists(const char *path);
int is_directory(const char *path);
int is_file(const char *path);
int is_executable(const char *path);
int is_symlink(const char *path);
int path_join(char *dest, size_t size, const char *path1, const char *path2);
int path_make_absolute(char *dest, size_t size, const char *path);

/* Command execution utilities */
int run_command_logged(const char *fmt, ...);
int run_command_capture(char *output, size_t size, const char *fmt, ...);
typedef int (*line_processor_fn)(const char *line, void *context);
int run_command_foreach_line(line_processor_fn callback, void *context, const char *fmt, ...);

/* JSON safe accessors */
const char* json_get_string(cJSON *obj, const char *key, const char *default_val);
int json_get_int(cJSON *obj, const char *key, int default_val);
int json_get_bool(cJSON *obj, const char *key, int default_val);
int json_set_string(cJSON *obj, const char *key, const char *value);
int json_set_int(cJSON *obj, const char *key, int value);
int json_set_bool(cJSON *obj, const char *key, int value);

/* Process utilities */
typedef struct {
    int pid;
    char name[256];
    char cmd[1024];
} process_info_t;
int find_process_by_port(int port, process_info_t *info);
int kill_process_graceful(int pid, int timeout_ms);
int is_port_available(int port);
int find_available_port(int start, int end);

/* Directory operations */
typedef int (*dir_entry_fn)(const char *path, const char *name, void *context);
int foreach_dir_entry(const char *path, dir_entry_fn callback, void *context);

/* Symlink utilities */
int create_or_update_symlink(const char *target, const char *link_path);
int symlink_points_to(const char *link_path, const char *expected_target);

/* Secure temporary file creation */
int create_temp_file(char *template, FILE **fp);
int create_temp_copy(const char *original, char *temp_path, size_t temp_size);

#endif /* TILL_COMMON_H */