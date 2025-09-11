/*
 * till_registry.c - Tekton installation registry and discovery
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "till_config.h"
#include "till_registry.h"
#include "till_common.h"
#include "cJSON.h"

#ifndef TILL_MAX_PATH
#define TILL_MAX_PATH 4096
#endif

/* Check if path contains a Tekton installation */
static int is_tekton_installation(const char *path) {
    char marker_path[TILL_MAX_PATH];
    
    /* Check for .env.local (the key Tekton file) */
    snprintf(marker_path, sizeof(marker_path), "%s/.env.local", path);
    if (path_exists(marker_path)) {
        return 1;
    }
    
    /* Check for src/tekton.py (alternative marker) */
    snprintf(marker_path, sizeof(marker_path), "%s/src/tekton.py", path);
    if (path_exists(marker_path)) {
        return 1;
    }
    
    return 0;
}

/* Extract installation name from .env.local */
static int get_installation_name(const char *path, char *name, size_t size) {
    char env_path[TILL_MAX_PATH];
    FILE *fp;
    char line[512];
    
    snprintf(env_path, sizeof(env_path), "%s/.env.local", path);
    fp = fopen(env_path, "r");
    if (!fp) return -1;
    
    while (fgets(line, sizeof(line), fp)) {
        char *value = NULL;
        
        /* Try both old and new naming conventions */
        if (strncmp(line, "TEKTON_REGISTRY_NAME=", 21) == 0) {
            value = line + 21;
        } else if (strncmp(line, "INSTALLATION_NAME=", 18) == 0) {
            value = line + 18;
        }
        
        if (value) {
            /* Remove quotes and newline */
            if (*value == '"') value++;
            char *end = strchr(value, '"');
            if (!end) end = strchr(value, '\n');
            if (end) *end = '\0';
            
            strncpy(name, value, size - 1);
            name[size - 1] = '\0';
            fclose(fp);
            return 0;
        }
    }
    
    fclose(fp);
    return -1;
}

/* Extract port bases from .env.local */
static int get_installation_ports(const char *path, int *main_port, int *ai_port) {
    char env_path[TILL_MAX_PATH];
    FILE *fp;
    char line[512];
    
    *main_port = 0;
    *ai_port = 0;
    
    snprintf(env_path, sizeof(env_path), "%s/.env.local", path);
    fp = fopen(env_path, "r");
    if (!fp) return -1;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "PORT_BASE=", 10) == 0) {
            *main_port = atoi(line + 10);
        } else if (strncmp(line, "AI_PORT_BASE=", 13) == 0) {
            *ai_port = atoi(line + 13);
        }
    }
    
    fclose(fp);
    return (*main_port > 0 && *ai_port > 0) ? 0 : -1;
}

/* Discover Tekton installations in a directory */
int discover_tektons(void) {
    char search_dir[TILL_MAX_PATH];
    char *home = getenv("HOME");
    
    if (!home) {
        till_log(LOG_ERROR, "Cannot determine home directory");
        return -1;
    }
    
    /* Search in projects/github */
    path_join(search_dir, sizeof(search_dir), home, TILL_PROJECTS_BASE);
    
    till_log(LOG_INFO, "Discovering Tekton installations in %s", search_dir);
    printf("Discovering existing Tekton installations...\n");
    printf("Searching in TEKTON_ROOT parent: %s\n", search_dir);
    
    /* Load or create registry */
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        till_error("Failed to create registry");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    
    /* Scan directory */
    DIR *dir = opendir(search_dir);
    if (!dir) {
        till_log(LOG_WARN, "Cannot open directory %s", search_dir);
        cJSON_Delete(registry);
        return -1;
    }
    
    struct dirent *entry;
    int found_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char full_path[TILL_MAX_PATH];
        path_join(full_path, sizeof(full_path), search_dir, entry->d_name);
        
        if (!is_directory(full_path)) continue;
        
        if (is_tekton_installation(full_path)) {
            char inst_name[256];
            if (get_installation_name(full_path, inst_name, sizeof(inst_name)) == 0) {
                /* Check if already in registry */
                cJSON *existing = cJSON_GetObjectItem(installations, inst_name);
                if (!existing) {
                    /* Add to registry */
                    cJSON *inst = cJSON_CreateObject();
                    cJSON_AddStringToObject(inst, "root", full_path);
                    
                    /* Try to get main_root */
                    char main_root[TILL_MAX_PATH];
                    if (strstr(inst_name, "coder-")) {
                        snprintf(main_root, sizeof(main_root), "%s/Tekton", search_dir);
                    } else {
                        strncpy(main_root, full_path, sizeof(main_root));
                    }
                    cJSON_AddStringToObject(inst, "main_root", main_root);
                    
                    /* Get ports */
                    int main_port, ai_port;
                    if (get_installation_ports(full_path, &main_port, &ai_port) == 0) {
                        cJSON_AddNumberToObject(inst, "port_base", main_port);
                        cJSON_AddNumberToObject(inst, "ai_port_base", ai_port);
                    }
                    
                    cJSON_AddStringToObject(inst, "mode", "solo");
                    cJSON_AddItemToObject(installations, inst_name, inst);
                    
                    printf("  [OK] Found: %s at %s\n", inst_name, full_path);
                    found_count++;
                } else {
                    /* Update path if changed */
                    cJSON *root_item = cJSON_GetObjectItem(existing, "root");
                    if (root_item && strcmp(root_item->valuestring, full_path) != 0) {
                        cJSON_SetValuestring(root_item, full_path);
                        printf("  [OK] Updated: %s at %s\n", inst_name, full_path);
                    } else {
                        printf("  [OK] Found: %s at %s\n", inst_name, full_path);
                    }
                    found_count++;
                }
            }
            /* If we can't get the installation name, just skip it silently */
        }
    }
    
    closedir(dir);
    
    /* Update last discovery time */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm);
    
    cJSON *last_discovery = cJSON_GetObjectItem(registry, "last_discovery");
    if (last_discovery) {
        cJSON_SetValuestring(last_discovery, timestamp);
    } else {
        cJSON_AddStringToObject(registry, "last_discovery", timestamp);
    }
    
    /* Save registry */
    int save_result = save_till_json("tekton/till-private.json", registry);
    if (save_result == 0) {
        if (found_count > 0) {
            printf("Found %d Tekton installation(s) - registry updated\n", found_count);
        } else {
            printf("Found %d Tekton installation(s) - no changes\n", found_count);
        }
        till_log(LOG_INFO, "Discovery complete: %d installations found", found_count);
    } else {
        till_error("Failed to save registry (error: %d)", save_result);
    }
    
    cJSON_Delete(registry);
    return 0;
}

/* Get primary Tekton installation path */
int get_primary_tekton_path(char *path, size_t size) {
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) {
        till_log(LOG_ERROR, "No Tekton registry found");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations) {
        cJSON_Delete(registry);
        till_log(LOG_ERROR, "No installations in registry");
        return -1;
    }
    
    /* First, try to find primary.tekton.development.us */
    cJSON *primary = cJSON_GetObjectItem(installations, "primary.tekton.development.us");
    if (primary) {
        cJSON *main_root = cJSON_GetObjectItem(primary, "main_root");
        if (main_root && main_root->valuestring) {
            strncpy(path, main_root->valuestring, size - 1);
            path[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    /* Otherwise, use the first installation's main_root */
    cJSON *first = installations->child;
    if (first) {
        cJSON *main_root = cJSON_GetObjectItem(first, "main_root");
        if (main_root && main_root->valuestring) {
            strncpy(path, main_root->valuestring, size - 1);
            path[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    cJSON_Delete(registry);
    till_log(LOG_ERROR, "No primary Tekton found");
    return -1;
}

/* Get primary Tekton name */
int get_primary_tekton_name(char *name, size_t size) {
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) return -1;
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations) {
        cJSON_Delete(registry);
        return -1;
    }
    
    /* Look for primary */
    if (cJSON_GetObjectItem(installations, "primary.tekton.development.us")) {
        strncpy(name, "primary.tekton.development.us", size - 1);
        name[size - 1] = '\0';
        cJSON_Delete(registry);
        return 0;
    }
    
    /* Use first installation */
    cJSON *first = installations->child;
    if (first && first->string) {
        strncpy(name, first->string, size - 1);
        name[size - 1] = '\0';
        cJSON_Delete(registry);
        return 0;
    }
    
    cJSON_Delete(registry);
    return -1;
}

/* Register a new Tekton installation */
int register_installation(const char *name, const char *path, int main_port, int ai_port, const char *mode) {
    /* Ensure tekton directory exists */
    char tekton_dir[TILL_MAX_PATH];
    if (build_till_path(tekton_dir, sizeof(tekton_dir), "tekton") != 0) {
        return -1;
    }
    ensure_directory(tekton_dir);
    
    /* Load or create registry */
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        till_error("Failed to create registry");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    
    /* Check if already exists */
    if (cJSON_GetObjectItem(installations, name)) {
        till_log(LOG_WARN, "Installation %s already registered", name);
    }
    
    /* Determine main_root */
    char main_root[TILL_MAX_PATH];
    if (strstr(name, "coder-")) {
        /* For Coder instances, main_root is the primary Tekton */
        if (get_primary_tekton_path(main_root, sizeof(main_root)) != 0) {
            /* No primary found, use parent directory */
            char *last_slash = strrchr(path, '/');
            if (last_slash) {
                size_t parent_len = last_slash - path;
                strncpy(main_root, path, parent_len);
                main_root[parent_len] = '\0';
                strcat(main_root, "/Tekton");
            } else {
                strncpy(main_root, path, sizeof(main_root));
            }
        }
    } else {
        /* For primary, main_root is itself */
        strncpy(main_root, path, sizeof(main_root));
    }
    
    /* Create installation entry */
    cJSON *inst = cJSON_CreateObject();
    cJSON_AddStringToObject(inst, "root", path);
    cJSON_AddStringToObject(inst, "main_root", main_root);
    cJSON_AddNumberToObject(inst, "port_base", main_port);
    cJSON_AddNumberToObject(inst, "ai_port_base", ai_port);
    cJSON_AddStringToObject(inst, "mode", mode ? mode : "solo");
    
    /* Add or update */
    cJSON_DeleteItemFromObject(installations, name);
    cJSON_AddItemToObject(installations, name, inst);
    
    /* Update last discovery time */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm);
    
    cJSON *last_discovery = cJSON_GetObjectItem(registry, "last_discovery");
    if (last_discovery) {
        cJSON_SetValuestring(last_discovery, timestamp);
    } else {
        cJSON_AddStringToObject(registry, "last_discovery", timestamp);
    }
    
    /* Save registry */
    if (save_till_json("tekton/till-private.json", registry) != 0) {
        till_log(LOG_ERROR, "Failed to save registry");
        cJSON_Delete(registry);
        return -1;
    }
    
    till_log(LOG_INFO, "Registered installation %s at %s", name, path);
    cJSON_Delete(registry);
    return 0;
}

/* Suggest next available port range */
int suggest_next_port_range(int *main_port, int *ai_port) {
    *main_port = 8000;  /* Default primary ports */
    *ai_port = 45000;
    
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) {
        return 0;  /* Use defaults */
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations || cJSON_GetArraySize(installations) == 0) {
        cJSON_Delete(registry);
        return 0;  /* Use defaults for first installation */
    }
    
    /* Find highest port in use */
    int max_main_port = 0;
    cJSON *inst = NULL;
    cJSON_ArrayForEach(inst, installations) {
        cJSON *port_item = cJSON_GetObjectItem(inst, "port_base");
        if (port_item) {
            int port = cJSON_GetNumberValue(port_item);
            if (port > max_main_port) {
                max_main_port = port;
            }
        }
    }
    
    /* If we found installations, increment from highest */
    if (max_main_port > 0) {
        *main_port = max_main_port + 100;
        *ai_port = 45000 - ((*main_port - 8000));  /* Inverse relationship */
    }
    
    cJSON_Delete(registry);
    return 0;
}

/* Validate installation name */
int validate_installation_name(const char *name) {
    if (!name || strlen(name) == 0) {
        return -1;
    }
    
    /* Check for valid characters */
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '.' && *p != '_') {
            return -1;
        }
    }
    
    /* Check it doesn't start with a dot or dash */
    if (name[0] == '.' || name[0] == '-') {
        return -1;
    }
    
    return 0;
}

/* Fuzzy match installation name */
int fuzzy_match_name(const char *input, char *matched, size_t size) {
    if (!input || !matched) return -1;
    
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) return -1;
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations) {
        cJSON_Delete(registry);
        return -1;
    }
    
    /* Convert input to lowercase for comparison */
    char lower_input[256];
    size_t i;
    for (i = 0; i < sizeof(lower_input) - 1 && input[i]; i++) {
        lower_input[i] = tolower(input[i]);
    }
    lower_input[i] = '\0';
    
    /* Try exact match first */
    cJSON *inst = NULL;
    cJSON_ArrayForEach(inst, installations) {
        const char *inst_name = inst->string;
        if (strcmp(inst_name, input) == 0) {
            strncpy(matched, inst_name, size - 1);
            matched[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    /* Try case-insensitive match */
    cJSON_ArrayForEach(inst, installations) {
        const char *inst_name = inst->string;
        char lower_inst[256];
        for (i = 0; i < sizeof(lower_inst) - 1 && inst_name[i]; i++) {
            lower_inst[i] = tolower(inst_name[i]);
        }
        lower_inst[i] = '\0';
        
        if (strcmp(lower_inst, lower_input) == 0) {
            strncpy(matched, inst_name, size - 1);
            matched[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    /* Try prefix match */
    cJSON_ArrayForEach(inst, installations) {
        const char *inst_name = inst->string;
        char lower_inst[256];
        for (i = 0; i < sizeof(lower_inst) - 1 && inst_name[i]; i++) {
            lower_inst[i] = tolower(inst_name[i]);
        }
        lower_inst[i] = '\0';
        
        if (strncmp(lower_inst, lower_input, strlen(lower_input)) == 0) {
            strncpy(matched, inst_name, size - 1);
            matched[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    /* Try contains match */
    cJSON_ArrayForEach(inst, installations) {
        const char *inst_name = inst->string;
        char lower_inst[256];
        for (i = 0; i < sizeof(lower_inst) - 1 && inst_name[i]; i++) {
            lower_inst[i] = tolower(inst_name[i]);
        }
        lower_inst[i] = '\0';
        
        if (strstr(lower_inst, lower_input) != NULL) {
            strncpy(matched, inst_name, size - 1);
            matched[size - 1] = '\0';
            cJSON_Delete(registry);
            return 0;
        }
    }
    
    cJSON_Delete(registry);
    return -1;
}