/*
 * till_common.c - Common utilities for Till
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "till_common.h"
#include "till_config.h"

/* Get the Till configuration directory path */
int get_till_dir(char *path, size_t size) {
    struct stat st;
    char test_path[PATH_MAX];
    char real_path[PATH_MAX];
    
    /* Strategy: Find .till directory, preferring the one in till installation */
    
    /* 1. Check if .till exists in current directory (for Tekton dirs with symlinks) */
    if (stat(TILL_DIR_NAME, &st) == 0) {
        if (realpath(TILL_DIR_NAME, real_path)) {
            strncpy(path, real_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
    
    /* 2. Check parent directory for till/.till */
    char parent_till[PATH_MAX];
    snprintf(parent_till, sizeof(parent_till), "../till/%s", TILL_DIR_NAME);
    if (stat(parent_till, &st) == 0) {
        if (realpath(parent_till, real_path)) {
            strncpy(path, real_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
    }
    
    /* 3. Check known locations */
    char *home = getenv("HOME");
    if (home) {
        /* Check projects/github/till/.till */
        snprintf(test_path, sizeof(test_path), "%s/%s/till/%s", home, TILL_PROJECTS_BASE, TILL_DIR_NAME);
        if (stat(test_path, &st) == 0) {
            strncpy(path, test_path, size - 1);
            path[size - 1] = '\0';
            return 0;
        }
        
        /* REMOVED: We do NOT check ~/.till anymore - only project directory */
    }
    
    /* 4. Try to find via the till executable */
    FILE *fp = popen("which till 2>/dev/null", "r");
    if (fp) {
        char exe_path[PATH_MAX];
        if (fgets(exe_path, sizeof(exe_path), fp) != NULL) {
            exe_path[strcspn(exe_path, "\n")] = '\0';
            pclose(fp);
            
            /* Get directory of executable */
            char *dir_end = strrchr(exe_path, '/');
            if (dir_end) {
                *dir_end = '\0';
                
                /* Check for .till in parent of executable location */
                snprintf(test_path, sizeof(test_path), "%s/../%s", exe_path, TILL_DIR_NAME);
                if (stat(test_path, &st) == 0) {
                    if (realpath(test_path, real_path)) {
                        strncpy(path, real_path, size - 1);
                        path[size - 1] = '\0';
                        return 0;
                    }
                }
            }
        } else {
            pclose(fp);
        }
    }
    
    /* 5. Last resort - create .till in current directory */
    fprintf(stderr, "Warning: Cannot find till/.till directory, using current directory\n");
    if (getcwd(real_path, sizeof(real_path))) {
        snprintf(path, size, "%s/.till", real_path);
        return 0;
    }
    
    return -1;
}