/*
 * till_menu.c - Menu management for till federation
 *
 * Handles adding and removing components from the menu of the day
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "till_config.h"
#include "till_menu.h"
#include "till_common.h"
#include "till_validate.h"
#include "till_security.h"
#include "cJSON.h"

#define MENU_FILE "menu_of_the_day.json"
#define MENU_BACKUP "menu_of_the_day.backup.json"

/* Load the current menu from file */
static cJSON* load_menu(const char *menu_path) {
    FILE *fp = fopen(menu_path, "r");
    if (!fp) {
        /* Create empty menu if doesn't exist */
        cJSON *menu = cJSON_CreateObject();
        cJSON_AddStringToObject(menu, "version", "1.0.0");
        cJSON_AddStringToObject(menu, "date", "");
        cJSON_AddObjectToObject(menu, "containers");
        return menu;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, fp);
    content[read_size] = '\0';
    fclose(fp);

    cJSON *menu = cJSON_Parse(content);
    free(content);

    if (!menu) {
        till_error("Failed to parse menu file: %s", cJSON_GetErrorPtr());
        return NULL;
    }

    return menu;
}

/* Save menu to file */
static int save_menu(const char *menu_path, cJSON *menu) {
    /* Update date */
    time_t now = time(NULL);
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", localtime(&now));
    cJSON_SetValuestring(cJSON_GetObjectItem(menu, "date"), date_str);

    /* Backup existing menu */
    char backup_path[TILL_MAX_PATH];
    snprintf(backup_path, sizeof(backup_path), "%s.backup", menu_path);
    rename(menu_path, backup_path);

    /* Write new menu */
    char *json_str = cJSON_Print(menu);
    if (!json_str) {
        till_error("Failed to serialize menu");
        return -1;
    }

    FILE *fp = fopen(menu_path, "w");
    if (!fp) {
        till_error("Cannot write menu file: %s", strerror(errno));
        free(json_str);
        return -1;
    }

    fprintf(fp, "%s\n", json_str);
    fclose(fp);
    free(json_str);

    till_log(LOG_INFO, "Menu updated successfully");
    return 0;
}

/* Parse availability string like "anonymous=optional,named=standard,trusted=standard" */
static cJSON* parse_availability(const char *avail_str) {
    cJSON *availability = cJSON_CreateObject();

    if (!avail_str || !*avail_str) {
        /* Default availability - all optional */
        cJSON_AddStringToObject(availability, "anonymous", "optional");
        cJSON_AddStringToObject(availability, "named", "optional");
        cJSON_AddStringToObject(availability, "trusted", "optional");
        return availability;
    }

    /* Skip "availability:" prefix if present */
    if (strncasecmp(avail_str, "availability:", 13) == 0) {
        avail_str += 13;
    }

    char *work_str = strdup(avail_str);
    char *token = strtok(work_str, ",");

    while (token) {
        char *equals = strchr(token, '=');
        if (equals) {
            *equals = '\0';
            char *level = token;
            char *type = equals + 1;

            /* Validate level and type */
            if ((strcmp(level, "anonymous") == 0 ||
                 strcmp(level, "named") == 0 ||
                 strcmp(level, "trusted") == 0) &&
                (strcmp(type, "optional") == 0 ||
                 strcmp(type, "standard") == 0)) {
                cJSON_AddStringToObject(availability, level, type);
            } else {
                till_warn("Invalid availability setting: %s=%s", level, type);
            }
        }
        token = strtok(NULL, ",");
    }

    free(work_str);

    /* Ensure all levels have values */
    if (!cJSON_GetObjectItem(availability, "anonymous")) {
        cJSON_AddStringToObject(availability, "anonymous", "optional");
    }
    if (!cJSON_GetObjectItem(availability, "named")) {
        cJSON_AddStringToObject(availability, "named", "optional");
    }
    if (!cJSON_GetObjectItem(availability, "trusted")) {
        cJSON_AddStringToObject(availability, "trusted", "standard");
    }

    return availability;
}

/* Add or replace component in menu */
int cmd_menu_add(int argc, char **argv) {
    if (argc < 2) {
        till_error("Usage: till menu add <component> <repo> [version] [availability] [description]");
        till_info("  Example: till menu add Tekton https://github.com/user/Tekton.git v1.0.0 anonymous=optional,named=standard \"Description here\"");
        till_info("  Note: If component exists, it will be replaced with new values");
        return 1;
    }

    const char *component_name = argv[0];
    const char *repo_url = argv[1];
    const char *version = "v1.0.0";
    const char *availability = NULL;
    const char *description = "";

    /* Parse optional arguments */
    int arg_idx = 2;

    /* Check for version (starts with 'v' or is a number) */
    if (argc > arg_idx && argv[arg_idx][0] == 'v') {
        version = argv[arg_idx];
        arg_idx++;
    }

    /* Check for availability (contains '=' or starts with 'availability:') */
    if (argc > arg_idx && strchr(argv[arg_idx], '=') != NULL) {
        /* Check for common misspellings */
        if (strncasecmp(argv[arg_idx], "availabil", 9) == 0 &&
            strncasecmp(argv[arg_idx], "availability:", 13) != 0) {
            till_error("Misspelled 'availability:' - please use correct spelling");
            return 1;
        }
        availability = argv[arg_idx];
        arg_idx++;
    }

    /* Remaining argument is description */
    if (argc > arg_idx) {
        description = argv[arg_idx];
    }

    /* Validate component name - basic check for reasonable names */
    if (strlen(component_name) == 0 || strlen(component_name) > 64) {
        till_error("Invalid component name: %s", component_name);
        return 1;
    }

    /* Check for invalid characters */
    for (const char *p = component_name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.') {
            till_error("Invalid character in component name: %s", component_name);
            return 1;
        }
    }

    /* Basic repo URL validation */
    if (strstr(repo_url, "github.com") == NULL && strstr(repo_url, "gitlab.com") == NULL) {
        till_warn("Repository URL doesn't appear to be from GitHub or GitLab: %s", repo_url);
        till_info("  Continuing anyway...");
    }

    /* Get menu path - always use .till directory in current till installation */
    char menu_path[TILL_MAX_PATH];
    snprintf(menu_path, sizeof(menu_path), ".till/%s", MENU_FILE);

    /* Load current menu */
    cJSON *menu = load_menu(menu_path);
    if (!menu) {
        till_error("Failed to load menu");
        return 1;
    }

    /* Get or create containers object */
    cJSON *containers = cJSON_GetObjectItem(menu, "containers");
    if (!containers) {
        containers = cJSON_CreateObject();
        cJSON_AddItemToObject(menu, "containers", containers);
    }

    /* Check if component already exists */
    cJSON *existing = cJSON_GetObjectItem(containers, component_name);
    if (existing) {
        till_info("Component '%s' already exists in menu, replacing...", component_name);
    }

    /* Create component entry */
    cJSON *component = cJSON_CreateObject();
    cJSON_AddStringToObject(component, "repo", repo_url);
    cJSON_AddStringToObject(component, "version", version);
    cJSON_AddStringToObject(component, "description", description);

    /* Add availability */
    cJSON *avail = parse_availability(availability);
    cJSON_AddItemToObject(component, "availability", avail);

    /* Add to containers */
    cJSON_DeleteItemFromObject(containers, component_name);  /* Remove if exists */
    cJSON_AddItemToObject(containers, component_name, component);

    /* Save menu */
    if (save_menu(menu_path, menu) != 0) {
        cJSON_Delete(menu);
        return 1;
    }

    if (existing) {
        till_log(LOG_INFO, "Replaced '%s' in menu", component_name);
    } else {
        till_log(LOG_INFO, "Added '%s' to menu", component_name);
    }
    till_info("  Repository: %s", repo_url);
    till_info("  Version: %s", version);
    if (availability) {
        till_info("  Availability: %s", availability);
    }
    if (description && *description) {
        till_info("  Description: %s", description);
    }

    cJSON_Delete(menu);
    return 0;
}

/* Remove component from menu */
int cmd_menu_remove(int argc, char **argv) {
    if (argc < 1) {
        till_error("Usage: till menu remove <component>");
        return 1;
    }

    const char *component_name = argv[0];

    /* Get menu path - always use .till directory in current till installation */
    char menu_path[TILL_MAX_PATH];
    snprintf(menu_path, sizeof(menu_path), ".till/%s", MENU_FILE);

    /* Load current menu */
    cJSON *menu = load_menu(menu_path);
    if (!menu) {
        till_error("Failed to load menu");
        return 1;
    }

    /* Get containers object */
    cJSON *containers = cJSON_GetObjectItem(menu, "containers");
    if (!containers) {
        till_error("No containers in menu");
        cJSON_Delete(menu);
        return 1;
    }

    /* Check if component exists */
    if (!cJSON_GetObjectItem(containers, component_name)) {
        till_error("Component '%s' not found in menu", component_name);
        cJSON_Delete(menu);
        return 1;
    }

    /* Remove component */
    cJSON_DeleteItemFromObject(containers, component_name);

    /* Save menu */
    if (save_menu(menu_path, menu) != 0) {
        cJSON_Delete(menu);
        return 1;
    }

    till_log(LOG_INFO, "Removed '%s' from menu", component_name);

    cJSON_Delete(menu);
    return 0;
}

/* Main menu command dispatcher */
int cmd_menu(int argc, char **argv) {
    if (argc < 1) {
        till_error("Usage: till menu <add|remove> ...");
        till_info("  till menu add <component> <repo> [version] [availability] [description]");
        till_info("  till menu remove <component>");
        return 1;
    }

    const char *subcommand = argv[0];

    if (strcmp(subcommand, "add") == 0) {
        return cmd_menu_add(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "remove") == 0) {
        return cmd_menu_remove(argc - 1, argv + 1);
    } else {
        till_error("Unknown menu subcommand: '%s'", subcommand);
        till_info("Available subcommands: add, remove");
        /* Debug: print all arguments */
        till_info("Debug - argc: %d", argc);
        for (int i = 0; i < argc; i++) {
            till_info("  argv[%d]: '%s'", i, argv[i]);
        }
        return 1;
    }
}
