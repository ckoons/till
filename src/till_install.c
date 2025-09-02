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

#include "till_install.h"
#include "cJSON.h"

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
static int get_primary_tekton_path(char *path, size_t size) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    cJSON *root = NULL;
    
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
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
    
    /* Common installation locations to check */
    const char *search_paths[] = {
        "./Tekton",
        "./Coder-A",
        "./Coder-B", 
        "./Coder-C",
        "./Coder-D",
        "./Coder-E",
        "../Tekton",
        "../Coder-A",
        "../Coder-B",
        "../Coder-C",
        "../Coder-D",
        "../Coder-E",
        "~/Tekton",
        "~/projects/Tekton",
        "~/projects/Coder-A",
        "~/projects/Coder-B",
        "~/projects/github/Tekton",
        "~/projects/github/Coder-A",
        "~/projects/github/Coder-B",
        "~/projects/github/Coder-C",
        NULL
    };
    
    cJSON *root = cJSON_CreateObject();
    cJSON *installations = cJSON_CreateObject();
    char *primary_name = NULL;
    int found_count = 0;
    
    /* Search each potential location */
    for (int i = 0; search_paths[i]; i++) {
        char expanded_path[TILL_MAX_PATH];
        char abs_path[TILL_MAX_PATH];
        
        /* Expand ~ to home directory */
        if (search_paths[i][0] == '~') {
            struct passwd *pw = getpwuid(getuid());
            snprintf(expanded_path, sizeof(expanded_path), "%s%s", 
                    pw->pw_dir, search_paths[i] + 1);
        } else {
            strcpy(expanded_path, search_paths[i]);
        }
        
        /* Check if directory exists */
        if (!dir_exists(expanded_path)) {
            continue;
        }
        
        /* Look for .env.local */
        char env_path[TILL_MAX_PATH];
        snprintf(env_path, sizeof(env_path), "%s/.env.local", expanded_path);
        
        FILE *fp = fopen(env_path, "r");
        if (!fp) {
            continue;
        }
        
        /* Parse .env.local to extract TEKTON_REGISTRY_NAME and ports */
        char line[512];
        char registry_name[TILL_MAX_NAME] = "";
        int port_base = -1;
        int ai_port_base = -1;
        
        while (fgets(line, sizeof(line), fp)) {
            char key[128], value[384];
            if (sscanf(line, "%127[^=]=%383s", key, value) == 2) {
                if (strcmp(key, "TEKTON_REGISTRY_NAME") == 0) {
                    strcpy(registry_name, value);
                } else if (strcmp(key, "TEKTON_PORT_BASE") == 0) {
                    port_base = atoi(value);
                } else if (strcmp(key, "TEKTON_AI_PORT_BASE") == 0) {
                    ai_port_base = atoi(value);
                }
            }
        }
        fclose(fp);
        
        /* If no registry name but we have port info, generate a name */
        if (strlen(registry_name) == 0 && port_base >= 0) {
            /* Determine name based on directory and port */
            const char *dir_name = strrchr(expanded_path, '/');
            if (dir_name) {
                dir_name++; /* Skip the slash */
                if (strcmp(dir_name, "Tekton") == 0) {
                    if (port_base == 8000) {
                        strcpy(registry_name, "primary.tekton.local");
                    } else {
                        snprintf(registry_name, sizeof(registry_name), "tekton.port%d.local", port_base);
                    }
                } else if (strncmp(dir_name, "Coder-", 6) == 0) {
                    char letter = dir_name[6];
                    snprintf(registry_name, sizeof(registry_name), "primary.tekton.local.coder-%c", tolower(letter));
                } else {
                    strcpy(registry_name, dir_name);
                }
            }
        }
        
        if (strlen(registry_name) > 0 || port_base >= 0) {
            /* Get absolute path */
            if (get_absolute_path(expanded_path, abs_path, sizeof(abs_path)) != 0) {
                continue;
            }
            
            /* Check if we already have this installation (deduplication) */
            cJSON *existing = cJSON_GetObjectItem(installations, registry_name);
            if (existing) {
                /* Already found this installation, skip duplicate */
                continue;
            }
            
            printf("  Found: %s at %s\n", registry_name, abs_path);
            
            /* Add to installations */
            cJSON *install = cJSON_CreateObject();
            cJSON_AddStringToObject(install, "path", abs_path);
            cJSON_AddNumberToObject(install, "port_base", port_base);
            cJSON_AddNumberToObject(install, "ai_port_base", ai_port_base);
            
            /* Determine mode based on name */
            if (strstr(registry_name, ".coder-")) {
                cJSON_AddStringToObject(install, "mode", "coder");
            } else if (!strchr(registry_name, '.')) {
                cJSON_AddStringToObject(install, "mode", "solo");
            } else {
                cJSON_AddStringToObject(install, "mode", "member");
                if (!primary_name) {
                    primary_name = strdup(registry_name);
                }
            }
            
            cJSON_AddItemToObject(installations, registry_name, install);
            found_count++;
        }
    }
    
    if (found_count > 0) {
        printf("Found %d Tekton installation(s)\n", found_count);
        
        /* Set primary if found */
        if (primary_name) {
            cJSON_AddStringToObject(root, "primary_tekton", primary_name);
            free(primary_name);
        }
        
        cJSON_AddItemToObject(root, "installations", installations);
        
        /* Save to till-private.json */
        ensure_directory(TILL_FEDERATION_DIR);
        
        char config_path[TILL_MAX_PATH];
        snprintf(config_path, sizeof(config_path), 
                "%s/%s", TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
        
        FILE *fp = fopen(config_path, "w");
        if (fp) {
            char *json_str = cJSON_Print(root);
            fprintf(fp, "%s\n", json_str);
            fclose(fp);
            free(json_str);
            printf("Saved discovery results to %s\n", config_path);
        }
    } else {
        printf("No existing Tekton installations found\n");
        cJSON_Delete(installations);
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
    FILE *fp;
    
    /* Get absolute path for TEKTON_ROOT */
    if (get_absolute_path(opts->path, abs_path, sizeof(abs_path)) != 0) {
        fprintf(stderr, "Failed to get absolute path\n");
        return -1;
    }
    
    /* Build .env.local path */
    snprintf(env_path, sizeof(env_path), "%s/.env.local", opts->path);
    
    /* Check if .env.local already exists */
    if (access(env_path, F_OK) == 0) {
        printf(".env.local already exists, skipping generation\n");
        return 0;
    }
    
    /* For primary Tekton with default ports, copy .env.local.example */
    if (opts->port_base == DEFAULT_PORT_BASE && !opts->is_coder) {
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
    
    /* Write Tekton Root */
    fprintf(fp, "# Tekton Root (for this Tekton environment)\n");
    fprintf(fp, "TEKTON_ROOT=%s\n", abs_path);
    fprintf(fp, "\n");
    
    /* Write Main Tekton Root */
    fprintf(fp, "# Main Tekton Root (primary Tekton on this host)\n");
    char main_root[TILL_MAX_PATH] = "";
    get_primary_tekton_path(main_root, sizeof(main_root));
    if (strlen(main_root) > 0) {
        fprintf(fp, "TEKTON_MAIN_ROOT=%s\n", main_root);
    } else {
        /* If this is the first/primary installation, use this path */
        fprintf(fp, "TEKTON_MAIN_ROOT=%s\n", abs_path);
    }
    fprintf(fp, "\n");
    
    /* Write Registry Name */
    fprintf(fp, "# Tekton Registry Name\n");
    if (strcmp(opts->mode, MODE_SOLO) == 0 || opts->is_coder) {
        fprintf(fp, "TEKTON_REGISTRY_NAME=None\n");
    } else {
        fprintf(fp, "TEKTON_REGISTRY_NAME=%s\n", opts->name);
    }
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
    
    /* Copy other environment variables from .env.local.example if available */
    char example_path[TILL_MAX_PATH];
    snprintf(example_path, sizeof(example_path), 
             "%s/.env.local.example", opts->path);
    
    FILE *example_fp = fopen(example_path, "r");
    if (example_fp) {
        char line[1024];
        int in_other_section = 0;
        
        fprintf(fp, "# Other Configuration (from .env.local.example)\n");
        
        while (fgets(line, sizeof(line), example_fp)) {
            /* Skip lines we've already written */
            if (strstr(line, "TEKTON_ROOT=") ||
                strstr(line, "TEKTON_PORT_BASE=") ||
                strstr(line, "TEKTON_AI_PORT_BASE=") ||
                strstr(line, "_PORT=") ||
                strstr(line, "_AI_PORT=")) {
                continue;
            }
            
            /* Skip comment blocks about ports */
            if (strstr(line, "# Component Ports") ||
                strstr(line, "# AI Specialist Ports") ||
                strstr(line, "# Tekton Port Ranges")) {
                in_other_section = 0;
                continue;
            }
            
            /* Check for start of non-port section */
            if (line[0] == '#' && !strstr(line, "Port")) {
                in_other_section = 1;
            }
            
            /* Write other configuration */
            if (in_other_section || 
                (line[0] != '#' && strchr(line, '=') && !strstr(line, "PORT"))) {
                fprintf(fp, "%s", line);
            }
        }
        
        fclose(example_fp);
    }
    
    fclose(fp);
    
    printf("Generated .env.local with custom ports\n");
    return 0;
}

/* Allocate ports for installation */
int allocate_ports(install_options_t *opts) {
    /* Parse mode to determine if this is a Coder-X installation */
    if (strncmp(opts->mode, "coder-", 6) == 0) {
        opts->is_coder = 1;
        opts->coder_letter = toupper(opts->mode[6]);
        
        /* Calculate port offset based on letter (A=1, B=2, C=3, etc) */
        int offset = opts->coder_letter - 'A' + 1;
        opts->port_base = DEFAULT_PORT_BASE + (offset * 100);
        opts->ai_port_base = DEFAULT_AI_PORT_BASE + (offset * 100);
        
        printf("Allocated ports for Coder-%c: base=%d, ai=%d\n",
               opts->coder_letter, opts->port_base, opts->ai_port_base);
    }
    
    /* TODO: Check for port conflicts */
    
    return 0;
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
             "%s/%s", TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
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
    cJSON_AddStringToObject(install, "path", abs_path);
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

/* Get primary Tekton name for auto-naming Coder-X */
int get_primary_tekton_name(char *name, size_t size) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    cJSON *root = NULL;
    
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
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
             "%s/%s", TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
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