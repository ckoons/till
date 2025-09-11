/*
 * till_hold.c - Hold/Release implementation
 * 
 * Implements component hold management to prevent unwanted updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>

#include "till_config.h"
#include "till_hold.h"
#include "till_common.h"
#include "till_registry.h"
#include "cJSON.h"

/* Load holds from registry */
cJSON *load_holds(void) {
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        return NULL;
    }
    
    cJSON *holds = cJSON_GetObjectItem(registry, "holds");
    if (!holds) {
        holds = cJSON_CreateObject();
        cJSON_AddItemToObject(registry, "holds", holds);
        save_till_json("tekton/till-private.json", registry);
    }
    
    /* Detach holds from registry before deleting registry */
    cJSON_DetachItemFromObject(registry, "holds");
    cJSON_Delete(registry);
    
    return holds;
}

/* Save holds to registry */
int save_holds(cJSON *holds) {
    if (!holds) return -1;
    
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        return -1;
    }
    
    /* Remove old holds if exists */
    cJSON_DeleteItemFromObject(registry, "holds");
    
    /* Add new holds */
    cJSON_AddItemToObject(registry, "holds", cJSON_Duplicate(holds, 1));
    
    /* Save registry */
    int result = save_till_json("tekton/till-private.json", registry);
    cJSON_Delete(registry);
    
    return result;
}

/* Format time for display */
void format_time(time_t t, char *buf, size_t size) {
    if (t == 0) {
        strncpy(buf, "Never", size - 1);
        buf[size - 1] = '\0';
    } else {
        struct tm *tm = localtime(&t);
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
    }
}

/* Format duration for display */
void format_duration(time_t seconds, char *buf, size_t size) {
    if (seconds < 60) {
        snprintf(buf, size, "%ld seconds", seconds);
    } else if (seconds < 3600) {
        snprintf(buf, size, "%ld minutes", seconds / 60);
    } else if (seconds < 86400) {
        snprintf(buf, size, "%.1f hours", seconds / 3600.0);
    } else if (seconds < 604800) {
        snprintf(buf, size, "%.1f days", seconds / 86400.0);
    } else {
        snprintf(buf, size, "%.1f weeks", seconds / 604800.0);
    }
}

/* Parse time specification */
int parse_time_spec(const char *spec, time_t *time_out) {
    if (!spec || !time_out) return -1;
    
    /* Handle special cases */
    if (strcasecmp(spec, "now") == 0) {
        *time_out = time(NULL);
        return 0;
    }
    
    if (strcasecmp(spec, "never") == 0 || strcasecmp(spec, "indefinite") == 0) {
        *time_out = 0;
        return 0;
    }
    
    /* Try to parse ISO format: YYYY-MM-DD HH:MM:SS */
    struct tm tm = {0};
    if (strptime(spec, "%Y-%m-%d %H:%M:%S", &tm) ||
        strptime(spec, "%Y-%m-%d %H:%M", &tm) ||
        strptime(spec, "%Y-%m-%d", &tm)) {
        *time_out = mktime(&tm);
        return 0;
    }
    
    /* Try relative time: +1h, +2d, +1w, etc */
    if (spec[0] == '+') {
        time_t duration;
        if (parse_duration(spec + 1, &duration) == 0) {
            *time_out = time(NULL) + duration;
            return 0;
        }
    }
    
    return -1;
}

/* Parse duration specification */
int parse_duration(const char *duration, time_t *seconds_out) {
    if (!duration || !seconds_out) return -1;
    
    char *endptr;
    long value = strtol(duration, &endptr, 10);
    if (value <= 0 || endptr == duration) {
        return -1;
    }
    
    /* Parse unit */
    while (*endptr && isspace(*endptr)) endptr++;
    
    if (*endptr == '\0' || strcasecmp(endptr, "s") == 0 || 
        strcasecmp(endptr, "sec") == 0 || strcasecmp(endptr, "second") == 0 ||
        strcasecmp(endptr, "seconds") == 0) {
        *seconds_out = value;
    }
    else if (strcasecmp(endptr, "m") == 0 || strcasecmp(endptr, "min") == 0 ||
             strcasecmp(endptr, "minute") == 0 || strcasecmp(endptr, "minutes") == 0) {
        *seconds_out = value * 60;
    }
    else if (strcasecmp(endptr, "h") == 0 || strcasecmp(endptr, "hr") == 0 ||
             strcasecmp(endptr, "hour") == 0 || strcasecmp(endptr, "hours") == 0) {
        *seconds_out = value * 3600;
    }
    else if (strcasecmp(endptr, "d") == 0 || strcasecmp(endptr, "day") == 0 ||
             strcasecmp(endptr, "days") == 0) {
        *seconds_out = value * 86400;
    }
    else if (strcasecmp(endptr, "w") == 0 || strcasecmp(endptr, "week") == 0 ||
             strcasecmp(endptr, "weeks") == 0) {
        *seconds_out = value * 604800;
    }
    else if (strcasecmp(endptr, "month") == 0 || strcasecmp(endptr, "months") == 0) {
        *seconds_out = value * 2592000; /* Approximate: 30 days */
    }
    else {
        return -1;
    }
    
    return 0;
}

/* Check if component is held */
int is_component_held(const char *component) {
    if (!component) return 0;
    
    cJSON *holds = load_holds();
    if (!holds) return 0;
    
    cJSON *hold = cJSON_GetObjectItem(holds, component);
    if (!hold) {
        cJSON_Delete(holds);
        return 0;
    }
    
    /* Check if hold has expired */
    cJSON *expires = cJSON_GetObjectItem(hold, "expires_at");
    if (expires && expires->valueint > 0) {
        time_t now = time(NULL);
        if (now > expires->valueint) {
            /* Hold has expired, remove it */
            cJSON_DeleteItemFromObject(holds, component);
            save_holds(holds);
            cJSON_Delete(holds);
            return 0;
        }
    }
    
    cJSON_Delete(holds);
    return 1;
}

/* Get hold information for component */
int get_hold_info(const char *component, hold_info_t *info) {
    if (!component || !info) return -1;
    
    memset(info, 0, sizeof(hold_info_t));
    strncpy(info->component, component, sizeof(info->component) - 1);
    
    cJSON *holds = load_holds();
    if (!holds) return -1;
    
    cJSON *hold = cJSON_GetObjectItem(holds, component);
    if (!hold) {
        cJSON_Delete(holds);
        return -1;
    }
    
    /* Extract hold information */
    cJSON *item = cJSON_GetObjectItem(hold, "held_at");
    if (item) info->held_at = item->valueint;
    
    item = cJSON_GetObjectItem(hold, "expires_at");
    if (item) info->expires_at = item->valueint;
    
    item = cJSON_GetObjectItem(hold, "reason");
    if (item && item->valuestring) {
        strncpy(info->reason, item->valuestring, sizeof(info->reason) - 1);
    }
    
    item = cJSON_GetObjectItem(hold, "held_by");
    if (item && item->valuestring) {
        strncpy(info->held_by, item->valuestring, sizeof(info->held_by) - 1);
    }
    
    cJSON_Delete(holds);
    return 0;
}

/* List all current holds */
int list_holds(hold_info_t **holds_out, int *count) {
    if (!holds_out || !count) return -1;
    
    *holds_out = NULL;
    *count = 0;
    
    cJSON *holds = load_holds();
    if (!holds) return 0;
    
    int num_holds = cJSON_GetArraySize(holds);
    if (num_holds == 0) {
        cJSON_Delete(holds);
        return 0;
    }
    
    hold_info_t *list = calloc(num_holds, sizeof(hold_info_t));
    if (!list) {
        cJSON_Delete(holds);
        return -1;
    }
    
    int index = 0;
    cJSON *hold = NULL;
    cJSON_ArrayForEach(hold, holds) {
        if (index >= num_holds) break;
        
        hold_info_t *info = &list[index];
        strncpy(info->component, hold->string, sizeof(info->component) - 1);
        
        cJSON *item = cJSON_GetObjectItem(hold, "held_at");
        if (item) info->held_at = item->valueint;
        
        item = cJSON_GetObjectItem(hold, "expires_at");
        if (item) info->expires_at = item->valueint;
        
        item = cJSON_GetObjectItem(hold, "reason");
        if (item && item->valuestring) {
            strncpy(info->reason, item->valuestring, sizeof(info->reason) - 1);
        }
        
        item = cJSON_GetObjectItem(hold, "held_by");
        if (item && item->valuestring) {
            strncpy(info->held_by, item->valuestring, sizeof(info->held_by) - 1);
        }
        
        index++;
    }
    
    *holds_out = list;
    *count = index;
    
    cJSON_Delete(holds);
    return 0;
}

/* Add a hold for a component */
int add_hold(const char *component, const char *reason, time_t expires_at) {
    if (!component) return -1;
    
    cJSON *holds = load_holds();
    if (!holds) {
        holds = cJSON_CreateObject();
    }
    
    /* Check if already held */
    cJSON *existing = cJSON_GetObjectItem(holds, component);
    if (existing) {
        till_warn("Component '%s' is already held", component);
        cJSON_Delete(holds);
        return -1;
    }
    
    /* Create hold entry */
    cJSON *hold = cJSON_CreateObject();
    cJSON_AddNumberToObject(hold, "held_at", time(NULL));
    cJSON_AddNumberToObject(hold, "expires_at", expires_at);
    
    if (reason && strlen(reason) > 0) {
        cJSON_AddStringToObject(hold, "reason", reason);
    }
    
    /* Get current user */
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        cJSON_AddStringToObject(hold, "held_by", pw->pw_name);
    }
    
    /* Add to holds */
    cJSON_AddItemToObject(holds, component, hold);
    
    /* Save */
    int result = save_holds(holds);
    cJSON_Delete(holds);
    
    if (result == 0) {
        char time_buf[64];
        if (expires_at > 0) {
            format_time(expires_at, time_buf, sizeof(time_buf));
            till_info("Hold placed on '%s' until %s", component, time_buf);
        } else {
            till_info("Hold placed on '%s' indefinitely", component);
        }
    }
    
    return result;
}

/* Remove a hold for a component */
int remove_hold(const char *component) {
    if (!component) return -1;
    
    cJSON *holds = load_holds();
    if (!holds) {
        till_warn("No holds found");
        return -1;
    }
    
    cJSON *hold = cJSON_GetObjectItem(holds, component);
    if (!hold) {
        till_warn("Component '%s' is not held", component);
        cJSON_Delete(holds);
        return -1;
    }
    
    /* Remove hold */
    cJSON_DeleteItemFromObject(holds, component);
    
    /* Save */
    int result = save_holds(holds);
    cJSON_Delete(holds);
    
    if (result == 0) {
        till_info("Hold released for '%s'", component);
    }
    
    return result;
}

/* Check and remove expired holds */
int cleanup_expired_holds(void) {
    cJSON *holds = load_holds();
    if (!holds) return 0;
    
    time_t now = time(NULL);
    int removed = 0;
    
    cJSON *hold = holds->child;
    while (hold) {
        cJSON *next = hold->next;
        
        cJSON *expires = cJSON_GetObjectItem(hold, "expires_at");
        if (expires && expires->valueint > 0 && expires->valueint < now) {
            till_info("Removing expired hold for '%s'", hold->string);
            cJSON_DeleteItemFromObject(holds, hold->string);
            removed++;
        }
        
        hold = next;
    }
    
    if (removed > 0) {
        save_holds(holds);
    }
    
    cJSON_Delete(holds);
    return removed;
}

/* Show hold status for all components */
void show_hold_status(void) {
    hold_info_t *holds = NULL;
    int count = 0;
    
    if (list_holds(&holds, &count) != 0) {
        till_error("Failed to list holds");
        return;
    }
    
    if (count == 0) {
        printf("No components are currently held.\n");
        return;
    }
    
    printf("\nCurrent Holds:\n");
    printf("==============\n\n");
    
    time_t now = time(NULL);
    
    for (int i = 0; i < count; i++) {
        hold_info_t *h = &holds[i];
        char held_time[64], expire_time[64];
        
        format_time(h->held_at, held_time, sizeof(held_time));
        format_time(h->expires_at, expire_time, sizeof(expire_time));
        
        printf("%d. %s\n", i + 1, h->component);
        printf("   Held by: %s\n", h->held_by[0] ? h->held_by : "Unknown");
        printf("   Since: %s\n", held_time);
        printf("   Expires: %s", expire_time);
        
        if (h->expires_at > 0 && h->expires_at > now) {
            char duration[64];
            format_duration(h->expires_at - now, duration, sizeof(duration));
            printf(" (in %s)", duration);
        } else if (h->expires_at > 0) {
            printf(" [EXPIRED]");
        }
        printf("\n");
        
        if (h->reason[0]) {
            printf("   Reason: %s\n", h->reason);
        }
        printf("\n");
    }
    
    free(holds);
}

/* Interactive hold selection */
int hold_interactive(void) {
    /* Get list of all components */
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        till_error("Cannot load registry");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations || cJSON_GetArraySize(installations) == 0) {
        till_warn("No components installed");
        cJSON_Delete(registry);
        return -1;
    }
    
    printf("\nInteractive Hold Mode\n");
    printf("=====================\n\n");
    
    /* List components */
    printf("Select components to hold:\n");
    int index = 1;
    cJSON *inst = NULL;
    cJSON_ArrayForEach(inst, installations) {
        const char *name = inst->string;
        if (is_component_held(name)) {
            printf("  %d. %s [ALREADY HELD]\n", index++, name);
        } else {
            printf("  %d. %s\n", index++, name);
        }
    }
    printf("  %d. All components\n", index);
    printf("  0. Cancel\n");
    
    printf("\nEnter selection (comma-separated for multiple): ");
    char selection[256];
    if (!fgets(selection, sizeof(selection), stdin)) {
        cJSON_Delete(registry);
        return -1;
    }
    
    /* Parse selection */
    char components[2048] = "";
    char *token = strtok(selection, ", \n");
    int select_all = 0;
    
    while (token) {
        int num = atoi(token);
        if (num == 0) {
            printf("Cancelled\n");
            cJSON_Delete(registry);
            return -1;
        }
        
        if (num == index) {
            select_all = 1;
            break;
        }
        
        /* Find component by index */
        int current = 1;
        cJSON_ArrayForEach(inst, installations) {
            if (current == num) {
                if (strlen(components) > 0) {
                    strncat(components, ",", sizeof(components) - strlen(components) - 1);
                }
                strncat(components, inst->string, sizeof(components) - strlen(components) - 1);
                break;
            }
            current++;
        }
        
        token = strtok(NULL, ", \n");
    }
    
    /* Get all components if selected */
    if (select_all) {
        components[0] = '\0';
        cJSON_ArrayForEach(inst, installations) {
            if (strlen(components) > 0) {
                strncat(components, ",", sizeof(components) - strlen(components) - 1);
            }
            strncat(components, inst->string, sizeof(components) - strlen(components) - 1);
        }
    }
    
    cJSON_Delete(registry);
    
    if (strlen(components) == 0) {
        till_warn("No components selected");
        return -1;
    }
    
    /* Get hold duration */
    printf("\nHold duration:\n");
    printf("  1. Indefinite\n");
    printf("  2. Until specific date/time\n");
    printf("  3. For a duration\n");
    printf("  0. Cancel\n");
    printf("\nChoice: ");
    
    char choice[10];
    if (!fgets(choice, sizeof(choice), stdin)) {
        return -1;
    }
    
    time_t expires_at = 0;
    
    switch (choice[0]) {
        case '1':
            expires_at = 0;  /* Indefinite */
            break;
            
        case '2': {
            printf("Enter date/time (YYYY-MM-DD HH:MM): ");
            char datetime[64];
            if (!fgets(datetime, sizeof(datetime), stdin)) {
                return -1;
            }
            /* Remove newline */
            datetime[strcspn(datetime, "\n")] = '\0';
            
            if (parse_time_spec(datetime, &expires_at) != 0) {
                till_error("Invalid date/time format");
                return -1;
            }
            break;
        }
        
        case '3': {
            printf("Enter duration (e.g., 1h, 2d, 1w): ");
            char duration[32];
            if (!fgets(duration, sizeof(duration), stdin)) {
                return -1;
            }
            /* Remove newline */
            duration[strcspn(duration, "\n")] = '\0';
            
            time_t seconds;
            if (parse_duration(duration, &seconds) != 0) {
                till_error("Invalid duration format");
                return -1;
            }
            expires_at = time(NULL) + seconds;
            break;
        }
        
        default:
            printf("Cancelled\n");
            return -1;
    }
    
    /* Get reason */
    printf("\nReason for hold (optional): ");
    char reason[1024];
    if (!fgets(reason, sizeof(reason), stdin)) {
        reason[0] = '\0';
    }
    /* Remove newline */
    reason[strcspn(reason, "\n")] = '\0';
    
    /* Confirm */
    printf("\nConfirm hold? [Y/n]: ");
    char confirm[10];
    if (!fgets(confirm, sizeof(confirm), stdin)) {
        return -1;
    }
    
    if (confirm[0] == 'n' || confirm[0] == 'N') {
        printf("Cancelled\n");
        return -1;
    }
    
    /* Apply holds */
    int success = 0, failed = 0;
    token = strtok(components, ",");
    
    while (token) {
        /* Trim whitespace */
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';
        
        if (add_hold(token, reason, expires_at) == 0) {
            success++;
        } else {
            failed++;
        }
        
        token = strtok(NULL, ",");
    }
    
    printf("\nHold Summary:\n");
    printf("  Successful: %d\n", success);
    if (failed > 0) {
        printf("  Failed: %d\n", failed);
    }
    
    return failed > 0 ? -1 : 0;
}

/* Interactive release selection */
int release_interactive(void) {
    hold_info_t *holds = NULL;
    int count = 0;
    
    if (list_holds(&holds, &count) != 0) {
        till_error("Failed to list holds");
        return -1;
    }
    
    if (count == 0) {
        printf("No components are currently held.\n");
        return 0;
    }
    
    printf("\nInteractive Release Mode\n");
    printf("========================\n\n");
    
    /* Show current holds */
    printf("Current holds:\n");
    time_t now = time(NULL);
    
    for (int i = 0; i < count; i++) {
        hold_info_t *h = &holds[i];
        char expire_time[64];
        format_time(h->expires_at, expire_time, sizeof(expire_time));
        
        printf("  %d. %s\n", i + 1, h->component);
        printf("     Held by: %s\n", h->held_by[0] ? h->held_by : "Unknown");
        printf("     Expires: %s", expire_time);
        
        if (h->expires_at > 0 && h->expires_at <= now) {
            printf(" [EXPIRED]");
        }
        printf("\n");
        
        if (h->reason[0]) {
            printf("     Reason: %s\n", h->reason);
        }
        printf("\n");
    }
    
    printf("  %d. Release all expired holds\n", count + 1);
    printf("  %d. Release all holds\n", count + 2);
    printf("  0. Cancel\n");
    
    printf("\nSelect holds to release (comma-separated for multiple): ");
    char selection[256];
    if (!fgets(selection, sizeof(selection), stdin)) {
        free(holds);
        return -1;
    }
    
    /* Parse selection */
    char *token = strtok(selection, ", \n");
    int release_all = 0;
    int release_expired = 0;
    int selected[count];
    memset(selected, 0, sizeof(selected));
    
    while (token) {
        int num = atoi(token);
        if (num == 0) {
            printf("Cancelled\n");
            free(holds);
            return -1;
        }
        
        if (num == count + 1) {
            release_expired = 1;
        } else if (num == count + 2) {
            release_all = 1;
            break;
        } else if (num > 0 && num <= count) {
            selected[num - 1] = 1;
        }
        
        token = strtok(NULL, ", \n");
    }
    
    /* Confirm */
    printf("\nConfirm release? [Y/n]: ");
    char confirm[10];
    if (!fgets(confirm, sizeof(confirm), stdin)) {
        free(holds);
        return -1;
    }
    
    if (confirm[0] == 'n' || confirm[0] == 'N') {
        printf("Cancelled\n");
        free(holds);
        return -1;
    }
    
    /* Release holds */
    int success = 0, failed = 0;
    
    for (int i = 0; i < count; i++) {
        int should_release = 0;
        
        if (release_all) {
            should_release = 1;
        } else if (release_expired && holds[i].expires_at > 0 && holds[i].expires_at <= now) {
            should_release = 1;
        } else if (selected[i]) {
            should_release = 1;
        }
        
        if (should_release) {
            if (remove_hold(holds[i].component) == 0) {
                success++;
            } else {
                failed++;
            }
        }
    }
    
    free(holds);
    
    printf("\nRelease Summary:\n");
    printf("  Released: %d\n", success);
    if (failed > 0) {
        printf("  Failed: %d\n", failed);
    }
    
    return failed > 0 ? -1 : 0;
}

/* Main hold command */
int till_hold_command(int argc, char *argv[]) {
    hold_options_t opts = {0};
    
    /* Parse arguments - skip argv[0] which is the command name */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            opts.interactive = 1;
        }
        else if (strcmp(argv[i], "--all") == 0) {
            opts.all_components = 1;
        }
        else if (strcmp(argv[i], "--force") == 0) {
            opts.force = 1;
        }
        else if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            strncpy(opts.from_time, argv[++i], sizeof(opts.from_time) - 1);
        }
        else if (strcmp(argv[i], "--until") == 0 && i + 1 < argc) {
            strncpy(opts.until_time, argv[++i], sizeof(opts.until_time) - 1);
        }
        else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            strncpy(opts.duration, argv[++i], sizeof(opts.duration) - 1);
        }
        else if (strcmp(argv[i], "--reason") == 0 && i + 1 < argc) {
            strncpy(opts.reason, argv[++i], sizeof(opts.reason) - 1);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Till Hold - Prevent component updates\n\n");
            printf("Usage: till hold [component(s)] [options]\n\n");
            printf("Options:\n");
            printf("  -i, --interactive    Interactive mode\n");
            printf("  --all                Hold all components\n");
            printf("  --until <time>       Hold until time (YYYY-MM-DD HH:MM)\n");
            printf("  --duration <period>  Hold for duration (e.g., 7d, 2w)\n");
            printf("  --reason <text>      Reason for hold\n");
            printf("  --force              Override existing holds\n");
            printf("  --help, -h           Show this help\n\n");
            printf("Examples:\n");
            printf("  till hold primary.tekton --duration 1w --reason \"Testing\"\n");
            printf("  till hold --all --until \"2024-01-15 00:00\"\n");
            printf("  till hold -i          # Interactive mode\n");
            return 0;
        }
        else if (argv[i][0] != '-') {
            /* Component name(s) */
            if (strlen(opts.components) > 0) {
                strncat(opts.components, ",", sizeof(opts.components) - strlen(opts.components) - 1);
            }
            strncat(opts.components, argv[i], sizeof(opts.components) - strlen(opts.components) - 1);
        }
    }
    
    /* Interactive mode */
    if (opts.interactive) {
        return hold_interactive();
    }
    
    /* Check if we have components */
    if (!opts.all_components && strlen(opts.components) == 0) {
        show_hold_status();
        return 0;
    }
    
    /* Get all components if --all */
    if (opts.all_components) {
        cJSON *registry = load_or_create_registry();
        if (registry) {
            cJSON *installations = cJSON_GetObjectItem(registry, "installations");
            if (installations) {
                opts.components[0] = '\0';
                cJSON *inst = NULL;
                cJSON_ArrayForEach(inst, installations) {
                    if (strlen(opts.components) > 0) {
                        strncat(opts.components, ",", sizeof(opts.components) - strlen(opts.components) - 1);
                    }
                    strncat(opts.components, inst->string, sizeof(opts.components) - strlen(opts.components) - 1);
                }
            }
            cJSON_Delete(registry);
        }
    }
    
    /* Parse expiration time */
    time_t expires_at = 0;
    if (strlen(opts.until_time) > 0) {
        if (parse_time_spec(opts.until_time, &expires_at) != 0) {
            till_error("Invalid time specification: %s", opts.until_time);
            return -1;
        }
    } else if (strlen(opts.duration) > 0) {
        time_t seconds;
        if (parse_duration(opts.duration, &seconds) != 0) {
            till_error("Invalid duration: %s", opts.duration);
            return -1;
        }
        expires_at = time(NULL) + seconds;
    }
    
    /* Apply holds */
    int success = 0, failed = 0;
    char *token = strtok(opts.components, ",");
    
    while (token) {
        /* Trim whitespace */
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';
        
        /* Check if already held */
        if (!opts.force && is_component_held(token)) {
            till_warn("Component '%s' is already held (use --force to override)", token);
            failed++;
        } else {
            if (opts.force) {
                remove_hold(token);  /* Remove existing hold */
            }
            if (add_hold(token, opts.reason, expires_at) == 0) {
                success++;
            } else {
                failed++;
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    if (success > 0) {
        printf("\nSuccessfully held %d component(s)\n", success);
    }
    if (failed > 0) {
        printf("Failed to hold %d component(s)\n", failed);
    }
    
    return failed > 0 ? -1 : 0;
}

/* Main release command */
int till_release_command(int argc, char *argv[]) {
    release_options_t opts = {0};
    
    /* Parse arguments - skip argv[0] which is the command name */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            opts.interactive = 1;
        }
        else if (strcmp(argv[i], "--all") == 0) {
            opts.all_components = 1;
        }
        else if (strcmp(argv[i], "--expired") == 0) {
            opts.expired_only = 1;
        }
        else if (strcmp(argv[i], "--force") == 0) {
            opts.force = 1;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Till Release - Allow component updates\n\n");
            printf("Usage: till release [component(s)] [options]\n\n");
            printf("Options:\n");
            printf("  -i, --interactive    Interactive mode\n");
            printf("  --all                Release all holds\n");
            printf("  --expired            Release only expired holds\n");
            printf("  --force              Force release even if not expired\n");
            printf("  --help, -h           Show this help\n\n");
            printf("Examples:\n");
            printf("  till release primary.tekton\n");
            printf("  till release --expired\n");
            printf("  till release -i       # Interactive mode\n");
            return 0;
        }
        else if (argv[i][0] != '-') {
            /* Component name(s) */
            if (strlen(opts.components) > 0) {
                strncat(opts.components, ",", sizeof(opts.components) - strlen(opts.components) - 1);
            }
            strncat(opts.components, argv[i], sizeof(opts.components) - strlen(opts.components) - 1);
        }
    }
    
    /* Interactive mode */
    if (opts.interactive) {
        return release_interactive();
    }
    
    /* Release expired holds */
    if (opts.expired_only) {
        int removed = cleanup_expired_holds();
        if (removed > 0) {
            printf("Released %d expired hold(s)\n", removed);
        } else {
            printf("No expired holds found\n");
        }
        return 0;
    }
    
    /* Check if we have components */
    if (!opts.all_components && strlen(opts.components) == 0) {
        show_hold_status();
        return 0;
    }
    
    /* Get all components if --all */
    if (opts.all_components) {
        hold_info_t *holds = NULL;
        int count = 0;
        
        if (list_holds(&holds, &count) == 0 && count > 0) {
            opts.components[0] = '\0';
            for (int i = 0; i < count; i++) {
                if (strlen(opts.components) > 0) {
                    strncat(opts.components, ",", sizeof(opts.components) - strlen(opts.components) - 1);
                }
                strncat(opts.components, holds[i].component, 
                       sizeof(opts.components) - strlen(opts.components) - 1);
            }
            free(holds);
        }
    }
    
    /* Release holds */
    int success = 0, failed = 0;
    char *token = strtok(opts.components, ",");
    
    while (token) {
        /* Trim whitespace */
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';
        
        if (remove_hold(token) == 0) {
            success++;
        } else {
            failed++;
        }
        
        token = strtok(NULL, ",");
    }
    
    if (success > 0) {
        printf("\nSuccessfully released %d hold(s)\n", success);
    }
    if (failed > 0) {
        printf("Failed to release %d hold(s)\n", failed);
    }
    
    return failed > 0 ? -1 : 0;
}