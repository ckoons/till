/*
 * till_install.h - Installation functionality for Till
 * 
 * Handles Tekton installation, port allocation, and .env.local generation
 */

#ifndef TILL_INSTALL_H
#define TILL_INSTALL_H

#include "till_config.h"

/* Component port mapping structure */
typedef struct {
    const char *name;
    int offset;
} component_port_t;

/* Installation options */
typedef struct {
    char name[TILL_MAX_NAME];          /* Registry name */
    char path[TILL_MAX_PATH];          /* Installation path */
    char mode[32];                      /* solo/observer/member */
    int port_base;                      /* Base port (8000, 8100, etc) */
    int ai_port_base;                   /* AI base port (45000, 45100, etc) */
    char registry_name[TILL_MAX_NAME]; /* TEKTON_REGISTRY_NAME */
    char tekton_main_root[TILL_MAX_PATH]; /* TEKTON_MAIN_ROOT path */
} install_options_t;

/* Function prototypes */
int till_install_tekton(install_options_t *opts);
int generate_env_local(install_options_t *opts);
int clone_tekton_repo(const char *path);
int validate_name(const char *name);
int allocate_ports(install_options_t *opts);
int register_installation(install_options_t *opts);
int get_primary_tekton_name(char *name, size_t size);
int fuzzy_match_name(const char *input, char *matched, size_t size);
int discover_tektons(void);
int ensure_directory(const char *path);
int get_primary_name(char *name, size_t size);
int save_installation_info(install_options_t *opts);

/* Component port offsets - matches .env.local.example */
static const component_port_t COMPONENT_PORTS[] = {
    {"ENGRAM_PORT", 0},
    {"HERMES_PORT", 1},
    {"ERGON_PORT", 2},
    {"RHETOR_PORT", 3},
    {"TERMA_PORT", 4},
    {"ATHENA_PORT", 5},
    {"PROMETHEUS_PORT", 6},
    {"HARMONIA_PORT", 7},
    {"TELOS_PORT", 8},
    {"SYNTHESIS_PORT", 9},
    {"TEKTON_CORE_PORT", 10},
    {"METIS_PORT", 11},
    {"APOLLO_PORT", 12},
    {"BUDGET_PORT", 13},
    {"PENIA_PORT", 13},  /* Same as BUDGET */
    {"SOPHIA_PORT", 14},
    {"NOESIS_PORT", 15},
    {"NUMA_PORT", 16},
    {"HEPHAESTUS_PORT", 80},  /* Special case */
    {"HEPHAESTUS_MCP_PORT", 88},  /* Special case */
    {"AISH_PORT", 97},
    {"AISH_MCP_PORT", 98},
    {"DB_MCP_PORT", 99},  /* At base + 99 */
    {NULL, -1}
};

/* AI port offsets - same as component offsets */
static const component_port_t AI_PORTS[] = {
    {"ENGRAM_AI_PORT", 0},
    {"HERMES_AI_PORT", 1},
    {"ERGON_AI_PORT", 2},
    {"RHETOR_AI_PORT", 3},
    {"TERMA_AI_PORT", 4},
    {"ATHENA_AI_PORT", 5},
    {"PROMETHEUS_AI_PORT", 6},
    {"HARMONIA_AI_PORT", 7},
    {"TELOS_AI_PORT", 8},
    {"SYNTHESIS_AI_PORT", 9},
    {"TEKTON_CORE_AI_PORT", 10},
    {"METIS_AI_PORT", 11},
    {"APOLLO_AI_PORT", 12},
    {"BUDGET_AI_PORT", 13},
    {"PENIA_AI_PORT", 13},  /* Same as BUDGET */
    {"SOPHIA_AI_PORT", 14},
    {"NOESIS_AI_PORT", 15},
    {"NUMA_AI_PORT", 16},
    {"HEPHAESTUS_AI_PORT", 80},  /* Special case */
    {NULL, -1}
};

#endif /* TILL_INSTALL_H */
