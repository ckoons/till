/*
 * till_host.h - Host management header for Till
 */

#ifndef TILL_HOST_H
#define TILL_HOST_H

/* Initialize Till SSH environment */
int till_host_init(void);

/* Add a new host */
int till_host_add(const char *name, const char *user_at_host);

/* Test SSH connectivity to a host */
int till_host_test(const char *name);

/* Setup Till on remote host */
int till_host_setup(const char *name);

/* Execute command on remote host */
int till_host_exec(const char *name, const char *command);

/* SSH interactive session to remote host */
int till_host_ssh(const char *name, int argc, char *argv[]);

/* Sync from remote host */
int till_host_sync(const char *name);

/* Show host status */
int till_host_status(const char *name);

/* Remove a host */
int till_host_remove(const char *name, int clean_remote);

/* Main host command handler */
int till_host_command(int argc, char *argv[]);

#endif /* TILL_HOST_H */