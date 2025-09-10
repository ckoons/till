/*
 * till_tekton.h - Tekton-specific installation and management
 */

#ifndef TILL_TEKTON_H
#define TILL_TEKTON_H

#include "till_install.h"

/* Tekton-specific functions */
int clone_tekton_repo(const char *path);
int generate_tekton_env(install_options_t *opts);
int install_tekton_dependencies(const char *path);
int create_till_symlink(const char *tekton_path);
int install_tekton(install_options_t *opts);
int update_tekton(const char *path);

#endif /* TILL_TEKTON_H */