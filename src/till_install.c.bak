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
#include <signal.h>

#include "till_install.h"
#include "till_common.h"
#include "till_registry.h"
#include "till_tekton.h"
#include "cJSON.h"

/* External global from till.c */
extern int g_interactive;

/* Forward declarations */

/* ensure_directory is now in till_common.c */

/* Main installation function */
int till_install_tekton(install_options_t *opts) {
    /* Delegate to Tekton-specific installer in till_tekton.c */
    return install_tekton(opts);
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

/* Wrapper for new process utilities */
static int check_port_in_use(int port, char *process_info, size_t info_size) {
    process_info_t info;
    int pid = find_process_by_port(port, &info);
    
    if (pid > 0 && process_info && info_size > 0) {
        snprintf(process_info, info_size, "%s (PID: %d)", info.name, info.pid);
    }
    
    return pid;
}

/* Wrapper for process killing */
static int kill_process(int pid) {
    return kill_process_graceful(pid, 1000);  /* 1 second timeout */
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
            if (!is_port_available(port + offset)) {
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

