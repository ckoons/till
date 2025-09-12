/*
 * till_federation.c - Global federation implementation
 * 
 * Implements asynchronous federation using GitHub Gists
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#include "till_federation.h"
#include "till_config.h"
#include "till_common.h"
#include "cJSON.h"

/* Check if federation is configured */
int federation_is_joined(void) {
    char path[TILL_MAX_PATH];
    if (build_till_path(path, sizeof(path), "federation.json") != 0) {
        return 0;
    }
    return path_exists(path);
}

/* Load federation configuration */
int load_federation_config(federation_config_t *config) {
    if (!config) return -1;
    
    cJSON *json = load_till_json("federation.json");
    if (!json) {
        return -1;
    }
    
    /* Load configuration fields */
    const char *site_id = json_get_string(json, "site_id", "");
    const char *gist_id = json_get_string(json, "gist_id", "");
    const char *token = json_get_string(json, "github_token_encrypted", "");
    const char *trust = json_get_string(json, "trust_level", TRUST_ANONYMOUS);
    const char *last_menu = json_get_string(json, "last_menu_date", "");
    
    strncpy(config->site_id, site_id, sizeof(config->site_id) - 1);
    strncpy(config->gist_id, gist_id, sizeof(config->gist_id) - 1);
    strncpy(config->github_token, token, sizeof(config->github_token) - 1);
    strncpy(config->trust_level, trust, sizeof(config->trust_level) - 1);
    strncpy(config->last_menu_date, last_menu, sizeof(config->last_menu_date) - 1);
    
    config->last_sync = (time_t)json_get_int(json, "last_sync", 0);
    config->auto_sync = json_get_bool(json, "auto_sync", 0);
    
    cJSON_Delete(json);
    return 0;
}

/* Save federation configuration */
int save_federation_config(const federation_config_t *config) {
    if (!config) return -1;
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    
    cJSON_AddStringToObject(json, "site_id", config->site_id);
    cJSON_AddStringToObject(json, "gist_id", config->gist_id);
    cJSON_AddStringToObject(json, "github_token_encrypted", config->github_token);
    cJSON_AddStringToObject(json, "trust_level", config->trust_level);
    cJSON_AddNumberToObject(json, "last_sync", (double)config->last_sync);
    cJSON_AddBoolToObject(json, "auto_sync", config->auto_sync);
    cJSON_AddStringToObject(json, "last_menu_date", config->last_menu_date);
    
    int result = save_till_json("federation.json", json);
    cJSON_Delete(json);
    
    return result;
}

/* Simple token encryption (XOR with key for now - should use proper encryption) */
int encrypt_token(const char *plain, char *encrypted, size_t size) {
    if (!plain || !encrypted) return -1;
    
    const char *key = "TillFederationKey2025";  /* Should use better key management */
    size_t key_len = strlen(key);
    size_t plain_len = strlen(plain);
    
    /* Simple XOR encryption - replace with proper encryption */
    for (size_t i = 0; i < plain_len && i < size - 1; i++) {
        encrypted[i] = plain[i] ^ key[i % key_len];
    }
    encrypted[plain_len] = '\0';
    
    /* Base64 encode the result for storage */
    /* TODO: Implement proper base64 encoding */
    
    return 0;
}

/* Decrypt token */
int decrypt_token(const char *encrypted, char *plain, size_t size) {
    if (!encrypted || !plain) return -1;
    
    /* TODO: Base64 decode first */
    
    const char *key = "TillFederationKey2025";
    size_t key_len = strlen(key);
    size_t enc_len = strlen(encrypted);
    
    /* XOR decryption (same as encryption for XOR) */
    for (size_t i = 0; i < enc_len && i < size - 1; i++) {
        plain[i] = encrypted[i] ^ key[i % key_len];
    }
    plain[enc_len] = '\0';
    
    return 0;
}

/* Prompt for GitHub token */
int prompt_for_token(char *token, size_t size) {
    printf("Enter GitHub Personal Access Token (with gist scope): ");
    fflush(stdout);
    
    /* Disable echo for password input */
    system("stty -echo");
    
    if (fgets(token, size, stdin) == NULL) {
        system("stty echo");
        printf("\n");
        return -1;
    }
    
    system("stty echo");
    printf("\n");
    
    /* Remove newline */
    char *newline = strchr(token, '\n');
    if (newline) *newline = '\0';
    
    return strlen(token) > 0 ? 0 : -1;
}

/* Generate unique site ID */
static void generate_site_id(char *site_id, size_t size) {
    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    /* Remove dots from hostname */
    for (char *p = hostname; *p; p++) {
        if (*p == '.') *p = '-';
    }
    
    time_t now = time(NULL);
    int random_val = rand() % 10000;
    
    snprintf(site_id, size, "%s-%ld-%04d", hostname, now, random_val);
}

/* Create initial gist manifest */
static int create_manifest_json(char *json_out, size_t size, const federation_config_t *config) {
    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    cJSON *manifest = cJSON_CreateObject();
    if (!manifest) return -1;
    
    cJSON_AddStringToObject(manifest, "site_id", config->site_id);
    cJSON_AddStringToObject(manifest, "hostname", hostname);
    cJSON_AddStringToObject(manifest, "till_version", TILL_VERSION);
    cJSON_AddStringToObject(manifest, "platform", PLATFORM_NAME);
    cJSON_AddStringToObject(manifest, "trust_level", config->trust_level);
    cJSON_AddNumberToObject(manifest, "created", time(NULL));
    cJSON_AddNumberToObject(manifest, "updated", time(NULL));
    
    char *json_str = cJSON_Print(manifest);
    if (json_str) {
        strncpy(json_out, json_str, size - 1);
        json_out[size - 1] = '\0';
        free(json_str);
    }
    
    cJSON_Delete(manifest);
    return 0;
}

/* Join the federation */
int till_federate_join(const char *trust_level, const char *token_arg) {
    federation_config_t config = {0};
    char token[256] = {0};
    
    /* Check if already joined */
    if (federation_is_joined()) {
        till_error("Already joined to federation. Use 'till federate leave' first.");
        return -1;
    }
    
    /* Validate trust level */
    if (!trust_level) {
        trust_level = TRUST_NAMED;  /* Default */
    }
    
    if (strcmp(trust_level, TRUST_ANONYMOUS) != 0 &&
        strcmp(trust_level, TRUST_NAMED) != 0 &&
        strcmp(trust_level, TRUST_TRUSTED) != 0) {
        till_error("Invalid trust level. Use: anonymous, named, or trusted");
        return -1;
    }
    
    /* Anonymous doesn't need token or gist */
    if (strcmp(trust_level, TRUST_ANONYMOUS) == 0) {
        printf("Joining federation as anonymous (read-only)...\n");
        
        generate_site_id(config.site_id, sizeof(config.site_id));
        strcpy(config.trust_level, TRUST_ANONYMOUS);
        config.auto_sync = 1;
        config.last_sync = 0;
        
        if (save_federation_config(&config) == 0) {
            printf("✓ Joined federation as anonymous\n");
            printf("  Site ID: %s\n", config.site_id);
            printf("  You can now use 'till federate pull' to get updates\n");
            return 0;
        } else {
            till_error("Failed to save federation configuration");
            return -1;
        }
    }
    
    /* Named and Trusted need GitHub token */
    if (token_arg && strlen(token_arg) > 0) {
        strncpy(token, token_arg, sizeof(token) - 1);
    } else {
        if (prompt_for_token(token, sizeof(token)) != 0) {
            till_error("Failed to get GitHub token");
            return -1;
        }
    }
    
    printf("Joining federation as %s...\n", trust_level);
    
    /* Generate site ID */
    generate_site_id(config.site_id, sizeof(config.site_id));
    strcpy(config.trust_level, trust_level);
    config.auto_sync = 1;
    config.last_sync = 0;
    
    /* Encrypt and store token */
    if (encrypt_token(token, config.github_token, sizeof(config.github_token)) != 0) {
        till_error("Failed to encrypt token");
        return -1;
    }
    
    /* Create gist with manifest */
    printf("Creating GitHub Gist...\n");
    
    char manifest_json[4096];
    if (create_manifest_json(manifest_json, sizeof(manifest_json), &config) != 0) {
        till_error("Failed to create manifest");
        return -1;
    }
    
    /* TODO: Actually create the gist via GitHub API */
    /* For now, simulate with a placeholder */
    printf("  Would create gist with manifest:\n");
    printf("  Site ID: %s\n", config.site_id);
    printf("  Trust Level: %s\n", config.trust_level);
    
    /* Save configuration */
    if (save_federation_config(&config) == 0) {
        printf("✓ Joined federation successfully\n");
        printf("  Site ID: %s\n", config.site_id);
        printf("  Trust Level: %s\n", config.trust_level);
        printf("  Run 'till federate sync' to start syncing\n");
        return 0;
    } else {
        till_error("Failed to save federation configuration");
        return -1;
    }
}

/* Leave the federation */
int till_federate_leave(int delete_gist) {
    if (!federation_is_joined()) {
        till_error("Not currently joined to federation");
        return -1;
    }
    
    federation_config_t config;
    if (load_federation_config(&config) != 0) {
        till_error("Failed to load federation configuration");
        return -1;
    }
    
    printf("Leaving federation...\n");
    
    /* TODO: Delete gist if requested */
    if (delete_gist && strlen(config.gist_id) > 0) {
        printf("  Would delete gist: %s\n", config.gist_id);
    }
    
    /* Remove local configuration */
    char path[TILL_MAX_PATH];
    if (build_till_path(path, sizeof(path), "federation.json") == 0) {
        if (unlink(path) == 0) {
            printf("✓ Left federation successfully\n");
            return 0;
        }
    }
    
    till_error("Failed to remove federation configuration");
    return -1;
}

/* Show federation status */
int till_federate_status(void) {
    if (!federation_is_joined()) {
        printf("Federation Status: Not Joined\n");
        printf("\nTo join the federation, use:\n");
        printf("  till federate join --anonymous    # Read-only access\n");
        printf("  till federate join --named        # Standard membership\n");
        printf("  till federate join --trusted      # Full participation\n");
        return 0;
    }
    
    federation_config_t config;
    if (load_federation_config(&config) != 0) {
        till_error("Failed to load federation configuration");
        return -1;
    }
    
    printf("Federation Status: Joined\n");
    printf("==================\n");
    printf("Site ID:     %s\n", config.site_id);
    printf("Trust Level: %s\n", config.trust_level);
    
    if (strlen(config.gist_id) > 0) {
        printf("Gist ID:     %s\n", config.gist_id);
    }
    
    if (config.last_sync > 0) {
        char time_str[64];
        struct tm *tm_info = localtime(&config.last_sync);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("Last Sync:   %s\n", time_str);
    } else {
        printf("Last Sync:   Never\n");
    }
    
    printf("Auto Sync:   %s\n", config.auto_sync ? "Enabled" : "Disabled");
    
    if (strlen(config.last_menu_date) > 0) {
        printf("Last Menu:   %s\n", config.last_menu_date);
    }
    
    return 0;
}

/* Main federate command handler */
int cmd_federate(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: till federate <command> [options]\n");
        printf("\nCommands:\n");
        printf("  join [--anonymous|--named|--trusted]  Join the federation\n");
        printf("  leave [--delete-gist]                 Leave the federation\n");
        printf("  pull                                  Get menu-of-the-day\n");
        printf("  push                                  Update your gist\n");
        printf("  sync                                  Pull, process, and push\n");
        printf("  status                                Show federation status\n");
        return 0;
    }
    
    const char *subcmd = argv[1];
    
    if (strcmp(subcmd, "join") == 0) {
        const char *trust_level = TRUST_NAMED;  /* Default */
        const char *token = NULL;
        
        /* Parse options */
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--anonymous") == 0) {
                trust_level = TRUST_ANONYMOUS;
            } else if (strcmp(argv[i], "--named") == 0) {
                trust_level = TRUST_NAMED;
            } else if (strcmp(argv[i], "--trusted") == 0) {
                trust_level = TRUST_TRUSTED;
            } else if (strncmp(argv[i], "--token=", 8) == 0) {
                token = argv[i] + 8;
            }
        }
        
        return till_federate_join(trust_level, token);
    }
    else if (strcmp(subcmd, "leave") == 0) {
        int delete_gist = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--delete-gist") == 0) {
                delete_gist = 1;
            }
        }
        return till_federate_leave(delete_gist);
    }
    else if (strcmp(subcmd, "status") == 0) {
        return till_federate_status();
    }
    else if (strcmp(subcmd, "pull") == 0) {
        printf("Federation pull not yet implemented\n");
        return 0;
    }
    else if (strcmp(subcmd, "push") == 0) {
        printf("Federation push not yet implemented\n");
        return 0;
    }
    else if (strcmp(subcmd, "sync") == 0) {
        printf("Federation sync not yet implemented\n");
        return 0;
    }
    else {
        till_error("Unknown federate command: %s", subcmd);
        return -1;
    }
}