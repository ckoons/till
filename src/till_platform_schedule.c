/*
 * till_platform_schedule.c - Platform-specific scheduling
 * 
 * Handles launchd (macOS), systemd (Linux), and cron (fallback)
 */

#include "till_platform.h"
#include "till_common.h"
#include "till_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

/* Get available scheduler */
scheduler_type_t platform_get_scheduler(void) {
#if PLATFORM_MACOS
    return SCHEDULER_LAUNCHD;
#elif PLATFORM_LINUX
    /* Check for systemd first */
    if (system("systemctl --version >/dev/null 2>&1") == 0) {
        return SCHEDULER_SYSTEMD;
    }
    /* Fall back to cron */
    if (system("which crontab >/dev/null 2>&1") == 0) {
        return SCHEDULER_CRON;
    }
    return SCHEDULER_NONE;
#else
    if (system("which crontab >/dev/null 2>&1") == 0) {
        return SCHEDULER_CRON;
    }
    return SCHEDULER_NONE;
#endif
}

/* Install launchd plist (macOS) */
#if PLATFORM_MACOS
static int install_launchd(const schedule_config_t *config) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    char plist_dir[PATH_MAX];
    char plist_path[PATH_MAX];
    
    if (config->user_level) {
        snprintf(plist_dir, sizeof(plist_dir), 
                "%s/Library/LaunchAgents", pw->pw_dir);
    } else {
        strncpy(plist_dir, "/Library/LaunchDaemons", sizeof(plist_dir) - 1);
        plist_dir[sizeof(plist_dir) - 1] = '\0';
    }
    
    /* Create directory if needed */
    platform_mkdir_p(plist_dir, 0755);
    
    snprintf(plist_path, sizeof(plist_path), 
            "%s/com.till.%s.plist", plist_dir, config->name);
    
    FILE *fp = fopen(plist_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" ");
    fprintf(fp, "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    fprintf(fp, "<plist version=\"1.0\">\n");
    fprintf(fp, "<dict>\n");
    fprintf(fp, "    <key>Label</key>\n");
    fprintf(fp, "    <string>com.till.%s</string>\n", config->name);
    fprintf(fp, "    <key>ProgramArguments</key>\n");
    fprintf(fp, "    <array>\n");
    
    /* Parse command into arguments */
    char *cmd_copy = strdup(config->command);
    char *token = strtok(cmd_copy, " ");
    while (token) {
        fprintf(fp, "        <string>%s</string>\n", token);
        token = strtok(NULL, " ");
    }
    free(cmd_copy);
    
    fprintf(fp, "    </array>\n");
    
    if (config->working_dir) {
        fprintf(fp, "    <key>WorkingDirectory</key>\n");
        fprintf(fp, "    <string>%s</string>\n", config->working_dir);
    }
    
    if (config->log_file) {
        fprintf(fp, "    <key>StandardOutPath</key>\n");
        fprintf(fp, "    <string>%s</string>\n", config->log_file);
    }
    
    if (config->error_file) {
        fprintf(fp, "    <key>StandardErrorPath</key>\n");
        fprintf(fp, "    <string>%s</string>\n", config->error_file);
    }
    
    /* Schedule */
    if (config->schedule) {
        /* Parse schedule - expecting HH:MM format */
        int hour = 3, minute = 0;  /* Default 3:00 AM */
        sscanf(config->schedule, "%d:%d", &hour, &minute);
        
        fprintf(fp, "    <key>StartCalendarInterval</key>\n");
        fprintf(fp, "    <dict>\n");
        fprintf(fp, "        <key>Hour</key>\n");
        fprintf(fp, "        <integer>%d</integer>\n", hour);
        fprintf(fp, "        <key>Minute</key>\n");
        fprintf(fp, "        <integer>%d</integer>\n", minute);
        fprintf(fp, "    </dict>\n");
    }
    
    fprintf(fp, "    <key>RunAtLoad</key>\n");
    fprintf(fp, "    <false/>\n");
    fprintf(fp, "</dict>\n");
    fprintf(fp, "</plist>\n");
    
    fclose(fp);
    
    /* Load the plist */
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), 
            "launchctl unload '%s' 2>/dev/null; launchctl load '%s'",
            plist_path, plist_path);
    
    return system(cmd);
}

static int remove_launchd(const char *name) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    char plist_path[PATH_MAX];
    char cmd[PATH_MAX * 2];
    
    /* Try user level first */
    snprintf(plist_path, sizeof(plist_path), 
            "%s/Library/LaunchAgents/com.till.%s.plist", pw->pw_dir, name);
    
    if (access(plist_path, F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "launchctl unload '%s'", plist_path);
        system(cmd);
        unlink(plist_path);
        return 0;
    }
    
    /* Try system level */
    snprintf(plist_path, sizeof(plist_path), 
            "/Library/LaunchDaemons/com.till.%s.plist", name);
    
    if (access(plist_path, F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "sudo launchctl unload '%s'", plist_path);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "sudo rm '%s'", plist_path);
        system(cmd);
        return 0;
    }
    
    return -1;
}
#endif

/* Install systemd timer (Linux) */
#if PLATFORM_LINUX
static int install_systemd(const schedule_config_t *config) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    char service_dir[PATH_MAX];
    char service_path[PATH_MAX];
    char timer_path[PATH_MAX];
    
    if (config->user_level) {
        snprintf(service_dir, sizeof(service_dir), 
                "%s/.config/systemd/user", pw->pw_dir);
    } else {
        strncpy(service_dir, "/etc/systemd/system", sizeof(service_dir) - 1);
        service_dir[sizeof(service_dir) - 1] = '\0';
    }
    
    /* Create directory if needed */
    platform_mkdir_p(service_dir, 0755);
    
    /* Write service file */
    snprintf(service_path, sizeof(service_path), 
            "%s/till-%s.service", service_dir, config->name);
    
    FILE *fp = fopen(service_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "[Unit]\n");
    fprintf(fp, "Description=Till %s Service\n\n", config->name);
    fprintf(fp, "[Service]\n");
    fprintf(fp, "Type=oneshot\n");
    fprintf(fp, "ExecStart=%s\n", config->command);
    
    if (config->working_dir) {
        fprintf(fp, "WorkingDirectory=%s\n", config->working_dir);
    }
    
    if (config->log_file) {
        fprintf(fp, "StandardOutput=append:%s\n", config->log_file);
    }
    
    if (config->error_file) {
        fprintf(fp, "StandardError=append:%s\n", config->error_file);
    }
    
    fclose(fp);
    
    /* Write timer file */
    snprintf(timer_path, sizeof(timer_path), 
            "%s/till-%s.timer", service_dir, config->name);
    
    fp = fopen(timer_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "[Unit]\n");
    fprintf(fp, "Description=Till %s Timer\n", config->name);
    fprintf(fp, "Requires=till-%s.service\n\n", config->name);
    fprintf(fp, "[Timer]\n");
    
    if (config->schedule) {
        /* Parse schedule */
        if (strchr(config->schedule, ':')) {
            /* Time format HH:MM */
            fprintf(fp, "OnCalendar=*-*-* %s:00\n", config->schedule);
        } else {
            /* Assume systemd calendar format */
            fprintf(fp, "OnCalendar=%s\n", config->schedule);
        }
    } else {
        fprintf(fp, "OnCalendar=daily\n");
    }
    
    fprintf(fp, "Persistent=true\n\n");
    fprintf(fp, "[Install]\n");
    fprintf(fp, "WantedBy=timers.target\n");
    
    fclose(fp);
    
    /* Enable and start timer */
    char cmd[PATH_MAX];
    const char *ctl = config->user_level ? "systemctl --user" : "systemctl";
    
    snprintf(cmd, sizeof(cmd), "%s daemon-reload", ctl);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "%s enable till-%s.timer", ctl, config->name);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "%s start till-%s.timer", ctl, config->name);
    return system(cmd);
}

static int remove_systemd(const char *name) {
    char cmd[PATH_MAX];
    
    /* Try user level first */
    snprintf(cmd, sizeof(cmd), "systemctl --user stop till-%s.timer 2>/dev/null", name);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "systemctl --user disable till-%s.timer", name);
        system(cmd);
        
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), 
                    "%s/.config/systemd/user/till-%s.service", pw->pw_dir, name);
            unlink(path);
            snprintf(path, sizeof(path), 
                    "%s/.config/systemd/user/till-%s.timer", pw->pw_dir, name);
            unlink(path);
        }
        return 0;
    }
    
    /* Try system level */
    snprintf(cmd, sizeof(cmd), "sudo systemctl stop till-%s.timer 2>/dev/null", name);
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "sudo systemctl disable till-%s.timer", name);
        system(cmd);
        
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/etc/systemd/system/till-%s.service", name);
        snprintf(cmd, sizeof(cmd), "sudo rm '%s'", path);
        system(cmd);
        
        snprintf(path, sizeof(path), "/etc/systemd/system/till-%s.timer", name);
        snprintf(cmd, sizeof(cmd), "sudo rm '%s'", path);
        system(cmd);
        
        return 0;
    }
    
    return -1;
}
#endif

/* Install cron job (fallback) */
static int install_cron(const schedule_config_t *config) {
    char cron_line[PATH_MAX * 2];
    char cmd[PATH_MAX * 3];
    
    /* Build cron schedule */
    const char *schedule = "0 3 * * *";  /* Default: 3 AM daily */
    
    if (config->schedule) {
        if (strchr(config->schedule, ':')) {
            /* Convert HH:MM to cron format */
            int hour = 3, minute = 0;
            sscanf(config->schedule, "%d:%d", &hour, &minute);
            char cron_sched[32];
            snprintf(cron_sched, sizeof(cron_sched), "%d %d * * *", minute, hour);
            schedule = cron_sched;
        } else if (strchr(config->schedule, ' ')) {
            /* Assume it's already in cron format */
            schedule = config->schedule;
        }
    }
    
    /* Build cron line */
    if (config->log_file && config->error_file) {
        snprintf(cron_line, sizeof(cron_line), 
                "%s cd %s && %s >> %s 2>> %s",
                schedule,
                config->working_dir ? config->working_dir : "$HOME",
                config->command,
                config->log_file,
                config->error_file);
    } else if (config->log_file) {
        snprintf(cron_line, sizeof(cron_line), 
                "%s cd %s && %s >> %s 2>&1",
                schedule,
                config->working_dir ? config->working_dir : "$HOME",
                config->command,
                config->log_file);
    } else {
        snprintf(cron_line, sizeof(cron_line), 
                "%s cd %s && %s",
                schedule,
                config->working_dir ? config->working_dir : "$HOME",
                config->command);
    }
    
    /* Add to crontab */
    snprintf(cmd, sizeof(cmd), 
            "(crontab -l 2>/dev/null | grep -v 'till-%s'; echo '# till-%s'; echo '%s') | crontab -",
            config->name, config->name, cron_line);
    
    return system(cmd);
}

static int remove_cron(const char *name) {
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), 
            "crontab -l 2>/dev/null | grep -v 'till-%s' | crontab -",
            name);
    return system(cmd);
}

/* Public interface - Install scheduled job */
int platform_schedule_install(const schedule_config_t *config) {
    if (!config || !config->name || !config->command) {
        return -1;
    }
    
    scheduler_type_t scheduler = platform_get_scheduler();
    
    switch (scheduler) {
#if PLATFORM_MACOS
        case SCHEDULER_LAUNCHD:
            return install_launchd(config);
#endif
#if PLATFORM_LINUX
        case SCHEDULER_SYSTEMD:
            return install_systemd(config);
#endif
        case SCHEDULER_CRON:
            return install_cron(config);
        default:
            return -1;
    }
}

/* Remove scheduled job */
int platform_schedule_remove(const char *name) {
    if (!name) return -1;
    
    scheduler_type_t scheduler = platform_get_scheduler();
    
    switch (scheduler) {
#if PLATFORM_MACOS
        case SCHEDULER_LAUNCHD:
            return remove_launchd(name);
#endif
#if PLATFORM_LINUX
        case SCHEDULER_SYSTEMD:
            return remove_systemd(name);
#endif
        case SCHEDULER_CRON:
            return remove_cron(name);
        default:
            return -1;
    }
}

/* Check if job exists */
int platform_schedule_exists(const char *name) {
    if (!name) return 0;
    
    scheduler_type_t scheduler = platform_get_scheduler();
    char cmd[PATH_MAX];
    
    switch (scheduler) {
#if PLATFORM_MACOS
        case SCHEDULER_LAUNCHD: {
            struct passwd *pw = getpwuid(getuid());
            if (!pw) return 0;
            
            char plist_path[PATH_MAX];
            snprintf(plist_path, sizeof(plist_path), 
                    "%s/Library/LaunchAgents/com.till.%s.plist", pw->pw_dir, name);
            
            if (access(plist_path, F_OK) == 0) {
                return 1;
            }
            
            snprintf(plist_path, sizeof(plist_path), 
                    "/Library/LaunchDaemons/com.till.%s.plist", name);
            
            return access(plist_path, F_OK) == 0;
        }
#endif
#if PLATFORM_LINUX
        case SCHEDULER_SYSTEMD:
            snprintf(cmd, sizeof(cmd), 
                    "systemctl --user list-timers till-%s.timer --no-pager 2>/dev/null | grep -q till-%s",
                    name, name);
            if (system(cmd) == 0) return 1;
            
            snprintf(cmd, sizeof(cmd), 
                    "systemctl list-timers till-%s.timer --no-pager 2>/dev/null | grep -q till-%s",
                    name, name);
            return system(cmd) == 0;
#endif
        case SCHEDULER_CRON:
            snprintf(cmd, sizeof(cmd), 
                    "crontab -l 2>/dev/null | grep -q 'till-%s'", name);
            return system(cmd) == 0;
            
        default:
            return 0;
    }
}