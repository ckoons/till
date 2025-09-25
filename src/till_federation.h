/*
 * till_federation.h - Global federation management for Till
 * 
 * Implements asynchronous federation using GitHub Gists as infrastructure
 */

#ifndef TILL_FEDERATION_H
#define TILL_FEDERATION_H

#include <time.h>

/* Federation trust levels */
#define TRUST_ANONYMOUS "anonymous"
#define TRUST_NAMED     "named"
#define TRUST_TRUSTED   "trusted"

/* Federation URLs */
#define FEDERATION_REPO_URL "https://raw.githubusercontent.com/tekton/till-federation/main"
#define MENU_OF_THE_DAY_PATH "/menu-of-the-day/latest.json"

/* Federation configuration */
typedef struct {
    char site_id[128];           /* Unique site identifier */
    char gist_id[64];            /* GitHub Gist ID */
    char trust_level[32];        /* anonymous|named|trusted */
    time_t last_sync;            /* Last sync timestamp */
    int auto_sync;               /* Auto-sync enabled */
    char last_menu_date[32];     /* Date of last processed menu */
} federation_config_t;

/* Menu directive structure */
typedef struct {
    char id[64];                 /* Unique directive ID */
    char type[32];               /* update|patch|telemetry|script */
    char target[32];             /* till|tekton|component */
    char condition[256];         /* Condition expression */
    char action[1024];           /* Command to execute */
    char priority[16];           /* low|medium|high|critical */
    int report_back;             /* Report results to gist */
} directive_t;

/* Menu of the day structure */
typedef struct {
    char date[32];               /* Menu date */
    char version[16];            /* Menu version */
    directive_t *directives;     /* Array of directives */
    int directive_count;         /* Number of directives */
    char **announcements;        /* Array of announcements */
    int announcement_count;      /* Number of announcements */
} menu_t;

/* Gist manifest structure */
typedef struct {
    char site_id[128];           /* Site identifier */
    char hostname[128];          /* Machine hostname */
    char till_version[32];       /* Till version */
    char platform[32];           /* Platform (darwin/linux) */
    char trust_level[32];        /* Trust level */
    time_t created;              /* Creation timestamp */
    time_t updated;              /* Last update timestamp */
} manifest_t;

/* Main federation functions */
int till_federate_join(const char *trust_level);
int till_federate_leave(int delete_gist);
int till_federate_status(void);
int till_federate_set(const char *key, const char *value);

/* Configuration management */
int load_federation_config(federation_config_t *config);
int save_federation_config(const federation_config_t *config);
int create_default_federation_config(void);
int federation_is_joined(void);

/* Menu processing */
int fetch_menu_of_the_day(menu_t *menu);
int process_directive(const directive_t *directive, char *result, size_t result_size);
int evaluate_condition(const char *condition);
int is_directive_completed(const char *directive_id);
int mark_directive_completed(const char *directive_id);

/* Federation status structure */
typedef struct {
    char site_id[128];
    char hostname[128];
    char platform[32];
    int till_version;
    int cpu_count;
    int installation_count;
    time_t uptime;
    time_t last_sync;
    char trust_level[32];
} federation_status_t;

/* Gist management */
int create_federation_gist(const char *site_id, char *gist_id, size_t gist_id_size);
int update_federation_gist(const char *gist_id, const char *content);
int delete_federation_gist(const char *gist_id);
int fetch_federation_gist(const char *gist_id, char *content, size_t content_size);

/* Status collection */
int collect_system_status(federation_status_t *status);
int create_status_json(const federation_status_t *status, char *json, size_t json_size);

/* GitHub API */
int github_api_call(const char *method, const char *url,
                    const char *data, char *response, size_t response_size);

/* Token management - simplified to just use gh CLI */
int get_github_token(char *token, size_t size);

/* Telemetry */
int collect_telemetry(char *telemetry_json, size_t size);
int should_report_telemetry(const char *trust_level);

/* Command handler */
int cmd_federate(int argc, char *argv[]);

/* Admin commands (owner only) */
int till_federate_admin(int argc, char *argv[]);
int till_federate_admin_process(void);
int till_federate_admin_status(int full);

#endif /* TILL_FEDERATION_H */