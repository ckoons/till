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
#include "till_commands.h"
#include "till_common.h"
#include "till_security.h"
#include "cJSON.h"

/* Global flags */
int g_interactive = 0;

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

/* Command handlers are now in till_commands.c */

/* Command table */
static const command_def_t commands[] = {
    {"sync",      cmd_sync,      "Pull updates for all Tekton installations", 0},
    {"watch",     cmd_watch,     "Set watch daemon frequency", 0},
    {"install",   cmd_install,   "Install Tekton or components", 0},
    {"uninstall", cmd_uninstall, "Uninstall component", 0},
    {"hold",      cmd_hold,      "Prevent component updates", 0},
    {"release",   cmd_release,   "Allow component updates", 0},
    {"host",      cmd_host,      "Manage remote hosts", 0},
    {"federate",  cmd_federate,  "Manage global federation", 0},
    {"status",    cmd_status,    "Show Till status", 0},
    {"run",       cmd_run,       "Run component command", 1},  /* Special case: needs argc-2, argv+2 */
    {"update",    cmd_update,    "Update Till from git", 0},
    {"repair",    cmd_repair,    "Check and repair Till configuration", 0},
    {"help",      cmd_help,      "Show help information", 0},
    {NULL, NULL, NULL, 0}
};
static int ensure_directories(void);
static int ensure_discovery(void);
/* Functions used by till_commands.c */
int file_exists(const char *path);
int dir_exists(const char *path);
int get_absolute_path(const char *relative, char *absolute, size_t size);
static int create_directory(const char *path);
int get_till_parent_dir(char *parent_dir, size_t size);
int check_till_updates(int quiet_mode);
int self_update_till(void);
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
        till_error("Failed to create Till directories");
        return EXIT_FILE_ERROR;
    }
    
    /* Log command start */
    if (argc > 1) {
        till_log(LOG_INFO, "Starting: till %s", argv[1]);
    } else {
        till_log(LOG_INFO, "Starting: till (dry run)");
    }
    
    /* Always run discovery and verify */
    ensure_discovery();
    
    /* No arguments - show dry run */
    if (argc == 1) {
        int result = cmd_dry_run();
        till_log(LOG_INFO, "Completed: till (dry run)");
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
            /* Standard command - pass argc-2, argv+2 to skip program name and command */
            result = cmd->handler(argc - 2, argv + 2);
        }
    } else {
        till_error("Unknown command '%s'", argv[1]);
        till_info("Try 'till --help' for usage information");
        till_log(LOG_ERROR, "Unknown command: %s", argv[1]);
        return EXIT_USAGE_ERROR;
    }
    
    till_log(LOG_INFO, "Completed: till %s", argv[1]);
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
    printf("  host sync           Sync hosts configuration across all machines\n");
    printf("  host status [name]  Show host status\n");
    printf("  host remove <name>  Remove host from configuration\n");
    printf("\nExamples:\n");
    printf("  till                      # Show what would be synced\n");
    printf("  till sync                 # Synchronize now\n");
    printf("  till install              # Install Tekton\n");
    printf("  till install --mode anonymous  # Anonymous installation\n");
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

/* Ensure discovery has been run */
static int ensure_discovery(void) {
    /* Always run discovery to verify current state */
    till_log(LOG_INFO, "Running discovery to verify installations");
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
        if (mkdir(path, TILL_DIR_PERMS) != 0) {
            if (errno != EEXIST) {
                till_error("Creating directory %s: %s", 
                        path, strerror(errno));
                return -1;
            }
        }
    }
    
    return 0;
}

/* Check if file exists */
int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/* Check if directory exists */
int dir_exists(const char *path) {
    return is_directory(path);
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
    if (is_directory(till_dir)) {
        return 0;
    }
    
    return -1;
}

/* Get till's installation directory (parent of till's location) */
int get_till_parent_dir(char *parent_dir, size_t size) {
    /* Use a known location for till */
    char *home = getenv("HOME");
    if (!home) {
        return -1;
    }
    
    /* Till is installed at projects/github/till */
    /* So parent directory is projects/github */
    snprintf(parent_dir, size, "%s/projects/github", home);
    
    /* Verify the directory exists */
    if (is_directory(parent_dir)) {
        return 0;
    }
    
    return -1;
}

/* Check for till updates */
int check_till_updates(int quiet_mode) {
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
int get_absolute_path(const char *relative, char *absolute, size_t size) {
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
int self_update_till(void) {
    char till_dir[TILL_MAX_PATH];
    char backup_path[TILL_MAX_PATH];
    char lock_file[TILL_MAX_PATH];
    char cmd[TILL_MAX_PATH * 2];
    int lock_fd;
    
    if (get_till_directory(till_dir, sizeof(till_dir)) != 0) {
        till_error("Could not determine till directory");
        return -1;
    }
    
    /* 1. LOCK - Prevent concurrent updates */
    snprintf(lock_file, sizeof(lock_file), "%s/.till-update.lock", till_dir);
    lock_fd = acquire_lock_file(lock_file, 5000);  /* 5 second timeout */
    if (lock_fd < 0) {
        if (errno == ETIMEDOUT) {
            printf("⚠️  Another till update in progress (timed out waiting)\n");
        } else {
            printf("⚠️  Could not acquire update lock\n");
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
        release_lock_file(lock_fd);
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
        if (system(cmd) != 0) {
            till_warn("Failed to stash changes");
        }
    }
    
    /* 4. UPDATE - Pull latest */
    printf("   Pulling latest changes...\n");
    snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && git pull --no-edit origin main 2>&1", till_dir);
    
    pipe = popen(cmd, "r");
    if (!pipe) {
        rollback_till(backup_path, current_exe);
        release_lock_file(lock_fd);
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
        release_lock_file(lock_fd);
        return -1;
    }
    
    /* 5. BUILD - Compile new version and install */
    printf("   Building and installing new version...\n");
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && make clean >/dev/null 2>&1 && make install 2>&1", till_dir);

    pipe = popen(cmd, "r");
    success = 1;
    while (fgets(output, sizeof(output), pipe)) {
        if (strstr(output, "error:") || strstr(output, "Error")) {
            printf("   %s", output);
            success = 0;
        } else if (strstr(output, "Build complete") ||
                   strstr(output, "Installation complete") ||
                   strstr(output, "Prerequisites verified") ||
                   strstr(output, "GitHub CLI authenticated") ||
                   strstr(output, "Till installation complete")) {
            printf("   %s", output);
        }
    }
    status = pclose(pipe);

    if (status != 0 || !success) {
        printf("   ❌ Build/install failed, rolling back\n");
        rollback_till(backup_path, current_exe);

        /* Also revert git */
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && git reset --hard HEAD~1", till_dir);
        if (system(cmd) != 0) {
            till_error("Failed to reset git repository");
        }

        release_lock_file(lock_fd);
        return -1;
    }
    
    /* 6. VERIFY - Test new executable */
    printf("   Verifying new version...\n");
    snprintf(cmd, sizeof(cmd), "\"%s\" --version 2>&1", current_exe);
    pipe = popen(cmd, "r");
    if (!pipe) {
        printf("   ❌ Verification failed, rolling back\n");
        rollback_till(backup_path, current_exe);
        release_lock_file(lock_fd);
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
        release_lock_file(lock_fd);
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
    till_error("Failed to restart with new version");
    return -1;
}

