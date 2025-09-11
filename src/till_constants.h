/*
 * till_constants.h - Non-configurable constants for Till
 * 
 * These constants define internal values that shouldn't be
 * changed by configuration. For configurable values, see till_config.h
 */

#ifndef TILL_CONSTANTS_H
#define TILL_CONSTANTS_H

/* Log Levels */
#define LOG_ERROR   0
#define LOG_WARN    1
#define LOG_INFO    2
#define LOG_DEBUG   3

/* Command Return Codes */
#define CMD_SUCCESS         0
#define CMD_ERROR          -1
#define CMD_NOT_FOUND      -2
#define CMD_INVALID_ARGS   -3
#define CMD_PERMISSION     -4
#define CMD_TIMEOUT        -5

/* Installation States */
#define INST_STATE_READY      0
#define INST_STATE_UPDATING   1
#define INST_STATE_ERROR      2
#define INST_STATE_HELD       3

/* Federation States */
#define FED_STATE_SOLO        0
#define FED_STATE_OBSERVER    1
#define FED_STATE_MEMBER      2
#define FED_STATE_PENDING     3

/* Lock States */
#define LOCK_AVAILABLE        0
#define LOCK_HELD             1
#define LOCK_TIMED_OUT        2

/* Discovery Results */
#define DISCOVER_SUCCESS      0
#define DISCOVER_NONE        -1
#define DISCOVER_ERROR       -2

/* JSON Operation Results */
#define JSON_SUCCESS          0
#define JSON_PARSE_ERROR     -1
#define JSON_FILE_ERROR      -2
#define JSON_INVALID_TYPE    -3

/* Maximum Values */
#define MAX_RETRIES           3
#define MAX_FEDERATION_MEMBERS 32
#define MAX_INSTALLATIONS     64
#define MAX_HOSTS            128

/* Timeout Values (seconds) */
#define DEFAULT_TIMEOUT       30
#define SSH_TIMEOUT          60
#define GIT_TIMEOUT         120
#define LOCK_TIMEOUT         10

/* Network Constants */
#define DEFAULT_SSH_PORT     22
#define DEFAULT_HTTP_PORT    80
#define DEFAULT_HTTPS_PORT  443

/* Component Types */
#define COMP_TYPE_TEKTON     0
#define COMP_TYPE_SOLUTION   1
#define COMP_TYPE_COMPONENT  2
#define COMP_TYPE_EXTERNAL   3

/* Scheduler Types */
#define SCHED_TYPE_NONE      0
#define SCHED_TYPE_CRON      1
#define SCHED_TYPE_LAUNCHD   2
#define SCHED_TYPE_SYSTEMD   3

/* Version Check Results */
#define VERSION_CURRENT      0
#define VERSION_OUTDATED     1
#define VERSION_NEWER        2
#define VERSION_UNKNOWN      3

#endif /* TILL_CONSTANTS_H */