/*
 * till_tekton.c - Tekton-specific installation and management
 * 
 * Handles Tekton repository cloning, setup, and configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include "till_config.h"
#include "till_tekton.h"
#include "till_common.h"
#include "till_registry.h"
#include "cJSON.h"

/* Tekton repository URL */
#define TEKTON_REPO_URL "https://github.com/Tekton-Development-Community/Tekton"

/* Clone Tekton repository */
int clone_tekton_repo(const char *path) {
    printf("Cloning Tekton repository to %s...\n", path);
    
    /* Check if directory already exists */
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            /* Check if it's already a git repo */
            char git_dir[TILL_MAX_PATH];
            snprintf(git_dir, sizeof(git_dir), "%s/.git", path);
            if (stat(git_dir, &st) == 0) {
                printf("  Directory already contains a git repository\n");
                printf("  Pulling latest changes...\n");
                
                char cmd[TILL_MAX_PATH * 2];
                snprintf(cmd, sizeof(cmd), "cd %s && git pull", path);
                return run_command(cmd, NULL, 0);
            } else {
                fprintf(stderr, "Error: Directory exists but is not a git repository: %s\n", path);
                return -1;
            }
        } else {
            fprintf(stderr, "Error: Path exists but is not a directory: %s\n", path);
            return -1;
        }
    }
    
    /* Clone the repository */
    char cmd[TILL_MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "git clone %s %s", TEKTON_REPO_URL, path);
    
    till_log(LOG_INFO, "Cloning Tekton from %s to %s", TEKTON_REPO_URL, path);
    
    if (run_command(cmd, NULL, 0) != 0) {
        fprintf(stderr, "Error: Failed to clone repository\n");
        till_log(LOG_ERROR, "Failed to clone Tekton repository");
        return -1;
    }
    
    printf("  Repository cloned successfully\n");
    return 0;
}

/* Generate .env.local file for Tekton */
int generate_tekton_env(install_options_t *opts) {
    char env_path[TILL_MAX_PATH];
    char env_example[TILL_MAX_PATH];
    FILE *fp_in, *fp_out;
    
    snprintf(env_path, sizeof(env_path), "%s/.env.local", opts->path);
    snprintf(env_example, sizeof(env_example), "%s/.env.local.example", opts->path);
    
    printf("Generating .env.local...\n");
    till_log(LOG_INFO, "Generating .env.local for %s", opts->name);
    
    /* Read .env.local.example */
    fp_in = fopen(env_example, "r");
    if (!fp_in) {
        fprintf(stderr, "Error: Cannot find .env.local.example in %s\n", opts->path);
        fprintf(stderr, "  Make sure Tekton repository was cloned correctly\n");
        till_log(LOG_ERROR, "Cannot find .env.local.example");
        return -1;
    }
    
    /* Create .env.local */
    fp_out = fopen(env_path, "w");
    if (!fp_out) {
        fclose(fp_in);
        fprintf(stderr, "Error: Cannot create .env.local\n");
        return -1;
    }
    
    /* Copy and modify content */
    char line[1024];
    while (fgets(line, sizeof(line), fp_in)) {
        char *key_start = line;
        while (*key_start == ' ' || *key_start == '\t') key_start++;
        
        /* Skip comments and empty lines */
        if (*key_start == '#' || *key_start == '\n' || *key_start == '\0') {
            fputs(line, fp_out);
            continue;
        }
        
        /* Find the key */
        char *equals = strchr(key_start, '=');
        if (!equals) {
            fputs(line, fp_out);
            continue;
        }
        
        /* Extract key */
        size_t key_len = equals - key_start;
        char key[256];
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        /* Handle specific keys */
        if (strcmp(key, "TEKTON_REGISTRY_NAME") == 0) {
            fprintf(fp_out, "TEKTON_REGISTRY_NAME=%s\n", opts->name);
        }
        else if (strcmp(key, "TEKTON_MODE") == 0) {
            fprintf(fp_out, "TEKTON_MODE=%s\n", opts->mode);
        }
        else if (strcmp(key, "TEKTON_ROOT") == 0) {
            char abs_path[TILL_MAX_PATH];
            if (realpath(opts->path, abs_path) != NULL) {
                fprintf(fp_out, "TEKTON_ROOT=%s\n", abs_path);
            } else {
                fprintf(fp_out, "TEKTON_ROOT=%s\n", opts->path);
            }
        }
        else if (strcmp(key, "TEKTON_MAIN_ROOT") == 0) {
            if (strlen(opts->tekton_main_root) > 0) {
                fprintf(fp_out, "TEKTON_MAIN_ROOT=%s\n", opts->tekton_main_root);
            } else {
                fputs(line, fp_out);
            }
        }
        /* Handle port assignments */
        else if (strstr(key, "_PORT") != NULL) {
            /* Find port offset for this component */
            int offset = -1;
            
            /* Check main component ports */
            for (int i = 0; COMPONENT_PORTS[i].name != NULL; i++) {
                if (strcmp(key, COMPONENT_PORTS[i].name) == 0) {
                    offset = COMPONENT_PORTS[i].offset;
                    break;
                }
            }
            
            /* Check AI ports */
            if (offset == -1) {
                for (int i = 0; AI_PORTS[i].name != NULL; i++) {
                    if (strcmp(key, AI_PORTS[i].name) == 0) {
                        offset = AI_PORTS[i].offset;
                        fprintf(fp_out, "%s=%d\n", key, opts->ai_port_base + offset);
                        continue;
                    }
                }
            }
            
            if (offset >= 0) {
                fprintf(fp_out, "%s=%d\n", key, opts->port_base + offset);
            } else {
                /* Unknown port - keep as is */
                fputs(line, fp_out);
            }
        }
        else {
            /* Keep other values as is */
            fputs(line, fp_out);
        }
    }
    
    fclose(fp_in);
    fclose(fp_out);
    
    printf("  .env.local generated with ports %d-%d and AI ports %d-%d\n",
           opts->port_base, opts->port_base + 99,
           opts->ai_port_base, opts->ai_port_base + 99);
    
    till_log(LOG_INFO, ".env.local generated successfully");
    return 0;
}

/* Install Python dependencies for Tekton */
int install_tekton_dependencies(const char *path) {
    printf("Installing Python dependencies...\n");
    till_log(LOG_INFO, "Installing Python dependencies for Tekton");
    
    char pip_cmd[TILL_MAX_PATH * 2];
    snprintf(pip_cmd, sizeof(pip_cmd), 
             "cd %s && pip install -e . > /dev/null 2>&1", path);
    
    printf("  Running pip install (this may take a few minutes)...\n");
    if (system(pip_cmd) != 0) {
        fprintf(stderr, "Warning: Failed to install Python dependencies\n");
        fprintf(stderr, "  You may need to run 'pip install -e .' manually in %s\n", path);
        till_log(LOG_WARN, "Failed to install Python dependencies automatically");
        /* Not a fatal error - continue with installation */
        return 0;
    }
    
    printf("  Python dependencies installed successfully\n");
    till_log(LOG_INFO, "Python dependencies installed successfully");
    return 0;
}

/* Create .till symlink in Tekton directory */
int create_till_symlink(const char *tekton_path) {
    char till_dir[TILL_MAX_PATH];
    char symlink_path[TILL_MAX_PATH];
    char *home = getenv("HOME");
    
    if (!home) {
        till_log(LOG_WARN, "Cannot determine home directory for .till symlink");
        return -1;
    }
    
    /* Point to the actual till/.till directory */
    snprintf(till_dir, sizeof(till_dir), "%s/projects/github/till/.till", home);
    snprintf(symlink_path, sizeof(symlink_path), "%s/.till", tekton_path);
    
    /* Check if .till already exists */
    struct stat st;
    if (lstat(symlink_path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            /* It's already a symlink, check if it points to the right place */
            char link_target[TILL_MAX_PATH];
            ssize_t len = readlink(symlink_path, link_target, sizeof(link_target) - 1);
            if (len > 0) {
                link_target[len] = '\0';
                if (strcmp(link_target, till_dir) == 0) {
                    printf("  .till symlink already exists and is correct\n");
                    return 0;
                }
            }
            /* Wrong target, remove and recreate */
            unlink(symlink_path);
        } else {
            fprintf(stderr, "Warning: .till exists but is not a symlink\n");
            return -1;
        }
    }
    
    /* Create the symlink */
    if (symlink(till_dir, symlink_path) != 0) {
        fprintf(stderr, "Warning: Could not create .till symlink: %s\n", strerror(errno));
        till_log(LOG_WARN, "Failed to create .till symlink: %s", strerror(errno));
        return -1;
    }
    
    printf("  Created .till symlink -> %s\n", till_dir);
    till_log(LOG_INFO, "Created .till symlink in %s", tekton_path);
    return 0;
}

/* Main Tekton installation function */
int install_tekton(install_options_t *opts) {
    printf("Installing Tekton...\n");
    printf("  Name: %s\n", opts->name);
    printf("  Path: %s\n", opts->path);
    printf("  Mode: %s\n", opts->mode);
    printf("  Port Base: %d\n", opts->port_base);
    printf("  AI Port Base: %d\n", opts->ai_port_base);
    
    till_log(LOG_INFO, "Starting Tekton installation: %s at %s", opts->name, opts->path);
    
    /* Step 1: Clone Tekton repository */
    if (clone_tekton_repo(opts->path) != 0) {
        fprintf(stderr, "Failed to clone Tekton repository\n");
        return -1;
    }
    
    /* Step 2: Install Python dependencies */
    install_tekton_dependencies(opts->path);
    
    /* Step 3: Generate .env.local */
    if (generate_tekton_env(opts) != 0) {
        fprintf(stderr, "Failed to generate .env.local\n");
        return -1;
    }
    
    /* Step 4: Register installation */
    if (register_installation(opts->name, opts->path, opts->port_base, 
                            opts->ai_port_base, opts->mode) != 0) {
        fprintf(stderr, "Failed to register installation\n");
        return -1;
    }
    
    /* Step 5: Create .till symlink */
    create_till_symlink(opts->path);
    
    /* Step 6: Setup Python tooling */
    printf("\nSetting up Python tooling...\n");
    char setup_cmd[TILL_MAX_PATH * 2];
    
    /* Install ipykernel for Jupyter support */
    snprintf(setup_cmd, sizeof(setup_cmd),
             "cd %s && python -m ipykernel install --user --name tekton-%s 2>/dev/null",
             opts->path, opts->name);
    if (system(setup_cmd) == 0) {
        printf("  Jupyter kernel 'tekton-%s' installed\n", opts->name);
    }
    
    /* Create initial directories if needed */
    char dir_path[TILL_MAX_PATH];
    const char *dirs[] = {"logs", "data", "output", NULL};
    for (int i = 0; dirs[i] != NULL; i++) {
        snprintf(dir_path, sizeof(dir_path), "%s/%s", opts->path, dirs[i]);
        ensure_directory(dir_path);
    }
    
    printf("\nTekton installation complete!\n");
    printf("Python dependencies have been installed.\n");
    printf("To start: cd %s && tekton start\n", opts->path);
    
    till_log(LOG_INFO, "Tekton installation completed successfully");
    return 0;
}

/* Update existing Tekton installation */
int update_tekton(const char *path) {
    printf("Updating Tekton at %s...\n", path);
    till_log(LOG_INFO, "Updating Tekton at %s", path);
    
    char cmd[TILL_MAX_PATH * 2];
    
    /* Git pull */
    snprintf(cmd, sizeof(cmd), "cd %s && git pull", path);
    if (run_command(cmd, NULL, 0) != 0) {
        fprintf(stderr, "Failed to pull updates\n");
        return -1;
    }
    
    /* Update Python dependencies */
    snprintf(cmd, sizeof(cmd), "cd %s && pip install -e . --upgrade", path);
    if (system(cmd) != 0) {
        fprintf(stderr, "Warning: Failed to update Python dependencies\n");
    }
    
    printf("Tekton updated successfully\n");
    return 0;
}