/*
 * till_schedule.c - Scheduling management for Till
 * 
 * Manages automatic synchronization scheduling and watch daemon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>

#include "till_config.h"
#include "till_schedule.h"
#include "cJSON.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Get schedule file path */
static int get_schedule_path(char *path, size_t size) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    snprintf(path, size, "%s/.till/schedule.json", pw->pw_dir);
    return 0;
}

/* Load schedule configuration */
static cJSON *load_schedule(void) {
    char path[PATH_MAX];
    if (get_schedule_path(path, sizeof(path)) != 0) return NULL;
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* Create default schedule */
        cJSON *schedule = cJSON_CreateObject();
        
        cJSON *sync = cJSON_CreateObject();
        cJSON_AddBoolToObject(sync, "enabled", cJSON_False);
        cJSON_AddNumberToObject(sync, "interval_hours", 24);
        cJSON_AddStringToObject(sync, "daily_at", "03:00");
        cJSON_AddNullToObject(sync, "next_run");
        cJSON_AddNullToObject(sync, "last_run");
        cJSON_AddStringToObject(sync, "last_status", "none");
        cJSON_AddNumberToObject(sync, "consecutive_failures", 0);
        cJSON_AddArrayToObject(sync, "history");
        
        cJSON_AddItemToObject(schedule, "sync", sync);
        
        cJSON *watch = cJSON_CreateObject();
        cJSON_AddBoolToObject(watch, "enabled", cJSON_False);
        cJSON_AddNullToObject(watch, "pid");
        cJSON_AddNullToObject(watch, "started");
        cJSON_AddNullToObject(watch, "daemon_type");
        
        cJSON_AddItemToObject(schedule, "watch", watch);
        
        return schedule;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);
    
    cJSON *schedule = cJSON_Parse(content);
    free(content);
    
    return schedule;
}

/* Save schedule configuration */
static int save_schedule(cJSON *schedule) {
    char path[PATH_MAX];
    if (get_schedule_path(path, sizeof(path)) != 0) return -1;
    
    char *output = cJSON_Print(schedule);
    if (!output) return -1;
    
    FILE *fp = fopen(path, "w");
    if (!fp) {
        free(output);
        return -1;
    }
    
    fprintf(fp, "%s", output);
    fclose(fp);
    free(output);
    
    return 0;
}

/* Parse time in HH:MM format */
static int parse_time(const char *time_str, int *hour, int *minute) {
    if (!time_str || strlen(time_str) != 5 || time_str[2] != ':') {
        return -1;
    }
    
    *hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    *minute = (time_str[3] - '0') * 10 + (time_str[4] - '0');
    
    if (*hour < 0 || *hour > 23 || *minute < 0 || *minute > 59) {
        return -1;
    }
    
    return 0;
}

/* Calculate next run time */
static time_t calculate_next_run(int interval_hours, const char *daily_at) {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    
    if (daily_at && strlen(daily_at) == 5) {
        /* Daily at specific time */
        int hour, minute;
        if (parse_time(daily_at, &hour, &minute) == 0) {
            struct tm next_run = *tm_now;
            next_run.tm_hour = hour;
            next_run.tm_min = minute;
            next_run.tm_sec = 0;
            
            time_t next_time = mktime(&next_run);
            
            /* If time has passed today, schedule for tomorrow */
            if (next_time <= now) {
                next_run.tm_mday++;
                next_time = mktime(&next_run);
            }
            
            return next_time;
        }
    }
    
    /* Interval-based */
    return now + (interval_hours * 3600);
}

/* Format time for display */
static void format_time(time_t t, char *buffer, size_t size) {
    struct tm *tm_time = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_time);
}

/* Configure watch daemon */
int till_watch_configure(int argc, char *argv[]) {
    cJSON *schedule = load_schedule();
    if (!schedule) {
        fprintf(stderr, "Error: Failed to load schedule configuration\n");
        return -1;
    }
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    if (!sync) {
        sync = cJSON_CreateObject();
        cJSON_AddItemToObject(schedule, "sync", sync);
    }
    
    /* Parse arguments */
    if (argc == 0) {
        /* No arguments - show status */
        return till_watch_status();
    }
    
    /* Check for special flags */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--status") == 0) {
            cJSON_Delete(schedule);
            return till_watch_status();
        }
        else if (strcmp(argv[i], "--enable") == 0) {
            cJSON_ReplaceItemInObject(sync, "enabled", cJSON_CreateBool(1));
            printf("Automatic sync enabled\n");
        }
        else if (strcmp(argv[i], "--disable") == 0) {
            cJSON_ReplaceItemInObject(sync, "enabled", cJSON_CreateBool(0));
            printf("Automatic sync disabled\n");
        }
        else if (strcmp(argv[i], "--daily-at") == 0 && i + 1 < argc) {
            const char *time_str = argv[++i];
            int hour, minute;
            
            if (parse_time(time_str, &hour, &minute) != 0) {
                fprintf(stderr, "Error: Invalid time format. Use HH:MM (24-hour)\n");
                cJSON_Delete(schedule);
                return -1;
            }
            
            cJSON_ReplaceItemInObject(sync, "daily_at", cJSON_CreateString(time_str));
            cJSON_ReplaceItemInObject(sync, "enabled", cJSON_CreateBool(1));
            
            printf("Daily sync scheduled at %s\n", time_str);
        }
        else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
            /* Numeric argument - interval in hours */
            int hours = atoi(argv[i]);
            if (hours <= 0 || hours > 168) {  /* Max 1 week */
                fprintf(stderr, "Error: Invalid interval. Must be 1-168 hours\n");
                cJSON_Delete(schedule);
                return -1;
            }
            
            cJSON_ReplaceItemInObject(sync, "interval_hours", cJSON_CreateNumber(hours));
            cJSON_ReplaceItemInObject(sync, "enabled", cJSON_CreateBool(1));
            
            printf("Sync interval set to %d hours\n", hours);
        }
    }
    
    /* Calculate next run time */
    cJSON *enabled = cJSON_GetObjectItem(sync, "enabled");
    if (enabled && cJSON_IsTrue(enabled)) {
        cJSON *interval = cJSON_GetObjectItem(sync, "interval_hours");
        cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
        
        time_t next_run = calculate_next_run(
            interval ? interval->valueint : 24,
            daily_at ? daily_at->valuestring : NULL
        );
        
        char time_str[64];
        format_time(next_run, time_str, sizeof(time_str));
        
        cJSON_ReplaceItemInObject(sync, "next_run", cJSON_CreateString(time_str));
        
        printf("Next sync scheduled for: %s\n", time_str);
        
        /* Create platform-specific scheduler */
        if (till_watch_install_scheduler() == 0) {
            printf("Scheduler installed successfully\n");
        } else {
            printf("Warning: Could not install system scheduler\n");
            printf("You may need to run 'till sync' manually\n");
        }
    }
    
    /* Save schedule */
    if (save_schedule(schedule) != 0) {
        fprintf(stderr, "Error: Failed to save schedule\n");
        cJSON_Delete(schedule);
        return -1;
    }
    
    cJSON_Delete(schedule);
    return 0;
}

/* Show watch status */
int till_watch_status(void) {
    cJSON *schedule = load_schedule();
    if (!schedule) {
        printf("No schedule configured\n");
        return 0;
    }
    
    printf("Till Sync Schedule\n");
    printf("==================\n\n");
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    if (!sync) {
        printf("No sync configuration found\n");
        cJSON_Delete(schedule);
        return 0;
    }
    
    cJSON *enabled = cJSON_GetObjectItem(sync, "enabled");
    cJSON *interval = cJSON_GetObjectItem(sync, "interval_hours");
    cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
    cJSON *next_run = cJSON_GetObjectItem(sync, "next_run");
    cJSON *last_run = cJSON_GetObjectItem(sync, "last_run");
    cJSON *last_status = cJSON_GetObjectItem(sync, "last_status");
    cJSON *failures = cJSON_GetObjectItem(sync, "consecutive_failures");
    
    printf("Status: %s\n", 
        (enabled && cJSON_IsTrue(enabled)) ? "ENABLED" : "DISABLED");
    
    if (interval) {
        printf("Interval: %d hours\n", interval->valueint);
    }
    
    if (daily_at && daily_at->valuestring) {
        printf("Daily at: %s\n", daily_at->valuestring);
    }
    
    if (next_run && next_run->valuestring) {
        printf("Next run: %s\n", next_run->valuestring);
    }
    
    if (last_run && last_run->valuestring) {
        printf("Last run: %s\n", last_run->valuestring);
    }
    
    if (last_status && last_status->valuestring) {
        printf("Last status: %s\n", last_status->valuestring);
    }
    
    if (failures && failures->valueint > 0) {
        printf("Warning: %d consecutive failures\n", failures->valueint);
    }
    
    /* Show recent history */
    cJSON *history = cJSON_GetObjectItem(sync, "history");
    if (history && cJSON_GetArraySize(history) > 0) {
        printf("\nRecent History:\n");
        printf("---------------\n");
        
        int count = 0;
        cJSON *entry;
        cJSON_ArrayForEach(entry, history) {
            if (++count > 5) break;  /* Show last 5 */
            
            cJSON *timestamp = cJSON_GetObjectItem(entry, "timestamp");
            cJSON *status = cJSON_GetObjectItem(entry, "status");
            cJSON *duration = cJSON_GetObjectItem(entry, "duration_seconds");
            
            if (timestamp && status) {
                printf("%s - %s", 
                    timestamp->valuestring,
                    status->valuestring);
                
                if (duration) {
                    printf(" (%ds)", duration->valueint);
                }
                printf("\n");
            }
        }
    }
    
    printf("\nCommands:\n");
    printf("  till watch --enable          Enable automatic sync\n");
    printf("  till watch --disable         Disable automatic sync\n");
    printf("  till watch 24                Set interval to 24 hours\n");
    printf("  till watch --daily-at 03:00  Run daily at 3 AM\n");
    
    cJSON_Delete(schedule);
    return 0;
}

/* Install platform-specific scheduler */
int till_watch_install_scheduler(void) {
#ifdef __APPLE__
    return till_watch_install_launchd();
#elif defined(__linux__)
    return till_watch_install_systemd();
#else
    return till_watch_install_cron();
#endif
}

/* Install launchd plist (macOS) */
int till_watch_install_launchd(void) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    char plist_path[PATH_MAX];
    snprintf(plist_path, sizeof(plist_path), 
        "%s/Library/LaunchAgents/com.till.sync.plist", pw->pw_dir);
    
    /* Load schedule to get timing */
    cJSON *schedule = load_schedule();
    if (!schedule) return -1;
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
    
    int hour = 3, minute = 0;  /* Default 3 AM */
    if (daily_at && daily_at->valuestring) {
        parse_time(daily_at->valuestring, &hour, &minute);
    }
    
    cJSON_Delete(schedule);
    
    /* Find till executable */
    char till_path[PATH_MAX];
    if (access("/usr/local/bin/till", X_OK) == 0) {
        strcpy(till_path, "/usr/local/bin/till");
    } else {
        /* Use current location */
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return -1;
        snprintf(till_path, sizeof(till_path), "%s/till", cwd);
    }
    
    /* Write plist file */
    FILE *fp = fopen(plist_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" ");
    fprintf(fp, "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    fprintf(fp, "<plist version=\"1.0\">\n");
    fprintf(fp, "<dict>\n");
    fprintf(fp, "    <key>Label</key>\n");
    fprintf(fp, "    <string>com.till.sync</string>\n");
    fprintf(fp, "    <key>ProgramArguments</key>\n");
    fprintf(fp, "    <array>\n");
    fprintf(fp, "        <string>%s</string>\n", till_path);
    fprintf(fp, "        <string>sync</string>\n");
    fprintf(fp, "    </array>\n");
    fprintf(fp, "    <key>StartCalendarInterval</key>\n");
    fprintf(fp, "    <dict>\n");
    fprintf(fp, "        <key>Hour</key>\n");
    fprintf(fp, "        <integer>%d</integer>\n", hour);
    fprintf(fp, "        <key>Minute</key>\n");
    fprintf(fp, "        <integer>%d</integer>\n", minute);
    fprintf(fp, "    </dict>\n");
    fprintf(fp, "    <key>StandardOutPath</key>\n");
    fprintf(fp, "    <string>%s/.till/logs/sync.log</string>\n", pw->pw_dir);
    fprintf(fp, "    <key>StandardErrorPath</key>\n");
    fprintf(fp, "    <string>%s/.till/logs/sync.error.log</string>\n", pw->pw_dir);
    fprintf(fp, "</dict>\n");
    fprintf(fp, "</plist>\n");
    
    fclose(fp);
    
    /* Load the plist */
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "launchctl unload %s 2>/dev/null; launchctl load %s",
        plist_path, plist_path);
    
    system(cmd);
    
    return 0;
}

/* Install systemd timer (Linux) */
int till_watch_install_systemd(void) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    /* Check if systemd is available */
    if (system("systemctl --version > /dev/null 2>&1") != 0) {
        /* Fall back to cron */
        return till_watch_install_cron();
    }
    
    char service_dir[PATH_MAX];
    snprintf(service_dir, sizeof(service_dir), 
        "%s/.config/systemd/user", pw->pw_dir);
    
    /* Create directory if needed */
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", service_dir);
    system(cmd);
    
    /* Load schedule to get timing */
    cJSON *schedule = load_schedule();
    if (!schedule) return -1;
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
    
    const char *time_spec = "03:00";  /* Default */
    if (daily_at && daily_at->valuestring) {
        time_spec = daily_at->valuestring;
    }
    
    cJSON_Delete(schedule);
    
    /* Find till executable */
    char till_path[PATH_MAX];
    if (access("/usr/local/bin/till", X_OK) == 0) {
        strcpy(till_path, "/usr/local/bin/till");
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return -1;
        snprintf(till_path, sizeof(till_path), "%s/till", cwd);
    }
    
    /* Write service file */
    char service_path[PATH_MAX];
    snprintf(service_path, sizeof(service_path), 
        "%s/till-sync.service", service_dir);
    
    FILE *fp = fopen(service_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "[Unit]\n");
    fprintf(fp, "Description=Till Sync Service\n\n");
    fprintf(fp, "[Service]\n");
    fprintf(fp, "Type=oneshot\n");
    fprintf(fp, "ExecStart=%s sync\n", till_path);
    fprintf(fp, "StandardOutput=append:%s/.till/logs/sync.log\n", pw->pw_dir);
    fprintf(fp, "StandardError=append:%s/.till/logs/sync.error.log\n", pw->pw_dir);
    
    fclose(fp);
    
    /* Write timer file */
    char timer_path[PATH_MAX];
    snprintf(timer_path, sizeof(timer_path), 
        "%s/till-sync.timer", service_dir);
    
    fp = fopen(timer_path, "w");
    if (!fp) return -1;
    
    fprintf(fp, "[Unit]\n");
    fprintf(fp, "Description=Till Sync Timer\n");
    fprintf(fp, "Requires=till-sync.service\n\n");
    fprintf(fp, "[Timer]\n");
    fprintf(fp, "OnCalendar=*-*-* %s:00\n", time_spec);
    fprintf(fp, "Persistent=true\n\n");
    fprintf(fp, "[Install]\n");
    fprintf(fp, "WantedBy=timers.target\n");
    
    fclose(fp);
    
    /* Enable and start timer */
    system("systemctl --user daemon-reload");
    system("systemctl --user enable till-sync.timer");
    system("systemctl --user start till-sync.timer");
    
    return 0;
}

/* Install cron job (fallback) */
int till_watch_install_cron(void) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    /* Load schedule to get timing */
    cJSON *schedule = load_schedule();
    if (!schedule) return -1;
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
    
    int hour = 3, minute = 0;  /* Default 3 AM */
    if (daily_at && daily_at->valuestring) {
        parse_time(daily_at->valuestring, &hour, &minute);
    }
    
    cJSON_Delete(schedule);
    
    /* Find till executable */
    char till_path[PATH_MAX];
    if (access("/usr/local/bin/till", X_OK) == 0) {
        strcpy(till_path, "/usr/local/bin/till");
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return -1;
        snprintf(till_path, sizeof(till_path), "%s/till", cwd);
    }
    
    /* Create cron entry */
    char cron_entry[PATH_MAX * 2];
    snprintf(cron_entry, sizeof(cron_entry),
        "%d %d * * * %s sync >> %s/.till/logs/cron.log 2>&1",
        minute, hour, till_path, pw->pw_dir);
    
    /* Add to crontab */
    char cmd[PATH_MAX * 3];
    snprintf(cmd, sizeof(cmd),
        "(crontab -l 2>/dev/null | grep -v 'till sync'; echo '%s') | crontab -",
        cron_entry);
    
    if (system(cmd) != 0) {
        fprintf(stderr, "Warning: Could not install cron job\n");
        return -1;
    }
    
    printf("Cron job installed: %d:%02d daily\n", hour, minute);
    return 0;
}

/* Record sync result */
int till_watch_record_sync(int success, int duration_seconds, int installations, int hosts) {
    cJSON *schedule = load_schedule();
    if (!schedule) return -1;
    
    cJSON *sync = cJSON_GetObjectItem(schedule, "sync");
    if (!sync) {
        cJSON_Delete(schedule);
        return -1;
    }
    
    /* Update last run time and status */
    time_t now = time(NULL);
    char time_str[64];
    format_time(now, time_str, sizeof(time_str));
    
    cJSON_ReplaceItemInObject(sync, "last_run", cJSON_CreateString(time_str));
    cJSON_ReplaceItemInObject(sync, "last_status", 
        cJSON_CreateString(success ? "success" : "failure"));
    
    /* Update consecutive failures */
    cJSON *failures = cJSON_GetObjectItem(sync, "consecutive_failures");
    if (success) {
        cJSON_ReplaceItemInObject(sync, "consecutive_failures", cJSON_CreateNumber(0));
    } else if (failures) {
        cJSON_ReplaceItemInObject(sync, "consecutive_failures", 
            cJSON_CreateNumber(failures->valueint + 1));
    }
    
    /* Calculate next run */
    cJSON *interval = cJSON_GetObjectItem(sync, "interval_hours");
    cJSON *daily_at = cJSON_GetObjectItem(sync, "daily_at");
    
    time_t next_run = calculate_next_run(
        interval ? interval->valueint : 24,
        daily_at ? daily_at->valuestring : NULL
    );
    
    format_time(next_run, time_str, sizeof(time_str));
    cJSON_ReplaceItemInObject(sync, "next_run", cJSON_CreateString(time_str));
    
    /* Add to history */
    cJSON *history = cJSON_GetObjectItem(sync, "history");
    if (!history) {
        history = cJSON_CreateArray();
        cJSON_AddItemToObject(sync, "history", history);
    }
    
    cJSON *entry = cJSON_CreateObject();
    format_time(now, time_str, sizeof(time_str));
    cJSON_AddStringToObject(entry, "timestamp", time_str);
    cJSON_AddStringToObject(entry, "status", success ? "success" : "failure");
    cJSON_AddNumberToObject(entry, "duration_seconds", duration_seconds);
    cJSON_AddNumberToObject(entry, "installations_synced", installations);
    cJSON_AddNumberToObject(entry, "hosts_synced", hosts);
    
    /* Add to beginning of array */
    cJSON_AddItemToArray(history, entry);
    
    /* Keep only last 10 entries */
    while (cJSON_GetArraySize(history) > 10) {
        cJSON_DeleteItemFromArray(history, 10);
    }
    
    save_schedule(schedule);
    cJSON_Delete(schedule);
    
    return 0;
}