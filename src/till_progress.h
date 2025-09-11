/*
 * till_progress.h - Progress indicator for long operations
 */

#ifndef TILL_PROGRESS_H
#define TILL_PROGRESS_H

/* Start progress indicator with message */
void progress_start(const char *message);

/* Stop progress indicator */
void progress_stop(void);

/* Update progress message */
void progress_update(const char *message);

/* Complete with final message */
void progress_complete(const char *message);

#endif /* TILL_PROGRESS_H */