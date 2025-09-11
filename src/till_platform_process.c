/*
 * till_platform_process.c - Platform-specific process management
 * 
 * Handles process discovery, port checking, and process control
 */

#include "till_platform.h"
#include "till_common.h"
#include "till_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Find process using a port - macOS implementation */
#if PLATFORM_MACOS
static int find_process_by_port_macos(int port, platform_process_info_t *info) {
    char cmd[TILL_LARGE_BUFFER];
    char output[TILL_XLARGE_BUFFER];
    
    /* Try lsof first */
    snprintf(cmd, sizeof(cmd), "lsof -i :%d -P -n -t 2>/dev/null | head -1", port);
    
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return 0;
    
    if (fgets(output, sizeof(output), pipe)) {
        int pid = atoi(output);
        pclose(pipe);
        
        if (pid > 0 && info) {
            info->pid = pid;
            info->port = port;
            
            /* Get process name */
            snprintf(cmd, sizeof(cmd), "ps -p %d -o comm= 2>/dev/null", pid);
            pipe = popen(cmd, "r");
            if (pipe) {
                if (fgets(info->name, sizeof(info->name), pipe)) {
                    /* Remove newline */
                    size_t len = strlen(info->name);
                    if (len > 0 && info->name[len-1] == '\n') {
                        info->name[len-1] = '\0';
                    }
                }
                pclose(pipe);
            }
            
            /* Get full command */
            snprintf(cmd, sizeof(cmd), "ps -p %d -o command= 2>/dev/null", pid);
            pipe = popen(cmd, "r");
            if (pipe) {
                if (fgets(info->command, sizeof(info->command), pipe)) {
                    /* Remove newline */
                    size_t len = strlen(info->command);
                    if (len > 0 && info->command[len-1] == '\n') {
                        info->command[len-1] = '\0';
                    }
                }
                pclose(pipe);
            }
        }
        
        return pid;
    }
    
    pclose(pipe);
    return 0;
}
#endif

/* Find process using a port - Linux implementation */
#if PLATFORM_LINUX
static int find_process_by_port_linux(int port, platform_process_info_t *info) {
    char cmd[TILL_LARGE_BUFFER];
    char output[TILL_XLARGE_BUFFER];
    
    /* Try ss first (faster than lsof) */
    snprintf(cmd, sizeof(cmd), 
             "ss -tulpn 2>/dev/null | grep ':%d ' | grep -oP '(?<=pid=)[0-9]+' | head -1", 
             port);
    
    FILE *pipe = popen(cmd, "r");
    if (pipe) {
        if (fgets(output, sizeof(output), pipe)) {
            int pid = atoi(output);
            pclose(pipe);
            
            if (pid > 0 && info) {
                info->pid = pid;
                info->port = port;
                
                /* Get process info from /proc */
                char proc_path[256];
                snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);
                
                FILE *fp = fopen(proc_path, "r");
                if (fp) {
                    if (fgets(info->name, sizeof(info->name), fp)) {
                        size_t len = strlen(info->name);
                        if (len > 0 && info->name[len-1] == '\n') {
                            info->name[len-1] = '\0';
                        }
                    }
                    fclose(fp);
                }
                
                /* Get command line */
                snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", pid);
                fp = fopen(proc_path, "r");
                if (fp) {
                    size_t n = fread(info->command, 1, sizeof(info->command) - 1, fp);
                    if (n > 0) {
                        /* Replace nulls with spaces */
                        for (size_t i = 0; i < n; i++) {
                            if (info->command[i] == '\0') {
                                info->command[i] = ' ';
                            }
                        }
                        info->command[n] = '\0';
                    }
                    fclose(fp);
                }
            }
            
            return pid;
        }
        pclose(pipe);
    }
    
    /* Fallback to lsof if available */
    snprintf(cmd, sizeof(cmd), "lsof -i :%d -P -n -t 2>/dev/null | head -1", port);
    pipe = popen(cmd, "r");
    if (pipe) {
        if (fgets(output, sizeof(output), pipe)) {
            int pid = atoi(output);
            pclose(pipe);
            
            if (pid > 0 && info) {
                info->pid = pid;
                info->port = port;
                
                /* Get process name */
                snprintf(cmd, sizeof(cmd), "ps -p %d -o comm= 2>/dev/null", pid);
                FILE *ps_pipe = popen(cmd, "r");
                if (ps_pipe) {
                    if (fgets(info->name, sizeof(info->name), ps_pipe)) {
                        size_t len = strlen(info->name);
                        if (len > 0 && info->name[len-1] == '\n') {
                            info->name[len-1] = '\0';
                        }
                    }
                    pclose(ps_pipe);
                }
            }
            
            return pid;
        }
        pclose(pipe);
    }
    
    /* Fallback to netstat */
    snprintf(cmd, sizeof(cmd), 
             "netstat -tulpn 2>/dev/null | grep ':%d ' | awk '{print $NF}' | cut -d'/' -f1 | head -1", 
             port);
    
    pipe = popen(cmd, "r");
    if (pipe) {
        if (fgets(output, sizeof(output), pipe)) {
            int pid = atoi(output);
            pclose(pipe);
            return pid;
        }
        pclose(pipe);
    }
    
    return 0;
}
#endif

/* Find process using a port - BSD implementation */
#if PLATFORM_BSD
static int find_process_by_port_bsd(int port, platform_process_info_t *info) {
    char cmd[TILL_LARGE_BUFFER];
    char output[TILL_XLARGE_BUFFER];
    
    /* Use sockstat on BSD */
    snprintf(cmd, sizeof(cmd), 
             "sockstat -4 -l -p %d | tail -n +2 | awk '{print $3}' | head -1", 
             port);
    
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return 0;
    
    if (fgets(output, sizeof(output), pipe)) {
        int pid = atoi(output);
        pclose(pipe);
        
        if (pid > 0 && info) {
            info->pid = pid;
            info->port = port;
            
            /* Get process name */
            snprintf(cmd, sizeof(cmd), "ps -p %d -o comm= 2>/dev/null", pid);
            pipe = popen(cmd, "r");
            if (pipe) {
                if (fgets(info->name, sizeof(info->name), pipe)) {
                    size_t len = strlen(info->name);
                    if (len > 0 && info->name[len-1] == '\n') {
                        info->name[len-1] = '\0';
                    }
                }
                pclose(pipe);
            }
        }
        
        return pid;
    }
    
    pclose(pipe);
    return 0;
}
#endif

/* Public interface - Find process by port */
int platform_find_process_by_port(int port, platform_process_info_t *info) {
    if (info) {
        memset(info, 0, sizeof(*info));
    }
    
#if PLATFORM_MACOS
    return find_process_by_port_macos(port, info);
#elif PLATFORM_LINUX
    return find_process_by_port_linux(port, info);
#elif PLATFORM_BSD
    return find_process_by_port_bsd(port, info);
#else
    return 0;
#endif
}

/* Check if port is available */
int platform_is_port_available(int port) {
    return platform_find_process_by_port(port, NULL) == 0;
}

/* Kill process gracefully */
int platform_kill_process(int pid, int timeout_ms) {
    if (pid <= 0) return -1;
    
    /* Try SIGTERM first */
    if (kill(pid, SIGTERM) != 0) {
        if (errno == ESRCH) {
            /* Process doesn't exist */
            return 0;
        }
        return -1;
    }
    
    /* Wait for process to terminate */
    int wait_time = 0;
    int check_interval = 100; /* 100ms */
    
    while (wait_time < timeout_ms) {
        if (kill(pid, 0) != 0) {
            /* Process terminated */
            return 0;
        }
        
        usleep(check_interval * 1000);
        wait_time += check_interval;
    }
    
    /* Force kill if still running */
    kill(pid, SIGKILL);
    usleep(100000); /* Wait 100ms for force kill */
    
    /* Check if process is gone */
    if (kill(pid, 0) != 0) {
        return 0;
    }
    
    return -1;
}

/* Check if process exists */
int platform_process_exists(int pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0;
}

/* Get process info by PID */
int platform_get_process_info(int pid, platform_process_info_t *info) {
    if (!info || pid <= 0) return -1;
    
    memset(info, 0, sizeof(*info));
    info->pid = pid;
    
    char cmd[TILL_LARGE_BUFFER];
    
#if PLATFORM_LINUX
    /* Try /proc first on Linux */
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);
    
    FILE *fp = fopen(proc_path, "r");
    if (fp) {
        if (fgets(info->name, sizeof(info->name), fp)) {
            size_t len = strlen(info->name);
            if (len > 0 && info->name[len-1] == '\n') {
                info->name[len-1] = '\0';
            }
        }
        fclose(fp);
        
        /* Get command line */
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", pid);
        fp = fopen(proc_path, "r");
        if (fp) {
            size_t n = fread(info->command, 1, sizeof(info->command) - 1, fp);
            if (n > 0) {
                /* Replace nulls with spaces */
                for (size_t i = 0; i < n; i++) {
                    if (info->command[i] == '\0') {
                        info->command[i] = ' ';
                    }
                }
                info->command[n] = '\0';
            }
            fclose(fp);
        }
        
        return 0;
    }
#endif
    
    /* Fallback to ps */
    snprintf(cmd, sizeof(cmd), "ps -p %d -o comm= 2>/dev/null", pid);
    FILE *pipe = popen(cmd, "r");
    if (pipe) {
        if (fgets(info->name, sizeof(info->name), pipe)) {
            size_t len = strlen(info->name);
            if (len > 0 && info->name[len-1] == '\n') {
                info->name[len-1] = '\0';
            }
            pclose(pipe);
            
            /* Get full command */
            snprintf(cmd, sizeof(cmd), "ps -p %d -o command= 2>/dev/null", pid);
            pipe = popen(cmd, "r");
            if (pipe) {
                if (fgets(info->command, sizeof(info->command), pipe)) {
                    len = strlen(info->command);
                    if (len > 0 && info->command[len-1] == '\n') {
                        info->command[len-1] = '\0';
                    }
                }
                pclose(pipe);
            }
            
            return 0;
        }
        pclose(pipe);
    }
    
    return -1;
}

/* List processes using ports in range */
int platform_list_port_processes(int start_port, int end_port, 
                                 platform_process_info_t **processes, 
                                 int *count) {
    if (!processes || !count) return -1;
    
    /* Allocate array for worst case */
    int max_procs = end_port - start_port + 1;
    *processes = calloc(max_procs, sizeof(platform_process_info_t));
    if (!*processes) return -1;
    
    *count = 0;
    
    for (int port = start_port; port <= end_port; port++) {
        platform_process_info_t info;
        if (platform_find_process_by_port(port, &info) > 0) {
            /* Check if we already have this PID */
            int found = 0;
            for (int i = 0; i < *count; i++) {
                if ((*processes)[i].pid == info.pid) {
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                (*processes)[*count] = info;
                (*count)++;
            }
        }
    }
    
    /* Resize array to actual size */
    if (*count > 0) {
        platform_process_info_t *resized = realloc(*processes, 
                                                   *count * sizeof(platform_process_info_t));
        if (resized) {
            *processes = resized;
        }
    } else {
        free(*processes);
        *processes = NULL;
    }
    
    return 0;
}

/* Execute command with timeout */
int platform_exec_timeout(const char *command, int timeout_ms, 
                         char *output, size_t output_size) {
    char cmd[TILL_HUGE_BUFFER];
    
    /* Check if timeout command is available */
    if (system("which timeout >/dev/null 2>&1") == 0) {
        /* Use GNU timeout */
        snprintf(cmd, sizeof(cmd), "timeout %.3f %s", 
                timeout_ms / 1000.0, command);
    } else {
        /* No timeout available, run without timeout */
        strncpy(cmd, command, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }
    
    if (output && output_size > 0) {
        FILE *pipe = popen(cmd, "r");
        if (!pipe) return -1;
        
        size_t total = 0;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) && total < output_size - 1) {
            size_t len = strlen(buffer);
            if (total + len >= output_size) {
                len = output_size - total - 1;
            }
            memcpy(output + total, buffer, len);
            total += len;
        }
        output[total] = '\0';
        
        int status = pclose(pipe);
        return WEXITSTATUS(status);
    } else {
        return system(cmd);
    }
}