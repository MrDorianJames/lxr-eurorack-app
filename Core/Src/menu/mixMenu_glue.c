/*
 * mixMenu_glue.c
 *
 * Connects the mixer (mixMenu.c) to the rest of the firmware. Display and
 * parameter access are shared with the config menu (cfgMenu_glue.c).
 */

#include "menu/mixMenu.h"
#include "menu/menu.h"

uint8_t mixGlue_isPerfPage(void) { return menu_activePage == PERF_PAGE; }
