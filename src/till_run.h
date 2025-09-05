/*
 * till_run.h - Component command execution header for Till
 */

#ifndef TILL_RUN_H
#define TILL_RUN_H

/* Main entry point for till run command */
int till_run_command(int argc, char *argv[]);

/* Check if Till can run a component */
int till_can_run_component(const char *component);

#endif /* TILL_RUN_H */