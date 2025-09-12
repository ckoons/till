#ifndef TILL_COMMANDS_H
#define TILL_COMMANDS_H

#include "till_install.h"

int cmd_sync(int argc, char **argv);
int cmd_watch(int argc, char **argv);
int cmd_install(int argc, char **argv);
int cmd_uninstall(int argc, char **argv);
int cmd_hold(int argc, char **argv);
int cmd_release(int argc, char **argv);
int cmd_host(int argc, char **argv);
int cmd_federate(int argc, char **argv);
int cmd_start(int argc, char **argv);
int cmd_stop(int argc, char **argv);
int cmd_restart(int argc, char **argv);
int cmd_status(int argc, char **argv);
int cmd_run(int argc, char **argv);
int cmd_update(int argc, char **argv);
int cmd_repair(int argc, char **argv);
int cmd_list(int argc, char **argv);
int cmd_info(int argc, char **argv);
int cmd_logs(int argc, char **argv);
int cmd_monitor(int argc, char **argv);
int cmd_backup(int argc, char **argv);
int cmd_restore(int argc, char **argv);
int cmd_config(int argc, char **argv);
int cmd_schedule(int argc, char **argv);
int cmd_registry(int argc, char **argv);
int cmd_export(int argc, char **argv);
int cmd_import(int argc, char **argv);
int cmd_test(int argc, char **argv);
int cmd_verify(int argc, char **argv);
int cmd_upgrade(int argc, char **argv);
int cmd_rollback(int argc, char **argv);
int cmd_clean(int argc, char **argv);
int cmd_exec(int argc, char **argv);
int cmd_set(int argc, char **argv);
int cmd_get(int argc, char **argv);
int cmd_help(int argc, char **argv);
int cmd_version(int argc, char **argv);
int cmd_federate(int argc, char **argv);

/* Special function for dry run mode */
int cmd_dry_run(void);

#endif