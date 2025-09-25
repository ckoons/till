/*
 * till_federation_admin.c - Admin commands for Till Federation (owner only)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "till_federation.h"
#include "till_common.h"
#include "cJSON.h"

#define REPO_OWNER "ckoons"
#define SECRET_GIST_DESC "Till Federation Admin Status (Private)"
#define ADMIN_CONFIG_FILE "admin.json"

/* Admin configuration structure */
typedef struct {
    char secret_gist_id[64];
    time_t last_processed;
    int total_processed;
} admin_config_t;

/* Verify current user is repo owner */
static int verify_owner(void) {
    FILE *fp;
    char buffer[128];
    char username[64] = {0};
    
    /* Get current GitHub username */
    fp = popen("gh api user --jq .login 2>/dev/null", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to get GitHub user\n");
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        strncpy(username, buffer, sizeof(username) - 1);
    }
    
    pclose(fp);
    
    if (strlen(username) == 0) {
        fprintf(stderr, "Error: Not authenticated with GitHub CLI\n");
        fprintf(stderr, "Run: gh auth login\n");
        return -1;
    }
    
    if (strcmp(username, REPO_OWNER) != 0) {
        fprintf(stderr, "Error: Only the repository owner (%s) can run admin commands\n", REPO_OWNER);
        fprintf(stderr, "Current user: %s\n", username);
        return -1;
    }
    
    return 0;
}

/* Load admin configuration */
static int load_admin_config(admin_config_t *config) {
    char path[512];
    FILE *fp;
    
    snprintf(path, sizeof(path), "%s/.till/%s", getenv("HOME"), ADMIN_CONFIG_FILE);
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        /* No config yet */
        memset(config, 0, sizeof(admin_config_t));
        return 0;
    }
    
    char buffer[1024];
    fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    cJSON *json = cJSON_Parse(buffer);
    if (json) {
        const char *gist_id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "secret_gist_id"));
        if (gist_id) {
            strncpy(config->secret_gist_id, gist_id, sizeof(config->secret_gist_id) - 1);
        }
        
        cJSON *last = cJSON_GetObjectItem(json, "last_processed");
        if (last) {
            config->last_processed = (time_t)cJSON_GetNumberValue(last);
        }
        
        cJSON *total = cJSON_GetObjectItem(json, "total_processed");
        if (total) {
            config->total_processed = cJSON_GetNumberValue(total);
        }
        
        cJSON_Delete(json);
    }
    
    return 0;
}

/* Save admin configuration */
static int save_admin_config(const admin_config_t *config) {
    char path[512];
    FILE *fp;
    
    snprintf(path, sizeof(path), "%s/.till/%s", getenv("HOME"), ADMIN_CONFIG_FILE);
    
    fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "secret_gist_id", config->secret_gist_id);
    cJSON_AddNumberToObject(json, "last_processed", config->last_processed);
    cJSON_AddNumberToObject(json, "total_processed", config->total_processed);
    
    char *json_str = cJSON_Print(json);
    fprintf(fp, "%s\n", json_str);
    free(json_str);
    cJSON_Delete(json);
    
    fclose(fp);
    return 0;
}

/* Find or create secret gist */
static int get_or_create_secret_gist(char *gist_id, size_t gist_id_size) {
    FILE *fp;
    char buffer[256];
    
    /* First check if we have a saved gist ID */
    admin_config_t config;
    if (load_admin_config(&config) == 0 && strlen(config.secret_gist_id) > 0) {
        /* Verify it still exists */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "gh api gists/%s --jq .id 2>/dev/null", config.secret_gist_id);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                pclose(fp);
                strncpy(gist_id, config.secret_gist_id, gist_id_size - 1);
                return 0;
            }
            pclose(fp);
        }
    }
    
    /* Create new secret gist */
    printf("Creating secret gist for admin status...\n");
    
    /* Create status.json file directly */
    const char *temp_file = "/tmp/status.json";
    FILE *tf = fopen(temp_file, "w");
    if (tf == NULL) {
        fprintf(stderr, "Error: Failed to create temp file\n");
        return -1;
    }
    
    fprintf(tf, "{\"last_processed\":null,\"total_sites\":0,\"sites\":{},\"statistics\":{}}");
    fclose(tf);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "gh gist create %s --desc \"%s\" 2>/dev/null | head -1",  /* secret is default */
        temp_file, SECRET_GIST_DESC);
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to create secret gist\n");
        unlink(temp_file);
        return -1;
    }
    
    /* Read first line which contains the URL */
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        /* Remove newline */
        buffer[strcspn(buffer, "\n")] = '\0';
        
        /* Check if it's a URL */
        if (strstr(buffer, "https://gist.github.com/") != NULL) {
            /* Extract gist ID from URL */
            char *id = strrchr(buffer, '/');
            if (id) {
                id++;
                strncpy(gist_id, id, gist_id_size - 1);
                gist_id[gist_id_size - 1] = '\0';
                
                /* Save for future use */
                strncpy(config.secret_gist_id, gist_id, sizeof(config.secret_gist_id) - 1);
                save_admin_config(&config);
                
                pclose(fp);
                unlink(temp_file);
                printf("Created secret gist: %s\n", gist_id);
                return 0;
            }
        }
        fprintf(stderr, "Error: Unexpected output: %s\n", buffer);
    } else {
        fprintf(stderr, "Error: No output from gist create command\n");
    }
    
    unlink(temp_file);
    return -1;
}

/* Process all federation gists */
int till_federate_admin_process(void) {
    if (verify_owner() != 0) {
        return -1;
    }
    
    printf("Till Federation Admin - Process\n");
    printf("================================\n\n");
    
    /* Get or create secret gist */
    char secret_gist_id[64];
    if (get_or_create_secret_gist(secret_gist_id, sizeof(secret_gist_id)) != 0) {
        fprintf(stderr, "Error: Failed to get/create secret gist\n");
        return -1;
    }
    
    printf("Using secret gist: %s\n\n", secret_gist_id);
    
    /* Find all Till Federation gists */
    printf("Searching for Till Federation gists...\n");
    
    FILE *fp;
    char cmd[512];
    char buffer[256];
    
    /* Create aggregated data */
    cJSON *report = cJSON_CreateObject();
    cJSON_AddStringToObject(report, "last_processed", "");
    cJSON_AddNumberToObject(report, "total_sites", 0);
    cJSON *sites = cJSON_CreateObject();
    cJSON_AddItemToObject(report, "sites", sites);
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddItemToObject(report, "statistics", stats);
    cJSON *malformed = cJSON_CreateArray();
    cJSON_AddItemToObject(report, "malformed", malformed);
    
    /* Statistics tracking */
    int total_found = 0;
    int total_processed = 0;
    int total_malformed = 0;
    int total_deleted = 0;
    
    cJSON *by_platform = cJSON_CreateObject();
    cJSON *by_trust = cJSON_CreateObject();
    int active_24h = 0;
    int active_7d = 0;
    
    time_t now = time(NULL);
    
    /* Get list of gists */
    snprintf(cmd, sizeof(cmd), 
        "gh gist list --limit 1000 2>/dev/null | grep 'Till Federation Status' | cut -f1");
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to list gists\n");
        cJSON_Delete(report);
        return -1;
    }
    
    /* Process each gist */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) continue;
        
        total_found++;
        printf("  Processing gist %s...", buffer);
        
        /* Fetch gist content */
        char fetch_cmd[512];
        snprintf(fetch_cmd, sizeof(fetch_cmd),
            "gh api gists/%s --jq '.files.\"status.json\".content' 2>/dev/null", buffer);
        
        FILE *content_fp = popen(fetch_cmd, "r");
        if (content_fp == NULL) {
            printf(" FAILED\n");
            total_malformed++;
            
            cJSON *mal = cJSON_CreateObject();
            cJSON_AddStringToObject(mal, "gist_id", buffer);
            cJSON_AddStringToObject(mal, "error", "Failed to fetch");
            cJSON_AddItemToArray(malformed, mal);
            continue;
        }
        
        /* Read content */
        char content[8192] = {0};
        size_t total_read = 0;
        while (fgets(content + total_read, sizeof(content) - total_read, content_fp) != NULL) {
            total_read = strlen(content);
            if (total_read >= sizeof(content) - 1) break;
        }
        pclose(content_fp);
        
        /* Parse JSON */
        cJSON *status = cJSON_Parse(content);
        if (status == NULL) {
            printf(" MALFORMED\n");
            total_malformed++;
            
            cJSON *mal = cJSON_CreateObject();
            cJSON_AddStringToObject(mal, "gist_id", buffer);
            cJSON_AddStringToObject(mal, "error", "Invalid JSON");
            cJSON_AddItemToArray(malformed, mal);
        } else {
            /* Extract data */
            const char *site_id = cJSON_GetStringValue(cJSON_GetObjectItem(status, "site_id"));
            const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(status, "hostname"));
            const char *platform = cJSON_GetStringValue(cJSON_GetObjectItem(status, "platform"));
            const char *trust_level = cJSON_GetStringValue(cJSON_GetObjectItem(status, "trust_level"));
            double last_sync = cJSON_GetNumberValue(cJSON_GetObjectItem(status, "last_sync"));
            int inst_count = cJSON_GetNumberValue(cJSON_GetObjectItem(status, "installation_count"));
            
            if (site_id) {
                printf(" OK (site: %s)\n", site_id);
                total_processed++;
                
                /* Add to sites */
                cJSON *site = cJSON_CreateObject();
                cJSON_AddStringToObject(site, "hostname", hostname ? hostname : "unknown");
                cJSON_AddStringToObject(site, "platform", platform ? platform : "unknown");
                cJSON_AddStringToObject(site, "trust_level", trust_level ? trust_level : "unknown");
                cJSON_AddNumberToObject(site, "last_sync", last_sync);
                cJSON_AddNumberToObject(site, "installation_count", inst_count);
                cJSON_AddStringToObject(site, "gist_id", buffer);
                cJSON_AddNumberToObject(site, "processed_at", now);
                cJSON_AddItemToObject(sites, site_id, site);
                
                /* Update statistics */
                if (platform) {
                    cJSON *plat_count = cJSON_GetObjectItem(by_platform, platform);
                    if (plat_count) {
                        int count = cJSON_GetNumberValue(plat_count);
                        cJSON_DeleteItemFromObject(by_platform, platform);
                        cJSON_AddNumberToObject(by_platform, platform, count + 1);
                    } else {
                        cJSON_AddNumberToObject(by_platform, platform, 1);
                    }
                }
                
                if (trust_level) {
                    cJSON *trust_count = cJSON_GetObjectItem(by_trust, trust_level);
                    if (trust_count) {
                        int count = cJSON_GetNumberValue(trust_count);
                        cJSON_DeleteItemFromObject(by_trust, trust_level);
                        cJSON_AddNumberToObject(by_trust, trust_level, count + 1);
                    } else {
                        cJSON_AddNumberToObject(by_trust, trust_level, 1);
                    }
                }
                
                /* Check activity */
                if (last_sync > 0) {
                    if (now - last_sync < 86400) active_24h++;
                    if (now - last_sync < 604800) active_7d++;
                }
            } else {
                printf(" MALFORMED (no site_id)\n");
                total_malformed++;
                
                cJSON *mal = cJSON_CreateObject();
                cJSON_AddStringToObject(mal, "gist_id", buffer);
                cJSON_AddStringToObject(mal, "error", "Missing site_id");
                if (hostname) cJSON_AddStringToObject(mal, "hostname", hostname);
                cJSON_AddItemToArray(malformed, mal);
            }
            
            cJSON_Delete(status);
        }
        
        /* Delete the gist */
        printf("    Deleting gist %s...", buffer);
        char delete_cmd[512];
        snprintf(delete_cmd, sizeof(delete_cmd), "gh gist delete %s 2>/dev/null", buffer);
        if (system(delete_cmd) == 0) {
            printf(" DELETED\n");
            total_deleted++;
        } else {
            printf(" FAILED\n");
        }
    }
    
    pclose(fp);
    
    /* Update report metadata */
    char time_str[64];
    time_t process_time = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&process_time));
    cJSON_DeleteItemFromObject(report, "last_processed");
    cJSON_AddStringToObject(report, "last_processed", time_str);
    cJSON_DeleteItemFromObject(report, "total_sites");
    cJSON_AddNumberToObject(report, "total_sites", total_processed);
    
    /* Add statistics */
    cJSON_AddItemToObject(stats, "by_platform", by_platform);
    cJSON_AddItemToObject(stats, "by_trust_level", by_trust);
    cJSON_AddNumberToObject(stats, "active_last_24h", active_24h);
    cJSON_AddNumberToObject(stats, "active_last_7d", active_7d);
    cJSON_AddNumberToObject(stats, "total_found", total_found);
    cJSON_AddNumberToObject(stats, "total_processed", total_processed);
    cJSON_AddNumberToObject(stats, "total_malformed", total_malformed);
    cJSON_AddNumberToObject(stats, "total_deleted", total_deleted);
    
    /* Save to secret gist */
    printf("\nSaving report to secret gist...\n");
    char *report_json = cJSON_Print(report);
    
    /* Create status.json file */
    const char *temp_file = "/tmp/status.json";
    FILE *tf = fopen(temp_file, "w");
    if (tf == NULL) {
        fprintf(stderr, "Error: Failed to create temp file\n");
        free(report_json);
        cJSON_Delete(report);
        return -1;
    }
    
    fprintf(tf, "%s", report_json);
    fclose(tf);
    
    /* Update gist - delete old file and add new one */
    snprintf(cmd, sizeof(cmd),
        "gh gist edit %s --add %s 2>/dev/null",
        secret_gist_id, temp_file);
    
    if (system(cmd) == 0) {
        printf("✓ Report saved to secret gist\n");
    } else {
        fprintf(stderr, "Error: Failed to update secret gist\n");
    }
    
    unlink(temp_file);
    free(report_json);
    cJSON_Delete(report);
    
    /* Update admin config */
    admin_config_t config;
    load_admin_config(&config);
    config.last_processed = process_time;
    config.total_processed += total_processed;
    save_admin_config(&config);
    
    /* Summary */
    printf("\n=== Process Summary ===\n");
    printf("Found:      %d gists\n", total_found);
    printf("Processed:  %d sites\n", total_processed);
    printf("Malformed:  %d gists\n", total_malformed);
    printf("Deleted:    %d gists\n", total_deleted);
    printf("Report:     Secret gist %s\n", secret_gist_id);
    
    return 0;
}

/* Display admin status */
int till_federate_admin_status(int full) {
    if (verify_owner() != 0) {
        return -1;
    }
    
    printf("Till Federation Admin - Status\n");
    printf("===============================\n\n");
    
    /* Get secret gist ID */
    admin_config_t config;
    if (load_admin_config(&config) != 0 || strlen(config.secret_gist_id) == 0) {
        fprintf(stderr, "Error: No admin data found. Run 'till federate admin process' first.\n");
        return -1;
    }
    
    /* Fetch secret gist content - skip first 2 lines (title and blank) */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gh gist view %s --filename status.json 2>/dev/null", config.secret_gist_id);
    
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to fetch secret gist\n");
        return -1;
    }
    
    /* Read content */
    char content[32768] = {0};
    size_t total_read = 0;
    while (fgets(content + total_read, sizeof(content) - total_read, fp) != NULL) {
        total_read = strlen(content);
        if (total_read >= sizeof(content) - 1) break;
    }
    pclose(fp);
    
    /* Parse JSON */
    cJSON *report = cJSON_Parse(content);
    if (report == NULL) {
        fprintf(stderr, "Error: Failed to parse admin status\n");
        return -1;
    }
    
    /* Display summary */
    const char *last_processed = cJSON_GetStringValue(cJSON_GetObjectItem(report, "last_processed"));
    int total_sites = cJSON_GetNumberValue(cJSON_GetObjectItem(report, "total_sites"));
    
    printf("Last Processed: %s\n", last_processed ? last_processed : "Never");
    printf("Total Sites:    %d\n\n", total_sites);
    
    /* Display statistics */
    cJSON *stats = cJSON_GetObjectItem(report, "statistics");
    if (stats) {
        printf("=== Statistics ===\n");
        
        cJSON *by_platform = cJSON_GetObjectItem(stats, "by_platform");
        if (by_platform) {
            printf("By Platform:\n");
            cJSON *item;
            cJSON_ArrayForEach(item, by_platform) {
                printf("  %-10s: %d\n", item->string, (int)cJSON_GetNumberValue(item));
            }
        }
        
        cJSON *by_trust = cJSON_GetObjectItem(stats, "by_trust_level");
        if (by_trust) {
            printf("\nBy Trust Level:\n");
            cJSON *item;
            cJSON_ArrayForEach(item, by_trust) {
                printf("  %-10s: %d\n", item->string, (int)cJSON_GetNumberValue(item));
            }
        }
        
        printf("\nActivity:\n");
        printf("  Last 24h:   %d sites\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "active_last_24h")));
        printf("  Last 7d:    %d sites\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "active_last_7d")));
        
        printf("\nProcessing:\n");
        printf("  Found:      %d gists\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "total_found")));
        printf("  Processed:  %d sites\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "total_processed")));
        printf("  Malformed:  %d gists\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "total_malformed")));
        printf("  Deleted:    %d gists\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(stats, "total_deleted")));
    }
    
    /* Display full site list if requested */
    if (full) {
        printf("\n=== All Sites ===\n");
        cJSON *sites = cJSON_GetObjectItem(report, "sites");
        if (sites) {
            cJSON *site;
            cJSON_ArrayForEach(site, sites) {
                printf("\nSite ID: %s\n", site->string);
                printf("  Hostname:     %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(site, "hostname")));
                printf("  Platform:     %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(site, "platform")));
                printf("  Trust Level:  %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(site, "trust_level")));
                
                double last_sync = cJSON_GetNumberValue(cJSON_GetObjectItem(site, "last_sync"));
                if (last_sync > 0) {
                    time_t sync_time = (time_t)last_sync;
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&sync_time));
                    printf("  Last Sync:    %s\n", time_str);
                } else {
                    printf("  Last Sync:    Never\n");
                }
                
                printf("  Installations: %d\n", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(site, "installation_count")));
            }
        }
        
        /* Show malformed gists */
        cJSON *malformed = cJSON_GetObjectItem(report, "malformed");
        if (malformed && cJSON_GetArraySize(malformed) > 0) {
            printf("\n=== Malformed Gists ===\n");
            cJSON *mal;
            cJSON_ArrayForEach(mal, malformed) {
                printf("Gist: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(mal, "gist_id")));
                printf("  Error: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(mal, "error")));
                const char *hostname = cJSON_GetStringValue(cJSON_GetObjectItem(mal, "hostname"));
                if (hostname) {
                    printf("  Hostname: %s\n", hostname);
                }
            }
        }
    }
    
    printf("\nSecret Gist: %s\n", config.secret_gist_id);
    printf("View online: https://gist.github.com/%s\n", config.secret_gist_id);
    
    cJSON_Delete(report);
    return 0;
}

/* Main admin command handler */
int till_federate_admin(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Till Federation Admin Commands (Owner Only)\n");
        printf("===========================================\n\n");
        printf("Usage: till federate admin <command> [options]\n\n");
        printf("Commands:\n");
        printf("  menu       Manage menu of the day\n");
        printf("  process    Process all federation gists and delete them\n");
        printf("  status     Show aggregated status from secret gist\n\n");
        printf("Menu Commands:\n");
        printf("  menu add <component> <availability>    Add component to menu\n");
        printf("  menu remove <component>                Remove from menu\n");
        printf("  menu show                              Show current menu\n\n");
        printf("Options:\n");
        printf("  --stats    Show statistics only (default)\n");
        printf("  --full     Show full site details\n\n");
        printf("Examples:\n");
        printf("  till federate admin menu add Tekton \"anonymous=standard,named=standard\"\n");
        printf("  till federate admin menu show\n");
        printf("  till federate admin process\n");
        printf("  till federate admin status --full\n");
        return 0;
    }
    
    const char *subcmd = argv[2];

    if (strcmp(subcmd, "menu") == 0) {
        /* Pass to menu handler, adjusting argc/argv */
        extern int cmd_menu(int argc, char **argv);
        return cmd_menu(argc - 2, argv + 2);
    }
    else if (strcmp(subcmd, "process") == 0) {
        return till_federate_admin_process();
    }
    else if (strcmp(subcmd, "status") == 0) {
        int full = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--full") == 0) {
                full = 1;
            }
        }
        return till_federate_admin_status(full);
    }
    else {
        fprintf(stderr, "Error: Unknown admin command: %s\n", subcmd);
        fprintf(stderr, "Use 'till federate admin' for help\n");
        return -1;
    }
}