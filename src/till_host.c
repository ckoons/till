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

/* SSH configuration template */
static const char *SSH_CONFIG_TEMPLATE = 
    "# Till SSH Configuration - Auto-generated\n"
    "# This file is managed by Till. Manual changes will be preserved.\n\n"
    "# Default settings for all Till hosts\n"
    "Host till-*\n"
    "    StrictHostKeyChecking accept-new\n"
    "    UserKnownHostsFile ~/.till/ssh/known_hosts\n"
    "    ControlMaster auto\n"
    "    ControlPath ~/.till/ssh/control/%%h-%%p-%%r\n"
    "    ControlPersist 10m\n"
    "    ServerAliveInterval 60\n"
    "    ServerAliveCountMax 3\n\n";

/* Initialize Till SSH environment */
int till_host_init(void) {
    char path[PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        fprintf(stderr, "Error: Cannot determine home directory\n");
        return -1;
    }
    
    /* Create ~/.till directory */
    snprintf(path, sizeof(path), "%s/.till", pw->pw_dir);
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    /* Create ~/.till/ssh directory */
    snprintf(path, sizeof(path), "%s/.till/ssh", pw->pw_dir);
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    /* Create ~/.till/ssh/control directory for SSH multiplexing */
    snprintf(path, sizeof(path), "%s/.till/ssh/control", pw->pw_dir);
    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    /* Create/check SSH config */
    snprintf(path, sizeof(path), "%s/.till/ssh/config", pw->pw_dir);
    struct stat st;
    if (stat(path, &st) != 0) {
        /* Create initial config */
        FILE *fp = fopen(path, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot create %s: %s\n", path, strerror(errno));
            return -1;
        }
        fprintf(fp, "%s", SSH_CONFIG_TEMPLATE);
        fclose(fp);
        chmod(path, 0600);
        printf("Created Till SSH configuration at %s\n", path);
    }
    
    /* Check for federation key */
    snprintf(path, sizeof(path), "%s/.till/ssh/till_federation_ed25519", pw->pw_dir);
    if (stat(path, &st) != 0) {
        printf("No federation key found. Run 'till host init' to generate one.\n");
    }
    
    /* Create hosts-local.json if it doesn't exist */
    snprintf(path, sizeof(path), "%s/.till/hosts-local.json", pw->pw_dir);
    if (stat(path, &st) != 0) {
        FILE *fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "{\n    \"hosts\": {},\n    \"updated\": \"%ld\"\n}\n", time(NULL));
            fclose(fp);
        }
    }
    
    return 0;
}

/* Generate federation SSH key */
static int generate_federation_key(void) {
    char path[PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    snprintf(path, sizeof(path), "%s/.till/ssh/till_federation_ed25519", pw->pw_dir);
    
    /* Check if key already exists */
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("Federation key already exists at %s\n", path);
        printf("To regenerate, delete the existing key first.\n");
        return 0;
    }
    
    /* Generate new key */
    char cmd[PATH_MAX + 256];
    snprintf(cmd, sizeof(cmd), 
        "ssh-keygen -t ed25519 -f %s -N '' -C 'till-federation@%s' 2>/dev/null",
        path, pw->pw_name);
    
    printf("Generating federation SSH key...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to generate SSH key\n");
        return -1;
    }
    
    /* Display public key */
    char pubkey_path[PATH_MAX];
    snprintf(pubkey_path, sizeof(pubkey_path), "%s.pub", path);
    
    FILE *fp = fopen(pubkey_path, "r");
    if (fp) {
        char line[1024];
        printf("\nFederation public key:\n");
        printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
        while (fgets(line, sizeof(line), fp)) {
            printf("%s", line);
        }
        printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
        printf("\nAdd this key to authorized_keys on remote hosts.\n");
        fclose(fp);
    }
    
    return 0;
}

/* Load hosts from JSON file */
static cJSON *load_hosts(void) {
    char path[PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return NULL;
    
    snprintf(path, sizeof(path), "%s/.till/hosts-local.json", pw->pw_dir);
    
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
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    snprintf(path, sizeof(path), "%s/.till/hosts-local.json", pw->pw_dir);
    
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

/* Add SSH config entry for a host */
static int add_ssh_config_entry(const char *name, const char *user, const char *host, int port) {
    char path[PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
    snprintf(path, sizeof(path), "%s/.till/ssh/config", pw->pw_dir);
    
    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(fp, "\n# Host: %s (added %s)\n", name, time_str);
    fprintf(fp, "Host till-%s\n", name);
    fprintf(fp, "    HostName %s\n", host);
    fprintf(fp, "    User %s\n", user);
    if (port != 22) {
        fprintf(fp, "    Port %d\n", port);
    }
    fprintf(fp, "    IdentityFile ~/.till/ssh/till_federation_ed25519\n");
    fprintf(fp, "    IdentitiesOnly yes\n\n");
    
    fclose(fp);
    
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
    cJSON_AddStringToObject(host_obj, "added", ctime(&(time_t){time(NULL)}));
    
    cJSON_AddItemToObject(hosts, name, host_obj);
    
    /* Add SSH config entry */
    if (add_ssh_config_entry(name, user, host, port) != 0) {
        fprintf(stderr, "Warning: Failed to add SSH config entry\n");
    }
    
    /* Save hosts */
    if (save_hosts(json) != 0) {
        fprintf(stderr, "Error: Failed to save hosts\n");
        cJSON_Delete(json);
        return -1;
    }
    
    printf("✓ Host '%s' added (%s@%s:%d)\n", name, user, host, port);
    printf("  SSH alias: till-%s\n", name);
    
    /* Test basic SSH connectivity */
    printf("\nTesting SSH connection...\n");
    char test_cmd[1024];
    struct passwd *pw = getpwuid(getuid());
    snprintf(test_cmd, sizeof(test_cmd),
        "ssh -o ConnectTimeout=5 -o PasswordAuthentication=no "
        "-i %s/.till/ssh/till_federation_ed25519 %s@%s -p %d 'echo OK' 2>/dev/null",
        pw->pw_dir, user, host, port);
    
    if (system(test_cmd) == 0) {
        printf("✓ SSH key already configured\n");
        printf("\nReady to use. Next steps:\n");
        printf("  1. Setup Till: till host setup %s\n", name);
        printf("  2. Deploy Tekton: till host deploy %s\n", name);
    } else {
        printf("✗ SSH key not configured\n");
        printf("\nTo enable passwordless access, copy your Till SSH key:\n");
        printf("  ssh-copy-id -i ~/.till/ssh/till_federation_ed25519.pub %s@%s", user, host);
        if (port != 22) {
            printf(" -p %d", port);
        }
        printf("\n\nThen test with: till host test %s\n", name);
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Test SSH connectivity to a host */
int till_host_test(const char *name) {
    if (!name) {
        fprintf(stderr, "Usage: till host test <name>\n");
        return -1;
    }
    
    /* Load hosts */
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "Error: Cannot load hosts\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    
    if (!host) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        cJSON_Delete(json);
        return -1;
    }
    
    /* Test SSH connection */
    char cmd[1024];
    struct passwd *pw = getpwuid(getuid());
    snprintf(cmd, sizeof(cmd), 
        "ssh -F %s/.till/ssh/config -o ConnectTimeout=5 till-%s 'echo TILL_TEST_SUCCESS'",
        pw->pw_dir, name);
    
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
        cJSON *status_item = cJSON_GetObjectItem(host, "status");
        if (status_item) {
            cJSON_SetValuestring(status_item, "connected");
        }
        
        /* Test Till installation */
        snprintf(cmd, sizeof(cmd),
            "ssh -F %s/.till/ssh/config till-%s 'which till 2>/dev/null'",
            pw->pw_dir, name);
        
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(line, sizeof(line), fp)) {
                printf("✓ Till found at: %s", line);
                if (status_item) {
                    cJSON_SetValuestring(status_item, "till-ready");
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
    
    /* First test connection */
    if (till_host_test(name) != 0) {
        fprintf(stderr, "Error: Cannot connect to host '%s'\n", name);
        return -1;
    }
    
    char cmd[4096];
    struct passwd *pw = getpwuid(getuid());
    
    /* Check if bootstrap script exists locally */
    
    /* Try to find bootstrap script */
    const char *paths[] = {
        "./scripts/bootstrap.sh",
        "../till/scripts/bootstrap.sh",
        "/usr/local/share/till/scripts/bootstrap.sh",
        NULL
    };
    
    const char *found_path = NULL;
    for (int i = 0; paths[i] != NULL; i++) {
        if (access(paths[i], R_OK) == 0) {
            found_path = paths[i];
            break;
        }
    }
    
    /* If we have local bootstrap script, use it */
    if (found_path) {
        printf("Installing Till on remote host using bootstrap script...\n");
        
        /* Send and execute bootstrap script */
        snprintf(cmd, sizeof(cmd),
            "ssh -F %s/.till/ssh/config till-%s 'bash -s' < %s",
            pw->pw_dir, name, found_path);
        
        if (system(cmd) != 0) {
            fprintf(stderr, "Error: Bootstrap script failed\n");
            fprintf(stderr, "Check remote host for missing dependencies\n");
            return -1;
        }
    } else {
        /* Fallback to downloading bootstrap script from GitHub */
        printf("Downloading and running Till bootstrap script...\n");
        
        snprintf(cmd, sizeof(cmd),
            "ssh -F %s/.till/ssh/config till-%s "
            "'curl -sSL https://raw.githubusercontent.com/ckoons/till/main/scripts/bootstrap.sh | bash'",
            pw->pw_dir, name);
        
        if (system(cmd) != 0) {
            fprintf(stderr, "Error: Failed to run bootstrap script\n");
            fprintf(stderr, "Please check internet connectivity and try again\n");
            return -1;
        }
    }
    
    /* Bootstrap script already handles installation to ~/.local/bin */
    printf("Till binary installed to ~/.local/bin/till\n");
    
    /* Verify installation - check multiple locations */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s "
        "'~/.local/bin/till --version 2>/dev/null || "
        "~/projects/github/till/till --version 2>/dev/null || "
        "~/.till/till/till --version 2>/dev/null'",
        pw->pw_dir, name);
    
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
                    cJSON_SetValuestring(status_item, "till-ready");
                }
            }
            save_hosts(json);
            cJSON_Delete(json);
        }
        
        return 0;
    } else {
        fprintf(stderr, "Warning: Till installed but not in PATH\n");
        fprintf(stderr, "Remote users should run: ~/projects/github/till/till\n");
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
    
    /* Check if host exists */
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "Error: No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    
    if (!host) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        fprintf(stderr, "Run 'till host status' to see configured hosts\n");
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON_Delete(json);
    
    /* Build SSH command */
    char cmd[4096];
    struct passwd *pw = getpwuid(getuid());
    
    /* Try to use till from PATH first, then fallback to known locations */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s "
        "'export PATH=\"$HOME/.local/bin:$PATH\"; %s'",
        pw->pw_dir, name, command);
    
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
    
    /* Check if host exists */
    cJSON *json = load_hosts();
    if (!json) {
        fprintf(stderr, "Error: No hosts configured\n");
        return -1;
    }
    
    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    cJSON *host = cJSON_GetObjectItem(hosts, name);
    
    if (!host) {
        fprintf(stderr, "Error: Host '%s' not found\n", name);
        fprintf(stderr, "Run 'till host status' to see configured hosts\n");
        cJSON_Delete(json);
        return -1;
    }
    
    cJSON_Delete(json);
    
    /* Build SSH command arguments */
    char till_dir[PATH_MAX];
    if (get_till_dir(till_dir, sizeof(till_dir)) != 0) {
        /* Fallback to old method if new one fails */
        struct passwd *pw = getpwuid(getuid());
        snprintf(till_dir, sizeof(till_dir), "%s/.till", pw->pw_dir);
    }
    
    /* Prepare SSH arguments */
    char config_path[PATH_MAX];
    char host_alias[256];
    
    snprintf(config_path, sizeof(config_path), "%s/ssh/config", till_dir);
    snprintf(host_alias, sizeof(host_alias), "till-%s", name);
    
    /* Build argument array for execvp */
    char **ssh_args = malloc((argc + 6) * sizeof(char *));
    if (!ssh_args) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }
    
    int i = 0;
    ssh_args[i++] = "ssh";
    ssh_args[i++] = "-F";
    ssh_args[i++] = config_path;
    ssh_args[i++] = "-t";  /* Force pseudo-terminal allocation for interactive use */
    ssh_args[i++] = host_alias;
    
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
            if (status && strcmp(status->valuestring, "till-ready") == 0) {
                printf("\nSyncing '%s'...\n", host_name);
                till_host_sync(host_name);
            }
        }
        
        cJSON_Delete(json);
        return 0;
    }
    
    /* Sync specific host */
    char cmd[1024];
    struct passwd *pw = getpwuid(getuid());
    
    printf("Syncing from '%s'...\n", name);
    
    /* Pull updates on remote */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s 'till sync'",
        pw->pw_dir, name);
    
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
    
    struct passwd *pw = getpwuid(getuid());
    if (!pw) return -1;
    
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
        printf("Cleaning up remote Till installation on '%s'...\n", name);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "ssh -F %s/.till/ssh/config till-%s 'rm -rf ~/projects/github/till ~/.till'",
            pw->pw_dir, name);
        
        if (system(cmd) != 0) {
            fprintf(stderr, "Warning: Failed to clean remote installation\n");
            /* Continue with local cleanup */
        }
    }
    
    /* 1. Remove from hosts JSON */
    printf("Removing host '%s' from configuration...\n", name);
    cJSON_DeleteItemFromObject(hosts, name);
    
    /* Save updated hosts file */
    if (save_hosts(json) != 0) {
        fprintf(stderr, "Error: Failed to update hosts file\n");
        if (saved_hostname) free(saved_hostname);
        cJSON_Delete(json);
        return -1;
    }
    cJSON_Delete(json);
    
    /* 2. Remove SSH config entry */
    printf("Removing SSH configuration for '%s'...\n", name);
    char ssh_config[PATH_MAX];
    char ssh_config_tmp[PATH_MAX];
    snprintf(ssh_config, sizeof(ssh_config), "%s/.till/ssh/config", pw->pw_dir);
    snprintf(ssh_config_tmp, sizeof(ssh_config_tmp), "%s.tmp", ssh_config);
    
    FILE *fp_in = fopen(ssh_config, "r");
    FILE *fp_out = fopen(ssh_config_tmp, "w");
    
    if (fp_in && fp_out) {
        char line[1024];
        char prev_line[1024] = "";
        int skip_block = 0;
        int skip_next_blank = 0;
        char host_pattern[256];
        snprintf(host_pattern, sizeof(host_pattern), "Host till-%s", name);
        
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
        "%s/.till/ssh/control/till-%s-*", pw->pw_dir, name);
    
    char cmd[PATH_MAX + 10];
    snprintf(cmd, sizeof(cmd), "rm -f %s 2>/dev/null", control_path);
    system(cmd);
    
    /* 4. Remove from known_hosts */
    printf("Cleaning up known_hosts entry...\n");
    char known_hosts[PATH_MAX];
    char known_hosts_tmp[PATH_MAX];
    snprintf(known_hosts, sizeof(known_hosts), "%s/.till/ssh/known_hosts", pw->pw_dir);
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
        printf("  SSH alias: till-%s\n", name);
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
        
        printf("\nSSH aliases: till-<name>\n");
        printf("Example: ssh till-remote1\n");
    }
    
    cJSON_Delete(json);
    return 0;
}

/* Print host command help */
static void print_host_help(void) {
    printf("Till Host Management Commands\n\n");
    printf("Usage: till host <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  init                    Initialize SSH environment and generate keys\n");
    printf("  add <name> <user@host>  Add a new host to Till management\n");
    printf("  test <name>             Test SSH connectivity to a host\n");
    printf("  setup <name>            Install Till on remote host\n");
    printf("  exec <name> <command>   Execute Till command on remote host\n");
    printf("  ssh <name> [command]    Open SSH session to remote host\n");
    printf("  sync [name]             Sync updates from remote host(s)\n");
    printf("  status [name]           Show host configuration and status\n");
    printf("  remove <name> [opts]    Remove a host from Till configuration\n");
    printf("\nOptions:\n");
    printf("  --help, -h              Show this help message\n");
    printf("\nExamples:\n");
    printf("  till host init                           # Generate SSH keys\n");
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
    printf("\nSSH Configuration:\n");
    printf("  SSH config: ~/.till/ssh/config\n");
    printf("  SSH keys:   ~/.till/ssh/till_federation_ed25519\n");
    printf("  Hosts file: ~/.till/hosts-local.json\n");
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
    
    if (strcmp(subcmd, "init") == 0) {
        /* Initialize SSH environment and generate key */
        if (till_host_init() != 0) {
            return -1;
        }
        return generate_federation_key();
    }
    else if (strcmp(subcmd, "add") == 0) {
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
    else {
        fprintf(stderr, "Unknown host subcommand: %s\n\n", subcmd);
        print_host_help();
        return -1;
    }
}