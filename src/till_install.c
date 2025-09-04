/*
 * till_install.c - Installation implementation for Till
 * 
 * Handles Tekton installation, port allocation, and .env.local generation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>

#include "till_install.h"
#include "cJSON.h"

/* External global from till.c */
extern int g_interactive;

/* Forward declarations - none needed, functions are in order */

/* Helper: Get absolute path */
static int get_absolute_path(const char *relative, char *absolute, size_t size) {
    if (relative[0] == '/') {
        /* Already absolute */
        strncpy(absolute, relative, size);
        return 0;
    }
    
    char cwd[TILL_MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return -1;
    }
    
    snprintf(absolute, size, "%s/%s", cwd, relative);
    return 0;
}

/* Helper: Check if directory exists */
static int dir_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Helper: Check if file exists */
static int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/* Helper: Ensure directory exists */
int ensure_directory(const char *path) {
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        /* Directory doesn't exist, create it */
        if (mkdir(path, 0755) != 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Helper: Get primary Tekton path */
int get_primary_tekton_path(char *path, size_t size) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    cJSON *root = NULL;
    
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    fp = fopen(config_path, "r");
    if (!fp) {
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    content[fsize] = '\0';
    fclose(fp);
    
    root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        return -1;
    }
    
    /* Look for primary Tekton */
    cJSON *primary = cJSON_GetObjectItem(root, "primary_tekton");
    if (primary && primary->valuestring) {
        /* Get the path for this installation */
        cJSON *installations = cJSON_GetObjectItem(root, "installations");
        if (installations) {
            cJSON *install = cJSON_GetObjectItem(installations, primary->valuestring);
            if (install) {
                cJSON *install_path = cJSON_GetObjectItem(install, "path");
                if (install_path && install_path->valuestring) {
                    strncpy(path, install_path->valuestring, size);
                    cJSON_Delete(root);
                    return 0;
                }
            }
        }
    }
    
    cJSON_Delete(root);
    return -1;
}

/* Discover existing Tekton installations */
int discover_tektons(void) {
    printf("Discovering existing Tekton installations...\n");
    
    /* Load existing configuration if it exists */
    char config_path[TILL_MAX_PATH];
    snprintf(config_path, sizeof(config_path), "%s/%s",
             TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    cJSON *old_root = NULL;
    cJSON *old_installations = NULL;
    
    if (file_exists(config_path)) {
        /* Backup existing config */
        char backup_path[TILL_MAX_PATH];
        snprintf(backup_path, sizeof(backup_path), "%s/%s",
                 TILL_TEKTON_DIR, TILL_PRIVATE_BACKUP);
        
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cp %s %s", config_path, backup_path);
        system(cmd);
        
        /* Load existing config for comparison */
        FILE *fp = fopen(config_path, "r");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            
            char *content = malloc(size + 1);
            fread(content, 1, size, fp);
            content[size] = '\0';
            fclose(fp);
            
            old_root = cJSON_Parse(content);
            free(content);
            
            if (old_root) {
                old_installations = cJSON_GetObjectItem(old_root, "installations");
            }
        }
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON *installations = cJSON_CreateObject();
    char *primary_name = NULL;
    int found_count = 0;
    int changes_count = 0;
    
    /* Determine search directory */
    char search_dir[TILL_MAX_PATH];
    const char *tekton_root_env = getenv("TEKTON_ROOT");
    
    if (tekton_root_env && strlen(tekton_root_env) > 0) {
        /* Use parent directory of TEKTON_ROOT */
        char *last_slash = strrchr(tekton_root_env, '/');
        if (last_slash) {
            size_t parent_len = last_slash - tekton_root_env;
            strncpy(search_dir, tekton_root_env, parent_len);
            search_dir[parent_len] = '\0';
        } else {
            strcpy(search_dir, ".");
        }
        printf("Searching in TEKTON_ROOT parent: %s\n", search_dir);
    } else {
        /* Use parent directory of till binary */
        char till_path[TILL_MAX_PATH];
        if (readlink("/proc/self/exe", till_path, sizeof(till_path)) > 0 ||
            realpath("./till", till_path)) {
            char *last_slash = strrchr(till_path, '/');
            if (last_slash) {
                *last_slash = '\0';  /* Remove till binary name */
                last_slash = strrchr(till_path, '/');
                if (last_slash) {
                    size_t parent_len = last_slash - till_path;
                    strncpy(search_dir, till_path, parent_len);
                    search_dir[parent_len] = '\0';
                } else {
                    strcpy(search_dir, ".");
                }
            } else {
                strcpy(search_dir, ".");
            }
        } else {
            /* Fallback to current directory */
            strcpy(search_dir, ".");
        }
        printf("Searching in till parent directory: %s\n", search_dir);
    }
    
    /* Open directory and scan for Tekton installations */
    DIR *dir = opendir(search_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", search_dir);
        cJSON_Delete(root);
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Build full path */
        char full_path[TILL_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", search_dir, entry->d_name);
    
        /* Check if it's a directory */
        if (!dir_exists(full_path)) {
            continue;
        }
        
        /* Validate it's a Tekton installation */
        char env_tekton_path[TILL_MAX_PATH];
        char env_local_path[TILL_MAX_PATH];
        snprintf(env_tekton_path, sizeof(env_tekton_path), "%s/.env.tekton", full_path);
        snprintf(env_local_path, sizeof(env_local_path), "%s/.env.local", full_path);
        
        /* Both files must exist for valid Tekton installation */
        if (!file_exists(env_tekton_path) || !file_exists(env_local_path)) {
            continue;
        }
        
        /* Now read .env.local for configuration */
        FILE *fp = fopen(env_local_path, "r");
        if (!fp) {
            continue;
        }
        
        /* Parse .env.local to extract configuration */
        char line[512];
        char registry_name[TILL_MAX_NAME] = "";
        char tekton_root[TILL_MAX_PATH] = "";
        char tekton_main_root[TILL_MAX_PATH] = "";
        int port_base = -1;
        int ai_port_base = -1;
        
        while (fgets(line, sizeof(line), fp)) {
            char key[128], value[384];
            if (sscanf(line, "%127[^=]=%383s", key, value) == 2) {
                if (strcmp(key, "TEKTON_ROOT") == 0) {
                    strcpy(tekton_root, value);
                } else if (strcmp(key, "TEKTON_MAIN_ROOT") == 0) {
                    strcpy(tekton_main_root, value);
                } else if (strcmp(key, "TEKTON_REGISTRY_NAME") == 0) {
                    strcpy(registry_name, value);
                } else if (strcmp(key, "TEKTON_PORT_BASE") == 0) {
                    port_base = atoi(value);
                } else if (strcmp(key, "TEKTON_AI_PORT_BASE") == 0) {
                    ai_port_base = atoi(value);
                }
            }
        }
        fclose(fp);
        
        /* Skip if we don't have required information */
        if (strlen(tekton_root) == 0) {
            /* If no TEKTON_ROOT in .env.local, skip this installation */
            if (TILL_DEBUG) {
                printf("  Skipping %s: No TEKTON_ROOT in .env.local\n", full_path);
            }
            continue;
        }
        
        /* If no registry name, use directory name as fallback */
        if (strlen(registry_name) == 0) {
            const char *dir_name = strrchr(tekton_root, '/');
            if (dir_name) {
                dir_name++; /* Skip the slash */
                snprintf(registry_name, sizeof(registry_name), "%s.local", dir_name);
            } else {
                strcpy(registry_name, "unknown.local");
            }
        }
        
        /* Use TEKTON_ROOT from .env.local as the authoritative path */
        char abs_path[TILL_MAX_PATH];
        strcpy(abs_path, tekton_root);
        
        /* Check if we already have this installation (deduplication) */
        cJSON *existing = cJSON_GetObjectItem(installations, registry_name);
        if (existing) {
            /* Already found this installation, skip duplicate */
            continue;
        }
        
        /* Check if this installation existed before */
        cJSON *old_install = NULL;
        if (old_installations) {
            old_install = cJSON_GetObjectItem(old_installations, registry_name);
        }
        
        if (!old_install) {
            printf("  [NEW] Found: %s at %s\n", registry_name, abs_path);
            changes_count++;
        } else {
            /* Check for changes */
            cJSON *old_root_item = cJSON_GetObjectItem(old_install, "root");
            cJSON *old_port = cJSON_GetObjectItem(old_install, "port_base");
            
            int changed = 0;
            if (old_root_item && strcmp(old_root_item->valuestring, abs_path) != 0) {
                printf("  [MOVED] %s: %s -> %s\n", registry_name, 
                       old_root_item->valuestring, abs_path);
                changed = 1;
                changes_count++;
            }
            if (old_port && old_port->valueint != port_base) {
                printf("  [PORT CHANGE] %s: port %d -> %d\n", registry_name,
                       old_port->valueint, port_base);
                changed = 1;
                changes_count++;
            }
            
            if (!changed) {
                printf("  [OK] Found: %s at %s\n", registry_name, abs_path);
            }
        }
        
        /* Set default ports if not found */
        if (port_base < 0) {
            port_base = DEFAULT_PORT_BASE;
        }
        if (ai_port_base < 0) {
            ai_port_base = DEFAULT_AI_PORT_BASE;
        }
        
        /* Add to installations */
        cJSON *install = cJSON_CreateObject();
        cJSON_AddStringToObject(install, "root", abs_path);
        if (strlen(tekton_main_root) > 0) {
            cJSON_AddStringToObject(install, "main_root", tekton_main_root);
        }
        cJSON_AddNumberToObject(install, "port_base", port_base);
        cJSON_AddNumberToObject(install, "ai_port_base", ai_port_base);
        
        /* Set mode to solo for discovered installations */
        cJSON_AddStringToObject(install, "mode", "solo");
        
        cJSON_AddItemToObject(installations, registry_name, install);
        found_count++;
    }
    
    closedir(dir);
    
    /* Check for missing installations */
    if (old_installations) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, old_installations) {
            const char *old_name = item->string;
            if (!cJSON_GetObjectItem(installations, old_name)) {
                cJSON *old_root_item = cJSON_GetObjectItem(item, "root");
                printf("  [MISSING] %s (was at %s)\n", old_name,
                       old_root_item ? old_root_item->valuestring : "unknown");
                changes_count++;
            }
        }
    }
    
    /* Report summary */
    if (found_count > 0) {
        printf("Found %d Tekton installation(s)", found_count);
        if (changes_count > 0) {
            printf(" with %d change(s)\n", changes_count);
        } else if (old_installations) {
            printf(" - no changes\n");
        } else {
            printf("\n");
        }
    } else {
        printf("No Tekton installations found\n");
    }
    
    /* Always save current state */
    if (primary_name) {
        cJSON_AddStringToObject(root, "primary_tekton", primary_name);
        free(primary_name);
    }
    
    cJSON_AddItemToObject(root, "installations", installations);
    
    /* Add metadata */
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    cJSON_AddStringToObject(root, "last_discovery", timestamp);
    cJSON_AddNumberToObject(root, "version", 1);
    
    /* Save to till-private.json */
    ensure_directory(TILL_HOME);
    ensure_directory(TILL_TEKTON_DIR);
    
    FILE *fp = fopen(config_path, "w");
    if (fp) {
        char *json_str = cJSON_Print(root);
        fprintf(fp, "%s\n", json_str);
        fclose(fp);
        free(json_str);
        
        if (changes_count > 0 || !old_root) {
            printf("Updated till-private.json\n");
        }
    }
    
    /* Clean up */
    if (old_root) {
        cJSON_Delete(old_root);
    }
    cJSON_Delete(root);
    return found_count;
}

/* Main installation function */
int till_install_tekton(install_options_t *opts) {
    printf("Installing Tekton...\n");
    printf("  Name: %s\n", opts->name);
    printf("  Path: %s\n", opts->path);
    printf("  Mode: %s\n", opts->mode);
    printf("  Port Base: %d\n", opts->port_base);
    printf("  AI Port Base: %d\n", opts->ai_port_base);
    
    /* Step 1: Clone Tekton repository */
    if (clone_tekton_repo(opts->path) != 0) {
        fprintf(stderr, "Failed to clone Tekton repository\n");
        return -1;
    }
    
    /* Step 2: Generate .env.local */
    if (generate_env_local(opts) != 0) {
        fprintf(stderr, "Failed to generate .env.local\n");
        return -1;
    }
    
    /* Step 3: Register installation locally */
    if (register_installation(opts) != 0) {
        fprintf(stderr, "Failed to register installation\n");
        return -1;
    }
    
    /* Step 4: If observer/member, register with federation */
    if (strcmp(opts->mode, MODE_OBSERVER) == 0 || 
        strcmp(opts->mode, MODE_MEMBER) == 0) {
        printf("Registering with federation as %s...\n", opts->mode);
        /* TODO: Implement federation registration */
    }
    
    printf("\nTekton installation complete!\n");
    printf("To start: cd %s && tekton start\n", opts->path);
    
    return 0;
}

/* Suggest next available port ranges based on existing installations */
int suggest_next_port_range(int *main_port, int *ai_port) {
    char config_path[TILL_MAX_PATH];
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    int max_main_port = 8000;
    int min_ai_port = 45000;
    
    FILE *fp = fopen(config_path, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char *content = malloc(size + 1);
        fread(content, 1, size, fp);
        content[size] = '\0';
        fclose(fp);
        
        cJSON *root = cJSON_Parse(content);
        free(content);
        
        if (root) {
            cJSON *installations = cJSON_GetObjectItem(root, "installations");
            if (installations) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, installations) {
                    cJSON *port_item = cJSON_GetObjectItem(item, "port_base");
                    cJSON *ai_port_item = cJSON_GetObjectItem(item, "ai_port_base");
                    
                    if (port_item && cJSON_IsNumber(port_item)) {
                        int port = port_item->valueint;
                        if (port >= max_main_port) {
                            max_main_port = port;
                        }
                    }
                    
                    if (ai_port_item && cJSON_IsNumber(ai_port_item)) {
                        int port = ai_port_item->valueint;
                        if (port <= min_ai_port) {
                            min_ai_port = port;
                        }
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    
    /* Suggest next main port range (increment by 100) */
    *main_port = max_main_port + 100;
    
    /* Suggest next AI port range (decrement by 1000) */
    *ai_port = min_ai_port - 1000;
    
    /* Ensure ranges are valid */
    if (*main_port > 60000) {
        *main_port = 9000;  /* Wrap around to 9000 */
    }
    if (*ai_port < 30000) {
        *ai_port = 50000;  /* Wrap around to 50000 */
    }
    
    return 0;
}

/* Get primary Tekton name for defaults */
int get_primary_name(char *name, size_t size) {
    /* Look for primary.tekton.development.us or first available */
    char config_path[TILL_MAX_PATH];
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    /* Try to find primary Tekton */
    FILE *fp = fopen(config_path, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char *content = malloc(file_size + 1);
        fread(content, 1, file_size, fp);
        content[file_size] = '\0';
        fclose(fp);
        
        cJSON *root = cJSON_Parse(content);
        free(content);
        
        if (root) {
            cJSON *installations = cJSON_GetObjectItem(root, "installations");
            if (installations) {
                cJSON *primary = cJSON_GetObjectItem(installations, "primary.tekton.development.us");
                if (primary) {
                    strncpy(name, "primary", size);
                    cJSON_Delete(root);
                    return 0;
                }
                
                /* Use first installation found */
                cJSON *item = installations->child;
                if (item) {
                    const char *reg_name = item->string;
                    /* Extract simple name from FQDN */
                    char *dot = strchr(reg_name, '.');
                    if (dot) {
                        size_t len = dot - reg_name;
                        if (len < size) {
                            strncpy(name, reg_name, len);
                            name[len] = '\0';
                        } else {
                            strncpy(name, reg_name, size - 1);
                        }
                    } else {
                        strncpy(name, reg_name, size - 1);
                    }
                    cJSON_Delete(root);
                    return 0;
                }
            }
            cJSON_Delete(root);
        }
    }
    
    /* Default if no installations found */
    strncpy(name, "primary", size);
    return 0;
}

/* Clone Tekton repository */
int clone_tekton_repo(const char *path) {
    /* Check if directory already exists */
    if (dir_exists(path)) {
        fprintf(stderr, "Error: Directory %s already exists\n", path);
        return -1;
    }
    
    /* Build git clone command */
    char cmd[TILL_MAX_COMMAND];
    snprintf(cmd, sizeof(cmd), 
             "git clone %s %s",
             TEKTON_REPO_URL, path);
    
    printf("Cloning Tekton from %s...\n", TEKTON_REPO_URL);
    
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Git clone failed (exit code %d)\n", result);
        fprintf(stderr, "Make sure you have git installed and authenticated\n");
        return -1;
    }
    
    return 0;
}

/* Generate .env.local file */
int generate_env_local(install_options_t *opts) {
    char env_path[TILL_MAX_PATH];
    char abs_path[TILL_MAX_PATH];
    char main_root[TILL_MAX_PATH] = "";
    FILE *fp;
    
    /* Get absolute path for TEKTON_ROOT */
    if (get_absolute_path(opts->path, abs_path, sizeof(abs_path)) != 0) {
        fprintf(stderr, "Failed to get absolute path\n");
        return -1;
    }
    
    /* Get TEKTON_MAIN_ROOT from opts or find from existing installations */
    if (strlen(opts->tekton_main_root) > 0) {
        strcpy(main_root, opts->tekton_main_root);
    } else {
        get_primary_tekton_path(main_root, sizeof(main_root));
        if (strlen(main_root) == 0) {
            /* This is the first installation, use self as main */
            strcpy(main_root, abs_path);
        }
    }
    
    /* Build .env.local path */
    snprintf(env_path, sizeof(env_path), "%s/.env.local", opts->path);
    
    /* Check if .env.local already exists */
    if (access(env_path, F_OK) == 0) {
        printf(".env.local already exists, skipping generation\n");
        return 0;
    }
    
    /* For primary Tekton with default ports, copy .env.local.example */
    if (opts->port_base == DEFAULT_PORT_BASE) {
        char example_path[TILL_MAX_PATH];
        char cmd[TILL_MAX_COMMAND];
        
        snprintf(example_path, sizeof(example_path), 
                 "%s/.env.local.example", opts->path);
        
        /* Copy example to .env.local */
        snprintf(cmd, sizeof(cmd), "cp %s %s", example_path, env_path);
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to copy .env.local.example\n");
            return -1;
        }
        
        /* Update TEKTON_ROOT and TEKTON_REGISTRY_NAME */
        snprintf(cmd, sizeof(cmd), 
                 "sed -i.bak 's|^TEKTON_ROOT=.*|TEKTON_ROOT=%s|' %s",
                 abs_path, env_path);
        system(cmd);
        
        /* Add TEKTON_REGISTRY_NAME */
        fp = fopen(env_path, "a");
        if (fp) {
            fprintf(fp, "\n# Tekton Registry Name\n");
            if (strcmp(opts->mode, MODE_SOLO) == 0) {
                fprintf(fp, "TEKTON_REGISTRY_NAME=None\n");
            } else {
                fprintf(fp, "TEKTON_REGISTRY_NAME=%s\n", opts->name);
            }
            fclose(fp);
        }
        
        /* Remove backup file */
        snprintf(cmd, sizeof(cmd), "rm -f %s.bak", env_path);
        system(cmd);
        
        printf("Generated .env.local from template\n");
        return 0;
    }
    
    /* Generate custom .env.local for non-default ports */
    printf("Generating custom .env.local...\n");
    
    fp = fopen(env_path, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", env_path);
        return -1;
    }
    
    /* Write header */
    fprintf(fp, "# Tekton Environment Configuration\n");
    fprintf(fp, "# Generated by Till on %s", ctime(&(time_t){time(NULL)}));
    fprintf(fp, "\n");
    
    /* Write Registry Name FIRST */
    fprintf(fp, "# Tekton Registry Name\n");
    if (strlen(opts->registry_name) > 0) {
        fprintf(fp, "TEKTON_REGISTRY_NAME=%s\n", opts->registry_name);
    } else if (strcmp(opts->mode, MODE_SOLO) == 0) {
        fprintf(fp, "TEKTON_REGISTRY_NAME=None\n");
    } else {
        fprintf(fp, "TEKTON_REGISTRY_NAME=%s\n", opts->name);
    }
    fprintf(fp, "\n");
    
    /* Write Tekton Root */
    fprintf(fp, "# Tekton Root (for this Tekton environment)\n");
    fprintf(fp, "TEKTON_ROOT=%s\n", abs_path);
    fprintf(fp, "\n");
    
    /* Write Main Tekton Root */
    fprintf(fp, "# Main Tekton Root (primary Tekton on this host)\n");
    fprintf(fp, "TEKTON_MAIN_ROOT=%s\n", main_root);
    fprintf(fp, "\n");
    
    /* Write Port Configuration */
    fprintf(fp, "# Tekton Port Ranges (for this Tekton environment)\n");
    fprintf(fp, "# Base ports: Component=%d, AI=%d\n", 
            opts->port_base, opts->ai_port_base);
    fprintf(fp, "\n");
    
    fprintf(fp, "# Base Configuration\n");
    fprintf(fp, "TEKTON_PORT_BASE=%d\n", opts->port_base);
    fprintf(fp, "TEKTON_AI_PORT_BASE=%d\n", opts->ai_port_base);
    fprintf(fp, "\n");
    
    /* Write Component Ports */
    fprintf(fp, "# Component Ports\n");
    for (int i = 0; COMPONENT_PORTS[i].name != NULL; i++) {
        int port = opts->port_base + COMPONENT_PORTS[i].offset;
        
        /* Handle special cases for HEPHAESTUS */
        if (strcmp(COMPONENT_PORTS[i].name, "HEPHAESTUS_PORT") == 0) {
            port = opts->port_base + 80;
        } else if (strcmp(COMPONENT_PORTS[i].name, "HEPHAESTUS_MCP_PORT") == 0) {
            port = opts->port_base + 88;
        }
        
        fprintf(fp, "%s=%d\n", COMPONENT_PORTS[i].name, port);
    }
    fprintf(fp, "\n");
    
    /* Write AI Ports */
    fprintf(fp, "# AI Specialist Ports\n");
    for (int i = 0; AI_PORTS[i].name != NULL; i++) {
        int port = opts->ai_port_base + AI_PORTS[i].offset;
        fprintf(fp, "%s=%d\n", AI_PORTS[i].name, port);
    }
    fprintf(fp, "\n");
    
    /* Don't copy other configuration - keep it clean */
    
    fclose(fp);
    
    printf("Generated .env.local with custom ports\n");
    return 0;
}

/* Check if a port is in use */
static int check_port_in_use(int port, char *process_info, size_t info_size) {
    char cmd[256];
    FILE *fp;
    char line[256];
    int in_use = 0;
    
    /* Try lsof first (most reliable on macOS) */
    snprintf(cmd, sizeof(cmd), "lsof -i :%d 2>/dev/null | grep LISTEN", port);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            in_use = 1;
            if (process_info && info_size > 0) {
                /* Extract process name and PID */
                char proc_name[64] = "";
                int pid = 0;
                sscanf(line, "%s %d", proc_name, &pid);
                snprintf(process_info, info_size, "%s (PID: %d)", proc_name, pid);
            }
        }
        pclose(fp);
    }
    
    return in_use;
}

/* Kill a process by PID */
static int kill_process(int pid) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "kill -9 %d 2>/dev/null", pid);
    return system(cmd);
}

/* Find available port range */
static int find_available_port_range(int *base_port, int range_size) {
    int start_port = *base_port;
    int max_attempts = 100; /* Try up to 100 different ranges */
    
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        int port = start_port + (attempt * 100);
        int all_clear = 1;
        
        /* Check if entire range is available */
        for (int offset = 0; offset < range_size; offset++) {
            char proc_info[256];
            if (check_port_in_use(port + offset, proc_info, sizeof(proc_info))) {
                all_clear = 0;
                break;
            }
        }
        
        if (all_clear) {
            *base_port = port;
            return 0;
        }
    }
    
    return -1; /* No available range found */
}

/* Allocate ports for installation */
int allocate_ports(install_options_t *opts) {
    printf("\nChecking port availability...\n");
    
    /* Structures to hold conflict information */
    typedef struct {
        int port;
        char process[256];
        int pid;
    } port_conflict_t;
    
    port_conflict_t conflicts[200];
    int num_conflicts = 0;
    
    /* Check main port range (100 ports) */
    printf("  Checking ports %d-%d...\n", opts->port_base, opts->port_base + 99);
    for (int i = 0; i < 100; i++) {
        int port = opts->port_base + i;
        char proc_info[256] = "";
        
        if (check_port_in_use(port, proc_info, sizeof(proc_info))) {
            if (num_conflicts < 200) {
                conflicts[num_conflicts].port = port;
                strncpy(conflicts[num_conflicts].process, proc_info, sizeof(conflicts[num_conflicts].process) - 1);
                
                /* Extract PID from process info */
                char *pid_str = strstr(proc_info, "PID: ");
                if (pid_str) {
                    conflicts[num_conflicts].pid = atoi(pid_str + 5);
                }
                num_conflicts++;
            }
        }
    }
    
    /* Check AI port range (100 ports) */
    printf("  Checking AI ports %d-%d...\n", opts->ai_port_base, opts->ai_port_base + 99);
    for (int i = 0; i < 100; i++) {
        int port = opts->ai_port_base + i;
        char proc_info[256] = "";
        
        if (check_port_in_use(port, proc_info, sizeof(proc_info))) {
            if (num_conflicts < 200) {
                conflicts[num_conflicts].port = port;
                strncpy(conflicts[num_conflicts].process, proc_info, sizeof(conflicts[num_conflicts].process) - 1);
                
                /* Extract PID from process info */
                char *pid_str = strstr(proc_info, "PID: ");
                if (pid_str) {
                    conflicts[num_conflicts].pid = atoi(pid_str + 5);
                }
                num_conflicts++;
            }
        }
    }
    
    /* If no conflicts, we're good */
    if (num_conflicts == 0) {
        printf("  ✓ All ports available\n");
        return 0;
    }
    
    /* Report conflicts */
    printf("\n⚠️  Port conflicts detected:\n");
    for (int i = 0; i < num_conflicts && i < 10; i++) { /* Show first 10 */
        printf("    Port %d: %s\n", conflicts[i].port, conflicts[i].process);
    }
    if (num_conflicts > 10) {
        printf("    ... and %d more conflicts\n", num_conflicts - 10);
    }
    
    /* If not interactive, try to find alternative ports automatically */
    if (!g_interactive) {
        printf("\nSearching for alternative port ranges...\n");
        
        int new_base = opts->port_base;
        int new_ai_base = opts->ai_port_base;
        
        if (find_available_port_range(&new_base, 100) == 0 &&
            find_available_port_range(&new_ai_base, 100) == 0) {
            printf("  Found available ranges:\n");
            printf("    Main ports: %d-%d\n", new_base, new_base + 99);
            printf("    AI ports: %d-%d\n", new_ai_base, new_ai_base + 99);
            
            opts->port_base = new_base;
            opts->ai_port_base = new_ai_base;
            return 0;
        } else {
            fprintf(stderr, "Error: Unable to find available port ranges\n");
            fprintf(stderr, "Please stop conflicting processes or use --interactive mode\n");
            return -1;
        }
    }
    
    /* Interactive mode - give user options */
    printf("\nOptions:\n");
    printf("  1. Kill conflicting processes and use requested ports\n");
    printf("  2. Find alternative port ranges automatically\n");
    printf("  3. Enter custom port ranges manually\n");
    printf("  4. Cancel installation\n");
    
    printf("\nChoice [1-4]: ");
    char choice[10];
    if (!fgets(choice, sizeof(choice), stdin)) {
        return -1;
    }
    
    switch (choice[0]) {
        case '1': {
            /* Kill conflicting processes */
            printf("\n⚠️  This will terminate the following processes:\n");
            
            /* Show unique processes */
            int shown_pids[100] = {0};
            int num_shown = 0;
            
            for (int i = 0; i < num_conflicts; i++) {
                int already_shown = 0;
                for (int j = 0; j < num_shown; j++) {
                    if (shown_pids[j] == conflicts[i].pid) {
                        already_shown = 1;
                        break;
                    }
                }
                
                if (!already_shown && conflicts[i].pid > 0) {
                    printf("    %s\n", conflicts[i].process);
                    shown_pids[num_shown++] = conflicts[i].pid;
                }
            }
            
            printf("\nProceed? [y/N]: ");
            if (!fgets(choice, sizeof(choice), stdin) || choice[0] != 'y') {
                printf("Cancelled\n");
                return -1;
            }
            
            /* Kill the processes */
            for (int i = 0; i < num_shown; i++) {
                if (shown_pids[i] > 0) {
                    printf("  Stopping PID %d...\n", shown_pids[i]);
                    kill_process(shown_pids[i]);
                }
            }
            
            /* Brief pause for processes to terminate */
            sleep(1);
            
            /* Verify ports are now free */
            int still_blocked = 0;
            for (int i = 0; i < num_conflicts; i++) {
                char proc_info[256];
                if (check_port_in_use(conflicts[i].port, proc_info, sizeof(proc_info))) {
                    still_blocked++;
                }
            }
            
            if (still_blocked > 0) {
                fprintf(stderr, "Warning: %d ports still in use after killing processes\n", still_blocked);
                fprintf(stderr, "Some processes may have restarted. Proceeding anyway...\n");
            } else {
                printf("  ✓ All conflicting processes stopped\n");
            }
            
            return 0;
        }
        
        case '2': {
            /* Find alternative ports automatically */
            printf("\nSearching for available port ranges...\n");
            
            int new_base = opts->port_base;
            int new_ai_base = opts->ai_port_base;
            
            if (find_available_port_range(&new_base, 100) == 0 &&
                find_available_port_range(&new_ai_base, 100) == 0) {
                printf("  Found available ranges:\n");
                printf("    Main ports: %d-%d\n", new_base, new_base + 99);
                printf("    AI ports: %d-%d\n", new_ai_base, new_ai_base + 99);
                
                printf("\nUse these ports? [Y/n]: ");
                if (!fgets(choice, sizeof(choice), stdin) || 
                    (choice[0] != 'n' && choice[0] != 'N')) {
                    opts->port_base = new_base;
                    opts->ai_port_base = new_ai_base;
                    printf("  ✓ Using alternative port ranges\n");
                    return 0;
                }
            } else {
                fprintf(stderr, "Error: Unable to find available port ranges\n");
                return -1;
            }
            break;
        }
        
        case '3': {
            /* Manual port entry */
            printf("\nEnter custom port ranges:\n");
            
            printf("  Main port base (e.g., 8000): ");
            char port_str[20];
            if (!fgets(port_str, sizeof(port_str), stdin)) {
                return -1;
            }
            int new_base = atoi(port_str);
            
            printf("  AI port base (e.g., 45000): ");
            if (!fgets(port_str, sizeof(port_str), stdin)) {
                return -1;
            }
            int new_ai_base = atoi(port_str);
            
            /* Validate ranges */
            if (new_base < 1024 || new_base > 65436 ||
                new_ai_base < 1024 || new_ai_base > 65436) {
                fprintf(stderr, "Error: Invalid port ranges\n");
                return -1;
            }
            
            /* Check if these are available */
            printf("\nChecking availability...\n");
            int conflicts_found = 0;
            
            for (int i = 0; i < 100; i++) {
                char proc_info[256];
                if (check_port_in_use(new_base + i, proc_info, sizeof(proc_info)) ||
                    check_port_in_use(new_ai_base + i, proc_info, sizeof(proc_info))) {
                    conflicts_found++;
                }
            }
            
            if (conflicts_found > 0) {
                printf("  ⚠️  %d ports in use in selected ranges\n", conflicts_found);
                printf("  Proceed anyway? [y/N]: ");
                if (!fgets(choice, sizeof(choice), stdin) || choice[0] != 'y') {
                    return -1;
                }
            } else {
                printf("  ✓ All ports available\n");
            }
            
            opts->port_base = new_base;
            opts->ai_port_base = new_ai_base;
            return 0;
        }
        
        case '4':
        default:
            printf("Installation cancelled\n");
            return -1;
    }
    
    return -1;
}

/* Register installation in till-private.json */
int register_installation(install_options_t *opts) {
    char config_path[TILL_MAX_PATH];
    char abs_path[TILL_MAX_PATH];
    cJSON *root = NULL;
    cJSON *installations = NULL;
    FILE *fp;
    
    /* Get absolute path */
    if (get_absolute_path(opts->path, abs_path, sizeof(abs_path)) != 0) {
        return -1;
    }
    
    /* Build config path */
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    /* Load existing config or create new */
    fp = fopen(config_path, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char *content = malloc(size + 1);
        fread(content, 1, size, fp);
        content[size] = '\0';
        fclose(fp);
        
        root = cJSON_Parse(content);
        free(content);
    }
    
    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "format", "till-private-v1");
        cJSON_AddStringToObject(root, "version", "1.0.0");
    }
    
    /* Get or create installations object */
    installations = cJSON_GetObjectItem(root, "installations");
    if (!installations) {
        installations = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "installations", installations);
    }
    
    /* Add this installation */
    cJSON *install = cJSON_CreateObject();
    cJSON_AddStringToObject(install, "root", abs_path);
    cJSON_AddNumberToObject(install, "port_base", opts->port_base);
    cJSON_AddNumberToObject(install, "ai_port_base", opts->ai_port_base);
    cJSON_AddStringToObject(install, "mode", opts->mode);
    
    char timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    cJSON_AddStringToObject(install, "installed", timestamp);
    
    cJSON_AddItemToObject(installations, opts->name, install);
    
    /* Mark as primary if it's the first installation */
    if (cJSON_GetArraySize(installations) == 1) {
        cJSON_AddStringToObject(root, "primary_tekton", opts->name);
    }
    
    /* Write back to file */
    char *json_str = cJSON_Print(root);
    fp = fopen(config_path, "w");
    if (fp) {
        fprintf(fp, "%s\n", json_str);
        fclose(fp);
        printf("Registered installation in till-private.json\n");
    }
    
    free(json_str);
    cJSON_Delete(root);
    
    return 0;
}

/* Get primary Tekton name */
int get_primary_tekton_name(char *name, size_t size) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    cJSON *root = NULL;
    
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    fp = fopen(config_path, "r");
    if (!fp) {
        return -1;  /* No config yet */
    }
    
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    content[fsize] = '\0';
    fclose(fp);
    
    root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        return -1;
    }
    
    cJSON *primary = cJSON_GetObjectItem(root, "primary_tekton");
    if (primary && primary->valuestring) {
        strncpy(name, primary->valuestring, size);
        cJSON_Delete(root);
        return 0;
    }
    
    cJSON_Delete(root);
    return -1;
}

/* Validate name (check if already exists) */
int validate_name(const char *name) {
    /* TODO: Check federation registry for name conflicts */
    
    /* For now, just validate format */
    if (strlen(name) == 0 || strlen(name) >= TILL_MAX_NAME) {
        return -1;
    }
    
    /* Suggest format but don't enforce */
    if (!strchr(name, '.')) {
        printf("Tip: Consider using format: name.category.geography\n");
    }
    
    return 0;
}

/* Fuzzy match name (case-insensitive partial matching) */
int fuzzy_match_name(const char *input, char *matched, size_t size) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    cJSON *root = NULL;
    char lower_input[TILL_MAX_NAME];
    
    /* Convert input to lowercase */
    int i;
    for (i = 0; input[i] && i < TILL_MAX_NAME - 1; i++) {
        lower_input[i] = tolower(input[i]);
    }
    lower_input[i] = '\0';
    
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    fp = fopen(config_path, "r");
    if (!fp) {
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    content[fsize] = '\0';
    fclose(fp);
    
    root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(root, "installations");
    if (!installations) {
        cJSON_Delete(root);
        return -1;
    }
    
    /* Search for fuzzy match */
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, installations) {
        char lower_name[TILL_MAX_NAME];
        const char *name = entry->string;
        
        /* Convert name to lowercase */
        for (i = 0; name[i] && i < TILL_MAX_NAME - 1; i++) {
            lower_name[i] = tolower(name[i]);
        }
        lower_name[i] = '\0';
        
        /* Check if input matches any part of the name */
        if (strstr(lower_name, lower_input)) {
            strncpy(matched, name, size);
            cJSON_Delete(root);
            return 0;
        }
    }
    
    cJSON_Delete(root);
    return -1;  /* No match found */
}
