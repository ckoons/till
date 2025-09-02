/*
 * till.c - Main program for Till (Tekton Lifecycle Manager)
 * 
 * Manages Tekton installations, updates, and federation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include "till_config.h"
#include "till_install.h"
#include "cJSON.h"

/* Command structure */
typedef enum {
    CMD_NONE,
    CMD_HELP,
    CMD_SYNC,
    CMD_WATCH,
    CMD_INSTALL,
    CMD_UNINSTALL,
    CMD_HOLD,
    CMD_RELEASE,
    CMD_UPDATE,
    CMD_HOST,
    CMD_FEDERATE,
    CMD_STATUS
} command_t;

typedef enum {
    SUBCMD_NONE,
    SUBCMD_ADD,
    SUBCMD_REMOVE,
    SUBCMD_LIST,
    SUBCMD_INIT,
    SUBCMD_LEAVE,
    SUBCMD_CHECK,
    SUBCMD_APPLY
} subcommand_t;

/* Function prototypes */
static void print_usage(const char *program);
static void print_version(void);
static command_t parse_command(const char *cmd);
static subcommand_t parse_subcommand(const char *subcmd);
static int cmd_help(int argc, char *argv[]);
static int cmd_sync(int argc, char *argv[]);
static int cmd_watch(int argc, char *argv[]);
static int cmd_install(int argc, char *argv[]);
static int cmd_uninstall(int argc, char *argv[]);
static int cmd_hold(int argc, char *argv[]);
static int cmd_release(int argc, char *argv[]);
static int cmd_update(int argc, char *argv[]);
static int cmd_host(int argc, char *argv[]);
static int cmd_federate(int argc, char *argv[]);
static int cmd_status(int argc, char *argv[]);
static int dry_run(void);
static int ensure_directories(void);
static int run_command(const char *cmd);
static int file_exists(const char *path);
static int create_directory(const char *path);

/* Main entry point */
int main(int argc, char *argv[]) {
    /* No arguments - show dry run */
    if (argc == 1) {
        return dry_run();
    }
    
    /* Parse first argument */
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        print_version();
        return EXIT_SUCCESS;
    }
    
    /* Ensure Till directories exist */
    if (ensure_directories() != 0) {
        fprintf(stderr, "Error: Failed to create Till directories\n");
        return EXIT_FILE_ERROR;
    }
    
    /* Parse and execute command */
    command_t cmd = parse_command(argv[1]);
    
    switch (cmd) {
        case CMD_SYNC:
            return cmd_sync(argc - 1, argv + 1);
        case CMD_WATCH:
            return cmd_watch(argc - 1, argv + 1);
        case CMD_INSTALL:
            return cmd_install(argc - 1, argv + 1);
        case CMD_UNINSTALL:
            return cmd_uninstall(argc - 1, argv + 1);
        case CMD_HOLD:
            return cmd_hold(argc - 1, argv + 1);
        case CMD_RELEASE:
            return cmd_release(argc - 1, argv + 1);
        case CMD_UPDATE:
            return cmd_update(argc - 1, argv + 1);
        case CMD_HOST:
            return cmd_host(argc - 1, argv + 1);
        case CMD_FEDERATE:
            return cmd_federate(argc - 1, argv + 1);
        case CMD_STATUS:
            return cmd_status(argc - 1, argv + 1);
        case CMD_HELP:
            return cmd_help(argc - 1, argv + 1);
        default:
            fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
            fprintf(stderr, "Try 'till --help' for usage information\n");
            return EXIT_USAGE_ERROR;
    }
}

/* Print usage information */
static void print_usage(const char *program) {
    printf("Till - Tekton Lifecycle Manager v%s\n\n", TILL_VERSION);
    printf("Usage: %s [command] [options]\n\n", program);
    printf("Commands:\n");
    printf("  (none)              Dry run - show what sync would do\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  sync                Run synchronization now\n");
    printf("  watch [hours]       Set watch daemon frequency (default: 24h)\n");
    printf("  install [options]   Install Tekton or components\n");
    printf("  uninstall <name>    Uninstall component\n");
    printf("  hold <component>    Prevent component updates\n");
    printf("  release <component> Allow component updates\n");
    printf("  update check        Check for available updates\n");
    printf("  update apply        Apply all pending updates\n");
    printf("  host add <name> <address> <path>  Add host\n");
    printf("  host remove <name>  Remove host\n");
    printf("  host list           List configured hosts\n");
    printf("  federate init       Initialize federation\n");
    printf("  federate leave      Leave federation\n");
    printf("  federate status     Show federation status\n");
    printf("  status              Show Till status\n");
    printf("\nExamples:\n");
    printf("  till                      # Show what would be synced\n");
    printf("  till sync                 # Synchronize now\n");
    printf("  till install              # Install Tekton\n");
    printf("  till install --mode solo  # Solo installation\n");
    printf("  till federate init --mode observer --name kant.philosophy.us\n");
    printf("  till host add laptop localhost /Users/casey/Tekton\n");
}

/* Print version information */
static void print_version(void) {
    printf("Till version %s\n", TILL_VERSION);
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Config version: %s\n", TILL_CONFIG_VERSION);
}

/* Parse command string */
static command_t parse_command(const char *cmd) {
    if (strcmp(cmd, "sync") == 0) return CMD_SYNC;
    if (strcmp(cmd, "watch") == 0) return CMD_WATCH;
    if (strcmp(cmd, "install") == 0) return CMD_INSTALL;
    if (strcmp(cmd, "uninstall") == 0) return CMD_UNINSTALL;
    if (strcmp(cmd, "hold") == 0) return CMD_HOLD;
    if (strcmp(cmd, "release") == 0) return CMD_RELEASE;
    if (strcmp(cmd, "update") == 0) return CMD_UPDATE;
    if (strcmp(cmd, "host") == 0) return CMD_HOST;
    if (strcmp(cmd, "federate") == 0) return CMD_FEDERATE;
    if (strcmp(cmd, "status") == 0) return CMD_STATUS;
    if (strcmp(cmd, "help") == 0) return CMD_HELP;
    return CMD_NONE;
}

/* Parse subcommand string */
static subcommand_t parse_subcommand(const char *subcmd) {
    if (!subcmd) return SUBCMD_NONE;
    if (strcmp(subcmd, "add") == 0) return SUBCMD_ADD;
    if (strcmp(subcmd, "remove") == 0) return SUBCMD_REMOVE;
    if (strcmp(subcmd, "list") == 0) return SUBCMD_LIST;
    if (strcmp(subcmd, "init") == 0) return SUBCMD_INIT;
    if (strcmp(subcmd, "leave") == 0) return SUBCMD_LEAVE;
    if (strcmp(subcmd, "check") == 0) return SUBCMD_CHECK;
    if (strcmp(subcmd, "apply") == 0) return SUBCMD_APPLY;
    return SUBCMD_NONE;
}

/* Dry run - show what sync would do */
static int dry_run(void) {
    printf("Till v%s - Dry Run Mode\n", TILL_VERSION);
    printf("=====================================\n\n");
    
    /* Check for menu updates */
    printf("Checking for updates from: %s\n", TILL_PUBLIC_URL);
    
    /* Check installed components */
    char installed_path[TILL_MAX_PATH];
    snprintf(installed_path, sizeof(installed_path), "%s/%s", 
             TILL_INSTALLED_DIR, "installed.json");
    
    if (file_exists(installed_path)) {
        printf("\nInstalled components:\n");
        printf("  [Would check versions and updates]\n");
    } else {
        printf("\nNo components installed yet.\n");
        printf("Run 'till install' to install Tekton.\n");
    }
    
    /* Check federation status */
    char private_config[TILL_MAX_PATH];
    snprintf(private_config, sizeof(private_config), "%s/%s",
             TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
    if (file_exists(private_config)) {
        printf("\nFederation status:\n");
        printf("  [Would check federation configuration]\n");
    } else {
        printf("\nNo federation configured.\n");
    }
    
    printf("\nTo execute these actions, run: till sync\n");
    return EXIT_SUCCESS;
}

/* Command: sync */
static int cmd_sync(int argc, char *argv[]) {
    printf("Running Till sync...\n");
    
    /* TODO: Implement actual sync logic */
    printf("1. Fetching menu-of-the-day from GitHub...\n");
    printf("2. Checking component versions...\n");
    printf("3. Updating components (respecting holds)...\n");
    printf("4. Updating federation registry...\n");
    
    printf("\nSync complete.\n");
    return EXIT_SUCCESS;
}

/* Command: watch */
static int cmd_watch(int argc, char *argv[]) {
    int hours = TILL_DEFAULT_WATCH_HOURS;
    
    if (argc > 1) {
        hours = atoi(argv[1]);
        if (hours <= 0) {
            fprintf(stderr, "Error: Invalid watch frequency\n");
            return EXIT_USAGE_ERROR;
        }
    }
    
    printf("Setting watch frequency to %d hours\n", hours);
    /* TODO: Implement watch daemon */
    return EXIT_SUCCESS;
}

/* Command: install */
static int cmd_install(int argc, char *argv[]) {
    install_options_t opts = {0};
    
    /* Check if till-private.json exists, if not, run discovery */
    char private_config[TILL_MAX_PATH];
    snprintf(private_config, sizeof(private_config), "%s/%s",
             TILL_FEDERATION_DIR, TILL_PRIVATE_CONFIG);
    
    if (!file_exists(private_config)) {
        printf("No Till configuration found. Discovering existing installations...\n");
        discover_tektons();
    }
    
    /* Set defaults */
    strcpy(opts.mode, MODE_SOLO);
    opts.port_base = DEFAULT_PORT_BASE;
    opts.ai_port_base = DEFAULT_AI_PORT_BASE;
    
    /* Parse arguments */
    if (argc < 2 || strcmp(argv[1], "tekton") != 0) {
        fprintf(stderr, "Usage: till install tekton [options]\n");
        return EXIT_USAGE_ERROR;
    }
    
    /* Parse options */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            strncpy(opts.mode, argv[++i], sizeof(opts.mode) - 1);
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            strncpy(opts.name, argv[++i], sizeof(opts.name) - 1);
            
            /* If name looks like Coder-X, set mode and path accordingly */
            if (strncmp(opts.name, "Coder-", 6) == 0 && strlen(opts.name) == 7) {
                char mode[32];
                snprintf(mode, sizeof(mode), "coder-%c", tolower(opts.name[6]));
                strncpy(opts.mode, mode, sizeof(opts.mode) - 1);
                
                /* Set path if not already specified */
                if (strlen(opts.path) == 0) {
                    snprintf(opts.path, sizeof(opts.path), "../%s", opts.name);
                }
                /* Clear name since Coder-X will auto-generate it */
                opts.name[0] = '\0';
            }
        } else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
            strncpy(opts.path, argv[++i], sizeof(opts.path) - 1);
        } else if (strcmp(argv[i], "--port-base") == 0 && i + 1 < argc) {
            opts.port_base = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ai-port-base") == 0 && i + 1 < argc) {
            opts.ai_port_base = atoi(argv[++i]);
        }
    }
    
    /* Handle Coder-X mode */
    if (strncmp(opts.mode, "coder-", 6) == 0) {
        /* Auto-allocate ports for Coder-X */
        if (allocate_ports(&opts) != 0) {
            fprintf(stderr, "Failed to allocate ports\n");
            return EXIT_GENERAL_ERROR;
        }
        
        /* Auto-generate name based on primary Tekton */
        if (strlen(opts.name) == 0) {
            char primary[TILL_MAX_NAME];
            if (get_primary_tekton_name(primary, sizeof(primary)) == 0) {
                snprintf(opts.name, sizeof(opts.name), "%s.%s", primary, opts.mode);
            } else {
                fprintf(stderr, "No primary Tekton found. Install primary first.\n");
                return EXIT_GENERAL_ERROR;
            }
        }
        
        /* Set default path - sibling to till */
        if (strlen(opts.path) == 0) {
            char coder_dir[TILL_MAX_PATH];
            snprintf(coder_dir, sizeof(coder_dir), "../Coder-%c", 
                     toupper(opts.mode[6]));
            strncpy(opts.path, coder_dir, sizeof(opts.path) - 1);
        }
    } else {
        /* Regular installation - require name for non-solo */
        if (strlen(opts.name) == 0) {
            if (strcmp(opts.mode, MODE_SOLO) != 0) {
                fprintf(stderr, "Error: --name required for %s mode\n", opts.mode);
                return EXIT_USAGE_ERROR;
            } else {
                strcpy(opts.name, "tekton-solo");
            }
        }
        
        /* Validate name */
        if (validate_name(opts.name) != 0) {
            fprintf(stderr, "Error: Invalid name '%s'\n", opts.name);
            return EXIT_USAGE_ERROR;
        }
        
        /* Set default path if not specified - sibling to till */
        if (strlen(opts.path) == 0) {
            strcpy(opts.path, "../Tekton");
        }
    }
    
    /* Run installation */
    return till_install_tekton(&opts);
}

/* Command: uninstall */
static int cmd_uninstall(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Component name required\n");
        return EXIT_USAGE_ERROR;
    }
    
    printf("Uninstalling %s...\n", argv[1]);
    /* TODO: Implement uninstall logic */
    return EXIT_SUCCESS;
}

/* Command: hold */
static int cmd_hold(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Component name required\n");
        return EXIT_USAGE_ERROR;
    }
    
    printf("Holding %s (preventing updates)...\n", argv[1]);
    /* TODO: Implement hold logic */
    return EXIT_SUCCESS;
}

/* Command: release */
static int cmd_release(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Component name required\n");
        return EXIT_USAGE_ERROR;
    }
    
    printf("Releasing hold on %s...\n", argv[1]);
    /* TODO: Implement release logic */
    return EXIT_SUCCESS;
}

/* Command: update */
static int cmd_update(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Subcommand required (check or apply)\n");
        return EXIT_USAGE_ERROR;
    }
    
    subcommand_t subcmd = parse_subcommand(argv[1]);
    
    switch (subcmd) {
        case SUBCMD_CHECK:
            printf("Checking for updates...\n");
            /* TODO: Implement update check */
            break;
        case SUBCMD_APPLY:
            printf("Applying updates...\n");
            /* TODO: Implement update apply */
            break;
        default:
            fprintf(stderr, "Error: Unknown subcommand '%s'\n", argv[1]);
            return EXIT_USAGE_ERROR;
    }
    
    return EXIT_SUCCESS;
}

/* Command: host */
static int cmd_host(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Subcommand required (add, remove, list)\n");
        return EXIT_USAGE_ERROR;
    }
    
    subcommand_t subcmd = parse_subcommand(argv[1]);
    
    switch (subcmd) {
        case SUBCMD_ADD:
            if (argc < 5) {
                fprintf(stderr, "Error: Usage: till host add <name> <address> <path>\n");
                return EXIT_USAGE_ERROR;
            }
            printf("Adding host %s at %s with path %s\n", argv[2], argv[3], argv[4]);
            /* TODO: Implement host add */
            break;
            
        case SUBCMD_REMOVE:
            if (argc < 3) {
                fprintf(stderr, "Error: Host name required\n");
                return EXIT_USAGE_ERROR;
            }
            printf("Removing host %s\n", argv[2]);
            /* TODO: Implement host remove */
            break;
            
        case SUBCMD_LIST:
            printf("Configured hosts:\n");
            /* TODO: Implement host list */
            break;
            
        default:
            fprintf(stderr, "Error: Unknown subcommand '%s'\n", argv[1]);
            return EXIT_USAGE_ERROR;
    }
    
    return EXIT_SUCCESS;
}

/* Command: federate */
static int cmd_federate(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Subcommand required (init, leave, status)\n");
        return EXIT_USAGE_ERROR;
    }
    
    if (strcmp(argv[1], "init") == 0) {
        const char *mode = MODE_OBSERVER;
        const char *name = NULL;
        
        /* Parse options */
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
                mode = argv[++i];
            } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
                name = argv[++i];
            }
        }
        
        printf("Initializing federation...\n");
        printf("Mode: %s\n", mode);
        if (name) {
            printf("Name: %s\n", name);
        }
        /* TODO: Implement federate init */
        
    } else if (strcmp(argv[1], "leave") == 0) {
        printf("Leaving federation...\n");
        /* TODO: Implement federate leave */
        
    } else if (strcmp(argv[1], "status") == 0) {
        printf("Federation status:\n");
        /* TODO: Implement federate status */
        
    } else {
        fprintf(stderr, "Error: Unknown subcommand '%s'\n", argv[1]);
        return EXIT_USAGE_ERROR;
    }
    
    return EXIT_SUCCESS;
}

/* Command: status */
static int cmd_status(int argc, char *argv[]) {
    printf("Till Status\n");
    printf("===========\n");
    printf("Version: %s\n", TILL_VERSION);
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Config: %s\n", TILL_CONFIG_VERSION);
    
    /* TODO: Show actual status */
    printf("\nComponents: [To be implemented]\n");
    printf("Federation: [To be implemented]\n");
    printf("Hosts: [To be implemented]\n");
    
    return EXIT_SUCCESS;
}

/* Command: help */
static int cmd_help(int argc, char *argv[]) {
    print_usage("till");
    return EXIT_SUCCESS;
}

/* Ensure Till directories exist */
static int ensure_directories(void) {
    const char *dirs[] = {
        TILL_HOME,
        TILL_CONFIG_DIR,
        TILL_TEKTON_DIR,
        TILL_FEDERATION_DIR,
        TILL_INSTALLED_DIR,
        NULL
    };
    
    for (int i = 0; dirs[i] != NULL; i++) {
        if (create_directory(dirs[i]) != 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Create directory if it doesn't exist */
static int create_directory(const char *path) {
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "Error creating directory %s: %s\n", 
                        path, strerror(errno));
                return -1;
            }
        }
    }
    
    return 0;
}

/* Check if file exists */
static int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/* Run shell command */
static int run_command(const char *cmd) {
    if (TILL_DEBUG) {
        printf("[DEBUG] Running: %s\n", cmd);
    }
    
    int result = system(cmd);
    
    if (result == -1) {
        fprintf(stderr, "Error: Failed to execute command\n");
        return EXIT_GENERAL_ERROR;
    }
    
    if (WIFEXITED(result)) {
        return WEXITSTATUS(result);
    }
    
    return EXIT_GENERAL_ERROR;
}