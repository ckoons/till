/*
 * till_common.h - Common utilities for Till
 */

#ifndef TILL_COMMON_H
#define TILL_COMMON_H

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Get the Till configuration directory path
 * Returns the path to till/.till/ directory
 * This is where all Till configuration and data is stored
 */
int get_till_dir(char *path, size_t size);

#endif /* TILL_COMMON_H */