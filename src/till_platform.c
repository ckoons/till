/*
 * till_platform.c - Platform abstraction implementation
 * 
 * Provides platform-specific implementations for macOS, Linux, and BSD
 */

#include "till_platform.h"
#include "till_common.h"
#include "till_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>

#if PLATFORM_MACOS
#define _DARWIN_C_SOURCE 1
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#elif PLATFORM_LINUX
#include <sys/sysinfo.h>
#endif

/* Platform name */
const char *platform_get_name(void) {
#if PLATFORM_MACOS
    return "macOS";
#elif PLATFORM_LINUX
    return "Linux";
#elif PLATFORM_BSD
    return "BSD";
#else
    return "Unknown";
#endif
}

/* Platform version */
const char *platform_get_version(void) {
    static char version[128];
    
#if PLATFORM_MACOS
    size_t size = sizeof(version);
    if (sysctlbyname("kern.osproductversion", version, &size, NULL, 0) == 0) {
        return version;
    }
#elif PLATFORM_LINUX
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VERSION=", 8) == 0) {
                char *start = strchr(line, '"');
                if (start) {
                    start++;
                    char *end = strchr(start, '"');
                    if (end) {
                        size_t len = end - start;
                        if (len < sizeof(version)) {
                            strncpy(version, start, len);
                            version[len] = '\0';
                            fclose(fp);
                            return version;
                        }
                    }
                }
            }
        }
        fclose(fp);
    }
#endif
    
    /* Fallback to uname */
    FILE *pipe = popen("uname -r", "r");
    if (pipe) {
        if (fgets(version, sizeof(version), pipe)) {
            /* Remove newline */
            size_t len = strlen(version);
            if (len > 0 && version[len-1] == '\n') {
                version[len-1] = '\0';
            }
        }
        pclose(pipe);
        return version;
    }
    
    return "Unknown";
}

/* Get home directory */
const char *platform_get_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) {
        return home;
    }
    
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_dir;
    }
    
    return NULL;
}

/* Get config directory */
const char *platform_get_config_dir(void) {
    static char config_dir[TILL_MAX_PATH];
    
#if PLATFORM_MACOS
    snprintf(config_dir, sizeof(config_dir), "%s/Library/Application Support", 
             platform_get_home_dir());
#elif PLATFORM_LINUX || PLATFORM_BSD
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        strncpy(config_dir, xdg_config, sizeof(config_dir) - 1);
        config_dir[sizeof(config_dir) - 1] = '\0';
    } else {
        snprintf(config_dir, sizeof(config_dir), "%s/.config", 
                platform_get_home_dir());
    }
#endif
    
    return config_dir;
}

/* Get temp directory */
const char *platform_get_temp_dir(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir) return tmpdir;
    
    tmpdir = getenv("TEMP");
    if (tmpdir) return tmpdir;
    
    tmpdir = getenv("TMP");
    if (tmpdir) return tmpdir;
    
    return "/tmp";
}

/* Create directory with parents */
int platform_mkdir_p(const char *path, int mode) {
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
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    
    return 0;
}

/* Set file permissions */
int platform_set_permissions(const char *path, int mode) {
    return chmod(path, mode);
}

/* Get CPU count */
int platform_get_cpu_count(void) {
#if PLATFORM_MACOS || PLATFORM_BSD
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0) == 0) {
        return count;
    }
#elif PLATFORM_LINUX
    return get_nprocs();
#endif
    
    /* Fallback */
#ifdef _SC_NPROCESSORS_ONLN
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs > 0) {
        return (int)nprocs;
    }
#endif
    
    return 1;
}

/* Get memory in MB */
int platform_get_memory_mb(void) {
#if PLATFORM_MACOS
    int64_t mem;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, NULL, 0) == 0) {
        return (int)(mem / (1024 * 1024));
    }
#elif PLATFORM_LINUX
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (int)(info.totalram * info.mem_unit / (1024 * 1024));
    }
#endif
    
    /* Fallback */
#ifdef _SC_PHYS_PAGES
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return (int)((pages * page_size) / (1024 * 1024));
    }
#endif
    
    return 0;
}

/* Get executable path */
int platform_get_executable_path(char *path, size_t size) {
#if PLATFORM_MACOS
    uint32_t bufsize = size;
    if (_NSGetExecutablePath(path, &bufsize) == 0) {
        char resolved[TILL_MAX_PATH];
        if (realpath(path, resolved)) {
            strncpy(path, resolved, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
#elif PLATFORM_LINUX
    ssize_t len = readlink("/proc/self/exe", path, size - 1);
    if (len > 0) {
        path[len] = '\0';
        return 0;
    }
#elif PLATFORM_BSD
    ssize_t len = readlink("/proc/curproc/file", path, size - 1);
    if (len > 0) {
        path[len] = '\0';
        return 0;
    }
#endif
    
    return -1;
}

/* Check admin privileges */
int platform_is_admin(void) {
    return geteuid() == 0;
}

/* Open URL in browser */
int platform_open_url(const char *url) {
    char cmd[TILL_XLARGE_BUFFER];
    
#if PLATFORM_MACOS
    snprintf(cmd, sizeof(cmd), "open '%s'", url);
#elif PLATFORM_LINUX
    /* Try xdg-open first, then fallback to specific browsers */
    if (system("which xdg-open >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "xdg-open '%s'", url);
    } else if (system("which firefox >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "firefox '%s'", url);
    } else if (system("which chromium >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "chromium '%s'", url);
    } else {
        return -1;
    }
#elif PLATFORM_BSD
    if (system("which xdg-open >/dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "xdg-open '%s'", url);
    } else {
        return -1;
    }
#endif
    
    return system(cmd);
}

/* Get platform capabilities */
void platform_get_capabilities(platform_capabilities_t *caps) {
    memset(caps, 0, sizeof(*caps));
    
#if PLATFORM_MACOS
    caps->has_launchd = 1;
    caps->has_lsof = (system("which lsof >/dev/null 2>&1") == 0);
#elif PLATFORM_LINUX
    caps->has_systemd = (system("systemctl --version >/dev/null 2>&1") == 0);
    caps->has_cron = (system("which crontab >/dev/null 2>&1") == 0);
    caps->has_lsof = (system("which lsof >/dev/null 2>&1") == 0);
    caps->has_ss = (system("which ss >/dev/null 2>&1") == 0);
    caps->has_netstat = (system("which netstat >/dev/null 2>&1") == 0);
#elif PLATFORM_BSD
    caps->has_cron = (system("which crontab >/dev/null 2>&1") == 0);
    caps->has_netstat = (system("which netstat >/dev/null 2>&1") == 0);
#endif
    
    caps->has_timeout_cmd = (system("which timeout >/dev/null 2>&1") == 0);
}