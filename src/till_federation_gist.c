/*
 * till_federation_gist.c - GitHub Gist operations for Till Federation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "till_federation.h"
#include "till_common.h"
#include "cJSON.h"

/* Create a new federation gist */
int create_federation_gist(const char *token, const char *site_id, char *gist_id, size_t gist_id_size) {
    char cmd[4096];
    char status_json[2048];
    FILE *fp;
    char buffer[256];
    
    /* Create initial status JSON */
    snprintf(status_json, sizeof(status_json),
        "{"
        "\"site_id\":\"%s\","
        "\"created\":%ld,"
        "\"last_updated\":%ld,"
        "\"till_version\":\"1.5.0\","
        "\"status\":\"active\""
        "}",
        site_id, time(NULL), time(NULL)
    );
    
    /* Use gh CLI to create gist */
    snprintf(cmd, sizeof(cmd),
        "gh api gists "
        "--field description='Till Federation Status for %s' "
        "--field public=true "
        "--field 'files[status.json][content]=%s' "
        "--jq .id",
        site_id, status_json
    );
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to create gist\n");
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        /* Remove newline */
        buffer[strcspn(buffer, "\n")] = '\0';
        strncpy(gist_id, buffer, gist_id_size - 1);
        gist_id[gist_id_size - 1] = '\0';
    }
    
    int result = pclose(fp);
    if (result != 0 || strlen(gist_id) == 0) {
        fprintf(stderr, "Error: Failed to create gist (exit code: %d)\n", result);
        return -1;
    }
    
    return 0;
}

/* Update an existing federation gist */
int update_federation_gist(const char *token, const char *gist_id, const char *content) {
    char cmd[8192];
    FILE *fp;
    char escaped_content[4096];
    const char *p = content;
    char *q = escaped_content;
    
    /* Escape special characters for shell */
    while (*p && q < escaped_content + sizeof(escaped_content) - 2) {
        if (*p == '"' || *p == '\\' || *p == '`' || *p == '$') {
            *q++ = '\\';
        }
        *q++ = *p++;
    }
    *q = '\0';
    
    /* Use gh CLI to update gist */
    snprintf(cmd, sizeof(cmd),
        "gh api gists/%s --method PATCH "
        "--field 'files[status.json][content]=%s' "
        ">/dev/null 2>&1",
        gist_id, escaped_content
    );
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to update gist\n");
        return -1;
    }
    
    int result = pclose(fp);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to update gist %s (exit code: %d)\n", gist_id, result);
        return -1;
    }
    
    return 0;
}

/* Delete a federation gist */
int delete_federation_gist(const char *token, const char *gist_id) {
    char cmd[512];
    FILE *fp;
    
    /* Use gh CLI to delete gist */
    snprintf(cmd, sizeof(cmd),
        "gh api gists/%s --method DELETE",
        gist_id
    );
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to delete gist\n");
        return -1;
    }
    
    int result = pclose(fp);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to delete gist %s (exit code: %d)\n", gist_id, result);
        return -1;
    }
    
    return 0;
}

/* Fetch a gist by ID */
int fetch_federation_gist(const char *gist_id, char *content, size_t content_size) {
    char cmd[512];
    FILE *fp;
    char buffer[4096];
    size_t total_read = 0;
    
    /* Use gh CLI to fetch gist content */
    snprintf(cmd, sizeof(cmd),
        "gh api gists/%s --jq '.files.\"status.json\".content'",
        gist_id
    );
    
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to fetch gist\n");
        return -1;
    }
    
    content[0] = '\0';
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);
        if (total_read + len < content_size - 1) {
            strcat(content, buffer);
            total_read += len;
        }
    }
    
    int result = pclose(fp);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to fetch gist %s (exit code: %d)\n", gist_id, result);
        return -1;
    }
    
    return 0;
}

/* Collect system status for federation */
int collect_system_status(federation_status_t *status) {
    char hostname[256];
    
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    /* Basic status collection */
    strncpy(status->hostname, hostname, sizeof(status->hostname) - 1);
    status->till_version = 150;  /* 1.5.0 */
    status->uptime = time(NULL);
    status->last_sync = time(NULL);
    status->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    
    /* Platform detection */
#ifdef __APPLE__
    strcpy(status->platform, "darwin");
#elif __linux__
    strcpy(status->platform, "linux");
#else
    strcpy(status->platform, "unknown");
#endif
    
    /* Collect installation count from registry */
    status->installation_count = 0;
    
    char registry_path[512];
    snprintf(registry_path, sizeof(registry_path), "%s/.till/tekton/till-private.json", getenv("HOME"));
    
    FILE *fp = fopen(registry_path, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        char *content = malloc(size + 1);
        if (content) {
            fread(content, 1, size, fp);
            content[size] = '\0';
            
            cJSON *json = cJSON_Parse(content);
            if (json) {
                cJSON *installations = cJSON_GetObjectItem(json, "installations");
                if (installations) {
                    status->installation_count = cJSON_GetArraySize(installations);
                }
                cJSON_Delete(json);
            }
            free(content);
        }
        fclose(fp);
    }
    
    return 0;
}

/* Create status JSON from federation status */
int create_status_json(const federation_status_t *status, char *json, size_t json_size) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "site_id", status->site_id);
    cJSON_AddStringToObject(root, "hostname", status->hostname);
    cJSON_AddStringToObject(root, "platform", status->platform);
    cJSON_AddNumberToObject(root, "till_version", status->till_version / 100.0);
    cJSON_AddNumberToObject(root, "cpu_count", status->cpu_count);
    cJSON_AddNumberToObject(root, "installation_count", status->installation_count);
    cJSON_AddNumberToObject(root, "uptime", status->uptime);
    cJSON_AddNumberToObject(root, "last_sync", status->last_sync);
    cJSON_AddStringToObject(root, "trust_level", status->trust_level);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        strncpy(json, json_str, json_size - 1);
        json[json_size - 1] = '\0';
        free(json_str);
    }
    
    cJSON_Delete(root);
    return 0;
}