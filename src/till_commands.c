/*
 * till_commands.c - Command handlers for Till
 * 
 * Implements all Till command functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "till_config.h"
#include "till_commands.h"
#include "till_install.h"
#include "till_host.h"
#include "till_hold.h"
#include "till_schedule.h"
#include "till_run.h"
#include "till_registry.h"
#include "till_common.h"
#include "cJSON.h"

/* External functions from till.c */
extern int file_exists(const char *path);
extern int dir_exists(const char *path);
extern int get_absolute_path(const char *relative, char *absolute, size_t size);
extern int get_till_parent_dir(char *parent_dir, size_t size);
extern int check_till_updates(int quiet_mode);
extern int self_update_till(void);
extern void till_log(int level, const char *format, ...);

/* External globals */
extern int g_interactive;

/* Forward declarations */
/* None needed currently */


/* Command: sync - Pull updates for all Tekton installations */
int cmd_sync(int argc, char *argv[]) {
    int dry_run = 0;
    int skip_till_update = 0;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        } else if (strcmp(argv[i], "--skip-till-update") == 0) {
            skip_till_update = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Till Sync - Update Till and all Tekton installations\n\n");
            printf("Usage: till sync [options]\n\n");
            printf("Options:\n");
            printf("  --dry-run           Check for updates without applying\n");
            printf("  --skip-till-update  Don't update Till itself\n");
            printf("  --help, -h          Show this help message\n");
            return 0;
        }
    }
    
    /* Check for Till updates first */
    if (!skip_till_update) {
        int behind = check_till_updates(1);
        if (behind > 0 && !dry_run) {
            return self_update_till();
        }
    }
    
    printf("Till Sync\n");
    printf("=========\n\n");
    
    if (dry_run) {
        printf("DRY RUN MODE - No changes will be made\n\n");
    }
    
    /* Load registry */
    cJSON *registry = load_or_create_registry();
    if (!registry) {
        till_warn("No Tekton installations found");
        return 0;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations || cJSON_GetArraySize(installations) == 0) {
        till_warn("No Tekton installations registered");
        cJSON_Delete(registry);
        return 0;
    }
    
    /* Clean up expired holds first */
    cleanup_expired_holds();
    
    /* Sync each installation */
    int total = 0, updated = 0, failed = 0, held = 0;
    
    cJSON *inst;
    cJSON_ArrayForEach(inst, installations) {
        const char *name = inst->string;
        const char *root = json_get_string(inst, "root", NULL);
        
        if (!root) continue;
        
        total++;
        printf("Checking %s...\n", name);
        
        /* Check if component is held */
        if (is_component_held(name)) {
            hold_info_t hold_info;
            if (get_hold_info(name, &hold_info) == 0) {
                printf("  🔒 HELD");
                if (hold_info.reason[0]) {
                    printf(" - %s", hold_info.reason);
                }
                if (hold_info.expires_at > 0) {
                    char time_buf[64];
                    format_time(hold_info.expires_at, time_buf, sizeof(time_buf));
                    printf(" (until %s)", time_buf);
                }
                printf("\n");
            } else {
                printf("  🔒 HELD\n");
            }
            held++;
            continue;
        }
        
        if (dry_run) {
            /* Just check status */
            char output[256];
            if (run_command_capture(output, sizeof(output),
                                  "cd \"%s\" && git status --porcelain 2>/dev/null | head -1",
                                  root) == 0 && strlen(output) > 0) {
                printf("  ⚠ Has local changes\n");
            } else {
                printf("  ✓ Clean\n");
            }
        } else {
            /* Actually update */
            if (run_command_logged("cd \"%s\" && git pull", root) == 0) {
                printf("  ✓ Updated\n");
                updated++;
            } else {
                printf("  ✗ Failed to update\n");
                failed++;
            }
        }
    }
    
    cJSON_Delete(registry);
    
    /* Summary */
    printf("\nSync Summary:\n");
    printf("  Total: %d installations\n", total);
    if (!dry_run) {
        printf("  Updated: %d\n", updated);
        if (held > 0) {
            printf("  Held: %d\n", held);
        }
        if (failed > 0) {
            printf("  Failed: %d\n", failed);
        }
    } else {
        if (held > 0) {
            printf("  Held: %d (would be skipped)\n", held);
        }
    }
    
    /* Record sync in schedule */
    if (!dry_run) {
        till_watch_record_sync(failed == 0, 0, updated, 0);
    }
    
    return failed > 0 ? 1 : 0;
}

/* Command: watch - Configure automatic sync */
int cmd_watch(int argc, char *argv[]) {
    return till_watch_configure(argc - 1, argv + 1);
}

/* Command: install - Install Tekton or components */
int cmd_install(int argc, char *argv[]) {
    install_options_t opts = {0};
    
    /* Set defaults */
    strncpy(opts.mode, MODE_SOLO, sizeof(opts.mode) - 1);
    opts.mode[sizeof(opts.mode) - 1] = '\0';
    opts.port_base = DEFAULT_PORT_BASE;
    opts.ai_port_base = DEFAULT_AI_PORT_BASE;
    
    /* Get primary Tekton path if available */
    char primary_path[TILL_MAX_PATH];
    if (get_primary_tekton_path(primary_path, sizeof(primary_path)) == 0) {
        strncpy(opts.tekton_main_root, primary_path, sizeof(opts.tekton_main_root) - 1);
        opts.tekton_main_root[sizeof(opts.tekton_main_root) - 1] = '\0';
    }
    
    /* Parse arguments */
    int has_args = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            strncpy(opts.mode, argv[++i], sizeof(opts.mode) - 1);
            has_args = 1;
        }
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            strncpy(opts.name, argv[++i], sizeof(opts.name) - 1);
            has_args = 1;
        }
        else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
            strncpy(opts.path, argv[++i], sizeof(opts.path) - 1);
            has_args = 1;
        }
        else if (strcmp(argv[i], "--port-base") == 0 && i + 1 < argc) {
            opts.port_base = atoi(argv[++i]);
            has_args = 1;
        }
        else if (strcmp(argv[i], "--ai-port-base") == 0 && i + 1 < argc) {
            opts.ai_port_base = atoi(argv[++i]);
            has_args = 1;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Till Install - Install Tekton\n\n");
            printf("Usage: till install [options]\n\n");
            printf("Options:\n");
            printf("  --mode MODE      Installation mode: solo, observer, member, coder-[a-c]\n");
            printf("  --name NAME      Federation name (FQN format)\n");
            printf("  --path PATH      Installation directory\n");
            printf("  --port-base PORT Starting port for components\n");
            printf("  --ai-port-base PORT Starting port for AI services\n");
            printf("  -i, --interactive Interactive mode\n\n");
            printf("Examples:\n");
            printf("  till install                        # Interactive installation\n");
            printf("  till install --mode solo            # Solo mode\n");
            printf("  till install --mode coder-a         # Development environment\n");
            return 0;
        }
    }
    
    /* Interactive mode - prompt for missing options */
    if (g_interactive && !has_args) {
        printf("\nTekton Interactive Installation\n");
        printf("================================\n\n");
        
        /* Prompt for mode */
        printf("Installation mode:\n");
        printf("  1. solo     - Standalone installation\n");
        printf("  2. observer - Read-only federation member\n");
        printf("  3. member   - Full federation member\n");
        printf("  4. coder-a  - Development environment A\n");
        printf("  5. coder-b  - Development environment B\n");
        printf("  6. coder-c  - Development environment C\n");
        printf("\nSelect mode [1-6] (default: 1): ");
        
        char choice[10];
        if (fgets(choice, sizeof(choice), stdin)) {
            int mode_choice = atoi(choice);
            switch (mode_choice) {
                case 2: strncpy(opts.mode, MODE_OBSERVER, sizeof(opts.mode) - 1); break;
                case 3: strncpy(opts.mode, MODE_MEMBER, sizeof(opts.mode) - 1); break;
                case 4: strncpy(opts.mode, "coder-a", sizeof(opts.mode) - 1); break;
                case 5: strncpy(opts.mode, "coder-b", sizeof(opts.mode) - 1); break;
                case 6: strncpy(opts.mode, "coder-c", sizeof(opts.mode) - 1); break;
                default: strncpy(opts.mode, MODE_SOLO, sizeof(opts.mode) - 1); break;
            }
        }
        
        /* Prompt for name */
        printf("\nInstallation name (e.g., 'primary', 'coder-a'): ");
        char name_input[256];
        if (fgets(name_input, sizeof(name_input), stdin)) {
            name_input[strcspn(name_input, "\n")] = 0;  /* Remove newline */
            if (strlen(name_input) > 0) {
                strncpy(opts.name, name_input, sizeof(opts.name) - 1);
            }
        }
        
        /* Prompt for path */
        char *home = getenv("HOME");
        char default_path[TILL_MAX_PATH];
        snprintf(default_path, sizeof(default_path), "%s/%s/%s",
                home, TILL_PROJECTS_BASE,
                strlen(opts.name) > 0 ? opts.name : "tekton");
        
        printf("\nInstallation path (default: %s): ", default_path);
        char path_input[TILL_MAX_PATH];
        if (fgets(path_input, sizeof(path_input), stdin)) {
            path_input[strcspn(path_input, "\n")] = 0;  /* Remove newline */
            if (strlen(path_input) > 0) {
                strncpy(opts.path, path_input, sizeof(opts.path) - 1);
            }
        }
        
        /* Prompt for ports */
        printf("\nBase port for services (default: %d): ", DEFAULT_PORT_BASE);
        char port_input[32];
        if (fgets(port_input, sizeof(port_input), stdin)) {
            int port = atoi(port_input);
            if (port > 0) {
                opts.port_base = port;
            }
        }
        
        printf("Base port for AI services (default: %d): ", DEFAULT_AI_PORT_BASE);
        if (fgets(port_input, sizeof(port_input), stdin)) {
            int port = atoi(port_input);
            if (port > 0) {
                opts.ai_port_base = port;
            }
        }
        
        /* Show summary */
        printf("\nInstallation Summary:\n");
        printf("  Mode: %s\n", opts.mode);
        printf("  Name: %s\n", strlen(opts.name) > 0 ? opts.name : "(auto-generated)");
        printf("  Path: %s\n", strlen(opts.path) > 0 ? opts.path : default_path);
        printf("  Port Base: %d\n", opts.port_base);
        printf("  AI Port Base: %d\n", opts.ai_port_base);
        printf("\nProceed with installation? [Y/n]: ");
        
        char confirm[10];
        if (fgets(confirm, sizeof(confirm), stdin)) {
            if (confirm[0] == 'n' || confirm[0] == 'N') {
                printf("Installation cancelled.\n");
                return 0;
            }
        }
    }
    
    /* Validate mode */
    if (strcmp(opts.mode, MODE_SOLO) != 0 && 
        strcmp(opts.mode, MODE_OBSERVER) != 0 &&
        strcmp(opts.mode, MODE_MEMBER) != 0 &&
        strncmp(opts.mode, "coder-", 6) != 0) {
        till_error("Invalid mode: %s", opts.mode);
        return EXIT_USAGE_ERROR;
    }
    
    /* Generate name if not provided */
    if (strlen(opts.name) == 0) {
        if (strcmp(opts.mode, MODE_MEMBER) == 0 || strcmp(opts.mode, MODE_OBSERVER) == 0) {
            if (!g_interactive) {
                till_error("--name required for %s mode", opts.mode);
                return EXIT_USAGE_ERROR;
            }
        } else {
            strncpy(opts.name, "tekton-solo", sizeof(opts.name) - 1);
            opts.name[sizeof(opts.name) - 1] = '\0';
        }
    }
    
    /* Validate name */
    if (validate_installation_name(opts.name) != 0) {
        till_error("Invalid name '%s'", opts.name);
        return EXIT_USAGE_ERROR;
    }
    
    /* Set default path if not specified */
    if (strlen(opts.path) == 0) {
        char *home = getenv("HOME");
        if (home) {
            snprintf(opts.path, sizeof(opts.path), "%s/%s/%s",
                    home, TILL_PROJECTS_BASE,
                    strlen(opts.name) > 0 ? opts.name : "Tekton");
        } else {
            till_error("Cannot determine home directory");
            return EXIT_FILE_ERROR;
        }
    }
    
    /* Allocate ports */
    if (allocate_ports(&opts) != 0) {
        return EXIT_GENERAL_ERROR;
    }
    
    /* Install */
    return till_install_tekton(&opts);
}

/* Command: uninstall - Uninstall component */
int cmd_uninstall(int argc, char *argv[]) {
    if (argc < 2) {
        till_error("Usage: till uninstall <name>");
        return EXIT_USAGE_ERROR;
    }
    
    const char *name = argv[1];
    
    /* Load registry */
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) {
        till_error("No installations found");
        return EXIT_FILE_ERROR;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    cJSON *inst = cJSON_GetObjectItem(installations, name);
    
    if (!inst) {
        till_error("Installation '%s' not found", name);
        cJSON_Delete(registry);
        return EXIT_FILE_ERROR;
    }
    
    const char *path = json_get_string(inst, "root", NULL);
    
    printf("This will remove the installation at: %s\n", path);
    printf("Are you sure? [y/N]: ");
    
    char response[10];
    if (!fgets(response, sizeof(response), stdin) || response[0] != 'y') {
        printf("Cancelled\n");
        cJSON_Delete(registry);
        return 0;
    }
    
    /* Remove from registry */
    cJSON_DeleteItemFromObject(installations, name);
    save_till_json("tekton/till-private.json", registry);
    cJSON_Delete(registry);
    
    printf("Removed '%s' from registry\n", name);
    printf("Note: Directory %s was not deleted\n", path);
    
    return 0;
}

/* Command: hold - Prevent component updates */
int cmd_hold(int argc, char *argv[]) {
    return till_hold_command(argc, argv);
}

/* Command: release - Allow component updates */
int cmd_release(int argc, char *argv[]) {
    return till_release_command(argc, argv);
}

/* Command: host - Manage remote hosts */
int cmd_host(int argc, char *argv[]) {
    return till_host_command(argc, argv);
}

/* Command: federate - Manage federation */
int cmd_federate(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    till_info("Federation functionality not yet implemented");
    return 0;
}

/* Command: status - Show Till status */
int cmd_status(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    printf("Till Status\n");
    printf("===========\n");
    printf("Version: %s\n", TILL_VERSION);
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Config: %s\n", TILL_CONFIG_VERSION);
    
    printf("\nComponents: [To be implemented]\n");
    printf("Federation: [To be implemented]\n");
    printf("Hosts: [To be implemented]\n");
    
    return 0;
}

/* Command: run - Run component command */
int cmd_run(int argc, char *argv[]) {
    return till_run_command(argc, argv);
}

/* Command: update - Update Till from git */
int cmd_update(int argc, char *argv[]) {
    (void)argc;  /* Unused */
    (void)argv;  /* Unused */
    int behind = check_till_updates(1);
    
    if (behind == 0) {
        printf("Till is up to date\n");
        return 0;
    }
    
    if (behind > 0) {
        printf("Till is %d commit%s behind\n", behind, behind == 1 ? "" : "s");
        return self_update_till();
    }
    
    return 0;
}

/* Command: help - Show help information */
int cmd_help(int argc, char *argv[]) {
    if (argc > 1) {
        const char *topic = argv[1];
        
        if (strcmp(topic, "host") == 0) {
            return cmd_host(2, (char*[]){"host", "--help", NULL});
        }
        else if (strcmp(topic, "run") == 0) {
            return cmd_run(2, (char*[]){"run", "--help", NULL});
        }
        else if (strcmp(topic, "install") == 0) {
            return cmd_install(2, (char*[]){"install", "--help", NULL});
        }
        else if (strcmp(topic, "sync") == 0) {
            return cmd_sync(2, (char*[]){"sync", "--help", NULL});
        }
        else if (strcmp(topic, "watch") == 0) {
            return cmd_watch(2, (char*[]){"watch", "--help", NULL});
        }
        else {
            printf("No help available for '%s'\n\n", topic);
        }
    }
    
    /* Print general usage */
    printf("Till - Tekton Lifecycle Manager v%s\n\n", TILL_VERSION);
    printf("Usage: till [options] [command] [arguments]\n\n");
    printf("Global options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  -i, --interactive   Interactive mode for supported commands\n");
    printf("\nCommands:\n");
    printf("  (none)              Dry run - show what sync would do\n");
    printf("  sync                Pull updates for all Tekton installations\n");
    printf("  watch               Set watch daemon frequency\n");
    printf("  install             Install Tekton or components\n");
    printf("  uninstall           Uninstall component\n");
    printf("  hold                Prevent component updates\n");
    printf("  release             Allow component updates\n");
    printf("  host                Manage remote hosts\n");
    printf("  federate            Manage federation\n");
    printf("  status              Show Till status\n");
    printf("  run                 Run component command\n");
    printf("  update              Update Till from git\n");
    printf("  help                Show help information\n");
    printf("\nFor detailed help on a command, use:\n");
    printf("  till help <command>\n");
    printf("  till <command> --help\n");
    
    return 0;
}

/* Dry run - show what sync would do */
int cmd_dry_run(void) {
    printf("Till v%s - Dry Run Mode\n", TILL_VERSION);
    printf("=====================================\n\n");
    
    printf("Checking for updates from: %s\n", TILL_REPO_URL);
    
    /* Check Till updates */
    int behind = check_till_updates(1);
    if (behind > 0) {
        printf("📦 Till has %d update%s available\n", behind, behind == 1 ? "" : "s");
    } else {
        printf("✓ Till is up to date\n");
    }
    
    /* Check Tekton installations */
    cJSON *registry = load_till_json("tekton/till-private.json");
    if (!registry) {
        printf("\nNo Tekton installations found\n");
        return 0;
    }
    
    cJSON *installations = cJSON_GetObjectItem(registry, "installations");
    if (!installations || cJSON_GetArraySize(installations) == 0) {
        printf("\nNo Tekton installations registered\n");
        cJSON_Delete(registry);
        return 0;
    }
    
    printf("\nTekton Installations:\n");
    
    cJSON *inst;
    cJSON_ArrayForEach(inst, installations) {
        const char *name = inst->string;
        const char *root = json_get_string(inst, "root", NULL);
        
        if (!root) continue;
        
        printf("  %s (%s):\n", name, root);
        
        /* Check git status */
        char output[256];
        if (run_command_capture(output, sizeof(output),
                              "cd \"%s\" && git status --porcelain 2>/dev/null | wc -l",
                              root) == 0) {
            int changes = atoi(output);
            if (changes > 0) {
                printf("    ⚠ %d local change%s\n", changes, changes == 1 ? "" : "s");
            } else {
                printf("    ✓ Clean working directory\n");
            }
        }
        
        /* Check if behind */
        if (run_command_capture(output, sizeof(output),
                              "cd \"%s\" && git fetch --quiet && git rev-list HEAD..origin/main --count 2>/dev/null",
                              root) == 0) {
            int behind = atoi(output);
            if (behind > 0) {
                printf("    📦 %d update%s available\n", behind, behind == 1 ? "" : "s");
            }
        }
    }
    
    cJSON_Delete(registry);
    
    printf("\nRun 'till sync' to apply updates\n");
    
    return 0;
}