/*
 * till_menu.h - Menu management for till federation
 */

#ifndef TILL_MENU_H
#define TILL_MENU_H

/* Menu command handlers */
int cmd_menu(int argc, char **argv);
int cmd_menu_add(int argc, char **argv);
int cmd_menu_remove(int argc, char **argv);

#endif /* TILL_MENU_H */