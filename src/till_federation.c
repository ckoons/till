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

/* Get global federation config path in user's home directory */
static int get_federation_config_path(char *path, size_t size) {
    const char *home = getenv("HOME");
    if (!home) {
        till_error("Cannot determine HOME directory");
        return -1;
    }
    
    snprintf(path, size, "%s/.till/federation.json", home);
    return 0;
}

/* Ensure global till directory exists */
static int ensure_global_till_dir(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return -1;
    }
    
    char path[TILL_MAX_PATH];
    snprintf(path, sizeof(path), "%s/.till", home);
    
    struct stat st;
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            till_error("Cannot create global till directory: %s", strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

/* Check if federation is configured */
int federation_is_joined(void) {
    char path[TILL_MAX_PATH];
    if (get_federation_config_path(path, sizeof(path)) != 0) {
        return 0;
    }
    
    struct stat st;
    int result = (stat(path, &st) == 0);
    
    /* Debug output */
    if (getenv("TILL_DEBUG")) {
        fprintf(stderr, "DEBUG: Checking federation config at: %s (exists: %s)\n", 
                path, result ? "yes" : "no");
    }
    
    return result;
}

/* Load federation configuration */
int load_federation_config(federation_config_t *config) {
    if (!config) return -1;
    
    char path[TILL_MAX_PATH];
    if (get_federation_config_path(path, sizeof(path)) != 0) {
        return -1;
    }
    
    cJSON *json = load_json_file(path);
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
    
    /* Ensure global till directory exists */
    if (ensure_global_till_dir() != 0) {
        return -1;
    }
    
    char path[TILL_MAX_PATH];
    if (get_federation_config_path(path, sizeof(path)) != 0) {
        return -1;
    }
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    
    cJSON_AddStringToObject(json, "site_id", config->site_id);
    cJSON_AddStringToObject(json, "gist_id", config->gist_id);
    cJSON_AddStringToObject(json, "github_token_encrypted", config->github_token);
    cJSON_AddStringToObject(json, "trust_level", config->trust_level);
    cJSON_AddNumberToObject(json, "last_sync", (double)config->last_sync);
    cJSON_AddBoolToObject(json, "auto_sync", config->auto_sync);
    cJSON_AddStringToObject(json, "last_menu_date", config->last_menu_date);
    
    int result = save_json_file(path, json);
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

/* Get token from gh CLI */
static int get_gh_token(char *token, size_t size) {
    FILE *fp;
    char buffer[256];
    
    /* Check if gh is authenticated */
    fp = popen("gh auth status >/dev/null 2>&1 && echo 'ok'", "r");
    if (fp == NULL) {
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) == NULL || strncmp(buffer, "ok", 2) != 0) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    
    /* Get the token */
    fp = popen("gh auth token 2>/dev/null", "r");
    if (fp == NULL) {
        return -1;
    }
    
    if (fgets(token, size, fp) != NULL) {
        /* Remove newline */
        size_t len = strlen(token);
        if (len > 0 && token[len-1] == '\n') {
            token[len-1] = '\0';
        }
        pclose(fp);
        
        /* Validate token has some length */
        if (strlen(token) > 10) {
            return 0;
        }
    }
    
    pclose(fp);
    return -1;
}

/* Check if gh has required scope */
static int check_gh_scope(const char *scope) {
    FILE *fp;
    char buffer[1024];
    
    /* Get token scopes via API */
    fp = popen("gh api user -i 2>/dev/null | grep 'X-OAuth-Scopes:'", "r");
    if (fp == NULL) {
        return 0;  /* Assume no scope if can't check */
    }
    
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        pclose(fp);
        return (strstr(buffer, scope) != NULL) ? 1 : 0;
    }
    
    pclose(fp);
    return 0;
}

/* Setup gh authentication if needed */
static int setup_gh_auth(void) {
    char buffer[256];
    FILE *fp;
    
    /* Check if gh is available */
    fp = popen("command -v gh >/dev/null 2>&1 && echo 'ok'", "r");
    if (fp == NULL) {
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) == NULL || strncmp(buffer, "ok", 2) != 0) {
        pclose(fp);
        printf("Error: GitHub CLI (gh) is required but not found.\n");
        printf("Please install: https://cli.github.com/\n");
        return -1;
    }
    pclose(fp);
    
    /* Check if authenticated */
    fp = popen("gh auth status >/dev/null 2>&1 && echo 'ok'", "r");
    if (fp == NULL) {
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) == NULL || strncmp(buffer, "ok", 2) != 0) {
        pclose(fp);
        printf("GitHub authentication required.\n");
        printf("Please run: gh auth login -s gist\n");
        printf("Then try again.\n");
        return -1;
    }
    pclose(fp);
    
    /* Check for gist scope */
    if (!check_gh_scope("gist")) {
        printf("GitHub token missing 'gist' scope.\n");
        printf("Please run: gh auth refresh -s gist\n");
        printf("Then try again.\n");
        return -1;
    }
    
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
        /* Token provided on command line */
        strncpy(token, token_arg, sizeof(token) - 1);
    } else {
        /* Try to get token from gh CLI first */
        if (get_gh_token(token, sizeof(token)) == 0) {
            printf("Using GitHub token from gh CLI\n");
            
            /* Verify scope */
            if (!check_gh_scope("gist")) {
                printf("Warning: Token may not have 'gist' scope\n");
                printf("To add scope, run: gh auth refresh -s gist\n");
            }
        } else {
            /* gh not available or not authenticated */
            if (setup_gh_auth() != 0) {
                /* Setup failed, try manual token */
                if (!isatty(STDIN_FILENO)) {
                    till_error("GitHub authentication required");
                    printf("Please run: gh auth login -s gist\n");
                    printf("Or use: till federate join --%s --token <token>\n", trust_level);
                    return -1;
                }
                
                printf("\nAlternatively, you can provide a token manually.\n");
                printf("Create a token at: https://github.com/settings/tokens\n");
                printf("Required scope: gist\n\n");
                
                if (prompt_for_token(token, sizeof(token)) != 0) {
                    till_error("Failed to get GitHub token");
                    return -1;
                }
            } else {
                /* Try again after setup */
                if (get_gh_token(token, sizeof(token)) != 0) {
                    till_error("Failed to get token after setup");
                    return -1;
                }
                printf("Using GitHub token from gh CLI\n");
            }
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
    
    /* Create gist if not anonymous */
    if (strcmp(trust_level, TRUST_ANONYMOUS) != 0) {
        printf("Creating GitHub Gist...\n");
        
        /* Create the gist */
        if (create_federation_gist(token, config.site_id, config.gist_id, sizeof(config.gist_id)) == 0) {
            printf("  Created gist: %s\n", config.gist_id);
        } else {
            fprintf(stderr, "Warning: Failed to create gist. You can retry with 'till federate push'\n");
            /* Don't fail join - user can retry push later */
        }
    }
    
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
    
    /* Delete gist if requested */
    if (delete_gist && strlen(config.gist_id) > 0) {
        printf("  Deleting gist: %s\n", config.gist_id);
        
        /* Get GitHub token */
        char token[256];
        if (get_gh_token(token, sizeof(token)) == 0) {
            if (delete_federation_gist(token, config.gist_id) == 0) {
                printf("  ✓ Gist deleted\n");
            } else {
                fprintf(stderr, "  Warning: Failed to delete gist\n");
            }
        } else {
            /* Try to decrypt stored token */
            char plain_token[256];
            if (decrypt_token(config.github_token, plain_token, sizeof(plain_token)) == 0) {
                if (delete_federation_gist(plain_token, config.gist_id) == 0) {
                    printf("  ✓ Gist deleted\n");
                } else {
                    fprintf(stderr, "  Warning: Failed to delete gist\n");
                }
            } else {
                fprintf(stderr, "  Warning: No token available to delete gist\n");
            }
        }
    }
    
    /* Remove global configuration */
    char path[TILL_MAX_PATH];
    if (get_federation_config_path(path, sizeof(path)) == 0) {
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

/* Pull updates from federation (menu-of-the-day) */
int till_federate_pull(void) {
    if (!federation_is_joined()) {
        till_error("Not joined to federation. Use 'till federate join' first");
        return -1;
    }
    
    federation_config_t config;
    if (load_federation_config(&config) != 0) {
        till_error("Failed to load federation configuration");
        return -1;
    }
    
    printf("Pulling federation updates...\n");
    
    /* TODO: Implement menu-of-the-day fetching */
    printf("  Checking for menu-of-the-day...\n");
    printf("  (Menu fetching not yet implemented)\n");
    
    /* For now, just update last sync time */
    config.last_sync = time(NULL);
    save_federation_config(&config);
    
    printf("✓ Pull complete\n");
    return 0;
}

/* Push status to federation gist */
int till_federate_push(void) {
    if (!federation_is_joined()) {
        till_error("Not joined to federation. Use 'till federate join' first");
        return -1;
    }
    
    federation_config_t config;
    if (load_federation_config(&config) != 0) {
        till_error("Failed to load federation configuration");
        return -1;
    }
    
    /* Anonymous members cannot push */
    if (strcmp(config.trust_level, TRUST_ANONYMOUS) == 0) {
        printf("Federation push not available for anonymous members\n");
        return 0;
    }
    
    printf("Pushing status to federation...\n");
    
    /* Get GitHub token */
    char token[256];
    if (get_gh_token(token, sizeof(token)) != 0) {
        /* Try to decrypt stored token */
        if (decrypt_token(config.github_token, token, sizeof(token)) != 0) {
            till_error("Failed to get GitHub token");
            return -1;
        }
    }
    
    /* Collect system status */
    federation_status_t status;
    if (collect_system_status(&status) != 0) {
        till_error("Failed to collect system status");
        return -1;
    }
    
    /* Copy site_id and trust_level to status */
    strncpy(status.site_id, config.site_id, sizeof(status.site_id) - 1);
    strncpy(status.trust_level, config.trust_level, sizeof(status.trust_level) - 1);
    
    /* Create status JSON */
    char json[4096];
    if (create_status_json(&status, json, sizeof(json)) != 0) {
        till_error("Failed to create status JSON");
        return -1;
    }
    
    /* Create or update gist */
    if (strlen(config.gist_id) == 0) {
        /* Need to create gist */
        printf("  Creating GitHub gist...\n");
        if (create_federation_gist(token, config.site_id, config.gist_id, sizeof(config.gist_id)) != 0) {
            till_error("Failed to create gist");
            return -1;
        }
        printf("  Created gist: %s\n", config.gist_id);
        
        /* Save config with new gist ID */
        save_federation_config(&config);
    }
    
    /* Update gist with current status */
    printf("  Updating gist status...\n");
    if (update_federation_gist(token, config.gist_id, json) != 0) {
        till_error("Failed to update gist");
        return -1;
    }
    
    /* Update last sync time */
    config.last_sync = time(NULL);
    save_federation_config(&config);
    
    printf("✓ Push complete\n");
    printf("  Gist: https://gist.github.com/%s\n", config.gist_id);
    return 0;
}

/* Synchronize with federation (pull + push) */
int till_federate_sync(void) {
    if (!federation_is_joined()) {
        till_error("Not joined to federation. Use 'till federate join' first");
        return -1;
    }
    
    federation_config_t config;
    if (load_federation_config(&config) != 0) {
        till_error("Failed to load federation configuration");
        return -1;
    }
    
    printf("Starting federation sync...\n");
    printf("========================\n\n");
    
    /* Step 1: Pull updates */
    printf("Step 1: Pulling menu-of-the-day...\n");
    if (till_federate_pull() != 0) {
        fprintf(stderr, "Warning: Pull failed, continuing with push\n");
    }
    
    /* Step 2: Push status (if not anonymous) */
    if (strcmp(config.trust_level, TRUST_ANONYMOUS) != 0) {
        printf("\nStep 2: Pushing status...\n");
        if (till_federate_push() != 0) {
            fprintf(stderr, "Warning: Push failed\n");
        }
    } else {
        printf("\nStep 2: Push skipped (anonymous member)\n");
    }
    
    printf("\n✓ Sync complete\n");
    return 0;
}

/* Main federate command handler */
int cmd_federate(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Till Federation Commands\n");
        printf("========================\n\n");
        printf("Usage: till federate <command> [options]\n\n");
        printf("Commands:\n");
        printf("  join      Join the federation at specified trust level\n");
        printf("  leave     Leave the federation\n");
        printf("  status    Show current federation status\n");
        printf("  pull      Pull updates from federation (menu-of-the-day)\n");
        printf("  push      Push status to federation gist\n");
        printf("  sync      Synchronize with federation (pull + push)\n");
        printf("  admin     Admin commands (owner only)\n");
        printf("  help      Show detailed help message\n\n");
        printf("Quick Examples:\n");
        printf("  till federate join --anonymous\n");
        printf("  till federate status\n");
        printf("\nUse 'till federate help' for more details\n");
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
        return till_federate_pull();
    }
    else if (strcmp(subcmd, "push") == 0) {
        return till_federate_push();
    }
    else if (strcmp(subcmd, "sync") == 0) {
        return till_federate_sync();
    }
    else if (strcmp(subcmd, "admin") == 0) {
        return till_federate_admin(argc, argv);
    }
    else if (strcmp(subcmd, "--help") == 0 || strcmp(subcmd, "help") == 0) {
        // Show help
        printf("Till Federation Commands\n");
        printf("========================\n\n");
        printf("Usage: till federate <command> [options]\n\n");
        printf("Commands:\n");
        printf("  join      Join the federation at specified trust level\n");
        printf("  leave     Leave the federation\n");
        printf("  status    Show current federation status\n");
        printf("  pull      Pull updates from federation (menu-of-the-day)\n");
        printf("  push      Push status to federation gist\n");
        printf("  sync      Synchronize with federation (pull + push)\n");
        printf("  help      Show this help message\n\n");
        printf("Join Options:\n");
        printf("  --anonymous       Join as anonymous (read-only, no identity)\n");
        printf("  --named          Join as named member (requires GitHub token)\n");
        printf("  --trusted        Join as trusted member (full telemetry)\n");
        printf("  --token <token>  Provide GitHub personal access token\n\n");
        printf("Leave Options:\n");
        printf("  --delete-gist    Delete the GitHub gist when leaving\n\n");
        printf("Examples:\n");
        printf("  till federate join --anonymous\n");
        printf("  till federate join --named --token ghp_xxxx\n");
        printf("  till federate status\n");
        printf("  till federate leave\n");
        return 0;
    }
    else {
        /* Check if user tried to use a flag as a command */
        if (subcmd[0] == '-') {
            if (strcmp(subcmd, "--anonymous") == 0 || 
                strcmp(subcmd, "--named") == 0 || 
                strcmp(subcmd, "--trusted") == 0) {
                fprintf(stderr, "Error: '%s' is a join option, not a command\n", subcmd);
                fprintf(stderr, "\nDid you mean: till federate join %s\n", subcmd);
                fprintf(stderr, "\nUse 'till federate help' for usage information\n");
            } else {
                fprintf(stderr, "Error: Unknown option: %s\n", subcmd);
                fprintf(stderr, "\nUse 'till federate help' for usage information\n");
            }
        } else {
            fprintf(stderr, "Error: Unknown federate command: %s\n", subcmd);
            fprintf(stderr, "\nAvailable commands: join, leave, status, pull, push, sync, help\n");
            fprintf(stderr, "Use 'till federate help' for usage information\n");
        }
        fflush(stderr);
        return -1;
    }
}