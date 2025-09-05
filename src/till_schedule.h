/*
 * till_schedule.h - Scheduling management header for Till
 */

#ifndef TILL_SCHEDULE_H
#define TILL_SCHEDULE_H

/* Configure watch daemon */
int till_watch_configure(int argc, char *argv[]);

/* Show watch status */
int till_watch_status(void);

/* Install platform-specific scheduler */
int till_watch_install_scheduler(void);

/* Platform-specific installers */
int till_watch_install_launchd(void);  /* macOS */
int till_watch_install_systemd(void);  /* Linux with systemd */
int till_watch_install_cron(void);     /* Fallback */

/* Record sync result for history */
int till_watch_record_sync(int success, int duration_seconds, int installations, int hosts);

#endif /* TILL_SCHEDULE_H */