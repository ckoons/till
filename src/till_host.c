/*
 * till_host.c - Host management for Till (Tekton Lifecycle Manager)
 * 
 * Manages remote hosts using SSH for Till operations.
 * "The Till Way" - Simple, works, hard to screw up.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

#include "till_config.h"
#include "till_host.h"
#include "till_common.h"
#include "till_security.h"
#include "till_platform.h"
#include "cJSON.h"

#ifndef TILL_MAX_PATH
#define TILL_MAX_PATH 4096
#endif

/* Parse user@host[:port] format */
static int parse_host_spec(const char *spec, char *user, size_t user_size, 
                          char *host, size_t host_size, int *port) {
    *port = 22;  /* Default SSH port */
    
    char *at = strchr(spec, '@');
    if (!at) return -1;
    
    size_t user_len = at - spec;
    if (user_len >= user_size) return -1;
    
    strncpy(user, spec, user_len);
    user[user_len] = '\0';
    
    char *colon = strchr(at + 1, ':');
    if (colon) {
        size_t host_len = colon - (at + 1);
        if (host_len >= host_size) return -1;
        strncpy(host, at + 1, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
    } else {
        strncpy(host, at + 1, host_size - 1);
        host[host_size - 1] = '\0';
    }
    
    return 0;
}

/* Run SSH command with timeout */
static int run_ssh_command(const char *user, const char *host, int port, 
                           const char *cmd, char *output, size_t output_size) {
    char ssh_cmd[4096];
    snprintf(ssh_cmd, sizeof(ssh_cmd),
        "ssh -o ConnectTimeout=5 -o BatchMode=yes %s@%s -p %d '%s' 2>/dev/null",
        user, host, port, cmd);
    
    return run_command(ssh_cmd, output, output_size);
}

/* Add a new host */
int till_host_add(const char *name, const char *user_at_host) {
    if (!name || !user_at_host) {
        till_log(LOG_ERROR, "Usage: till host add <name> <user>@<host>[:port]");
        return -1;
    }
    
    till_log(LOG_INFO, "Adding host '%s'", name);
    printf("Adding host '%s'...\n", name);
    
    /* Parse user@host:port */
    char user[256], host[256];
    int port;
    if (parse_host_spec(user_at_host, user, sizeof(user), 
                       host, sizeof(host), &port) != 0) {
        till_log(LOG_ERROR, "Invalid host specification: %s", user_at_host);
        till_error("Invalid format. Use: user@host[:port]\n");
        return -1;
    }
    
    /* Load or create hosts file */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        json = cJSON_CreateObject();
        cJSON_AddObjectToObject(json, "hosts");
        json_set_string(json, "updated", "0");
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        hosts = cJSON_AddObjectToObject(json, "hosts");
    }
    
    /* Check if host already exists */
    cJSON *existing_host = cJSON_GetObjectItem(hosts, name);
    if (existing_host) {
        /* Check if it's a removed host that can be re-added */
        const char *state = cJSON_GetStringValue(cJSON_GetObjectItem(existing_host, "state"));
        if (state && strcmp(state, "removed") == 0) {
            printf("Re-activating previously removed host '%s'\n", name);
            till_log(LOG_INFO, "Re-activating previously removed host '%s'", name);
            /* Remove the old entry to replace with fresh one */
            cJSON_DeleteItemFromObject(hosts, name);
        } else {
            till_log(LOG_ERROR, "Host '%s' already exists and is active", name);
            till_error("Host '%s' already exists and is active\n", name);
            cJSON_Delete(json);
            return -1;
        }
    }
    
    /* Add host entry */
    cJSON *host_obj = cJSON_CreateObject();
    json_set_string(host_obj, "user", user);
    json_set_string(host_obj, "host", host);
    json_set_int(host_obj, "port", port);
    json_set_string(host_obj, "status", "untested");
    
    /* Add timestamp */
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%a %b %d %H:%M:%S %Y", localtime(&now));
    json_set_string(host_obj, "added", timestamp);
    
    cJSON_AddItemToObject(hosts, name, host_obj);
    
    /* Update timestamp */
    char ts[32];
    snprintf(ts, sizeof(ts), "%ld", time(NULL));
    cJSON_SetValuestring(cJSON_GetObjectItem(json, "updated"), ts);
    
    /* Save hosts file */
    if (save_till_json("hosts-local.json", json) != 0) {
        till_log(LOG_ERROR, "Failed to save hosts file");
        till_error("Failed to save hosts\n");
        cJSON_Delete(json);
        return -1;
    }
    
    printf("✓ Host '%s' added (%s@%s:%d)\n", name, user, host, port);
    
    /* Update SSH config */
    if (add_ssh_config_entry(name, user, host, port) == 0) {
        printf("✓ SSH config updated\n");
    }
    
    cJSON_Delete(json);
    
    printf("\nUse 'till host test %s' to test connectivity\n", name);
    printf("Use 'till host setup %s' to install Till remotely\n", name);
    
    return 0;
}

/* Test SSH connectivity to a host */
int till_host_test(const char *name) {
    if (!name) {
        till_log(LOG_ERROR, "Usage: till host test <name>");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_log(LOG_ERROR, "No hosts configured");
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        till_log(LOG_ERROR, "Host '%s' not found", name);
        till_error("Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
    const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
    int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
    if (!port) port = 22;
    
    printf("Testing connection to '%s' (%s@%s:%d)...\n", name, user, hostname, port);
    
    /* First test basic connectivity with ping */
    printf("  Checking host reachability... ");
    fflush(stdout);
    if (platform_ping_host(hostname, 3000) == 0) {
        printf("✓\n");
    } else {
        printf("⚠ (ping failed, host may still be reachable)\n");
    }
    
    /* Test port connectivity */
    printf("  Checking SSH port %d... ", port);
    fflush(stdout);
    if (platform_test_port(hostname, port, 3000) == 0) {
        printf("✓\n");
    } else {
        printf("✗\n");
        printf("\nSSH port %d appears to be closed or filtered.\n", port);
        printf("Please verify:\n");
        printf("  - SSH service is running on the remote host\n");
        printf("  - Firewall allows connections on port %d\n", port);
        printf("  - The correct port is specified\n");
        cJSON_Delete(json);
        return -1;
    }
    
    /* Test SSH connection */
    printf("  Testing SSH authentication... ");
    fflush(stdout);
    if (run_ssh_command(user, hostname, port, "echo TILL_TEST_SUCCESS", NULL, 0) == 0) {
        printf("✓\n");
        printf("\n✓ SSH connection successful\n");
        till_log(LOG_INFO, "SSH test successful for host '%s'", name);
        
        /* Update status */
        cJSON_SetValuestring(cJSON_GetObjectItem(host, "status"), "ready");
        save_till_json("hosts-local.json", json);
        
        cJSON_Delete(json);
        return 0;
    } else {
        printf("✗ SSH connection failed\n");
        printf("\nPlease verify:\n");
        printf("  - The host is reachable\n");
        printf("  - SSH is enabled on the remote host\n");
        printf("  - Your SSH keys are configured\n");
        
        till_log(LOG_WARN, "SSH test failed for host '%s'", name);
        cJSON_Delete(json);
        return -1;
    }
}

/* Setup Till on remote host */
int till_host_setup(const char *name) {
    if (!name) {
        till_log(LOG_ERROR, "Usage: till host setup <name>");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_log(LOG_ERROR, "No hosts configured");
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        till_log(LOG_ERROR, "Host '%s' not found", name);
        till_error("Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
    const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
    int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
    if (!port) port = 22;
    
    printf("Setting up Till on '%s'...\n", name);
    till_log(LOG_INFO, "Setting up Till on host '%s'", name);
    
    /* Check if Till already exists */
    char output[1024];
    if (run_ssh_command(user, hostname, port, 
                       "test -d ~/" TILL_REMOTE_INSTALL_PATH " && echo EXISTS",
                       output, sizeof(output)) == 0 && strstr(output, "EXISTS")) {
        printf("✓ Till already installed on remote\n");
        
        /* Try to update it */
        printf("Updating Till on remote...\n");
        if (run_ssh_command(user, hostname, port,
                           "cd ~/" TILL_REMOTE_INSTALL_PATH " && git pull && make clean && make",
                           NULL, 0) == 0) {
            printf("✓ Till updated successfully\n");
            till_log(LOG_INFO, "Till updated on host '%s'", name);
        } else {
            printf("⚠ Warning: Till update failed (may have local changes)\n");
            till_log(LOG_WARN, "Till update failed on host '%s'", name);
        }
    } else {
        /* Install fresh */
        printf("Installing Till on remote host...\n");
        
        char install_cmd[2048];
        snprintf(install_cmd, sizeof(install_cmd),
            "mkdir -p ~/%s && cd ~/%s && "
            "git clone %s.git %s && "
            "cd %s && make",
            TILL_PROJECTS_BASE, TILL_PROJECTS_BASE, 
            TILL_REPO_URL, TILL_GITHUB_REPO,
            TILL_GITHUB_REPO);
        
        if (run_ssh_command(user, hostname, port, install_cmd, NULL, 0) != 0) {
            till_log(LOG_ERROR, "Failed to install Till on host '%s'", name);
            till_error("Failed to install Till on remote\n");
            till_error("Please ensure the remote host has:\n");
            till_error("  - git installed\n");
            till_error("  - C compiler (gcc/clang) installed\n");
            till_error("  - Internet connectivity to GitHub\n");
            cJSON_Delete(json);
            return -1;
        }
        
        printf("✓ Till installed successfully\n");
        till_log(LOG_INFO, "Till installed on host '%s'", name);
    }
    
    /* Verify installation - till runs from its build directory */
    if (run_ssh_command(user, hostname, port, 
                       "~/" TILL_REMOTE_INSTALL_PATH "/till --version",
                       output, sizeof(output)) == 0) {
        printf("✓ Till is working on remote host\n");
        printf("✓ Till location: ~/%s/till\n", TILL_REMOTE_INSTALL_PATH);
        
        /* Update status */
        cJSON_SetValuestring(cJSON_GetObjectItem(host, "status"), "ready");
        save_till_json("hosts-local.json", json);
        
        till_log(LOG_INFO, "Till setup complete on host '%s'", name);
    } else {
        printf("✗ Error: Till verification failed\n");
        till_log(LOG_ERROR, "Till verification failed on host '%s'", name);
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Execute command on remote host */
int till_host_exec(const char *name, const char *command) {
    if (!name || !command) {
        till_log(LOG_ERROR, "Usage: till host exec <name> <command>");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_log(LOG_ERROR, "No hosts configured");
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        till_log(LOG_ERROR, "Host '%s' not found", name);
        till_error("Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
    const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
    int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
    if (!port) port = 22;
    
    /* Validate we got the values */
    if (!user || !hostname) {
        till_log(LOG_ERROR, "Host '%s' missing user or hostname", name);
        till_error("Host '%s' is misconfigured\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    till_log(LOG_INFO, "Executing command on host '%s': %s", name, command);
    
    /* If command starts with "till ", use the full path */
    char actual_command[4096];
    if (strncmp(command, "till ", 5) == 0) {
        snprintf(actual_command, sizeof(actual_command),
                 "~/" TILL_REMOTE_INSTALL_PATH "/%s", command);
    } else {
        strncpy(actual_command, command, sizeof(actual_command) - 1);
        actual_command[sizeof(actual_command) - 1] = '\0';
    }
    
    /* Execute command - must use user/hostname before freeing json */
    char ssh_cmd[8192];
    snprintf(ssh_cmd, sizeof(ssh_cmd),
        "ssh -o ConnectTimeout=5 %s@%s -p %d '%s'",
        user, hostname, port, actual_command);
    
    cJSON_Delete(json);
    
    return run_command(ssh_cmd, NULL, 0);
}

/* SSH interactive session to remote host */
int till_host_ssh(const char *name, int argc, char *argv[]) {
    if (!name) {
        till_log(LOG_ERROR, "Usage: till host ssh <name> [ssh args]");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_log(LOG_ERROR, "No hosts configured");
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        till_log(LOG_ERROR, "Host '%s' not found", name);
        till_error("Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
    const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
    int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
    if (!port) port = 22;
    
    cJSON_Delete(json);
    
    till_log(LOG_INFO, "SSH session to host '%s'", name);
    
    /* Build SSH command with properly escaped arguments */
    char ssh_cmd[4096];
    if (build_ssh_command_safe(ssh_cmd, sizeof(ssh_cmd), user, hostname, port, argc, argv) != 0) {
        till_error("Failed to build SSH command - invalid arguments");
        return -1;
    }
    
    return system(ssh_cmd);
}

/* Remove a host */
int till_host_remove(const char *name, int clean_remote) {
    if (!name) {
        till_log(LOG_ERROR, "Usage: till host remove <name> [--clean-remote]");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_log(LOG_ERROR, "No hosts configured");
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    if (!host) {
        till_log(LOG_ERROR, "Host '%s' not found", name);
        till_error("Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    /* If requested, try to clean up remote (with timeout) */
    if (clean_remote) {
        const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
        const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
        int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
        if (!port) port = 22;
        
        printf("Attempting to clean remote Till installation (5 second timeout)...\n");
        if (run_ssh_command(user, hostname, port,
                           "rm -rf ~/" TILL_REMOTE_INSTALL_PATH,
                           NULL, 0) == 0) {
            printf("✓ Remote Till installation cleaned up\n");
            till_log(LOG_INFO, "Cleaned remote Till on host '%s'", name);
        } else {
            printf("⚠ Could not clean remote (host may be unreachable)\n");
            till_log(LOG_WARN, "Could not clean remote Till on host '%s'", name);
        }
    }
    
    /* Remove host from JSON */
    printf("Removing host '%s'...\n", name);
    cJSON_DeleteItemFromObject(hosts, name);
    
    if (save_till_json("hosts-local.json", json) != 0) {
        till_log(LOG_ERROR, "Failed to update hosts file");
        till_error("Failed to update hosts file\n");
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON_Delete(json);
    
    /* Remove from SSH config */
    if (remove_ssh_config_entry(name) == 0) {
        printf("✓ SSH configuration cleaned up\n");
    }
    
    /* Clean up SSH control sockets */
    run_command("rm -f ~/.ssh/ctl-* 2>/dev/null", NULL, 0);
    
    printf("✓ Host '%s' removed successfully\n", name);
    till_log(LOG_INFO, "Removed host '%s'", name);
    return 0;
}

/* Show host status */
int till_host_status(const char *name) {
    cJSON *json = load_till_json("hosts-local.json");
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
            till_log(LOG_ERROR, "Host '%s' not found", name);
            till_error("Host '%s' not found\n", name);
            cJSON_Delete(json);
            return -1;
        }
        
        printf("Host: %s\n", name);
        printf("  User: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "user")));
        printf("  Host: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "host")));
        printf("  Port: %d\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port")));
        printf("  Status: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "status")));
        printf("  Added: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(host, "added")));
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
        printf("Example: ssh <name>\n");
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Print host command help */
static void print_host_help(void) {
    printf("Till Host Management Commands\n\n");
    printf("Usage: till host <command> [args]\n\n");
    printf("Commands:\n");
    printf("  add <name> <user>@<host>[:port]  Add a new host\n");
    printf("  test <name>                      Test SSH connectivity\n");
    printf("  setup <name>                     Install Till on remote host\n");
    printf("  update [name]                    Update Till on host(s)\n");
    printf("  sync [name]                      Sync Tekton installations on host(s)\n");
    printf("  exec <name> <command>            Execute command on remote\n");
    printf("  ssh <name> [args]                SSH to remote host\n");
    printf("  remove <name> [--clean-remote]   Remove a host\n");
    printf("  status [name]                    Show host(s) status\n");
    printf("  list                             List all hosts\n");
    printf("\nCommands with optional [name]:\n");
    printf("  - If name provided: operates on specific host\n");
    printf("  - If name omitted: operates on all configured hosts\n");
    printf("\nExamples:\n");
    printf("  till host add m2 user@192.168.1.100\n");
    printf("  till host setup m2\n");
    printf("  till host update              # Update Till on all hosts\n");
    printf("  till host update m2           # Update Till on specific host\n");
    printf("  till host sync                # Sync all hosts\n");
    printf("  till host sync m2             # Sync specific host\n");
    printf("  till host exec m2 'till status'\n");
}

/* Update host configurations across all machines (internal function) */
static int till_host_update_configs(void) {
    printf("Updating host configurations...\n");
    till_log(LOG_INFO, "Updating host configurations");
    
    /* Load local hosts file */
    cJSON *local_json = load_till_json("hosts-local.json");
    if (!local_json) {
        till_error("No hosts configured\n");
        return -1;
    }
    
    cJSON *local_hosts = cJSON_GetObjectItem(local_json, "hosts");
    if (!local_hosts) {
        till_error("Invalid hosts file\n");
        cJSON_Delete(local_json);
        return -1;
    }
    
    /* Create merged hosts object */
    cJSON *merged_hosts = cJSON_CreateObject();
    
    /* First, copy all local hosts to merged */
    cJSON *host;
    cJSON_ArrayForEach(host, local_hosts) {
        const char *host_name = host->string;
        cJSON *host_copy = cJSON_Duplicate(host, 1);
        
        /* Get hostname for local machine */
        if (strcmp(host_name, "local") == 0) {
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                json_set_string(host_copy, "hostname", hostname);
            }
            json_set_string(host_copy, "till_configured", "yes");
        }
        
        cJSON_AddItemToObject(merged_hosts, host_name, host_copy);
    }
    
    /* Process each remote host */
    int hosts_processed = 0;
    int hosts_failed = 0;
    
    cJSON_ArrayForEach(host, local_hosts) {
        const char *host_name = host->string;
        
        /* Skip local host */
        if (strcmp(host_name, "local") == 0) {
            continue;
        }
        
        printf("\nProcessing host '%s'...\n", host_name);
        
        const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
        const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
        int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
        if (!port) port = 22;
        
        /* Step 1: Run till update on remote */
        printf("  Running till update...\n");
        char output[1024];
        if (run_ssh_command(user, hostname, port, 
                           "cd ~/projects/github/till && git pull && make",
                           output, sizeof(output)) == 0) {
            printf("    ✓ Till updated\n");
        } else {
            printf("    ⚠ Update failed (till might not be installed)\n");
        }
        
        /* Step 2: Get hostname from remote */
        printf("  Getting hostname...\n");
        if (run_ssh_command(user, hostname, port, "hostname", 
                           output, sizeof(output)) == 0) {
            /* Remove newline */
            char *nl = strchr(output, '\n');
            if (nl) *nl = '\0';
            
            printf("    ✓ Hostname: %s\n", output);
            
            /* Update host entry with hostname */
            cJSON *merged_host = cJSON_GetObjectItem(merged_hosts, host_name);
            if (merged_host) {
                json_set_string(merged_host, "hostname", output);
            }
        } else {
            printf("    ✗ Failed to get hostname\n");
        }
        
        /* Step 3: Find till installation and check if configured */
        printf("  Checking till configuration...\n");
        char find_till_cmd[512];
        snprintf(find_till_cmd, sizeof(find_till_cmd),
                "if [ -f /usr/local/till/till ]; then echo 'yes:/usr/local/till'; "
                "elif [ -f /opt/till/till ]; then echo 'yes:/opt/till'; "
                "elif [ -f ~/projects/github/till/till ]; then echo \"yes:$(cd ~/projects/github/till && pwd)\"; "
                "elif which till >/dev/null 2>&1; then echo \"yes:$(which till | xargs dirname)\"; "
                "else echo 'no:'; fi");
        
        if (run_ssh_command(user, hostname, port, find_till_cmd, output, sizeof(output)) == 0) {
            /* Remove newline */
            char *nl = strchr(output, '\n');
            if (nl) *nl = '\0';
            
            /* Parse response - format is "configured:path" */
            char *colon = strchr(output, ':');
            if (colon) {
                *colon = '\0';
                char *till_configured = output;
                char *till_path = colon + 1;
                
                printf("    ✓ Till configured: %s\n", till_configured);
                if (strlen(till_path) > 0) {
                    printf("    Till path: %s\n", till_path);
                }
                
                /* Update host entry */
                cJSON *merged_host = cJSON_GetObjectItem(merged_hosts, host_name);
                if (merged_host) {
                    json_set_string(merged_host, "till_configured", till_configured);
                    if (strlen(till_path) > 0) {
                        json_set_string(merged_host, "till_path", till_path);
                    }
                }
            }
        }
        
        /* Step 4: Pull remote hosts file */
        printf("  Fetching remote hosts file...\n");
        
        /* First find where till is installed on remote */
        char find_till_cmd2[512];
        char remote_till_dir[1024] = "";
        snprintf(find_till_cmd2, sizeof(find_till_cmd2),
                "if [ -d /usr/local/till/.till ]; then echo /usr/local/till/.till; "
                "elif [ -d /opt/till/.till ]; then echo /opt/till/.till; "
                "elif [ -d ~/projects/github/till/.till ]; then echo ~/projects/github/till/.till; "
                "elif [ -d ./.till ]; then pwd | sed 's|$|/.till|'; "
                "else which till 2>/dev/null | xargs dirname 2>/dev/null | sed 's|$|/.till|'; fi");
        
        run_ssh_command(user, hostname, port, find_till_cmd2, remote_till_dir, sizeof(remote_till_dir));
        char *newline = strchr(remote_till_dir, '\n');
        if (newline) *newline = '\0';
        
        char remote_hosts[8192] = "";
        if (strlen(remote_till_dir) > 0) {
            char cat_cmd[1024];
            snprintf(cat_cmd, sizeof(cat_cmd), "cat %s/hosts-local.json 2>/dev/null", remote_till_dir);
            if (run_ssh_command(user, hostname, port, cat_cmd,
                               remote_hosts, sizeof(remote_hosts)) == 0 && strlen(remote_hosts) > 0) {
            
            cJSON *remote_json = cJSON_Parse(remote_hosts);
            if (remote_json) {
                cJSON *remote_host_list = cJSON_GetObjectItem(remote_json, "hosts");
                if (remote_host_list) {
                    /* Merge remote hosts */
                    cJSON *remote_host;
                    cJSON_ArrayForEach(remote_host, remote_host_list) {
                        const char *remote_host_name = remote_host->string;
                        
                        /* Don't overwrite existing entries, but add new ones */
                        if (!cJSON_GetObjectItem(merged_hosts, remote_host_name)) {
                            cJSON *host_copy = cJSON_Duplicate(remote_host, 1);
                            cJSON_AddItemToObject(merged_hosts, remote_host_name, host_copy);
                            printf("    + Added host '%s' from remote\n", remote_host_name);
                        }
                    }
                }
                cJSON_Delete(remote_json);
                printf("    ✓ Merged remote hosts\n");
            } else {
                printf("    ⚠ Invalid remote hosts file\n");
            }
        } else {
            printf("    ⚠ No remote hosts file found in %s\n", remote_till_dir);
        }
        } else {
            printf("    ⚠ Could not determine till directory on remote\n");
        }
        
        hosts_processed++;
    }
    
    /* Create new hosts file without local_hostname */
    cJSON *new_json = cJSON_CreateObject();
    cJSON_AddItemToObject(new_json, "hosts", merged_hosts);
    
    /* Update timestamp */
    char ts[32];
    snprintf(ts, sizeof(ts), "%ld", time(NULL));
    json_set_string(new_json, "updated", ts);
    
    /* Save merged hosts locally */
    printf("\nSaving merged hosts file...\n");
    if (save_till_json("hosts-local.json", new_json) == 0) {
        printf("✓ Local hosts file updated\n");
    } else {
        till_error("Failed to save merged hosts file\n");
        cJSON_Delete(local_json);
        cJSON_Delete(new_json);
        return -1;
    }
    
    /* Push merged hosts to all remotes */
    printf("\nPushing hosts file to all remotes...\n");
    char *hosts_json_str = cJSON_PrintUnformatted(new_json);
    
    cJSON_ArrayForEach(host, local_hosts) {
        const char *host_name = host->string;
        
        /* Skip local host */
        if (strcmp(host_name, "local") == 0) {
            continue;
        }
        
        const char *user = cJSON_GetStringValue(cJSON_GetObjectItem(host, "user"));
        const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(host, "host"));
        int port = cJSON_GetNumberValue(cJSON_GetObjectItem(host, "port"));
        if (!port) port = 22;
        
        /* Check if till is configured */
        cJSON *merged_host = cJSON_GetObjectItem(merged_hosts, host_name);
        const char *till_configured = json_get_string(merged_host, "till_configured", "no");
        
        if (strcmp(till_configured, "yes") == 0) {
            printf("  Updating '%s'...\n", host_name);
            
            /* Check if we have stored till_path */
            const char *stored_till_path = json_get_string(merged_host, "till_path", NULL);
            char till_dir[1024] = "";
            
            if (stored_till_path && strlen(stored_till_path) > 0) {
                /* Use stored path */
                snprintf(till_dir, sizeof(till_dir), "%s/.till", stored_till_path);
                printf("    Using stored till path: %s\n", till_dir);
            } else {
                /* Find where till is installed on remote */
                char find_till_cmd[512];
                snprintf(find_till_cmd, sizeof(find_till_cmd),
                        "if [ -d /usr/local/till/.till ]; then echo /usr/local/till/.till; "
                        "elif [ -d /opt/till/.till ]; then echo /opt/till/.till; "
                        "elif [ -d ~/projects/github/till/.till ]; then echo ~/projects/github/till/.till; "
                        "elif [ -d ./.till ]; then pwd | sed 's|$|/.till|'; "
                        "else which till 2>/dev/null | xargs dirname 2>/dev/null | sed 's|$|/.till|'; fi");
                
                if (run_ssh_command(user, hostname, port, find_till_cmd, till_dir, sizeof(till_dir)) == 0) {
                    /* Remove newline */
                    char *newline = strchr(till_dir, '\n');
                    if (newline) *newline = '\0';
                    printf("    Found till directory: %s\n", till_dir);
                }
            }
            
            if (strlen(till_dir) > 0) {
                /* Create .till directory if needed */
                char mkdir_cmd[1024];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", till_dir);
                run_ssh_command(user, hostname, port, mkdir_cmd, NULL, 0);
                
                /* Use cat with heredoc to write the file safely */
                char cmd[20480];
                snprintf(cmd, sizeof(cmd),
                        "cat > %s/hosts-local.json << 'EOF'\n%s\nEOF",
                        till_dir, hosts_json_str);
                
                if (run_ssh_command(user, hostname, port, cmd, NULL, 0) == 0) {
                    printf("    ✓ Updated\n");
                } else {
                    printf("    ✗ Failed to write hosts file\n");
                    hosts_failed++;
                }
            } else {
                printf("    ✗ Could not find till directory on remote\n");
                hosts_failed++;
            }
        } else {
            printf("  Skipping '%s' (till not configured)\n", host_name);
        }
    }
    
    free(hosts_json_str);
    cJSON_Delete(local_json);
    cJSON_Delete(new_json);
    
    /* Summary */
    printf("\n");
    printf("=============================\n");
    printf("Host Sync Complete\n");
    printf("=============================\n");
    printf("Hosts processed: %d\n", hosts_processed);
    if (hosts_failed > 0) {
        printf("Failed updates: %d\n", hosts_failed);
    }
    
    till_log(LOG_INFO, "Host sync complete: %d processed, %d failed", 
             hosts_processed, hosts_failed);
    
    return hosts_failed > 0 ? 1 : 0;
}

/* Run a Till command on a specific remote host */
static int run_till_on_host(const char *host_name, const char *till_cmd) {
    /* Load hosts file */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_error("No hosts configured");
        return -1;
    }

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        cJSON_Delete(json);
        till_error("Invalid hosts file");
        return -1;
    }

    /* Find the specific host */
    cJSON *host = cJSON_GetObjectItem(hosts, host_name);
    if (!host) {
        cJSON_Delete(json);
        till_error("Host '%s' not found", host_name);
        return -1;
    }

    const char *user = json_get_string(host, "user", NULL);
    const char *hostname = json_get_string(host, "host", NULL);
    int port = json_get_int(host, "port", 22);

    if (!user || !hostname) {
        cJSON_Delete(json);
        till_error("Invalid host configuration for '%s'", host_name);
        return -1;
    }

    printf("Running 'till %s' on %s...\n", till_cmd, host_name);

    /* Run the till command on remote host */
    char output[8192];
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "till %s", till_cmd);

    int result = run_ssh_command(user, hostname, port, cmd, output, sizeof(output));

    /* Display output with host prefix */
    if (strlen(output) > 0) {
        char *line = strtok(output, "\n");
        while (line) {
            printf("  [%s] %s\n", host_name, line);
            line = strtok(NULL, "\n");
        }
    }

    if (result == 0) {
        printf("  ✓ Command completed on %s\n", host_name);
    } else {
        printf("  ✗ Command failed on %s\n", host_name);
    }

    cJSON_Delete(json);
    return result;
}

/* Run a Till command on all remote hosts */
static int run_till_on_all_hosts(const char *till_cmd) {
    /* Load hosts file */
    cJSON *json = load_till_json("hosts-local.json");
    if (!json) {
        till_error("No hosts configured");
        return -1;
    }

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (!hosts) {
        cJSON_Delete(json);
        till_error("Invalid hosts file");
        return -1;
    }

    int total_hosts = 0;
    int successful = 0;
    int failed = 0;

    /* Iterate through all hosts */
    cJSON *host;
    cJSON_ArrayForEach(host, hosts) {
        const char *host_name = host->string;

        /* Skip local host */
        if (strcmp(host_name, "local") == 0) {
            continue;
        }

        const char *user = json_get_string(host, "user", NULL);
        const char *hostname = json_get_string(host, "host", NULL);
        int port = json_get_int(host, "port", 22);

        if (!user || !hostname) {
            printf("  ⚠ Skipping %s: invalid configuration\n", host_name);
            continue;
        }

        total_hosts++;
        printf("\n[%s] Running 'till %s'...\n", host_name, till_cmd);

        /* Run the till command on remote host */
        char output[8192];
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "till %s", till_cmd);

        int result = run_ssh_command(user, hostname, port, cmd, output, sizeof(output));

        /* Display output with host prefix */
        if (strlen(output) > 0) {
            char *line = strtok(output, "\n");
            while (line) {
                printf("  [%s] %s\n", host_name, line);
                line = strtok(NULL, "\n");
            }
        }

        if (result == 0) {
            printf("  ✓ Completed on %s\n", host_name);
            successful++;
        } else {
            printf("  ✗ Failed on %s\n", host_name);
            failed++;
        }
    }

    /* Summary */
    printf("\n");
    printf("=============================\n");
    printf("Command: till %s\n", till_cmd);
    printf("=============================\n");
    printf("Total hosts: %d\n", total_hosts);
    printf("Successful: %d\n", successful);
    if (failed > 0) {
        printf("Failed: %d\n", failed);
    }

    cJSON_Delete(json);
    return failed > 0 ? 1 : 0;
}

/* Update Till on remote host(s) */
int till_host_update(const char *host_name) {
    if (host_name) {
        /* Update specific host */
        printf("Updating Till on host '%s'...\n", host_name);
        till_log(LOG_INFO, "Updating Till on host: %s", host_name);
        return run_till_on_host(host_name, "update");
    } else {
        /* Update all hosts */
        printf("Updating Till on all hosts...\n");
        till_log(LOG_INFO, "Updating Till on all hosts");
        return run_till_on_all_hosts("update");
    }
}

/* Sync Tekton installations on remote host(s) */
int till_host_sync(const char *host_name) {
    if (host_name) {
        /* Sync specific host */
        printf("Syncing host '%s'...\n", host_name);
        till_log(LOG_INFO, "Syncing host: %s", host_name);
        return run_till_on_host(host_name, "sync");
    } else {
        /* Sync all hosts */
        printf("Syncing all hosts...\n");
        till_log(LOG_INFO, "Syncing all hosts");
        return run_till_on_all_hosts("sync");
    }
}

/* Main host command handler */
int till_host_command(int argc, char *argv[]) {
    if (argc < 1) {
        print_host_help();
        return 0;
    }

    const char *subcmd = argv[0];
    
    /* Check for help flag */
    if (strcmp(subcmd, "--help") == 0 || strcmp(subcmd, "-h") == 0) {
        print_host_help();
        return 0;
    }
    
    if (strcmp(subcmd, "add") == 0) {
        if (argc < 3) {
            till_error("Usage: till host add <name> <user>@<host>[:port]\n");
            return -1;
        }
        return till_host_add(argv[1], argv[2]);
    }
    else if (strcmp(subcmd, "test") == 0) {
        if (argc < 2) {
            till_error("Usage: till host test <name>\n");
            return -1;
        }
        return till_host_test(argv[1]);
    }
    else if (strcmp(subcmd, "setup") == 0) {
        if (argc < 2) {
            till_error("Usage: till host setup <name>\n");
            return -1;
        }
        return till_host_setup(argv[1]);
    }
    else if (strcmp(subcmd, "exec") == 0) {
        if (argc < 3) {
            till_error("Usage: till host exec <name> <command>\n");
            return -1;
        }
        /* Combine remaining arguments into command */
        char command[4096] = "";
        for (int i = 2; i < argc; i++) {
            if (i > 2) strcat(command, " ");
            strcat(command, argv[i]);
        }
        return till_host_exec(argv[1], command);
    }
    else if (strcmp(subcmd, "ssh") == 0) {
        if (argc < 2) {
            till_error("Usage: till host ssh <name> [args]\n");
            return -1;
        }
        return till_host_ssh(argv[1], argc - 2, argv + 2);
    }
    else if (strcmp(subcmd, "remove") == 0) {
        if (argc < 2) {
            till_error("Usage: till host remove <name> [--clean-remote]\n");
            return -1;
        }
        int clean_remote = (argc > 2 && strcmp(argv[2], "--clean-remote") == 0);
        return till_host_remove(argv[1], clean_remote);
    }
    else if (strcmp(subcmd, "status") == 0 || strcmp(subcmd, "list") == 0) {
        return till_host_status(argc > 1 ? argv[1] : NULL);
    }
    else if (strcmp(subcmd, "update") == 0) {
        const char *host_name = (argc > 1) ? argv[1] : NULL;
        return till_host_update(host_name);
    }
    else if (strcmp(subcmd, "sync") == 0) {
        const char *host_name = (argc > 1) ? argv[1] : NULL;
        return till_host_sync(host_name);
    }
    else {
        till_error("Unknown host subcommand: %s\n\n", subcmd);
        print_host_help();
        return -1;
    }
}