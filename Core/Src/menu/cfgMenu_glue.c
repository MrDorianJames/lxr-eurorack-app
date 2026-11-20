/*
 * cfgMenu_glue.c
 *
 * Connects the config menu (cfgMenu.c) to the rest of the firmware.
 */

#include "menu/cfgMenu.h"
#include "menu/lsMenu.h"
#include "menu/menu.h"
#include "Preset/PresetManager.h"
#include "Parameters.h"

/* passed by the build (Core/CMakeLists.txt); fallback as in main.c */
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.51"
#endif

void cfgGlue_show(const char *row0, const char *row1, int8_t cursorRow, int8_t cursorCol)
{
	lsGlue_show(row0, row1, cursorRow, cursorCol);	/* same display buffer and cursor */
}
void cfgGlue_requestRepaint(void)				{ menu_repaintAll(); }
uint8_t cfgGlue_isConfigPage(void)				{ return menu_activePage == MENU_MIDI_PAGE; }
uint8_t cfgGlue_kitValue(uint16_t paramIdx)		{ return activeParameters[paramIdx]; }
void cfgGlue_setKitValue(uint16_t paramIdx, uint8_t value) { activeParameters[paramIdx] = value; }
void cfgGlue_sendParameter(uint16_t paramIdx)	{ preset_sendParameter(paramIdx); }
uint8_t cfgGlue_getScreensaver(void)			{ return activeParameters[PAR_SCREENSAVER_ON_OFF] ? 1 : 0; }
void cfgGlue_setScreensaver(uint8_t on)			{ activeParameters[PAR_SCREENSAVER_ON_OFF] = on ? 1 : 0; }
uint8_t cfgGlue_getCvGlobal(void)				{ return activeParameters[PAR_CV_SAVE] ? 1 : 0; }
void cfgGlue_setCvGlobal(uint8_t global)		{ activeParameters[PAR_CV_SAVE] = global ? 1 : 0; }
const char* cfgGlue_stockVersion(void)			{ return FIRMWARE_VERSION; }
