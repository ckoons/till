/*
 * till_config.h - Configuration constants for Till
 * 
 * All paths, URLs, and configuration values in one place.
 * No hardcoded values elsewhere in the code.
 */

#ifndef TILL_CONFIG_H
#define TILL_CONFIG_H

/* Version Information */
#define TILL_VERSION "1.0.0"
#define TILL_CONFIG_VERSION "till-config-v1"

/* GitHub Configuration */
#define TILL_GITHUB_BASE "https://github.com"
#define TILL_GITHUB_USER "ckoons"  /* Will change to tekton org later */
#define TILL_GITHUB_REPO "till"

/* Default URLs */
#define TILL_REPO_URL "https://github.com/" TILL_GITHUB_USER "/" TILL_GITHUB_REPO
#define TILL_REPO_GIT_URL "git@github.com:" TILL_GITHUB_USER "/" TILL_GITHUB_REPO ".git"
#define TEKTON_REPO_URL TILL_GITHUB_BASE "/" TILL_GITHUB_USER "/Tekton"

/* Local Paths */
#define TILL_HOME ".till"
#define TILL_CONFIG_DIR ".till/config"
#define TILL_TEKTON_DIR ".till/tekton"
#define TILL_LOGS_DIR ".till/logs"

/* Configuration Files */
#define TILL_PRIVATE_CONFIG "till-private.json"
#define TILL_PRIVATE_BACKUP "till-private.json.bak"
#define TILL_HOSTS_CONFIG "till-hosts.json"

/* Federation Configuration */
#define TILL_DEFAULT_MODE "solo"
#define TILL_FEDERATION_BRANCH "federation"
#define TILL_REGISTRATION_PREFIX "registration-"
#define TILL_DEREGISTRATION_PREFIX "deregistration-"

/* Timing Configuration */
#define TILL_DEFAULT_WATCH_HOURS 24
#define TILL_DEFAULT_TTL_HOURS 72
#define TILL_SYNC_INTERVAL_SECONDS 86400  /* 24 hours */

/* Port Configuration */
#define DEFAULT_PORT_BASE 8000
#define DEFAULT_AI_PORT_BASE 45000
#define PORT_RANGE_SIZE 100

/* SSH Key Configuration - DEPRECATED, using user's SSH keys now */

/* Remote Installation Paths - NO HOME DIRECTORY REFERENCES */
#define TILL_REMOTE_INSTALL_PATH "projects/github/till"  /* Relative to user home */
#define TILL_REMOTE_BINARY_PATH ".local/bin/till"        /* Relative to user home */
#define TILL_PROJECTS_BASE "projects/github"              /* Base for projects */

/* Local Till Directories - ALL RELATIVE TO PROJECT */
#define TILL_DIR_NAME ".till"
#define TILL_RC_DIR ".tillrc"
#define TILL_COMMANDS_DIR ".tillrc/commands"

/* Status Values */
#define TILL_STATUS_READY "ready"
#define TILL_STATUS_PENDING "pending"
#define TILL_STATUS_ERROR "error"

/* Private Config Format */
#define TILL_PRIVATE_FORMAT "private-v1"

/* Branch TTL Values (in hours) */
#define TTL_PUBLIC_FACE 72      /* 3 days */
#define TTL_COMPONENT 24        /* 1 day */
#define TTL_SOLUTION 168        /* 7 days */
#define TTL_ANNOUNCEMENT 1      /* 1 hour */
#define TTL_HANDSHAKE 0.25      /* 15 minutes */

/* Command Configuration */
#define TILL_MAX_ARGS 10
#define TILL_MAX_PATH 4096
#define TILL_MAX_COMMAND 8192
#define TILL_MAX_URL 2048
#define TILL_MAX_NAME 256
#define TILL_MAX_FQN 512

/* JSON Configuration */
#define JSON_INDENT 2
#define JSON_MAX_SIZE 1048576  /* 1MB max JSON file */

/* Git Commands */
#define GIT_CMD "git"
#define GH_CMD "gh"

/* Installation Modes */
#define MODE_SOLO "solo"
#define MODE_OBSERVER "observer"
#define MODE_MEMBER "member"

/* Component Status */
#define STATUS_INSTALLED "installed"
#define STATUS_AVAILABLE "available"
#define STATUS_HELD "held"
#define STATUS_UPDATING "updating"
#define STATUS_ERROR "error"

/* Exit Codes */
#define EXIT_SUCCESS 0
#define EXIT_GENERAL_ERROR 1
#define EXIT_USAGE_ERROR 2
#define EXIT_FILE_ERROR 3
#define EXIT_GIT_ERROR 4
#define EXIT_JSON_ERROR 5
#define EXIT_NETWORK_ERROR 6
#define EXIT_AUTH_ERROR 7

/* Debug Configuration */
#ifdef DEBUG
#define TILL_DEBUG 1
#else
#define TILL_DEBUG 0
#endif

/* Platform Detection */
#ifdef __APPLE__
#define PLATFORM_MAC 1
#define PLATFORM_NAME "mac"
#elif __linux__
#define PLATFORM_LINUX 1
#define PLATFORM_NAME "linux"
#else
#define PLATFORM_UNKNOWN 1
#define PLATFORM_NAME "unknown"
#endif

#endif /* TILL_CONFIG_H */
