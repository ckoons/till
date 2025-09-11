/*
 * till_platform.h - Platform abstraction layer for Till
 * 
 * Provides platform-specific implementations for:
 * - Process management
 * - Scheduling (launchd/systemd/cron)
 * - File system operations
 * - Network operations
 */

#ifndef TILL_PLATFORM_H
#define TILL_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* Platform detection */
#ifdef __APPLE__
    #define PLATFORM_MACOS 1
    #define PLATFORM_LINUX 0
    #define PLATFORM_BSD 0
#elif __linux__
    #define PLATFORM_MACOS 0
    #define PLATFORM_LINUX 1
    #define PLATFORM_BSD 0
#elif __FreeBSD__ || __NetBSD__ || __OpenBSD__
    #define PLATFORM_MACOS 0
    #define PLATFORM_LINUX 0
    #define PLATFORM_BSD 1
#else
    #error "Unsupported platform"
#endif

/* Process information structure */
typedef struct {
    int pid;
    char name[256];
    char command[1024];
    int port;
} platform_process_info_t;

/* Scheduler types */
typedef enum {
    SCHEDULER_NONE = 0,
    SCHEDULER_LAUNCHD,    /* macOS */
    SCHEDULER_SYSTEMD,    /* Linux with systemd */
    SCHEDULER_CRON,       /* Unix/Linux fallback */
    SCHEDULER_INIT        /* Traditional init */
} scheduler_type_t;

/* Schedule configuration */
typedef struct {
    const char *name;           /* Job name */
    const char *command;         /* Command to run */
    const char *working_dir;     /* Working directory */
    const char *log_file;        /* Log file path */
    const char *error_file;      /* Error log path */
    const char *schedule;        /* Schedule specification (cron format or time) */
    int user_level;             /* User-level (1) or system-level (0) */
} schedule_config_t;

/* Platform capability flags */
typedef struct {
    int has_launchd;
    int has_systemd;
    int has_cron;
    int has_lsof;
    int has_netstat;
    int has_ss;
    int has_timeout_cmd;
} platform_capabilities_t;

/* Process Management Functions */

/* Find process using a specific port */
int platform_find_process_by_port(int port, platform_process_info_t *info);

/* List all processes using ports in range */
int platform_list_port_processes(int start_port, int end_port, 
                                 platform_process_info_t **processes, 
                                 int *count);

/* Check if a port is available */
int platform_is_port_available(int port);

/* Kill a process gracefully with timeout */
int platform_kill_process(int pid, int timeout_ms);

/* Check if process is running */
int platform_process_exists(int pid);

/* Get process info by PID */
int platform_get_process_info(int pid, platform_process_info_t *info);

/* Scheduling Functions */

/* Get available scheduler type */
scheduler_type_t platform_get_scheduler(void);

/* Install scheduled job */
int platform_schedule_install(const schedule_config_t *config);

/* Remove scheduled job */
int platform_schedule_remove(const char *name);

/* Check if job is scheduled */
int platform_schedule_exists(const char *name);

/* List scheduled jobs */
int platform_schedule_list(char ***names, int *count);

/* File System Functions */

/* Get user home directory */
const char *platform_get_home_dir(void);

/* Get system config directory */
const char *platform_get_config_dir(void);

/* Get temp directory */
const char *platform_get_temp_dir(void);

/* Create directory with proper permissions */
int platform_mkdir_p(const char *path, int mode);

/* Set file permissions */
int platform_set_permissions(const char *path, int mode);

/* Network Functions */

/* Get list of network interfaces */
int platform_get_interfaces(char ***interfaces, int *count);

/* Get IP address for interface */
int platform_get_interface_ip(const char *interface, char *ip, size_t size);

/* System Information */

/* Get platform name */
const char *platform_get_name(void);

/* Get platform version */
const char *platform_get_version(void);

/* Get CPU count */
int platform_get_cpu_count(void);

/* Get total memory in MB */
int platform_get_memory_mb(void);

/* Get platform capabilities */
void platform_get_capabilities(platform_capabilities_t *caps);

/* Utility Functions */

/* Execute command with timeout */
int platform_exec_timeout(const char *command, int timeout_ms, 
                         char *output, size_t output_size);

/* Get current executable path */
int platform_get_executable_path(char *path, size_t size);

/* Check if running with admin/root privileges */
int platform_is_admin(void);

/* Open URL in default browser */
int platform_open_url(const char *url);

#endif /* TILL_PLATFORM_H */