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
    
    fprintf(fp, "\n# Host: %s (added %s)\n", name, ctime(&(time_t){time(NULL)}));
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
    
    printf("Added host '%s' (%s@%s:%d)\n", name, user, host, port);
    printf("SSH alias: till-%s\n", name);
    printf("Test with: till host test %s\n", name);
    
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
    
    char cmd[2048];
    struct passwd *pw = getpwuid(getuid());
    
    /* Check if remote directory exists */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s 'mkdir -p ~/projects/github 2>/dev/null'",
        pw->pw_dir, name);
    
    printf("Creating remote directory structure...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to create remote directories\n");
        return -1;
    }
    
    /* Clone Till repository */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s "
        "'cd ~/projects/github && [ -d till ] || git clone https://github.com/tillfed/till.git'",
        pw->pw_dir, name);
    
    printf("Cloning Till repository...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to clone Till repository\n");
        return -1;
    }
    
    /* Build Till */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s 'cd ~/projects/github/till && make'",
        pw->pw_dir, name);
    
    printf("Building Till...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Failed to build Till\n");
        fprintf(stderr, "You may need to install build tools on the remote host\n");
        return -1;
    }
    
    /* Create symlink */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s "
        "'ln -sf ~/projects/github/till/till /usr/local/bin/till 2>/dev/null || "
        "sudo ln -sf ~/projects/github/till/till /usr/local/bin/till'",
        pw->pw_dir, name);
    
    printf("Installing Till...\n");
    system(cmd);  /* May fail if no sudo, that's OK */
    
    /* Verify installation */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s 'till --version 2>/dev/null || ~/projects/github/till/till --version'",
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

/* Deploy to remote host */
int till_host_deploy(const char *name, const char *installation) {
    if (!name) {
        fprintf(stderr, "Usage: till host deploy <name> [installation]\n");
        return -1;
    }
    
    /* Load local installations */
    char config_path[PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    snprintf(config_path, sizeof(config_path), "%s/.till/tekton/till-private.json", pw->pw_dir);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: No local installations found. Run 'till discover' first.\n");
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);
    
    cJSON *config = cJSON_Parse(content);
    free(content);
    
    if (!config) {
        fprintf(stderr, "Error: Invalid configuration\n");
        return -1;
    }
    
    cJSON *installations = cJSON_GetObjectItem(config, "installations");
    if (!installations) {
        fprintf(stderr, "Error: No installations found\n");
        cJSON_Delete(config);
        return -1;
    }
    
    const char *source_path = NULL;
    const char *deploy_name = installation;
    
    if (installation) {
        /* Find specific installation */
        cJSON *inst = cJSON_GetObjectItem(installations, installation);
        if (!inst) {
            fprintf(stderr, "Error: Installation '%s' not found\n", installation);
            cJSON_Delete(config);
            return -1;
        }
        cJSON *root = cJSON_GetObjectItem(inst, "root");
        if (root) {
            source_path = root->valuestring;
        }
    } else {
        /* Use primary Tekton */
        cJSON *inst = NULL;
        cJSON_ArrayForEach(inst, installations) {
            if (strstr(inst->string, "primary")) {
                cJSON *root = cJSON_GetObjectItem(inst, "root");
                if (root) {
                    source_path = root->valuestring;
                    deploy_name = inst->string;
                }
                break;
            }
        }
    }
    
    if (!source_path) {
        fprintf(stderr, "Error: Cannot find installation to deploy\n");
        cJSON_Delete(config);
        return -1;
    }
    
    printf("Deploying '%s' to '%s'...\n", deploy_name, name);
    printf("Source: %s\n", source_path);
    
    /* Use rsync to deploy */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "rsync -avz --delete "
        "--exclude='.git' "
        "--exclude='*.pyc' "
        "--exclude='__pycache__' "
        "--exclude='logs/' "
        "--exclude='tmp/' "
        "-e 'ssh -F %s/.till/ssh/config' "
        "%s/ till-%s:~/projects/github/Tekton/",
        pw->pw_dir, source_path, name);
    
    printf("Syncing files...\n");
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Deployment failed\n");
        cJSON_Delete(config);
        return -1;
    }
    
    printf("✓ Successfully deployed to '%s'\n", name);
    
    /* Run discovery on remote */
    snprintf(cmd, sizeof(cmd),
        "ssh -F %s/.till/ssh/config till-%s "
        "'cd ~/projects/github/Tekton && till discover'",
        pw->pw_dir, name);
    
    printf("Running remote discovery...\n");
    system(cmd);
    
    cJSON_Delete(config);
    return 0;
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

/* Main host command handler */
int till_host_command(int argc, char *argv[]) {
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
    else if (strcmp(subcmd, "deploy") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: till host deploy <name> [installation]\n");
            return -1;
        }
        return till_host_deploy(argv[2], argc > 3 ? argv[3] : NULL);
    }
    else if (strcmp(subcmd, "sync") == 0) {
        return till_host_sync(argc > 2 ? argv[2] : NULL);
    }
    else if (strcmp(subcmd, "status") == 0) {
        return till_host_status(argc > 2 ? argv[2] : NULL);
    }
    else {
        fprintf(stderr, "Unknown host subcommand: %s\n", subcmd);
        fprintf(stderr, "Available commands:\n");
        fprintf(stderr, "  init    - Initialize SSH environment and generate federation key\n");
        fprintf(stderr, "  add     - Add a new host\n");
        fprintf(stderr, "  test    - Test connection to a host\n");
        fprintf(stderr, "  setup   - Install Till on remote host\n");
        fprintf(stderr, "  deploy  - Deploy Tekton to remote host\n");
        fprintf(stderr, "  sync    - Sync updates from remote host\n");
        fprintf(stderr, "  status  - Show host status\n");
        return -1;
    }
}