/*
 * till_hold.h - Hold/Release management for Till
 * 
 * Manages component update holds to prevent unwanted updates
 */

#ifndef TILL_HOLD_H
#define TILL_HOLD_H

#include <time.h>
#include "cJSON.h"

/* Hold information structure */
typedef struct {
    char component[256];      /* Component name/pattern */
    time_t held_at;           /* When hold was created */
    time_t expires_at;        /* When hold expires (0 = never) */
    char reason[1024];        /* Reason for hold */
    char held_by[256];        /* User who created hold */
} hold_info_t;

/* Hold command options */
typedef struct {
    char components[2048];    /* Comma-separated component list */
    char from_time[64];       /* Start time (ISO format or "now") */
    char until_time[64];      /* End time (ISO format or "never") */
    char duration[32];        /* Duration (e.g., "7d", "2w", "1month") */
    char reason[1024];        /* Reason for hold */
    int all_components;       /* Hold all components */
    int force;               /* Force override existing hold */
    int interactive;         /* Interactive mode */
} hold_options_t;

/* Release command options */
typedef struct {
    char components[2048];    /* Comma-separated component list */
    int all_components;       /* Release all holds */
    int expired_only;        /* Only release expired holds */
    int force;               /* Force release even if not expired */
    int interactive;         /* Interactive mode */
} release_options_t;

/* Main entry points */

/* Hold components from updates */
int till_hold_command(int argc, char *argv[]);

/* Release components from hold */
int till_release_command(int argc, char *argv[]);

/* Check if component is held */
int is_component_held(const char *component);

/* Get hold information for component */
int get_hold_info(const char *component, hold_info_t *info);

/* List all current holds */
int list_holds(hold_info_t **holds, int *count);

/* Hold management functions */

/* Add a hold for a component */
int add_hold(const char *component, const char *reason, 
             time_t expires_at);

/* Remove a hold for a component */
int remove_hold(const char *component);

/* Check and remove expired holds */
int cleanup_expired_holds(void);

/* Parse time specifications */
int parse_time_spec(const char *spec, time_t *time_out);

/* Parse duration specifications */
int parse_duration(const char *duration, time_t *seconds_out);

/* Interactive mode functions */

/* Interactive hold selection */
int hold_interactive(void);

/* Interactive release selection */  
int release_interactive(void);

/* Display functions */

/* Show hold status for all components */
void show_hold_status(void);

/* Show detailed hold information */
void show_hold_details(const char *component);

/* Utility functions */

/* Load holds from registry */
cJSON *load_holds(void);

/* Save holds to registry */
int save_holds(cJSON *holds);

/* Format time for display */
void format_time(time_t t, char *buf, size_t size);

/* Format duration for display */
void format_duration(time_t seconds, char *buf, size_t size);

#endif /* TILL_HOLD_H */