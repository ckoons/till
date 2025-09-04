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
#include <stdarg.h>

#include "till_config.h"
#include "till_install.h"
#include "cJSON.h"

/* Global flags */
int g_interactive = 0;
static FILE *g_log_file = NULL;
static time_t g_start_time = 0;

/* Log levels */
typedef enum {
    LOG_START,
    LOG_END,
    LOG_INFO,
    LOG_DISCOVER,
    LOG_FOUND,
    LOG_CHANGE,
    LOG_UPDATE,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

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

/* External functions from till_install.c */
extern int suggest_next_port_range(int *main_port, int *ai_port);
extern int get_primary_tekton_path(char *path, size_t size);
static command_t parse_command(const char *cmd);
static subcommand_t parse_subcommand(const char *subcmd);
static int cmd_help(int argc, char *argv[]);
static int cmd_sync(int argc, char *argv[]);
static int cmd_watch(int argc, char *argv[]);
static int cmd_install(int argc, char *argv[]);
static int cmd_uninstall(int argc, char *argv[]);
static int cmd_hold(int argc, char *argv[]);
static int cmd_release(int argc, char *argv[]);
static int cmd_host(int argc, char *argv[]);
static int cmd_federate(int argc, char *argv[]);
static int cmd_status(int argc, char *argv[]);
static int dry_run(void);
static int ensure_directories(void);
static int ensure_discovery(void);
static void init_logging(void);
static void close_logging(void);
static void till_log(log_level_t level, const char *format, ...);
/* static int run_command(const char *cmd); */ /* Currently unused */
static int file_exists(const char *path);
static int dir_exists(const char *path);
static int get_absolute_path(const char *relative, char *absolute, size_t size);
static int create_directory(const char *path);

/* Main entry point */
int main(int argc, char *argv[]) {
    /* First pass - look for global flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            g_interactive = 1;
            /* Remove from argv array */
            for (int j = i; j < argc - 1; j++) {
                argv[j] = argv[j + 1];
            }
            argc--;
            i--; /* Check same position again */
        }
    }
    
    /* Parse help and version first (no setup needed) */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            print_version();
            return EXIT_SUCCESS;
        }
    }
    
    /* Ensure Till directories exist */
    if (ensure_directories() != 0) {
        fprintf(stderr, "Error: Failed to create Till directories\n");
        return EXIT_FILE_ERROR;
    }
    
    /* Initialize logging */
    init_logging();
    
    /* Log command start */
    if (argc > 1) {
        till_log(LOG_START, "till %s", argv[1]);
    } else {
        till_log(LOG_START, "till (dry run)");
    }
    
    /* Always run discovery and verify */
    ensure_discovery();
    
    /* No arguments - show dry run */
    if (argc == 1) {
        int result = dry_run();
        till_log(LOG_END, "till (dry run)");
        close_logging();
        return result;
    }
    
    /* Parse and execute command */
    command_t cmd = parse_command(argv[1]);
    int result = 0;
    
    switch (cmd) {
        case CMD_SYNC:
            result = cmd_sync(argc - 1, argv + 1);
            break;
        case CMD_WATCH:
            result = cmd_watch(argc - 1, argv + 1);
            break;
        case CMD_INSTALL:
            result = cmd_install(argc - 1, argv + 1);
            break;
        case CMD_UNINSTALL:
            result = cmd_uninstall(argc - 1, argv + 1);
            break;
        case CMD_HOLD:
            result = cmd_hold(argc - 1, argv + 1);
            break;
        case CMD_RELEASE:
            result = cmd_release(argc - 1, argv + 1);
            break;
        case CMD_HOST:
            result = cmd_host(argc - 1, argv + 1);
            break;
        case CMD_FEDERATE:
            result = cmd_federate(argc - 1, argv + 1);
            break;
        case CMD_STATUS:
            result = cmd_status(argc - 1, argv + 1);
            break;
        case CMD_HELP:
            result = cmd_help(argc - 1, argv + 1);
            break;
        default:
            fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
            fprintf(stderr, "Try 'till --help' for usage information\n");
            till_log(LOG_ERROR, "Unknown command: %s", argv[1]);
            close_logging();
            return EXIT_USAGE_ERROR;
    }
    
    till_log(LOG_END, "till %s", argv[1]);
    close_logging();
    return result;
}

/* Print usage information */
static void print_usage(const char *program) {
    printf("Till - Tekton Lifecycle Manager v%s\n\n", TILL_VERSION);
    printf("Usage: %s [options] [command] [arguments]\n\n", program);
    printf("Global options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  -i, --interactive   Interactive mode for supported commands\n");
    printf("\nCommands:\n");
    printf("  (none)              Dry run - show what sync would do\n");
    printf("  sync                Run synchronization now\n");
    printf("  watch [hours]       Set watch daemon frequency (default: 24h)\n");
    printf("  install [options]   Install Tekton or components\n");
    printf("  uninstall <name>    Uninstall component\n");
    printf("  hold <component>    Prevent component updates\n");
    printf("  release <component> Allow component updates\n");
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
    printf("Checking for updates from: %s\n", TILL_REPO_URL);
    
    /* Check installed components */
    char installed_path[TILL_MAX_PATH];
    snprintf(installed_path, sizeof(installed_path), "%s/%s", 
             TILL_TEKTON_DIR, "installed.json");
    
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
             TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
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
    (void)argc;  /* Suppress unused parameter warning */
    (void)argv;
    
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
            /* Path will be set in interactive mode based on name */
            /* For non-interactive, use the name */
            snprintf(opts.path, sizeof(opts.path), "../%s", 
                     strlen(opts.name) > 0 ? opts.name : "Tekton");
        }
    }
    
    /* Interactive mode - prompt for confirmation and adjustments */
    if (g_interactive) {
        char input[256];
        
        /* Auto-suggest ports if not set */
        if (opts.port_base == DEFAULT_PORT_BASE && opts.ai_port_base == DEFAULT_AI_PORT_BASE) {
            /* Check if default ports are taken and suggest alternatives */
            suggest_next_port_range(&opts.port_base, &opts.ai_port_base);
        }
        
        printf("\n=== Tekton Interactive Installation ===\n");
        printf("Press Enter to accept defaults, ESC to quit\n\n");
        
        /* Question 1: Name */
        char default_name[TILL_MAX_NAME];
        if (strlen(opts.name) == 0) {
            /* Generate default name based on mode */
            if (strncmp(opts.mode, "coder-", 6) == 0) {
                char letter = toupper(opts.mode[6]);
                snprintf(default_name, sizeof(default_name), "Coder-%c", letter);
            } else {
                get_primary_name(default_name, sizeof(default_name));
            }
            strcpy(opts.name, default_name);
        } else {
            strcpy(default_name, opts.name);
        }
        
        printf("1. Name [%s]: ", default_name);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                strncpy(opts.name, input, sizeof(opts.name) - 1);
            }
        }
        
        /* Question 2: Registry Name */
        printf("2. Tekton Registry Name [%s]: ", 
               strlen(opts.registry_name) > 0 ? opts.registry_name : opts.name);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                strncpy(opts.registry_name, input, sizeof(opts.registry_name) - 1);
            } else if (strlen(opts.registry_name) == 0) {
                strcpy(opts.registry_name, opts.name);
            }
        }
        
        /* Question 3: Path */
        char default_path[TILL_MAX_PATH];
        /* Generate clean absolute path without "../" */
        char current_dir[TILL_MAX_PATH];
        if (getcwd(current_dir, sizeof(current_dir))) {
            /* Get parent directory of current directory */
            char *last_slash = strrchr(current_dir, '/');
            if (last_slash) {
                *last_slash = '\0';  /* Remove last component to get parent */
                snprintf(default_path, sizeof(default_path), "%s/%s", current_dir, opts.name);
            } else {
                snprintf(default_path, sizeof(default_path), "../%s", opts.name);
            }
        } else {
            snprintf(default_path, sizeof(default_path), "../%s", opts.name);
        }
        strcpy(opts.path, default_path);
        
        printf("3. Installation Path [%s]: ", default_path);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                strncpy(opts.path, input, sizeof(opts.path) - 1);
            }
        }
        
        /* Question 4: TEKTON_MAIN_ROOT */
        char default_main_root[TILL_MAX_PATH] = "";
        get_primary_tekton_path(default_main_root, sizeof(default_main_root));
        if (strlen(default_main_root) == 0) {
            /* This will be the first installation */
            get_absolute_path(opts.path, default_main_root, sizeof(default_main_root));
        }
        
        printf("4. TEKTON_MAIN_ROOT [%s]: ", default_main_root);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                strncpy(opts.tekton_main_root, input, sizeof(opts.tekton_main_root) - 1);
            } else {
                strcpy(opts.tekton_main_root, default_main_root);
            }
        }
        
        /* Question 5: Mode */
        printf("5. Mode (solo/observer/member) [%s]: ", opts.mode);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                /* Validate mode */
                if (strcmp(input, "solo") == 0 || 
                    strcmp(input, "observer") == 0 || 
                    strcmp(input, "member") == 0) {
                    strncpy(opts.mode, input, sizeof(opts.mode) - 1);
                } else {
                    printf("  Invalid mode, keeping: %s\n", opts.mode);
                }
            }
        }
        
        /* Question 6: Port Base */
        printf("6. Port Base [%d]: ", opts.port_base);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                int port = atoi(input);
                if (port >= 1024 && port <= 65435) {
                    opts.port_base = port;
                } else {
                    printf("  Invalid port, keeping: %d\n", opts.port_base);
                }
            }
        }
        
        /* Question 7: AI Port Base */
        printf("7. AI Port Base [%d]: ", opts.ai_port_base);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                int port = atoi(input);
                if (port >= 1024 && port <= 65435) {
                    opts.ai_port_base = port;
                } else {
                    printf("  Invalid port, keeping: %d\n", opts.ai_port_base);
                }
            }
        }
        
        /* Show summary and confirm */
        printf("\n=== Installation Summary ===\n");
        printf("  Name:           %s\n", opts.name);
        printf("  Registry Name:  %s\n", 
               strlen(opts.registry_name) > 0 ? opts.registry_name : opts.name);
        printf("  Path:           %s\n", opts.path);
        printf("  Main Root:      %s\n", opts.tekton_main_root);
        printf("  Mode:           %s\n", opts.mode);
        printf("  Port Base:      %d-%d\n", opts.port_base, opts.port_base + 99);
        printf("  AI Port Base:   %d-%d\n", opts.ai_port_base, opts.ai_port_base + 99);
        
        /* Check for issues */
        int has_issues = 0;
        if (dir_exists(opts.path)) {
            printf("\n⚠️  WARNING: Directory already exists: %s\n", opts.path);
            has_issues = 1;
        }
        
        /* Quick port check */
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "lsof -i :%d 2>/dev/null | grep -q LISTEN", opts.port_base);
        if (system(cmd) == 0) {
            printf("⚠️  WARNING: Port %d may be in use\n", opts.port_base);
            has_issues = 1;
        }
        
        if (has_issues) {
            printf("\nThere are warnings. Port conflicts will be handled during installation.\n");
        }
        
        printf("\nProceed with installation? [Y/n]: ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 'n' || input[0] == 'N' || input[0] == 27) {
                printf("Installation cancelled.\n");
                return EXIT_SUCCESS;
            }
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

/* Command: host */
static int cmd_host(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Subcommand required (add, remove, list)\n");
        return EXIT_USAGE_ERROR;
    }
    
    subcommand_t subcmd = parse_subcommand(argv[1]);
    
    switch (subcmd) {
        case SUBCMD_ADD: {
            char name[64], address[256], path[TILL_MAX_PATH];
            
            if (g_interactive && argc < 5) {
                /* Interactive mode - prompt for host details */
                char input[256];
                printf("\n=== Interactive Host Configuration ===\n\n");
                
                /* Get name */
                printf("Host name: ");
                fflush(stdout);
                if (!fgets(input, sizeof(input), stdin)) {
                    return EXIT_USAGE_ERROR;
                }
                input[strcspn(input, "\n")] = 0;
                strncpy(name, input, sizeof(name) - 1);
                
                /* Get address */
                printf("Host address (IP or hostname): ");
                fflush(stdout);
                if (!fgets(input, sizeof(input), stdin)) {
                    return EXIT_USAGE_ERROR;
                }
                input[strcspn(input, "\n")] = 0;
                strncpy(address, input, sizeof(address) - 1);
                
                /* Get path */
                printf("Tekton path on remote host: ");
                fflush(stdout);
                if (!fgets(input, sizeof(input), stdin)) {
                    return EXIT_USAGE_ERROR;
                }
                input[strcspn(input, "\n")] = 0;
                strncpy(path, input, sizeof(path) - 1);
                
                /* Confirm */
                printf("\nAdd host '%s' at %s:%s? [Y/n]: ", name, address, path);
                fflush(stdout);
                if (fgets(input, sizeof(input), stdin)) {
                    if (input[0] == 'n' || input[0] == 'N') {
                        printf("Host addition cancelled.\n");
                        return EXIT_SUCCESS;
                    }
                }
            } else if (argc >= 5) {
                /* Command line arguments provided */
                strncpy(name, argv[2], sizeof(name) - 1);
                strncpy(address, argv[3], sizeof(address) - 1);
                strncpy(path, argv[4], sizeof(path) - 1);
            } else {
                fprintf(stderr, "Error: Usage: till host add <name> <address> <path>\n");
                fprintf(stderr, "       or use --interactive mode\n");
                return EXIT_USAGE_ERROR;
            }
            
            printf("Adding host %s at %s with path %s\n", name, address, path);
            /* TODO: Implement actual host add */
            break;
        }
            
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
    (void)argc;
    (void)argv;
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
    (void)argc;
    (void)argv;
    print_usage("till");
    return EXIT_SUCCESS;
}

/* Ensure discovery has been run */
static int ensure_discovery(void) {
    /* Always run discovery to verify current state */
    till_log(LOG_DISCOVER, "Running discovery to verify installations");
    discover_tektons();
    return 0;
}

/* Ensure Till directories exist */
static int ensure_directories(void) {
    const char *dirs[] = {
        TILL_HOME,
        TILL_CONFIG_DIR,
        TILL_TEKTON_DIR,
        TILL_LOGS_DIR,
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

/* Check if directory exists */
static int dir_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Get absolute path */
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

/* Run shell command - currently unused but may be needed for TODO implementations */
#if 0
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
#endif

/* Initialize logging */
static void init_logging(void) {
    char log_path[TILL_MAX_PATH];
    char date[32];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    
    g_start_time = now;
    
    strftime(date, sizeof(date), "%Y-%m-%d", tm_now);
    snprintf(log_path, sizeof(log_path), "%s/till-%s.log", TILL_LOGS_DIR, date);
    
    g_log_file = fopen(log_path, "a");
    if (!g_log_file) {
        /* Silently fail - logging is not critical */
        return;
    }
}

/* Close logging */
static void close_logging(void) {
    if (g_log_file) {
        if (g_start_time > 0) {
            time_t end_time = time(NULL);
            double elapsed = difftime(end_time, g_start_time);
            fprintf(g_log_file, "[ELAPSED] %.1fs\n\n", elapsed);
        }
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/* Log a message */
static void till_log(log_level_t level, const char *format, ...) {
    if (!g_log_file) return;
    
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_now);
    
    const char *level_str;
    switch (level) {
        case LOG_START:     level_str = "START"; break;
        case LOG_END:       level_str = "END"; break;
        case LOG_INFO:      level_str = "INFO"; break;
        case LOG_DISCOVER:  level_str = "DISCOVER"; break;
        case LOG_FOUND:     level_str = "FOUND"; break;
        case LOG_CHANGE:    level_str = "CHANGE"; break;
        case LOG_UPDATE:    level_str = "UPDATE"; break;
        case LOG_WARNING:   level_str = "WARNING"; break;
        case LOG_ERROR:     level_str = "ERROR"; break;
        default:            level_str = "UNKNOWN"; break;
    }
    
    fprintf(g_log_file, "[%s] [%s] ", timestamp, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}
