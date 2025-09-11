/*
 * till_progress.c - Progress indicator for long operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#include "till_config.h"
#include "till_progress.h"

/* Progress state */
static struct {
    int active;
    const char *message;
    pthread_t thread;
    pthread_mutex_t mutex;
    int dots;
    struct timeval start_time;
} progress_state = {
    .active = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .dots = 0
};

/* Progress thread function */
static void *progress_thread(void *arg) {
    (void)arg;  /* Unused */
    
    while (1) {
        pthread_mutex_lock(&progress_state.mutex);
        if (!progress_state.active) {
            pthread_mutex_unlock(&progress_state.mutex);
            break;
        }
        
        /* Calculate elapsed time */
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - progress_state.start_time.tv_sec);
        
        /* Update dots every 2 seconds */
        int new_dots = (elapsed / 2) % 6;  /* 0-5 dots, cycle every 10 seconds */
        
        if (new_dots != progress_state.dots) {
            /* Clear current line */
            fprintf(stderr, "\r%-60s\r", "");
            
            /* Print message with dots */
            fprintf(stderr, "%s", progress_state.message);
            for (int i = 0; i < new_dots; i++) {
                fprintf(stderr, ".");
            }
            fflush(stderr);
            
            progress_state.dots = new_dots;
        }
        
        pthread_mutex_unlock(&progress_state.mutex);
        
        /* Sleep for 100ms before checking again */
        usleep(100000);
    }
    
    return NULL;
}

/* Start progress indicator */
void progress_start(const char *message) {
    pthread_mutex_lock(&progress_state.mutex);
    
    if (progress_state.active) {
        /* Already running, update message */
        progress_state.message = message;
        progress_state.dots = 0;
        gettimeofday(&progress_state.start_time, NULL);
    } else {
        /* Start new progress */
        progress_state.active = 1;
        progress_state.message = message;
        progress_state.dots = 0;
        gettimeofday(&progress_state.start_time, NULL);
        
        /* Create thread */
        pthread_create(&progress_state.thread, NULL, progress_thread, NULL);
    }
    
    pthread_mutex_unlock(&progress_state.mutex);
}

/* Stop progress indicator */
void progress_stop(void) {
    pthread_mutex_lock(&progress_state.mutex);
    
    if (progress_state.active) {
        progress_state.active = 0;
        pthread_mutex_unlock(&progress_state.mutex);
        
        /* Wait for thread to finish */
        pthread_join(progress_state.thread, NULL);
        
        /* Clear the line */
        fprintf(stderr, "\r%-60s\r", "");
        fflush(stderr);
    } else {
        pthread_mutex_unlock(&progress_state.mutex);
    }
}

/* Update progress message */
void progress_update(const char *message) {
    pthread_mutex_lock(&progress_state.mutex);
    
    if (progress_state.active) {
        progress_state.message = message;
        progress_state.dots = 0;
        gettimeofday(&progress_state.start_time, NULL);
        
        /* Clear and update immediately */
        fprintf(stderr, "\r%-60s\r%s", "", message);
        fflush(stderr);
    }
    
    pthread_mutex_unlock(&progress_state.mutex);
}

/* Complete with message */
void progress_complete(const char *message) {
    progress_stop();
    if (message) {
        fprintf(stderr, "%s\n", message);
        fflush(stderr);
    }
}