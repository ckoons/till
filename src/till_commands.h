#ifndef TILL_COMMANDS_H
#define TILL_COMMANDS_H

#include "till_install.h"

/* Implemented Till commands */
int cmd_sync(int argc, char **argv);
int cmd_watch(int argc, char **argv);
int cmd_install(int argc, char **argv);
int cmd_uninstall(int argc, char **argv);
int cmd_hold(int argc, char **argv);
int cmd_release(int argc, char **argv);
int cmd_host(int argc, char **argv);
int cmd_federate(int argc, char **argv);
int cmd_status(int argc, char **argv);
int cmd_run(int argc, char **argv);
int cmd_update(int argc, char **argv);
int cmd_repair(int argc, char **argv);
int cmd_help(int argc, char **argv);

/* Special function for dry run mode */
int cmd_dry_run(void);

#endif