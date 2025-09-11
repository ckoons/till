/*
 * till_run.c - Component command execution for Till
 * 
 * Discovers and executes commands from installed components
 * via their TILL_COMMANDS_DIR/ directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>

#include "till_config.h"
#include "till_run.h"
#include "till_common.h"
#include "cJSON.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Component structure */
typedef struct {
    char name[256];
    char root[PATH_MAX];
    char registry_name[256];
    int has_commands;
} component_t;

/* Load installed components from till-private.json */
static cJSON *load_installations(void) {
    /* Use common function to load till JSON */
    return load_till_json("tekton/till-private.json");
}

/* Extract component name from registry name */
static void extract_component_name(const char *registry_name, char *component) {
    /* For Tekton installations, component is always "tekton" */
    if (strstr(registry_name, "tekton")) {
        strncpy(component, "tekton", 256 - 1);
        component[256 - 1] = '\0';
        return;
    }
    
    /* For other components, use first part of registry name */
    const char *dot = strchr(registry_name, '.');
    if (dot) {
        size_t len = dot - registry_name;
        strncpy(component, registry_name, len);
        component[len] = '\0';
    } else {
        strncpy(component, registry_name, 256 - 1);
        component[256 - 1] = '\0';
    }
}

/* Check if component has TILL_COMMANDS_DIR directory */
static int has_command_directory(const char *root) {
    char cmd_dir[PATH_MAX];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/%s", root, TILL_COMMANDS_DIR);
    
    return is_directory(cmd_dir);
}

/* List available commands for a component */
static int list_component_commands(const char *component, const char *root) {
    char cmd_dir[PATH_MAX];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/%s", root, TILL_COMMANDS_DIR);
    
    DIR *dir = opendir(cmd_dir);
    if (!dir) {
        return 0;
    }
    
    printf("  %s:\n", component);
    
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] != '.') {
            char cmd_path[PATH_MAX];
            snprintf(cmd_path, sizeof(cmd_path), "%s/%s", cmd_dir, entry->d_name);
            
            if (is_executable(cmd_path)) {
                printf("    - %s\n", entry->d_name);
                count++;
            }
        }
    }
    
    closedir(dir);
    
    if (count == 0) {
        printf("    (no executable commands found)\n");
    }
    
    return count;
}

/* List all available components and their commands */
static int list_all_components(void) {
    cJSON *json = load_installations();
    if (!json) {
        till_error("No installations found. Run 'till install' first.");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(json, "installations");
    if (!installations) {
        till_error("No installations found in configuration.");
        cJSON_Delete(json);
        return -1;
    }
    
    printf("Available components and commands:\n\n");
    
    /* Track unique components */
    component_t components[100];
    int comp_count = 0;
    
    cJSON *installation;
    cJSON_ArrayForEach(installation, installations) {
        const char *registry_name = installation->string;
        cJSON *root_item = cJSON_GetObjectItem(installation, "root");
        
        if (root_item && cJSON_IsString(root_item)) {
            const char *root = root_item->valuestring;
            
            /* Extract component type */
            char component[256];
            extract_component_name(registry_name, component);
            
            /* Check if we've already listed this component type */
            int found = 0;
            for (int i = 0; i < comp_count; i++) {
                if (strcmp(components[i].name, component) == 0) {
                    found = 1;
                    break;
                }
            }
            
            if (!found && has_command_directory(root)) {
                strncpy(components[comp_count].name, component, sizeof(components[comp_count].name) - 1);
                components[comp_count].name[sizeof(components[comp_count].name) - 1] = '\0';
                strncpy(components[comp_count].root, root, sizeof(components[comp_count].root) - 1);
                components[comp_count].root[sizeof(components[comp_count].root) - 1] = '\0';
                strncpy(components[comp_count].registry_name, registry_name, sizeof(components[comp_count].registry_name) - 1);
                components[comp_count].registry_name[sizeof(components[comp_count].registry_name) - 1] = '\0';
                components[comp_count].has_commands = 1;
                comp_count++;
                
                list_component_commands(component, root);
                printf("\n");
            }
        }
    }
    
    if (comp_count == 0) {
        printf("No components with executable commands found.\n");
        printf("Components must have a TILL_COMMANDS_DIR/ directory with executable scripts.\n");
    } else {
        printf("Usage: till run <component> <command> [arguments...]\n");
        printf("Example: till run tekton start\n");
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Find component root by name or registry name */
static char *find_component_root(const char *component_name) {
    static char root[PATH_MAX];
    
    cJSON *json = load_installations();
    if (!json) {
        return NULL;
    }
    
    cJSON *installations = cJSON_GetObjectItem(json, "installations");
    if (!installations) {
        cJSON_Delete(json);
        return NULL;
    }
    
    /* First try exact registry name match */
    cJSON *installation;
    cJSON_ArrayForEach(installation, installations) {
        const char *registry_name = installation->string;
        if (strcmp(component_name, registry_name) == 0) {
            cJSON *root_item = cJSON_GetObjectItem(installation, "root");
            if (root_item && cJSON_IsString(root_item)) {
                strncpy(root, root_item->valuestring, PATH_MAX - 1);
                root[PATH_MAX - 1] = '\0';
                cJSON_Delete(json);
                return root;
            }
        }
    }
    
    /* Then try component type match (e.g., "tekton" matches any tekton installation) */
    /* For now, return the primary/first match */
    cJSON_ArrayForEach(installation, installations) {
        const char *registry_name = installation->string;
        char comp_type[256];
        extract_component_name(registry_name, comp_type);
        
        if (strcmp(component_name, comp_type) == 0) {
            /* Prefer "primary" installation */
            if (strstr(registry_name, "primary")) {
                cJSON *root_item = cJSON_GetObjectItem(installation, "root");
                if (root_item && cJSON_IsString(root_item)) {
                    strncpy(root, root_item->valuestring, PATH_MAX - 1);
                root[PATH_MAX - 1] = '\0';
                    cJSON_Delete(json);
                    return root;
                }
            }
        }
    }
    
    /* If no primary, return first match */
    cJSON_ArrayForEach(installation, installations) {
        const char *registry_name = installation->string;
        char comp_type[256];
        extract_component_name(registry_name, comp_type);
        
        if (strcmp(component_name, comp_type) == 0) {
            cJSON *root_item = cJSON_GetObjectItem(installation, "root");
            if (root_item && cJSON_IsString(root_item)) {
                strncpy(root, root_item->valuestring, PATH_MAX - 1);
                root[PATH_MAX - 1] = '\0';
                cJSON_Delete(json);
                return root;
            }
        }
    }
    
    cJSON_Delete(json);
    return NULL;
}

/* Execute a component command */
static int execute_component_command(const char *component, const char *command, 
                                    int argc, char *argv[]) {
    /* Find component root */
    char *root = find_component_root(component);
    if (!root) {
        till_error("Component '%s' not found.", component);
        till_info("Run 'till run' to see available components.");
        return -1;
    }
    
    /* Build path to command */
    char cmd_path[PATH_MAX];
    snprintf(cmd_path, sizeof(cmd_path), "%s/TILL_COMMANDS_DIR/%s", root, command);
    
    /* Check if command exists and is executable */
    if (!path_exists(cmd_path)) {
        till_error("Command '%s' not found for component '%s'.", 
                command, component);
        
        /* List available commands */
        char cmd_dir[PATH_MAX];
        snprintf(cmd_dir, sizeof(cmd_dir), "%s/%s", root, TILL_COMMANDS_DIR);
        
        DIR *dir = opendir(cmd_dir);
        if (dir) {
            printf("\nAvailable commands for %s:\n", component);
            struct dirent *entry;
            while ((entry = readdir(dir))) {
                if (entry->d_name[0] != '.') {
                    char check_path[PATH_MAX];
                    snprintf(check_path, sizeof(check_path), "%s/%s", 
                            cmd_dir, entry->d_name);
                    if (is_executable(check_path)) {
                        printf("  - %s\n", entry->d_name);
                    }
                }
            }
            closedir(dir);
        } else {
            till_error("Component '%s' has no TILL_COMMANDS_DIR directory.", 
                    component);
        }
        return -1;
    }
    
    if (!is_executable(cmd_path)) {
        till_error("Command '%s' is not executable.", command);
        till_info("Fix with: chmod +x %s", cmd_path);
        return -1;
    }
    
    /* Build arguments array for execv */
    char **exec_args = malloc((argc + 2) * sizeof(char *));
    if (!exec_args) {
        till_error("Memory allocation failed");
        return -1;
    }
    
    exec_args[0] = cmd_path;
    for (int i = 0; i < argc; i++) {
        exec_args[i + 1] = argv[i];
    }
    exec_args[argc + 1] = NULL;
    
    /* Change to component root directory before executing */
    char original_dir[PATH_MAX];
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        free(exec_args);
        return -1;
    }
    
    if (chdir(root) != 0) {
        till_error("Cannot change to component directory: %s", 
                strerror(errno));
        free(exec_args);
        return -1;
    }
    
    /* Log the command execution */
    till_log(LOG_INFO, "Executing command: %s %s for component %s", command, 
             argc > 0 ? argv[0] : "", component);
    
    /* Execute the command */
    pid_t pid = fork();
    if (pid == -1) {
        till_error("Fork failed: %s", strerror(errno));
        till_log(LOG_ERROR, "Fork failed for command %s: %s", command, strerror(errno));
        chdir(original_dir);
        free(exec_args);
        return -1;
    }
    
    if (pid == 0) {
        /* Child process */
        execv(cmd_path, exec_args);
        /* If we get here, exec failed */
        till_error("Failed to execute command: %s", strerror(errno));
        exit(127);
    }
    
    /* Parent process - wait for child */
    int status;
    waitpid(pid, &status, 0);
    
    /* Restore original directory */
    chdir(original_dir);
    free(exec_args);
    
    /* Log result */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            till_log(LOG_INFO, "Command %s completed successfully", command);
        } else {
            till_log(LOG_WARN, "Command %s exited with code %d", command, exit_code);
        }
        return exit_code;
    } else {
        till_log(LOG_ERROR, "Command %s terminated abnormally", command);
    }
    
    return -1;
}

/* Print run command help */
static void print_run_help(void) {
    printf("Till Run - Execute Component Commands\n\n");
    printf("Usage: till run [component] [command] [arguments]\n\n");
    printf("Description:\n");
    printf("  Execute commands defined in component TILL_COMMANDS_DIR/ directories.\n");
    printf("  Each executable file in a component's TILL_COMMANDS_DIR/ directory\n");
    printf("  becomes an available command.\n\n");
    printf("Usage patterns:\n");
    printf("  till run                         List all components with commands\n");
    printf("  till run <component>             List commands for a component\n");
    printf("  till run <component> <command>   Execute a component command\n\n");
    printf("Examples:\n");
    printf("  till run                         # Show all available components\n");
    printf("  till run tekton                  # List tekton commands\n");
    printf("  till run tekton status           # Run tekton status command\n");
    printf("  till run tekton start            # Start tekton\n");
    printf("  till run tekton stop --force     # Stop tekton with arguments\n\n");
    printf("Creating commands:\n");
    printf("  1. Create TILL_COMMANDS_DIR/ directory in component root\n");
    printf("  2. Add executable scripts (chmod +x)\n");
    printf("  3. Scripts receive arguments and run in component directory\n\n");
    printf("Note: Commands are discovered from all Tekton installations\n");
    printf("      managed by Till.\n");
}

/* Main entry point for till run command */
int till_run_command(int argc, char *argv[]) {
    /* Check for help flag */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_run_help();
            return 0;
        }
    }
    
    if (argc < 1) {
        /* No arguments - list all components and commands */
        return list_all_components();
    }
    
    if (argc < 2) {
        /* Only component specified - list its commands */
        char *root = find_component_root(argv[0]);
        if (!root) {
            till_error("Component '%s' not found.", argv[0]);
            return list_all_components();
        }
        
        printf("Available commands for %s:\n", argv[0]);
        if (list_component_commands(argv[0], root) == 0) {
            printf("Component '%s' has no executable commands.\n", argv[0]);
        }
        printf("\nUsage: till run %s <command> [arguments...]\n", argv[0]);
        return 0;
    }
    
    /* Component and command specified - execute */
    const char *component = argv[0];
    const char *command = argv[1];
    
    return execute_component_command(component, command, argc - 2, argv + 2);
}

/* Check if Till can run a component */
int till_can_run_component(const char *component) {
    char *root = find_component_root(component);
    if (!root) return 0;
    
    return has_command_directory(root);
}