/*
 * till_host.h - Host management header for Till
 */

#ifndef TILL_HOST_H
#define TILL_HOST_H

/* Add a new host */
int till_host_add(const char *name, const char *user_at_host);

/* Test SSH connectivity to a host */
int till_host_test(const char *name);

/* Execute command on remote host */
int till_host_exec(const char *name, const char *command);

/* SSH interactive session to remote host */
int till_host_ssh(const char *name, int argc, char *argv[]);

/* Remove a host */
int till_host_remove(const char *name, int clean_remote);

/* Show host status */
int till_host_status(const char *name);

/* Update Till on remote host(s) */
int till_host_update(const char *host_name);

/* Sync Tekton installations on remote host(s) */
int till_host_sync(const char *host_name);

/* Main host command handler */
int till_host_command(int argc, char *argv[]);

#endif /* TILL_HOST_H */