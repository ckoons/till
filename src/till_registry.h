/*
 * till_registry.h - Tekton installation registry and discovery
 */

#ifndef TILL_REGISTRY_H
#define TILL_REGISTRY_H

/* Discover existing Tekton installations */
int discover_tektons(void);

/* Get primary Tekton installation path */
int get_primary_tekton_path(char *path, size_t size);

/* Get primary Tekton name */
int get_primary_tekton_name(char *name, size_t size);

/* Register a new Tekton installation */
int register_installation(const char *name, const char *path, int main_port, int ai_port, const char *mode);

/* Suggest next available port range */
int suggest_next_port_range(int *main_port, int *ai_port);

/* Validate installation name */
int validate_installation_name(const char *name);

/* Fuzzy match installation name */
int fuzzy_match_name(const char *input, char *matched, size_t size);

#endif /* TILL_REGISTRY_H */