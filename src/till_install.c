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
#include "till_common.h"
#include "till_registry.h"
#include "cJSON.h"

/* External global from till.c */
extern int g_interactive;

/* Forward declarations */
static int register_installation_wrapper(install_options_t *opts);

/* Helper: Get absolute path */
static int get_absolute_path(const char *relative, char *absolute, size_t size) {
    if (relative[0] == '/') {
        /* Already absolute */
        strncpy(absolute, relative, size);
        return 0;
    }
    
    /* Use realpath to properly resolve the path including .. components */
    char resolved[TILL_MAX_PATH];
    if (realpath(relative, resolved) != NULL) {
        strncpy(absolute, resolved, size);
        return 0;
    }
    
    /* If realpath fails (e.g., path doesn't exist yet), construct it manually */
    char cwd[TILL_MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return -1;
    }
    
    /* Simple path resolution for non-existent paths */
    if (strncmp(relative, "../", 3) == 0) {
        /* Go up one directory */
        char *last_slash = strrchr(cwd, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(absolute, size, "%s/%s", cwd, relative + 3);
        } else {
            snprintf(absolute, size, "%s/%s", cwd, relative);
        }
    } else {
        snprintf(absolute, size, "%s/%s", cwd, relative);
    }
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

/* ensure_directory is now in till_common.c */

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
    
    /* Step 1.5: Install Python dependencies */
    printf("Installing Python dependencies...\n");
    char pip_cmd[TILL_MAX_PATH * 2];
    snprintf(pip_cmd, sizeof(pip_cmd), 
             "cd %s && pip install -e . > /dev/null 2>&1", opts->path);
    
    printf("  Running pip install (this may take a few minutes)...\n");
    if (system(pip_cmd) != 0) {
        fprintf(stderr, "Warning: Failed to install Python dependencies\n");
        fprintf(stderr, "  You may need to run 'pip install -e .' manually in %s\n", opts->path);
        /* Not a fatal error - continue with installation */
    } else {
        printf("  Python dependencies installed successfully\n");
    }
    
    /* Step 2: Generate .env.local */
    if (generate_env_local(opts) != 0) {
        fprintf(stderr, "Failed to generate .env.local\n");
        return -1;
    }
    
    /* Step 3: Register installation locally */
    if (register_installation_wrapper(opts) != 0) {
        fprintf(stderr, "Failed to register installation\n");
        return -1;
    }
    
    /* Step 3.5: Create .till symlink in the Tekton directory */
    char till_dir[TILL_MAX_PATH];
    char symlink_path[TILL_MAX_PATH];
    char *home = getenv("HOME");
    if (home) {
        /* Point to the actual till/.till directory */
        snprintf(till_dir, sizeof(till_dir), "%s/projects/github/till/.till", home);
        snprintf(symlink_path, sizeof(symlink_path), "%s/.till", opts->path);
        
        /* Remove existing symlink if it exists */
        unlink(symlink_path);
        
        /* Create symlink from Tekton/.till -> ~/.till */
        if (symlink(till_dir, symlink_path) == 0) {
            printf("Created .till symlink in %s\n", opts->path);
        } else {
            fprintf(stderr, "Warning: Could not create .till symlink: %s\n", strerror(errno));
            /* Not a fatal error - continue */
        }
    }
    
    /* Step 4: If observer/member, register with federation */
    if (strcmp(opts->mode, MODE_OBSERVER) == 0 || 
        strcmp(opts->mode, MODE_MEMBER) == 0) {
        printf("Registering with federation as %s...\n", opts->mode);
        /* TODO: Implement federation registration */
    }
    
    printf("\nTekton installation complete!\n");
    printf("Python dependencies have been installed.\n");
    printf("To start: cd %s && tekton start\n", opts->path);
    
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
            if (strlen(opts->registry_name) > 0) {
                fprintf(fp, "TEKTON_REGISTRY_NAME=%s\n", opts->registry_name);
            } else {
                fprintf(fp, "TEKTON_REGISTRY_NAME=None\n");
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
    } else {
        /* Should not happen if interactive mode works correctly */
        fprintf(fp, "TEKTON_REGISTRY_NAME=None\n");
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

/* Wrapper for new register_installation */
static int register_installation_wrapper(install_options_t *opts) {
    return register_installation(opts->name, opts->path, opts->port_base, opts->ai_port_base, opts->mode);
}

