/*
 * till_host.c - Host management for Till (Tekton Lifecycle Manager)
 * 
 * Manages remote hosts using SSH and rsync for federation.
 * "The Till Way" - Simple, works, hard to screw up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>

#include "till_config.h"
#include "till_host.h"
#include "till_common.h"
#include "cJSON.h"

/* PATH_MAX fallback for systems that don't define it */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Forward declarations */
static cJSON *load_hosts(void);
static int save_hosts(cJSON *json);
static int propagate_hosts_update(void);
static int send_hosts_to_remote(const char *name);
static int merge_hosts_from_remote(const char *json_str);

/* Helper to get host connection info from JSON */
static int get_host_info(const char *name, char *user, size_t user_size, 
                        char *host, size_t host_size, int *port) {
    cJSON *json = load_hosts();
    if (!json) return -1;
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON *host_obj = cJSON_GetObjectItem(hosts, name);
    if (!host_obj) {
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON *user_item = cJSON_GetObjectItem(host_obj, "user");
    cJSON *host_item = cJSON_GetObjectItem(host_obj, "host");
    cJSON *port_item = cJSON_GetObjectItem(host_obj, "port");
    
    if (!user_item || !host_item) {
        cJSON_Delete(json);
        return -1;
    }
    
    strncpy(user, cJSON_GetStringValue(user_item), user_size - 1);
    user[user_size - 1] = '\0';
    strncpy(host, cJSON_GetStringValue(host_item), host_size - 1);
    host[host_size - 1] = '\0';
    *port = port_item ? cJSON_GetNumberValue(port_item) : 22;
    
    cJSON_Delete(json);
    return 0;
}


/* Load hosts from JSON file */
static cJSON *load_hosts(void) {
    char path[PATH_MAX];
    // Till dir check
    char till_dir[PATH_MAX];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) return NULL;
    
    snprintf(path, sizeof(path), "%s/hosts-local.json", till_dir);
    
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);
    
    cJSON *json = cJSON_Parse(content);
    free(content);
    
    return json;
}

/* Save hosts to JSON file */
static int save_hosts(cJSON *json) {
    char path[PATH_MAX];
    // Till dir check
    char till_dir[PATH_MAX];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) return -1;
    
    snprintf(path, sizeof(path), "%s/hosts-local.json", till_dir);
    
    /* Update timestamp */
    cJSON *updated = cJSON_GetObjectItem(json, "updated");
    if (updated) {
        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "%ld", time(NULL));
        cJSON_SetValuestring(updated, timestamp);
    }
    
    char *output = cJSON_Print(json);
    if (!output) return -1;
    
    FILE *fp = fopen(path, "w");
    if (!fp) {
        free(output);
        return -1;
    }
    
    fprintf(fp, "%s", output);
    fclose(fp);
    free(output);
    
    return 0;
}

/* Add a new host */
int till_host_add(const char *name, const char *user_at_host) {
    if (!name || !user_at_host) {
        fprintf(stderr, "Usage: till host add <name> <user>@<host>[:port]\n");
        return -1;
    }
    
    printf("Adding host '%s'...\n", name);
    
    /* Parse user@host:port */
    char *at = strchr(user_at_host, '@');
    if (!at) {
        fprintf(stderr, "Error: Invalid format. Use: user@host[:port]\n");
        return -1;
    }
    
    char user[256];
    char host[256];
    int port = 22;
    
    size_t user_len = at - user_at_host;
    if (user_len >= sizeof(user)) {
        fprintf(stderr, "Error: Username too long\n");
        return -1;
    }
    strncpy(user, user_at_host, user_len);
    user[user_len] = '\0';
    
    char *colon = strchr(at + 1, ':');
    if (colon) {
        size_t host_len = colon - (at + 1);
        if (host_len >= sizeof(host)) {
            fprintf(stderr, "Error: Hostname too long\n");
            return -1;
        }
        strncpy(host, at + 1, host_len);
        host[host_len] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(host, at + 1, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }
    
    /* Load existing hosts */
    cJSON *json = load_hosts();
    if (!json) {
        json = cJSON_CreateObject();
        cJSON_AddObjectToObject(json, "hosts");
        cJSON_AddStringToObject(json, "updated", "0");
        
        /* Add local hostname on first creation */
        char local_hostname[256];
        if (gethostname(local_hostname, sizeof(local_hostname)) == 0) {
            cJSON_AddStringToObject(json, "local_hostname", local_hostname);
        }
    }
    
    /* Ensure local_hostname exists */
    if (!cJSON_GetObjectItem(json, "local_hostname")) {
        char local_hostname[256];
        if (gethostname(local_hostname, sizeof(local_hostname)) == 0) {
            cJSON_AddStringToObject(json, "local_hostname", local_hostname);
        }
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        hosts = cJSON_AddObjectToObject(json, "hosts");
    }
    
    /* Check if host already exists */
    if (cJSON_GetObjectItem(hosts, name)) {
        fprintf(stderr, "Error: Host '%s' already exists\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    /* Add host to JSON */
    cJSON *host_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(host_obj, "user", user);
    cJSON_AddStringToObject(host_obj, "host", host);
    cJSON_AddNumberToObject(host_obj, "port", port);
    cJSON_AddStringToObject(host_obj, "status", "untested");
    
    /* Add timestamp properly */
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    cJSON_AddStringToObject(host_obj, "added", timestamp);
    cJSON_AddStringToObject(host_obj, "last_seen", timestamp);
    
    /* Version for update tracking */
    cJSON_AddNumberToObject(host_obj, "version", 1);
    
    cJSON_AddItemToObject(hosts, name, host_obj);
    
    /* Get Till directory */
    char till_dir[PATH_MAX];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) {
        fprintf(stderr, "Error: Cannot find Till directory\n");
        cJSON_Delete(json);
        return -1;
    }
    
    /* Save hosts first */
    if (save_hosts(json) != 0) {
        fprintf(stderr, "Error: Failed to save hosts\n");
        cJSON_Delete(json);
        return -1;
    }
    
    printf("✓ Host '%s' added (%s@%s:%d)\n", name, user, host, port);
    
    /* Test SSH connectivity */
    printf("\nTesting SSH connectivity...\n");
    char ssh_test[512];
    snprintf(ssh_test, sizeof(ssh_test), 
        "ssh -o ConnectTimeout=5 -o BatchMode=yes -o StrictHostKeyChecking=no "
        "-o PasswordAuthentication=no -p %d %s@%s exit 2>/dev/null || "
        "ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no -p %d %s@%s exit 2>/dev/null",
        port, user, host, port, user, host);
    
    if (system(ssh_test) != 0) {
        printf("✗ Cannot connect via SSH to %s@%s:%d\n", user, host, port);
        printf("  Please verify:\n");
        printf("  - The hostname/IP is correct\n");
        printf("  - SSH is enabled on the remote host\n");
        printf("  - Your credentials are correct\n");
        printf("\nFor macOS remote hosts:\n");
        printf("  1. Go to System Settings > General > Sharing\n");
        printf("  2. Turn on 'Remote Login'\n");
        cJSON_Delete(json);
        return -1;
    }
    printf("✓ SSH connectivity confirmed\n");
    
    /* Detect remote platform and hostname */
    printf("\nDetecting remote platform...\n");
    char detect_cmd[512];
    char platform[64] = "unknown";
    char actual_hostname[256] = "";
    
    snprintf(detect_cmd, sizeof(detect_cmd),
        "ssh %s@%s -p %d 'uname -s' 2>/dev/null",
        user, host, port);
    
    FILE *fp = popen(detect_cmd, "r");
    if (fp) {
        if (fgets(platform, sizeof(platform), fp)) {
            /* Remove newline */
            platform[strcspn(platform, "\n")] = '\0';
        }
        pclose(fp);
    }
    printf("✓ Remote platform: %s\n", platform);
    
    /* Get actual hostname from remote */
    snprintf(detect_cmd, sizeof(detect_cmd),
        "ssh %s@%s -p %d 'hostname' 2>/dev/null",
        user, host, port);
    
    fp = popen(detect_cmd, "r");
    if (fp) {
        if (fgets(actual_hostname, sizeof(actual_hostname), fp)) {
            /* Remove newline */
            actual_hostname[strcspn(actual_hostname, "\n")] = '\0';
            printf("✓ Remote hostname: %s\n", actual_hostname);
            
            /* Update host object with actual hostname */
            cJSON_AddStringToObject(host_obj, "actual_hostname", actual_hostname);
        }
        pclose(fp);
    }
    
    /* Install Till on remote */
    printf("\nInstalling Till on remote host...\n");
    
    /* Check if Till already exists */
    char check_till[512];
    snprintf(check_till, sizeof(check_till),
        "ssh %s@%s -p %d 'test -d " TILL_REMOTE_INSTALL_PATH " && echo EXISTS' 2>/dev/null",
        user, host, port);
    
    fp = popen(check_till, "r");
    char exists[32] = "";
    if (fp) {
        fgets(exists, sizeof(exists), fp);
        pclose(fp);
    }
    
    if (strstr(exists, "EXISTS")) {
        printf("✓ Till already installed on remote\n");
        
        /* Update it with till update */
        char update_cmd[1024];
        snprintf(update_cmd, sizeof(update_cmd),
            "ssh %s@%s -p %d 'cd " TILL_REMOTE_INSTALL_PATH " && ./till update' 2>&1",
            user, host, port);
        
        printf("  Updating Till on remote...\n");
        if (system(update_cmd) == 0) {
            printf("✓ Till updated successfully\n");
        } else {
            printf("⚠ Warning: Till update failed (may be local changes)\n");
        }
    } else {
        /* Install fresh */
        char install_cmd[2048];
        snprintf(install_cmd, sizeof(install_cmd),
            "ssh %s@%s -p %d '"
            "mkdir -p ~/%s && "
            "cd ~/%s && "
            "git clone " TILL_REPO_URL ".git && "
            "cd till && "
            "make && "
            "./till update' 2>&1",
            user, host, port, TILL_PROJECTS_BASE, TILL_PROJECTS_BASE);
        
        printf("  Cloning and building Till...\n");
        if (system(install_cmd) != 0) {
            printf("✗ Failed to install Till on remote\n");
            printf("  Try manually: ssh %s\n", name);
            cJSON_Delete(json);
            return -1;
        }
        printf("✓ Till installed and initialized successfully\n");
    }
    
    /* Verify Till works on remote */
    printf("\nVerifying Till installation...\n");
    char verify_cmd[512];
    snprintf(verify_cmd, sizeof(verify_cmd),
        "ssh %s@%s -p %d '" TILL_REMOTE_INSTALL_PATH "/till --version' 2>/dev/null",
        user, host, port);
    
    fp = popen(verify_cmd, "r");
    if (fp) {
        char version[128];
        if (fgets(version, sizeof(version), fp)) {
            printf("✓ Remote Till: %s", version);
        }
        pclose(fp);
    }
    
    /* Update host status to 'ready' */
    cJSON_SetValuestring(cJSON_GetObjectItem(host_obj, "status"), "ready");
    save_hosts(json);
    
    /* Send our hosts list to the newly added host */
    printf("\nSharing host information with remote...\n");
    if (send_hosts_to_remote(name) == 0) {
        printf("✓ Host information shared\n");
    } else {
        printf("⚠ Warning: Could not share host information\n");
    }
    
    /* Propagate the update to all other hosts */
    printf("\nPropagating update to other hosts...\n");
    if (propagate_hosts_update() == 0) {
        printf("✓ All hosts updated\n");
    } else {
        printf("⚠ Warning: Some hosts may not have been updated\n");
    }
    
    printf("\n✅ Host '%s' fully configured and ready!\n", name);
    printf("\nYou can now use:\n");
    printf("  till host ssh %s          # Interactive shell\n", name);
    printf("  till host exec %s <cmd>   # Run remote commands\n", name);
    printf("  till host status %s       # Check status\n", name);
    
    cJSON_Delete(json);
    return 0;
}

/* Test SSH connectivity to a host */
int till_host_test(const char *name) {
    if (!name) {
        fprintf(stderr, "Usage: till host test <name>\n");
        return -1;
    }
    
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        return -1;
    }
    
    /* Load hosts for status update */
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "Error: Cannot load hosts\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host_obj = cJSON_GetObjectItem(hosts, name);
    
    /* Test SSH connection */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), 
        "ssh -o ConnectTimeout=5 %s@%s -p %d 'echo TILL_TEST_SUCCESS'",
        user, host, port);
    
    printf("Testing connection to '%s'...\n", name);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Error: Failed to test connection\n");
        cJSON_Delete(json);
        return -1;
    }
    
    char line[256];
    int success = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "TILL_TEST_SUCCESS")) {
            success = 1;
        }
    }
    
    int status = pclose(fp);
    
    if (success && status == 0) {
        printf("✓ Connection successful\n");
        
        /* Update status in JSON */
        cJSON *status_item = cJSON_GetObjectItem(host_obj, "status");
        if (status_item) {
            cJSON_SetValuestring(status_item, "connected");
        }
        
        /* Test Till installation */
        snprintf(cmd, sizeof(cmd),
            "ssh %s@%s -p %d 'which till 2>/dev/null'",
            user, host, port);
        
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(line, sizeof(line), fp)) {
                printf("✓ Till found at: %s", line);
                if (status_item) {
                    cJSON_SetValuestring(status_item, TILL_STATUS_READY);
                }
            } else {
                printf("⚠ Till not found on remote host\n");
                printf("  Run: till host setup %s\n", name);
            }
            pclose(fp);
        }
        
        save_hosts(json);
    } else {
        printf("✗ Connection failed\n");
        printf("Check:\n");
        printf("  1. SSH key is in remote authorized_keys\n");
        printf("  2. Host is reachable\n");
        printf("  3. SSH service is running\n");
    }
    
    cJSON_Delete(json);
    return success ? 0 : -1;
}

/* Setup Till on remote host */
int till_host_setup(const char *name) {
    if (!name) {
        fprintf(stderr, "Usage: till host setup <name>\n");
        return -1;
    }
    
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        return -1;
    }
    
    /* First test connection */
    if (till_host_test(name) != 0) {
        fprintf(stderr, "Error: Cannot connect to host '%s'\n", name);
        return -1;
    }
    
    char cmd[4096];
    
    /* Use git clone approach - NO bootstrap script */
    printf("Installing Till on remote host from GitHub...\n");
    
    /* Clone, build, and run till update - use user's SSH setup for installation */
    snprintf(cmd, sizeof(cmd),
        "ssh %s@%s -p %d '"
        "mkdir -p ~/%s && "
        "cd ~/%s && "
        "if [ -d till ]; then "
        "  cd till && git pull && make && ./till update; "
        "else "
        "  git clone " TILL_REPO_URL ".git && "
        "  cd till && make && ./till update; "
        "fi' 2>&1",
        user, host, port, TILL_PROJECTS_BASE, TILL_PROJECTS_BASE);
    
    printf("  Cloning from GitHub, building, and initializing...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to install Till\n");
        fprintf(stderr, "Check remote host for:\n");
        fprintf(stderr, "  - git installed\n");
        fprintf(stderr, "  - make installed\n");
        fprintf(stderr, "  - C compiler (gcc/clang) installed\n");
        fprintf(stderr, "  - Internet connectivity to GitHub\n");
        return -1;
    }
    
    printf("✓ Till installed to ~/%s/till\n", TILL_PROJECTS_BASE);
    
    /* Verify installation - check multiple locations */
    snprintf(cmd, sizeof(cmd),
        "ssh %s@%s -p %d "
        "'~/%s --version 2>/dev/null || "
        "~/" TILL_REMOTE_INSTALL_PATH "/till --version 2>/dev/null || "
        "till --version 2>/dev/null'",
        user, host, port, TILL_REMOTE_BINARY_PATH);
    
    printf("Verifying Till installation...\n");
    if (system(cmd) == 0) {
        printf("✓ Till successfully installed on '%s'\n", name);
        
        /* Update host status */
        cJSON *json = load_hosts();
        if (json) {
            cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
            cJSON *host = cJSON_GetObjectItem(hosts, name);
            if (host) {
                cJSON *status_item = cJSON_GetObjectItem(host, "status");
                if (status_item) {
                    cJSON_SetValuestring(status_item, TILL_STATUS_READY);
                }
            }
            save_hosts(json);
            cJSON_Delete(json);
        }
        
        return 0;
    } else {
        fprintf(stderr, "Warning: Till installed but not in PATH\n");
        fprintf(stderr, "Remote users should run: %s/till\n", TILL_REMOTE_INSTALL_PATH);
        return 0;
    }
}

/* Execute command on remote host */
int till_host_exec(const char *name, const char *command) {
    if (!name || !command) {
        fprintf(stderr, "Usage: till host exec <name> <command>\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  till host exec m2 'till status'\n");
        fprintf(stderr, "  till host exec m2 'till install tekton --mode solo'\n");
        fprintf(stderr, "  till host exec m2 'till run tekton status'\n");
        return -1;
    }
    
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        fprintf(stderr, "Run 'till host status' to see configured hosts\n");
        return -1;
    }
    
    /* Build SSH command */
    char cmd[4096];
    
    /* Try to use till from PATH first, then fallback to known locations */
    snprintf(cmd, sizeof(cmd),
        "ssh %s@%s -p %d "
        "'export PATH=\"$HOME/.local/bin:$PATH\"; %s'",
        user, host, port, command);
    
    printf("Executing on '%s': %s\n", name, command);
    
    /* Execute the command and return its exit status */
    int result = system(cmd);
    
    if (result == -1) {
        fprintf(stderr, "Error: Failed to execute command\n");
        return -1;
    }
    
    /* Return the exit status of the remote command */
    return WEXITSTATUS(result);
}

/* SSH interactive session to remote host */
int till_host_ssh(const char *name, int argc, char *argv[]) {
    if (!name) {
        fprintf(stderr, "Usage: till host ssh <name> [command...]\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  till host ssh m2                # Open interactive shell\n");
        fprintf(stderr, "  till host ssh m2 vim config.txt # Run interactive command\n");
        return -1;
    }
    
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        fprintf(stderr, "Run 'till host status' to see configured hosts\n");
        return -1;
    }
    
    /* Build argument array for execvp */
    char **ssh_args = malloc((argc + 10) * sizeof(char *));
    if (!ssh_args) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }
    
    /* Build connection string */
    char connection[512];
    snprintf(connection, sizeof(connection), "%s@%s", user, host);
    
    /* Build port string */
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int i = 0;
    ssh_args[i++] = "ssh";
    ssh_args[i++] = "-t";  /* Force pseudo-terminal allocation for interactive use */
    ssh_args[i++] = "-p";
    ssh_args[i++] = port_str;
    ssh_args[i++] = connection;
    
    /* Add any additional command arguments */
    for (int j = 0; j < argc; j++) {
        ssh_args[i++] = argv[j];
    }
    ssh_args[i] = NULL;
    
    /* Execute SSH, replacing the current process */
    printf("Connecting to '%s'...\n", name);
    fflush(stdout);
    
    execvp("ssh", ssh_args);
    
    /* If we get here, exec failed */
    fprintf(stderr, "Error: Failed to execute SSH: %s\n", strerror(errno));
    free(ssh_args);
    return -1;
}

/* Sync from remote host */
int till_host_sync(const char *name) {
    if (!name) {
        /* Sync all hosts */
        cJSON *json = load_hosts();
        if (!json) {
            fprintf(stderr, "Error: No hosts configured\n");
            return -1;
        }
        
        cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
        cJSON *host = NULL;
        
        printf("Syncing all hosts...\n");
        cJSON_ArrayForEach(host, hosts) {
            const char *host_name = host->string;
            cJSON *status = cJSON_GetObjectItem(host, "status");
            if (status && strcmp(status->valuestring, TILL_STATUS_READY) == 0) {
                printf("\nSyncing '%s'...\n", host_name);
                till_host_sync(host_name);
            }
        }
        
        cJSON_Delete(json);
        return 0;
    }
    
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        return -1;
    }
    
    /* Sync specific host */
    char cmd[1024];
    
    printf("Syncing from '%s'...\n", name);
    
    /* Pull updates on remote */
    snprintf(cmd, sizeof(cmd),
        "ssh %s@%s -p %d 'till sync'",
        user, host, port);
    
    if (system(cmd) != 0) {
        fprintf(stderr, "Warning: Remote sync failed\n");
    }
    
    return 0;
}

/* Remove a host */
int till_host_remove(const char *name, int clean_remote) {
    if (!name) {
        fprintf(stderr, "Usage: till host remove <name> [--clean-remote]\n");
        return -1;
    }
    
    // Till dir check
    char till_dir[PATH_MAX];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) return -1;
    
    /* Load existing hosts */
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "Error: No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        fprintf(stderr, "Error: No hosts configured\n");
        cJSON_Delete(json);
        return -1;
    }
    
    /* Check if host exists */
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    /* Get the hostname before we delete the host entry */
    char *saved_hostname = NULL;
    cJSON *host_field = cJSON_GetObjectItem(host, "host");
    if (host_field && host_field->valuestring) {
        saved_hostname = strdup(host_field->valuestring);
    }
    
    /* If requested, clean up remote Till installation */
    if (clean_remote) {
        /* Get host info before removing */
        char user[256], host_addr[256];
        int port;
        if (get_host_info(name, user, sizeof(user), host_addr, sizeof(host_addr), &port) == 0) {
            printf("Cleaning up remote Till installation on '%s'...\n", name);
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                "ssh %s@%s -p %d 'rm -rf ~/" TILL_REMOTE_INSTALL_PATH "'",
                user, host_addr, port);
            
            if (system(cmd) != 0) {
                fprintf(stderr, "Warning: Failed to clean remote installation\n");
                /* Continue with local cleanup */
            }
        } else {
            fprintf(stderr, "Warning: Cannot clean remote - host info not found\n");
        }
    }
    
    /* 1. Delete host entry from JSON */
    printf("Removing host '%s'...\n", name);
    cJSON_DeleteItemFromObject(hosts, name);
    
    /* Save updated hosts file */
    if (save_hosts(json) != 0) {
        fprintf(stderr, "Error: Failed to update hosts file\n");
        if (saved_hostname) free(saved_hostname);
        cJSON_Delete(json);
        return -1;
    }
    
    /* Propagate the removal to other hosts */
    printf("Propagating removal to other hosts...\n");
    if (propagate_hosts_update() == 0) {
        printf("✓ All hosts updated\n");
    } else {
        printf("⚠ Warning: Some hosts may not have been updated\n");
    }
    
    cJSON_Delete(json);
    
    /* 2. Remove SSH config entry */
    printf("Removing SSH configuration for '%s'...\n", name);
    char ssh_config[PATH_MAX];
    char ssh_config_tmp[PATH_MAX];
    snprintf(ssh_config, sizeof(ssh_config), "%s/ssh/config", till_dir);
    snprintf(ssh_config_tmp, sizeof(ssh_config_tmp), "%s.tmp", ssh_config);
    
    FILE *fp_in = fopen(ssh_config, "r");
    FILE *fp_out = fopen(ssh_config_tmp, "w");
    
    if (fp_in && fp_out) {
        char line[1024];
        char prev_line[1024] = "";
        int skip_block = 0;
        int skip_next_blank = 0;
        char host_pattern[256];
        snprintf(host_pattern, sizeof(host_pattern), "Host %s", name);
        
        while (fgets(line, sizeof(line), fp_in)) {
            /* Check if this is the start of the host block to remove */
            if (strstr(line, host_pattern) != NULL) {
                skip_block = 1;
                skip_next_blank = 1;
                /* Don't write the previous line if it's a comment for this host */
                if (strstr(prev_line, name) != NULL && prev_line[0] == '#') {
                    /* Clear prev_line so it won't be written */
                    prev_line[0] = '\0';
                }
                continue;
            }
            
            /* End of a host block */
            if (skip_block) {
                if (line[0] == '\n' || strncmp(line, "Host ", 5) == 0 || 
                    strncmp(line, "#", 1) == 0) {
                    skip_block = 0;
                    if (line[0] == '\n' && skip_next_blank) {
                        skip_next_blank = 0;
                        continue; /* Skip the blank line after the block */
                    }
                } else {
                    continue; /* Skip lines in the block */
                }
            }
            
            /* Write the previous line if it exists */
            if (prev_line[0] != '\0') {
                fputs(prev_line, fp_out);
            }
            
            /* Save current line as previous for next iteration */
            strcpy(prev_line, line);
        }
        
        /* Write the last line if it wasn't part of a removed block */
        if (prev_line[0] != '\0' && !skip_block) {
            fputs(prev_line, fp_out);
        }
        
        fclose(fp_in);
        fclose(fp_out);
        
        /* Replace original with temp */
        if (rename(ssh_config_tmp, ssh_config) != 0) {
            fprintf(stderr, "Error: Failed to update SSH config\n");
            unlink(ssh_config_tmp);
            return -1;
        }
    } else {
        if (fp_in) fclose(fp_in);
        if (fp_out) fclose(fp_out);
        fprintf(stderr, "Warning: Could not update SSH config\n");
    }
    
    /* 3. Clean up SSH control socket if it exists */
    printf("Cleaning up SSH control sockets...\n");
    char control_path[PATH_MAX];
    snprintf(control_path, sizeof(control_path), 
        "%s/ssh/control/%s-*", till_dir, name);
    
    char cmd[PATH_MAX + 10];
    snprintf(cmd, sizeof(cmd), "rm -f %s 2>/dev/null", control_path);
    system(cmd);
    
    /* 4. Remove from known_hosts */
    printf("Cleaning up known_hosts entry...\n");
    char known_hosts[PATH_MAX];
    char known_hosts_tmp[PATH_MAX];
    snprintf(known_hosts, sizeof(known_hosts), "%s/ssh/known_hosts", till_dir);
    snprintf(known_hosts_tmp, sizeof(known_hosts_tmp), "%s.tmp", known_hosts);
    
    if (saved_hostname) {
        fp_in = fopen(known_hosts, "r");
        fp_out = fopen(known_hosts_tmp, "w");
        
        if (fp_in && fp_out) {
            char line[4096];
            while (fgets(line, sizeof(line), fp_in)) {
                /* Skip lines containing the hostname */
                if (strstr(line, saved_hostname) == NULL) {
                    fputs(line, fp_out);
                }
            }
            
            fclose(fp_in);
            fclose(fp_out);
            
            rename(known_hosts_tmp, known_hosts);
        } else {
            if (fp_in) fclose(fp_in);
            if (fp_out) fclose(fp_out);
        }
        
        free(saved_hostname);
    }
    
    printf("✓ Host '%s' removed successfully\n", name);
    return 0;
}

/* Show host status */
int till_host_status(const char *name) {
    cJSON *json = load_hosts();
    if (!json) {
        printf("No hosts configured.\n");
        printf("Run: till host add <name> <user>@<host>\n");
        return 0;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts || cJSON_GetArraySize(hosts) == 0) {
        printf("No hosts configured.\n");
        printf("Run: till host add <name> <user>@<host>\n");
        cJSON_Delete(json);
        return 0;
    }
    
    if (name) {
        /* Show specific host */
        cJSON *host = cJSON_GetObjectItem(hosts, name);
        if (!host) {
            fprintf(stderr, "Error: Host '%s' not found\n", name);
            cJSON_Delete(json);
            return -1;
        }
        
        printf("Host: %s\n", name);
        printf("  User: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "user")));
        printf("  Host: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "host")));
        printf("  Port: %d\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port")));
        printf("  Status: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "status")));
        printf("  SSH alias: %s\n", name);
    } else {
        /* Show all hosts */
        printf("Configured hosts:\n");
        printf("%-20s %-30s %-15s\n", "Name", "Host", "Status");
        printf("%-20s %-30s %-15s\n", "----", "----", "------");
        
        cJSON *host = NULL;
        cJSON_ArrayForEach(host, hosts) {
            const char *host_name = host->string;
            const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
            const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
            const char *status = cJSON_GetStringValue(cJSON_GetObjectItem(host, "status"));
            
            printf("%-20s %s@%-25s %-15s\n", host_name, user, hostname, status);
        }
        
        printf("\nSSH aliases: <name>\n");
        printf("Example: ssh remote1\n");
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Print host command help */
static void print_host_help(void) {
    printf("Till Host Management Commands\n\n");
    printf("Usage: till host <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  add <name> <user@host>  Add a new host to Till management\n");
    printf("  test <name>             Test SSH connectivity to a host\n");
    printf("  setup <name>            Install Till on remote host\n");
    printf("  exec <name> <command>   Execute Till command on remote host\n");
    printf("  ssh <name> [command]    Open SSH session to remote host\n");
    printf("  sync                    Sync host list with all known hosts\n");
    printf("  status [name]           Show host configuration and status\n");
    printf("  remove <name> [opts]    Remove a host (soft delete)\n");
    printf("\nOptions:\n");
    printf("  --help, -h              Show this help message\n");
    printf("\nExamples:\n");
    printf("  till host add m2 casey@10.10.10.2       # Add host 'm2'\n");
    printf("  till host test m2                       # Test connection\n");
    printf("  till host setup m2                      # Install Till on m2\n");
    printf("  till host exec m2 'till status'         # Run status on m2\n");
    printf("  till host exec m2 'till install tekton' # Install Tekton on m2\n");
    printf("  till host ssh m2                        # Open interactive shell\n");
    printf("  till host ssh m2 ls -la                 # Run command on m2\n");
    printf("  till host sync                          # Sync all hosts\n");
    printf("  till host remove m2                     # Remove host (keep remote)\n");
    printf("  till host remove m2 --clean-remote      # Remove and clean remote\n");
    printf("\nConfiguration:\n");
    printf("  Hosts file: .till/hosts-local.json\n");
    printf("  Uses your existing SSH keys and configuration\n");
}

/* Main host command handler */
int till_host_command(int argc, char *argv[]) {
    /* Check for help flag at any position */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_host_help();
            return 0;
        }
    }
    
    if (argc < 2) {
        /* Default to status */
        return till_host_status(NULL);
    }
    
    const char *subcmd = argv[1];
    
    if (strcmp(subcmd, "add") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: till host add <name> <user>@<host>[:port]\n");
            return -1;
        }
        return till_host_add(argv[2], argv[3]);
    }
    else if (strcmp(subcmd, "test") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: till host test <name>\n");
            return -1;
        }
        return till_host_test(argv[2]);
    }
    else if (strcmp(subcmd, "setup") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: till host setup <name>\n");
            return -1;
        }
        return till_host_setup(argv[2]);
    }
    else if (strcmp(subcmd, "exec") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: till host exec <name> <command>\n");
            return -1;
        }
        /* Combine remaining arguments into a single command string */
        char command[4096] = "";
        for (int i = 3; i < argc; i++) {
            if (i > 3) strcat(command, " ");
            strcat(command, argv[i]);
        }
        return till_host_exec(argv[2], command);
    }
    else if (strcmp(subcmd, "ssh") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: till host ssh <name> [command]\n");
            return -1;
        }
        return till_host_ssh(argv[2], argc - 3, argv + 3);
    }
    else if (strcmp(subcmd, "sync") == 0) {
        return till_host_sync(argc > 2 ? argv[2] : NULL);
    }
    else if (strcmp(subcmd, "status") == 0) {
        return till_host_status(argc > 2 ? argv[2] : NULL);
    }
    else if (strcmp(subcmd, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: till host remove <name> [--clean-remote]\n");
            return -1;
        }
        int clean_remote = 0;
        if (argc > 3 && strcmp(argv[3], "--clean-remote") == 0) {
            clean_remote = 1;
        }
        return till_host_remove(argv[2], clean_remote);
    }
    else if (strcmp(subcmd, "sync") == 0) {
        /* Manual sync command */
        return till_host_sync_all();
    }
    else if (strcmp(subcmd, "merge") == 0) {
        /* Receive and merge hosts from stdin (for remote calls) */
        char buffer[65536];
        size_t total = 0;
        size_t n;
        while ((n = fread(buffer + total, 1, sizeof(buffer) - total - 1, stdin)) > 0) {
            total += n;
        }
        buffer[total] = '\0';
        return merge_hosts_from_remote(buffer);
    }
    else {
        fprintf(stderr, "Unknown host subcommand: %s\n\n", subcmd);
        print_host_help();
        return -1;
    }
}

/* Propagate hosts update to all known hosts */
static int propagate_hosts_update(void) {
    cJSON *json = load_hosts();
    if (!json) return -1;
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        cJSON_Delete(json);
        return -1;
    }
    
    const char *local_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(json, "local_hostname"));
    if (!local_hostname) {
        /* Get it now if missing */
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            local_hostname = hostname;
        }
    }
    
    int success_count = 0;
    int total_count = 0;
    
    /* Iterate through all hosts */
    cJSON *host = NULL;
    cJSON_ArrayForEach(host, hosts) {
        const char *name = host->string;
        const char *actual_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "actual_hostname"));
        
        /* Skip self (check both name and actual_hostname) */
        if (local_hostname && actual_hostname && strcmp(actual_hostname, local_hostname) == 0) {
            continue;
        }
        
        /* Skip localhost entries - never propagate to localhost */
        const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
        if (hostname && (strcmp(hostname, "localhost") == 0 || 
                        strcmp(hostname, "127.0.0.1") == 0 ||
                        strcmp(hostname, "::1") == 0)) {
            continue;
        }
        
        
        total_count++;
        if (send_hosts_to_remote(name) == 0) {
            success_count++;
        }
    }
    
    cJSON_Delete(json);
    return (success_count == total_count) ? 0 : -1;
}

/* Send hosts file to a remote host */
static int send_hosts_to_remote(const char *name) {
    /* Get host info */
    char user[256], host[256];
    int port;
    if (get_host_info(name, user, sizeof(user), host, sizeof(host), &port) != 0) {
        return -1;
    }
    
    /* Load current hosts */
    cJSON *json = load_hosts();
    if (!json) return -1;
    
    /* Convert to string */
    char *json_str = cJSON_Print(json);
    cJSON_Delete(json);
    if (!json_str) return -1;
    
    /* Send to remote via SSH */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ssh -o ConnectTimeout=5 %s@%s -p %d '" TILL_REMOTE_INSTALL_PATH "/till host merge' 2>/dev/null",
        user, host, port);
    
    FILE *fp = popen(cmd, "w");
    if (!fp) {
        free(json_str);
        return -1;
    }
    
    fwrite(json_str, 1, strlen(json_str), fp);
    int result = pclose(fp);
    free(json_str);
    
    return (result == 0) ? 0 : -1;
}

/* Merge hosts from remote (called via SSH) */
static int merge_hosts_from_remote(const char *json_str) {
    if (!json_str || strlen(json_str) == 0) {
        return -1;
    }
    
    /* Parse incoming JSON */
    cJSON *incoming = cJSON_Parse(json_str);
    if (!incoming) {
        return -1;
    }
    
    /* Load local hosts */
    cJSON *local = load_hosts();
    if (!local) {
        local = cJSON_CreateObject();
        cJSON_AddObjectToObject(local, "hosts");
    }
    
    /* Ensure local_hostname exists */
    const char *local_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(local, "local_hostname"));
    if (!local_hostname) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            cJSON_AddStringToObject(local, "local_hostname", hostname);
            local_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(local, "local_hostname"));
        }
    }
    
    /* Get hosts objects */
    cJSON *local_hosts = cJSON_GetObjectItem(local, "hosts");
    cJSON *incoming_hosts = cJSON_GetObjectItem(incoming, "hosts");
    
    if (!local_hosts) {
        local_hosts = cJSON_AddObjectToObject(local, "hosts");
    }
    
    if (incoming_hosts) {
        /* Merge each host */
        cJSON *host = NULL;
        cJSON_ArrayForEach(host, incoming_hosts) {
            const char *name = host->string;
            const char *actual_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "actual_hostname"));
            
            /* Skip self */
            if (local_hostname && actual_hostname && strcmp(actual_hostname, local_hostname) == 0) {
                continue;
            }
            
            /* Check if we have this host */
            cJSON *existing = cJSON_GetObjectItem(local_hosts, name);
            if (existing) {
                /* Compare versions */
                int local_version = cJSON_GetNumberValue(cJSON_GetObjectItem(existing, "version"));
                int incoming_version = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "version"));
                
                if (incoming_version > local_version) {
                    /* Replace with newer version */
                    cJSON_DeleteItemFromObject(local_hosts, name);
                    cJSON_AddItemToObject(local_hosts, name, cJSON_Duplicate(host, 1));
                }
            } else {
                /* Add new host */
                cJSON_AddItemToObject(local_hosts, name, cJSON_Duplicate(host, 1));
            }
        }
    }
    
    /* Save merged hosts */
    int result = save_hosts(local);
    cJSON_Delete(local);
    cJSON_Delete(incoming);
    
    return result;
}

/* Manual sync with all hosts */
int till_host_sync_all(void) {
    printf("Synchronizing with all hosts...\n");
    
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts || cJSON_GetArraySize(hosts) == 0) {
        printf("No remote hosts to sync with\n");
        cJSON_Delete(json);
        return 0;
    }
    
    const char *local_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(json, "local_hostname"));
    
    int success = 0;
    int failed = 0;
    
    /* Exchange with each host */
    cJSON *host = NULL;
    cJSON_ArrayForEach(host, hosts) {
        const char *name = host->string;
        const char *actual_hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "actual_hostname"));
        
        /* Skip self */
        if (local_hostname && actual_hostname && strcmp(actual_hostname, local_hostname) == 0) {
            continue;
        }
        
        
        printf("  Syncing with %s... ", name);
        fflush(stdout);
        
        /* Get their hosts */
        char user[256], hostname[256];
        int port;
        if (get_host_info(name, user, sizeof(user), hostname, sizeof(hostname), &port) == 0) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                "ssh -o ConnectTimeout=5 %s@%s -p %d 'cat ~/.till/hosts-local.json' 2>/dev/null",
                user, hostname, port);
            
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char buffer[65536];
                size_t total = 0;
                size_t n;
                while ((n = fread(buffer + total, 1, sizeof(buffer) - total - 1, fp)) > 0) {
                    total += n;
                }
                buffer[total] = '\0';
                pclose(fp);
                
                if (total > 0 && merge_hosts_from_remote(buffer) == 0) {
                    /* Send our updated list back */
                    if (send_hosts_to_remote(name) == 0) {
                        printf("✓\n");
                        success++;
                    } else {
                        printf("⚠ (partial)\n");
                        failed++;
                    }
                } else {
                    printf("✗\n");
                    failed++;
                }
            } else {
                printf("✗\n");
                failed++;
            }
        } else {
            printf("✗\n");
            failed++;
        }
    }
    
    cJSON_Delete(json);
    
    printf("\nSync complete: %d successful, %d failed\n", success, failed);
    return (failed > 0) ? -1 : 0;
}