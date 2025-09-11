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
    if (cJSON_GetObjectItem(hosts, name)) {
        till_log(LOG_ERROR, "Host '%s' already exists", name);
        till_error("Host '%s' already exists\n", name);
        cJSON_Delete(json);
        return -1;
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
            "git clone %s " TILL_REMOTE_INSTALL_PATH " && "
            "cd " TILL_REMOTE_INSTALL_PATH " && make",
            TILL_PROJECTS_BASE, TILL_PROJECTS_BASE, TILL_REPO_URL);
        
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
    printf("  exec <name> <command>            Execute command on remote\n");
    printf("  ssh <name> [args]                SSH to remote host\n");
    printf("  remove <name> [--clean-remote]   Remove a host\n");
    printf("  status [name]                    Show host(s) status\n");
    printf("  list                             List all hosts\n");
    printf("\nExamples:\n");
    printf("  till host add m2 user@192.168.1.100\n");
    printf("  till host test m2\n");
    printf("  till host setup m2\n");
    printf("  till host exec m2 'till status'\n");
    printf("  till host ssh m2\n");
}

/* Main host command handler */
int till_host_command(int argc, char *argv[]) {
    if (argc < 2) {
        print_host_help();
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    /* Check for help flag */
    if (strcmp(subcmd, "--help") == 0 || strcmp(subcmd, "-h") == 0) {
        print_host_help();
        return 0;
    }
    
    if (strcmp(subcmd, "add") == 0) {
        if (argc < 4) {
            till_error("Usage: till host add <name> <user>@<host>[:port]\n");
            return -1;
        }
        return till_host_add(argv[2], argv[3]);
    }
    else if (strcmp(subcmd, "test") == 0) {
        if (argc < 3) {
            till_error("Usage: till host test <name>\n");
            return -1;
        }
        return till_host_test(argv[2]);
    }
    else if (strcmp(subcmd, "setup") == 0) {
        if (argc < 3) {
            till_error("Usage: till host setup <name>\n");
            return -1;
        }
        return till_host_setup(argv[2]);
    }
    else if (strcmp(subcmd, "exec") == 0) {
        if (argc < 4) {
            till_error("Usage: till host exec <name> <command>\n");
            return -1;
        }
        /* Combine remaining arguments into command */
        char command[4096] = "";
        for (int i = 3; i < argc; i++) {
            if (i > 3) strcat(command, " ");
            strcat(command, argv[i]);
        }
        return till_host_exec(argv[2], command);
    }
    else if (strcmp(subcmd, "ssh") == 0) {
        if (argc < 3) {
            till_error("Usage: till host ssh <name> [args]\n");
            return -1;
        }
        return till_host_ssh(argv[2], argc - 3, argv + 3);
    }
    else if (strcmp(subcmd, "remove") == 0) {
        if (argc < 3) {
            till_error("Usage: till host remove <name> [--clean-remote]\n");
            return -1;
        }
        int clean_remote = (argc > 3 && strcmp(argv[3], "--clean-remote") == 0);
        return till_host_remove(argv[2], clean_remote);
    }
    else if (strcmp(subcmd, "status") == 0 || strcmp(subcmd, "list") == 0) {
        return till_host_status(argc > 2 ? argv[2] : NULL);
    }
    else {
        till_error("Unknown host subcommand: %s\n\n", subcmd);
        print_host_help();
        return -1;
    }
}