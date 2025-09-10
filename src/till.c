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
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>

#include "till_config.h"
#include "till_install.h"
#include "till_registry.h"
#include "till_host.h"
#include "till_schedule.h"
#include "till_run.h"
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

/* Command handler function type */
typedef int (*command_handler_t)(int argc, char *argv[]);

/* Command structure */
typedef struct {
    const char *name;
    command_handler_t handler;
    const char *description;
    int pass_full_argc;  /* 0 = pass argc-1, argv+1; 1 = pass full argc, argv */
} command_def_t;

/* Function prototypes */
static void print_usage(const char *program);
static void print_version(void);

/* Command handlers */
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
static int cmd_run(int argc, char *argv[]);
static int cmd_update(int argc, char *argv[]);

/* Command table */
static const command_def_t commands[] = {
    {"sync",      cmd_sync,      "Pull updates for all Tekton installations", 0},
    {"watch",     cmd_watch,     "Set watch daemon frequency", 0},
    {"install",   cmd_install,   "Install Tekton or components", 0},
    {"uninstall", cmd_uninstall, "Uninstall component", 0},
    {"hold",      cmd_hold,      "Prevent component updates", 0},
    {"release",   cmd_release,   "Allow component updates", 0},
    {"host",      cmd_host,      "Manage remote hosts", 0},
    {"federate",  cmd_federate,  "Manage federation", 0},
    {"status",    cmd_status,    "Show Till status", 0},
    {"run",       cmd_run,       "Run component command", 1},  /* Special case: needs argc-2, argv+2 */
    {"update",    cmd_update,    "Update Till from git", 0},
    {"help",      cmd_help,      "Show help information", 0},
    {NULL, NULL, NULL, 0}
};
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
static int get_till_parent_dir(char *parent_dir, size_t size);
static int check_till_updates(int quiet_mode);
static int self_update_till(void);
static void rollback_till(const char *backup, const char *target);
static int get_till_directory(char *till_dir, size_t size);

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
    
    /* Find and execute command */
    const command_def_t *cmd = NULL;
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[1], commands[i].name) == 0) {
            cmd = &commands[i];
            break;
        }
    }
    
    int result = 0;
    if (cmd) {
        /* Execute command with appropriate argument passing */
        if (cmd->pass_full_argc || strcmp(cmd->name, "run") == 0) {
            /* Special case for 'run' command - needs argc-2, argv+2 */
            result = cmd->handler(argc - 2, argv + 2);
        } else {
            /* Standard command - pass argc-1, argv+1 */
            result = cmd->handler(argc - 1, argv + 1);
        }
    } else {
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
    
    /* Print commands from table */
    for (int i = 0; commands[i].name != NULL; i++) {
        printf("  %-18s  %s\n", commands[i].name, commands[i].description);
    }
    
    /* Additional host subcommands */
    printf("\nHost subcommands:\n");
    printf("  host add <name> <user>@<host>  Add remote host\n");
    printf("  host test <name>    Test host connectivity\n");
    printf("  host setup <name>   Install Till on remote host\n");
    printf("  host exec <name> <cmd>  Execute command on remote host\n");
    printf("  host ssh <name> [cmd]   Open SSH session to remote host\n");
    printf("  host sync [name]    Sync from remote host(s)\n");
    printf("  host status [name]  Show host status\n");
    printf("  host remove <name>  Remove host from configuration\n");
    printf("\nExamples:\n");
    printf("  till                      # Show what would be synced\n");
    printf("  till sync                 # Synchronize now\n");
    printf("  till install              # Install Tekton\n");
    printf("  till install --mode solo  # Solo installation\n");
    printf("  till host add laptop casey@192.168.1.100\n");
    printf("\nFor detailed help on a command, use:\n");
    printf("  till help <command>     # Show help for specific command\n");
    printf("  till <command> --help   # Alternative help syntax\n\n");
    printf("Examples:\n");
    printf("  till help host          # Show host command help\n");
    printf("  till host --help        # Same as above\n");
    printf("  till help run           # Show run command help\n");
}

/* Print version information */
static void print_version(void) {
    printf("Till version %s\n", TILL_VERSION);
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Config version: %s\n", TILL_CONFIG_VERSION);
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
    
    /* Check for till updates */
    check_till_updates(0);  /* verbose mode */
    
    return EXIT_SUCCESS;
}

/* Command: sync */
static int cmd_sync(int argc, char *argv[]) {
    char config_path[TILL_MAX_PATH];
    FILE *fp;
    int total_updated = 0;
    int total_failed = 0;
    int total_skipped = 0;
    int dry_run = 0;
    
    /* Check for --dry-run option */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-n") == 0) {
            dry_run = 1;
            break;
        }
    }
    
    /* First, check and update till itself if needed (not in dry-run mode) */
    if (!dry_run) {
        int updates = check_till_updates(1);  /* quiet mode */
        if (updates > 0) {
            printf("Updating till first (%d update%s available)...\n", 
                   updates, updates == 1 ? "" : "s");
            if (self_update_till() == 0) {
                /* Will re-exec, so we never get here */
                return EXIT_SUCCESS;
            }
            /* If update failed, continue with sync anyway */
            printf("Till update failed, continuing with sync...\n\n");
        }
    }
    
    if (dry_run) {
        printf("Running Till sync (DRY RUN - no changes will be made)...\n");
        till_log(LOG_INFO, "Starting sync operation (dry run)");
    } else {
        printf("Running Till sync...\n");
        till_log(LOG_INFO, "Starting sync operation");
    }
    
    /* Read till-private.json to get all installations */
    snprintf(config_path, sizeof(config_path), 
             "%s/%s", TILL_TEKTON_DIR, TILL_PRIVATE_CONFIG);
    
    fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: No installations found. Run 'till install' first.\n");
        till_log(LOG_ERROR, "No installations found");
        return EXIT_FILE_ERROR;
    }
    
    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return EXIT_GENERAL_ERROR;
    }
    
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);
    
    /* Parse JSON */
    cJSON *root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        fprintf(stderr, "Error: Failed to parse till-private.json\n");
        till_log(LOG_ERROR, "Failed to parse till-private.json");
        return EXIT_FILE_ERROR;
    }
    
    /* Get installations object */
    cJSON *installations = cJSON_GetObjectItem(root, "installations");
    if (!installations || !cJSON_IsObject(installations)) {
        fprintf(stderr, "No installations found in configuration\n");
        cJSON_Delete(root);
        return EXIT_SUCCESS;
    }
    
    /* Count installations */
    int install_count = cJSON_GetArraySize(installations);
    if (install_count == 0) {
        printf("No Tekton installations to sync.\n");
        cJSON_Delete(root);
        return EXIT_SUCCESS;
    }
    
    printf("Found %d Tekton installation%s to sync\n\n", 
           install_count, install_count == 1 ? "" : "s");
    
    /* Iterate through each installation */
    cJSON *installation = NULL;
    const char *registry_name = NULL;
    
    cJSON_ArrayForEach(installation, installations) {
        registry_name = installation->string;
        cJSON *root_item = cJSON_GetObjectItem(installation, "root");
        
        if (!root_item || !cJSON_IsString(root_item)) {
            printf("⚠️  Skipping %s: no root path found\n", registry_name);
            total_skipped++;
            continue;
        }
        
        const char *tekton_path = root_item->valuestring;
        
        /* Check if directory exists */
        if (!dir_exists(tekton_path)) {
            printf("⚠️  Skipping %s: directory not found (%s)\n", 
                   registry_name, tekton_path);
            till_log(LOG_WARNING, "Directory not found for %s: %s", 
                    registry_name, tekton_path);
            total_skipped++;
            continue;
        }
        
        /* Check if it's a git repository */
        char git_dir[TILL_MAX_PATH];
        snprintf(git_dir, sizeof(git_dir), "%s/.git", tekton_path);
        if (!dir_exists(git_dir)) {
            printf("⚠️  Skipping %s: not a git repository\n", registry_name);
            till_log(LOG_WARNING, "%s is not a git repository", registry_name);
            total_skipped++;
            continue;
        }
        
        /* Check for holds (future feature) */
        cJSON *hold_item = cJSON_GetObjectItem(installation, "hold");
        if (hold_item && cJSON_IsTrue(hold_item)) {
            printf("🔒 Skipping %s: updates are on hold\n", registry_name);
            till_log(LOG_INFO, "%s is on hold, skipping", registry_name);
            total_skipped++;
            continue;
        }
        
        /* Perform git pull or check status */
        printf("📦 %s %s...\n", dry_run ? "Checking" : "Updating", registry_name);
        printf("   Path: %s\n", tekton_path);
        
        if (dry_run) {
            /* In dry-run mode, just check git status */
            char cmd[TILL_MAX_PATH * 2];
            snprintf(cmd, sizeof(cmd), 
                    "cd \"%s\" && git fetch --dry-run 2>&1", tekton_path);
            
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char output[1024];
                int has_updates = 0;
                while (fgets(output, sizeof(output), pipe)) {
                    if (strlen(output) > 1) {
                        has_updates = 1;
                        break;
                    }
                }
                pclose(pipe);
                
                if (has_updates) {
                    printf("   🔄 Updates available (would pull)\n");
                    total_updated++;
                } else {
                    printf("   ✅ Already up to date\n");
                }
            } else {
                printf("   ⚠️  Could not check status\n");
                total_skipped++;
            }
        } else {
            /* Actually perform git pull */
            char cmd[TILL_MAX_PATH * 2];
            snprintf(cmd, sizeof(cmd), 
                    "cd \"%s\" && git pull --quiet 2>&1", tekton_path);
            
            FILE *pipe = popen(cmd, "r");
            if (!pipe) {
                printf("   ❌ Failed to run git pull\n");
                till_log(LOG_ERROR, "Failed to run git pull for %s", registry_name);
                total_failed++;
                continue;
            }
            
            /* Read git pull output */
            char output[1024];
            char full_output[4096] = "";
            while (fgets(output, sizeof(output), pipe)) {
                strncat(full_output, output, sizeof(full_output) - strlen(full_output) - 1);
            }
            
            int status = pclose(pipe);
            
            if (status == 0) {
                /* Check if there were actual updates */
                if (strstr(full_output, "Already up to date") || 
                    strstr(full_output, "Already up-to-date")) {
                    printf("   ✅ Already up to date\n");
                } else {
                    printf("   ✅ Updated successfully\n");
                    if (strlen(full_output) > 0 && !strstr(full_output, "Fast-forward")) {
                        /* Show what was updated if not a simple fast-forward */
                        printf("   %s", full_output);
                    }
                    total_updated++;
                }
                till_log(LOG_INFO, "Successfully synced %s", registry_name);
            } else {
                printf("   ❌ Git pull failed\n");
                if (strlen(full_output) > 0) {
                    printf("   Error: %s", full_output);
                }
                till_log(LOG_ERROR, "Git pull failed for %s: %s", 
                        registry_name, full_output);
                total_failed++;
            }
        }
        
        printf("\n");
    }
    
    cJSON_Delete(root);
    
    /* Print summary */
    printf("%s Summary:\n", dry_run ? "Dry Run" : "Sync");
    if (dry_run) {
        printf("  🔄 Would update: %d\n", total_updated);
    } else {
        printf("  ✅ Updated:      %d\n", total_updated);
    }
    printf("  ⚠️  Skipped:      %d\n", total_skipped);
    if (total_failed > 0) {
        printf("  ❌ Failed:       %d\n", total_failed);
    }
    
    till_log(LOG_INFO, "%s complete: %d %s, %d skipped, %d failed", 
            dry_run ? "Dry run" : "Sync",
            total_updated, dry_run ? "would update" : "updated", 
            total_skipped, total_failed);
    
    printf("\n%s complete.\n", dry_run ? "Dry run" : "Sync");
    return (total_failed > 0) ? EXIT_GENERAL_ERROR : EXIT_SUCCESS;
}

/* Command: watch */
static int cmd_watch(int argc, char *argv[]) {
    /* Delegate to schedule module */
    return till_watch_configure(argc - 1, argv + 1);
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
            char till_parent[TILL_MAX_PATH];
            if (get_till_parent_dir(till_parent, sizeof(till_parent)) == 0) {
                snprintf(opts.path, sizeof(opts.path), "%s/Coder-%c", 
                         till_parent, toupper(opts.mode[6]));
            } else {
                /* Fallback to relative path */
                char coder_dir[TILL_MAX_PATH];
                snprintf(coder_dir, sizeof(coder_dir), "../Coder-%c", 
                         toupper(opts.mode[6]));
                strncpy(opts.path, coder_dir, sizeof(opts.path) - 1);
            }
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
        if (validate_installation_name(opts.name) != 0) {
            fprintf(stderr, "Error: Invalid name '%s'\n", opts.name);
            return EXIT_USAGE_ERROR;
        }
        
        /* Set default path if not specified - sibling to till */
        if (strlen(opts.path) == 0) {
            /* Use till's parent directory as the base */
            char till_parent[TILL_MAX_PATH];
            if (get_till_parent_dir(till_parent, sizeof(till_parent)) == 0) {
                snprintf(opts.path, sizeof(opts.path), "%s/%s", till_parent,
                         strlen(opts.name) > 0 ? opts.name : "Tekton");
            } else {
                /* Fallback to relative path */
                snprintf(opts.path, sizeof(opts.path), "../%s", 
                         strlen(opts.name) > 0 ? opts.name : "Tekton");
            }
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
        char default_registry[TILL_MAX_NAME];
        if (strlen(opts.registry_name) == 0) {
            /* Generate default: lowercase(name).tekton.development.us */
            char lower_name[TILL_MAX_NAME];
            int i;
            for (i = 0; opts.name[i] && i < TILL_MAX_NAME - 1; i++) {
                lower_name[i] = tolower(opts.name[i]);
            }
            lower_name[i] = '\0';
            snprintf(default_registry, sizeof(default_registry), 
                     "%s.tekton.development.us", lower_name);
        } else {
            strcpy(default_registry, opts.registry_name);
        }
        
        printf("2. Tekton Registry Name [%s]: ", default_registry);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (input[0] == 27) {  /* ESC key */
                printf("\nInstallation cancelled.\n");
                return EXIT_SUCCESS;
            }
            if (input[0] != '\n') {
                input[strcspn(input, "\n")] = 0;
                strncpy(opts.registry_name, input, sizeof(opts.registry_name) - 1);
            } else {
                strcpy(opts.registry_name, default_registry);
            }
        }
        
        /* Check for reserved names */
        if (strcmp(opts.registry_name, "primary.tekton.development.us") == 0 ||
            strcmp(opts.registry_name, "tekton.tekton.development.us") == 0) {
            printf("\nError: '%s' is a reserved registry name.\n", opts.registry_name);
            printf("Please use a different name.\n");
            return EXIT_USAGE_ERROR;
        }
        
        /* Question 3: Path */
        char default_path[TILL_MAX_PATH];
        /* Use till's parent directory as the base for installations */
        char till_parent[TILL_MAX_PATH];
        if (get_till_parent_dir(till_parent, sizeof(till_parent)) == 0) {
            /* Install as sibling to till */
            snprintf(default_path, sizeof(default_path), "%s/%s", till_parent, opts.name);
        } else {
            /* Fallback: use current directory */
            char current_dir[TILL_MAX_PATH];
            if (getcwd(current_dir, sizeof(current_dir))) {
                snprintf(default_path, sizeof(default_path), "%s/%s", current_dir, opts.name);
            } else {
                snprintf(default_path, sizeof(default_path), "./%s", opts.name);
            }
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
    /* Delegate to host management module */
    return till_host_command(argc, argv);
}

/* Command: federate */
static int cmd_federate(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Federation features are not yet implemented.\n");
    printf("This will allow multiple Till instances to coordinate in the future.\n");
    
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

/* Command: run */
static int cmd_run(int argc, char *argv[]) {
    /* Delegate to till_run module */
    return till_run_command(argc, argv);
}

/* Command: update - Update Till from git repository */
static int cmd_update(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Till Update - Self-update from git repository\n");
    printf("==============================================\n\n");
    
    /* Get Till directory */
    char till_dir[PATH_MAX];
    if (get_till_directory(till_dir, sizeof(till_dir)) != 0) {
        fprintf(stderr, "Error: Cannot find Till directory\n");
        fprintf(stderr, "Till must be installed via git clone\n");
        return EXIT_FILE_ERROR;
    }
    
    printf("Till directory: %s\n", till_dir);
    
    /* Step 1: Check if we're in a git repository */
    printf("\n1. Checking git repository...\n");
    char git_check[PATH_MAX + 64];
    snprintf(git_check, sizeof(git_check), "cd '%s' && git rev-parse --git-dir >/dev/null 2>&1", till_dir);
    
    if (system(git_check) != 0) {
        fprintf(stderr, "✗ Not a git repository\n");
        fprintf(stderr, "Till must be installed via: git clone " TILL_REPO_URL "\n");
        return EXIT_FILE_ERROR;
    }
    printf("✓ Git repository found\n");
    
    /* Step 2: Check current version and branch */
    printf("\n2. Checking current version...\n");
    printf("  Version: %s\n", TILL_VERSION);
    
    char branch_cmd[PATH_MAX + 64];
    snprintf(branch_cmd, sizeof(branch_cmd), "cd '%s' && git branch --show-current", till_dir);
    FILE *fp = popen(branch_cmd, "r");
    if (fp) {
        char branch[128];
        if (fgets(branch, sizeof(branch), fp)) {
            branch[strcspn(branch, "\n")] = '\0';
            printf("  Branch: %s\n", branch);
        }
        pclose(fp);
    }
    
    /* Step 3: Check for local changes */
    printf("\n3. Checking for local changes...\n");
    char status_cmd[PATH_MAX + 64];
    snprintf(status_cmd, sizeof(status_cmd), "cd '%s' && git status --porcelain", till_dir);
    
    fp = popen(status_cmd, "r");
    if (fp) {
        char line[256];
        int has_changes = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (!has_changes) {
                printf("⚠ Local changes detected:\n");
                has_changes = 1;
            }
            printf("  %s", line);
        }
        pclose(fp);
        
        if (has_changes) {
            printf("\nStashing local changes...\n");
            char stash_cmd[PATH_MAX + 64];
            snprintf(stash_cmd, sizeof(stash_cmd), "cd '%s' && git stash", till_dir);
            system(stash_cmd);
        } else {
            printf("✓ No local changes\n");
        }
    }
    
    /* Step 4: Pull latest from git */
    printf("\n4. Pulling latest updates...\n");
    char pull_cmd[PATH_MAX + 64];
    snprintf(pull_cmd, sizeof(pull_cmd), "cd '%s' && git pull", till_dir);
    
    if (system(pull_cmd) != 0) {
        fprintf(stderr, "✗ Git pull failed\n");
        fprintf(stderr, "Check your network connection and git configuration\n");
        return EXIT_GENERAL_ERROR;
    }
    printf("✓ Updates pulled successfully\n");
    
    /* Step 5: Rebuild Till */
    printf("\n5. Rebuilding Till...\n");
    char make_cmd[PATH_MAX + 64];
    snprintf(make_cmd, sizeof(make_cmd), "cd '%s' && make clean && make", till_dir);
    
    if (system(make_cmd) != 0) {
        fprintf(stderr, "✗ Build failed\n");
        fprintf(stderr, "Check compilation errors above\n");
        return EXIT_GENERAL_ERROR;
    }
    printf("✓ Till rebuilt successfully\n");
    
    /* Step 6: Update installed version */
    printf("\n6. Updating installed version...\n");
    
    /* Check where till is installed */
    fp = popen("which till 2>/dev/null", "r");
    if (fp) {
        char installed_path[PATH_MAX];
        if (fgets(installed_path, sizeof(installed_path), fp)) {
            installed_path[strcspn(installed_path, "\n")] = '\0';
            printf("  Installed at: %s\n", installed_path);
            
            /* Determine if it's user or system install */
            if (strstr(installed_path, "/.local/bin/")) {
                /* User installation */
                printf("  Updating user installation...\n");
                char install_cmd[PATH_MAX + 64];
                snprintf(install_cmd, sizeof(install_cmd), "cd '%s' && make install-user", till_dir);
                
                if (system(install_cmd) == 0) {
                    printf("✓ User installation updated\n");
                } else {
                    printf("⚠ Failed to update user installation\n");
                    printf("  Run manually: cd %s && make install-user\n", till_dir);
                }
            } else if (strstr(installed_path, "/usr/local/bin/")) {
                /* System installation */
                printf("  System installation detected\n");
                printf("  Run with sudo: cd %s && sudo make install\n", till_dir);
            } else {
                printf("  Custom installation path\n");
                printf("  Copy manually: cp %s/till %s\n", till_dir, installed_path);
            }
        } else {
            printf("  No installed version found\n");
            printf("  Install with: cd %s && make install-user\n", till_dir);
        }
        pclose(fp);
    }
    
    /* Step 7: Show new version */
    printf("\n7. Verifying update...\n");
    char version_cmd[PATH_MAX + 64];
    snprintf(version_cmd, sizeof(version_cmd), "%s/till --version", till_dir);
    system(version_cmd);
    
    printf("\n✅ Till update complete!\n");
    
    printf("\nNext steps:\n");
    printf("  - Restart your shell or run: hash -r\n");
    printf("  - Verify with: till --version\n");
    printf("  - Update remote hosts: till host exec <name> 'till update'\n");
    
    return EXIT_SUCCESS;
}

/* Command: help */
static int cmd_help(int argc, char *argv[]) {
    /* Check if help is requested for a specific command */
    if (argc > 1) {
        const char *topic = argv[1];
        
        if (strcmp(topic, "host") == 0) {
            /* Delegate to host help */
            char *help_args[] = {"host", "--help"};
            return till_host_command(2, help_args);
        }
        else if (strcmp(topic, "run") == 0) {
            /* Delegate to run help */
            char *help_args[] = {"--help"};
            return till_run_command(1, help_args);
        }
        else if (strcmp(topic, "sync") == 0) {
            printf("Till Sync - Synchronize Tekton Installations\n\n");
            printf("Usage: till sync [options]\n\n");
            printf("Options:\n");
            printf("  --dry-run, -n    Show what would be synced without making changes\n");
            printf("  --force          Force sync even if no updates detected\n");
            printf("  --parallel       Sync all installations in parallel\n\n");
            printf("Examples:\n");
            printf("  till sync              # Sync all installations\n");
            printf("  till sync --dry-run    # Preview sync operation\n");
            printf("  till sync --force      # Force sync even if up-to-date\n");
            return 0;
        }
        else if (strcmp(topic, "install") == 0) {
            printf("Till Install - Install Tekton\n\n");
            printf("Usage: till install [options]\n\n");
            printf("Options:\n");
            printf("  --mode MODE      Installation mode: solo, observer, member, coder-[a-c]\n");
            printf("  --name NAME      Federation name (FQN format)\n");
            printf("  --path PATH      Installation directory\n");
            printf("  --port-base PORT Starting port for components\n\n");
            printf("Examples:\n");
            printf("  till install                        # Interactive installation\n");
            printf("  till install --mode solo            # Solo mode\n");
            printf("  till install --mode coder-a         # Development environment\n");
            return 0;
        }
        else {
            printf("No help available for '%s'\n\n", topic);
        }
    }
    
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

/* Get till's installation directory */
static int get_till_directory(char *till_dir, size_t size) {
    char *home = getenv("HOME");
    if (!home) {
        return -1;
    }
    
    /* Till is installed at projects/github/till */
    snprintf(till_dir, size, "%s/%s/till", home, TILL_PROJECTS_BASE);
    
    /* Verify the directory exists */
    struct stat st;
    if (stat(till_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    
    return -1;
}

/* Get till's installation directory (parent of till's location) */
static int get_till_parent_dir(char *parent_dir, size_t size) {
    /* Use a known location for till */
    char *home = getenv("HOME");
    if (!home) {
        return -1;
    }
    
    /* Till is installed at projects/github/till */
    /* So parent directory is projects/github */
    snprintf(parent_dir, size, "%s/projects/github", home);
    
    /* Verify the directory exists */
    struct stat st;
    if (stat(parent_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    
    return -1;
}

/* Check for till updates */
static int check_till_updates(int quiet_mode) {
    char till_dir[TILL_MAX_PATH];
    char cmd[TILL_MAX_PATH * 2];
    
    if (get_till_directory(till_dir, sizeof(till_dir)) != 0) {
        return -1;
    }
    
    /* Fetch latest without pulling */
    snprintf(cmd, sizeof(cmd), 
        "cd \"%s\" && git fetch --quiet origin main 2>/dev/null", till_dir);
    
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return -1;
    pclose(pipe);
    
    /* Check if we're behind */
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && git rev-list HEAD..origin/main --count 2>/dev/null", till_dir);
    
    pipe = popen(cmd, "r");
    if (!pipe) return -1;
    
    char output[32];
    if (fgets(output, sizeof(output), pipe)) {
        int behind = atoi(output);
        if (behind > 0 && !quiet_mode) {
            printf("\n📦 Till update available: %d commit%s behind\n", 
                   behind, behind == 1 ? "" : "s");
            printf("   Run 'till sync' to update till and all Tektons\n\n");
        }
        pclose(pipe);
        return behind;
    }
    
    pclose(pipe);
    return 0;
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

/* Rollback till to backup version */
static void rollback_till(const char *backup, const char *target) {
    printf("   Rolling back to previous version...\n");
    rename(backup, target);
}

/* Self-update till with backup and rollback */
static int self_update_till(void) {
    char till_dir[TILL_MAX_PATH];
    char backup_path[TILL_MAX_PATH];
    char lock_file[TILL_MAX_PATH];
    char cmd[TILL_MAX_PATH * 2];
    int lock_fd;
    
    if (get_till_directory(till_dir, sizeof(till_dir)) != 0) {
        fprintf(stderr, "Error: Could not determine till directory\n");
        return -1;
    }
    
    /* 1. LOCK - Prevent concurrent updates */
    snprintf(lock_file, sizeof(lock_file), "%s/.till-update.lock", till_dir);
    lock_fd = open(lock_file, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (lock_fd < 0) {
        if (errno == EEXIST) {
            printf("⚠️  Another till update in progress\n");
            return -1;
        }
        return -1;
    }
    
    /* 2. BACKUP - Save current executable */
    char current_exe[TILL_MAX_PATH];
    snprintf(current_exe, sizeof(current_exe), "%s/till", till_dir);
    snprintf(backup_path, sizeof(backup_path), "%s/till.backup-%ld", 
             till_dir, (long)time(NULL));
    
    printf("📦 Updating till...\n");
    printf("   Backing up to: %s\n", backup_path);
    
    if (rename(current_exe, backup_path) != 0) {
        printf("   ❌ Backup failed: %s\n", strerror(errno));
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    /* 3. CHECK - Ensure clean working directory */
    snprintf(cmd, sizeof(cmd), 
        "cd \"%s\" && git status --porcelain 2>/dev/null", till_dir);
    
    FILE *pipe = popen(cmd, "r");
    char changes[256];
    int has_changes = 0;
    if (pipe) {
        if (fgets(changes, sizeof(changes), pipe)) {
            if (strlen(changes) > 0) {
                has_changes = 1;
            }
        }
        pclose(pipe);
    }
    
    if (has_changes) {
        printf("   ⚠️  Uncommitted changes detected\n");
        printf("   Stashing changes...\n");
        
        /* Stash changes */
        snprintf(cmd, sizeof(cmd),
            "cd \"%s\" && git stash push -m 'till-auto-update-%ld' 2>&1",
            till_dir, (long)time(NULL));
        system(cmd);
    }
    
    /* 4. UPDATE - Pull latest */
    printf("   Pulling latest changes...\n");
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && git pull --no-edit origin main 2>&1", till_dir);
    
    pipe = popen(cmd, "r");
    if (!pipe) {
        rollback_till(backup_path, current_exe);
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    char output[1024];
    int success = 1;
    while (fgets(output, sizeof(output), pipe)) {
        /* Show important output */
        if (strstr(output, "Fast-forward") || 
            strstr(output, "files changed") ||
            strstr(output, "insertions") ||
            strstr(output, "deletions")) {
            printf("   %s", output);
        }
        if (strstr(output, "error:") || strstr(output, "fatal:")) {
            printf("   %s", output);
            success = 0;
        }
    }
    int status = pclose(pipe);
    
    if (status != 0 || !success) {
        printf("   ❌ Git pull failed, rolling back\n");
        rollback_till(backup_path, current_exe);
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    /* 5. BUILD - Compile new version */
    printf("   Building new version...\n");
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && make clean >/dev/null 2>&1 && make 2>&1", till_dir);
    
    pipe = popen(cmd, "r");
    success = 1;
    while (fgets(output, sizeof(output), pipe)) {
        if (strstr(output, "error:") || strstr(output, "Error")) {
            printf("   %s", output);
            success = 0;
        } else if (strstr(output, "Build complete")) {
            printf("   %s", output);
        }
    }
    status = pclose(pipe);
    
    if (status != 0 || !success) {
        printf("   ❌ Build failed, rolling back\n");
        rollback_till(backup_path, current_exe);
        
        /* Also revert git */
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && git reset --hard HEAD~1", till_dir);
        system(cmd);
        
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    /* 6. VERIFY - Test new executable */
    printf("   Verifying new version...\n");
    snprintf(cmd, sizeof(cmd), "\"%s\" --version 2>&1", current_exe);
    pipe = popen(cmd, "r");
    if (!pipe) {
        printf("   ❌ Verification failed, rolling back\n");
        rollback_till(backup_path, current_exe);
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    /* Read version output */
    if (fgets(output, sizeof(output), pipe)) {
        printf("   New version: %s", output);
    }
    status = pclose(pipe);
    
    if (status != 0) {
        printf("   ❌ Verification failed, rolling back\n");
        rollback_till(backup_path, current_exe);
        close(lock_fd);
        unlink(lock_file);
        return -1;
    }
    
    /* 7. CLEANUP - Remove backup after success */
    printf("   ✅ Till updated successfully\n");
    unlink(backup_path);
    
    /* Show what changed */
    snprintf(cmd, sizeof(cmd), 
        "cd \"%s\" && git log --oneline -5", till_dir);
    printf("\n   Recent changes:\n");
    pipe = popen(cmd, "r");
    while (fgets(output, sizeof(output), pipe)) {
        printf("     %s", output);
    }
    pclose(pipe);
    
    /* 8. UNLOCK */
    close(lock_fd);
    unlink(lock_file);
    
    /* 9. RE-EXEC - Run new version for the sync */
    printf("\n   Restarting with new version...\n\n");
    execl(current_exe, "till", "sync", NULL);
    
    /* If execl fails, return error */
    fprintf(stderr, "Error: Failed to restart with new version\n");
    return -1;
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
