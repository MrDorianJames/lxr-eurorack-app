/*
 * lsMenu_glue.c
 *
 * Connects the Load/Save menu (lsMenu.c) to the rest of the firmware:
 * display, page and mode switching, LEDs and the kit/morph state.
 * Kept separate so lsMenu.c can be tested on a PC.
 */

#include "menu/lsMenu.h"
#include "menu/menu.h"
#include "Preset/PresetManager.h"
#include "buttonHandler.h"
#include "ledHandler.h"
#include "stm32f7xx_hal.h"
#include <string.h>

/* defined in menu.c */
extern char editDisplayBuffer[2][17];
extern uint8_t visibleCursor;
void menu_handleLoadSaveMenu(int8_t inc, uint8_t btnClicked);
void menu_lsParamName(uint16_t idx, char *row16);
void menu_lsRestoreParam(uint16_t idx, uint8_t value);
void menu_lsReloadCurrentKit(void);

/* ---------------------------------------------------------------------- */
void lsGlue_show(const char *row0, const char *row1, int8_t cursorRow, int8_t cursorCol)
{
	memcpy(editDisplayBuffer[0], row0, 16);
	memcpy(editDisplayBuffer[1], row1, 16);
	editDisplayBuffer[0][16] = 0;
	editDisplayBuffer[1][16] = 0;

	/* same encoding as VIS_CURS() in menu.c: column, row, visible */
	if(cursorCol >= 0)	visibleCursor = (uint8_t)((cursorCol << 2) | ((cursorRow & 1) << 1) | 1);
	else				visibleCursor = 0;
}
/* ---------------------------------------------------------------------- */
void lsGlue_requestRepaint(void)
{
	menu_repaintAll();
}
/* ---------------------------------------------------------------------- */
void lsGlue_enterOverviewPage(void)
{
	led_clearAllBlinkLeds();
	led_setMode2(SELECT_MODE_LOAD);
	menu_switchPage(LOAD_PAGE);
	menu_repaintAll();
}
/* ---------------------------------------------------------------------- */
void lsGlue_enterStockKitPage(uint8_t save, uint8_t morph)
{
	menu_setLoadSaveCategory(morph ? SAVE_TYPE_MORPH : SAVE_TYPE_KIT);
	menu_switchPage(save ? SAVE_PAGE : LOAD_PAGE);
	menu_repaintAll();
}
/* ---------------------------------------------------------------------- */
/* leaving the Load/Save menu goes to the Perform page, like pressing Perf */
void lsGlue_leaveMenu(void)
{
	buttonHandler_enterPerformMode();
}
/* ---------------------------------------------------------------------- */
void lsGlue_stockScroll(int8_t inc)
{
	menu_handleLoadSaveMenu(inc, 0);
	menu_repaintAll();
}
/* ---------------------------------------------------------------------- */
uint8_t lsGlue_isOnStockPage(void)
{
	return (menu_activePage == LOAD_PAGE) || (menu_activePage == SAVE_PAGE);
}
/* ---------------------------------------------------------------------- */
uint8_t lsGlue_activeVoice(void)
{
	return menu_getActiveVoice();
}
/* ---------------------------------------------------------------------- */
void lsGlue_currentKitName(char *name8)
{
	const char *n = savefileNames[menu_loadedPresetNr[CURRENT_KIT]][KIT_NAMES];
	for(uint8_t i = 0; i < 8; i++)
	{
		name8[i] = (n[i] == 0) ? ' ' : n[i];
	}
}
/* ---------------------------------------------------------------------- */
void lsGlue_kitName(uint8_t slot, char *name8)
{
	const char *n = savefileNames[slot][KIT_NAMES];
	for(uint8_t i = 0; i < 8; i++)
	{
		name8[i] = (n[i] == 0) ? ' ' : n[i];
	}
}
/* ---------------------------------------------------------------------- */
uint8_t lsGlue_isMorphActive(void)
{
	return preset_morphValue > 0;
}
/* ---------------------------------------------------------------------- */
uint8_t* lsGlue_kitParameters(void)
{
	return activeParameters;
}
/* ---------------------------------------------------------------------- */
uint32_t lsGlue_nowMs(void)
{
	return HAL_GetTick();
}
/* ---------------------------------------------------------------------- */
uint8_t lsGlue_numNameChars(void)
{
	return menu_numNameChars();
}
/* ---------------------------------------------------------------------- */
char lsGlue_nameChar(uint8_t index)
{
	return menu_nameChar(index);
}
/* ---------------------------------------------------------------------- */
const uint8_t* lsGlue_kitParamsOfSlot(uint8_t slot)
{
	return mainParameters[slot & 0x7F];
}
/* ---------------------------------------------------------------------- */
uint8_t lsGlue_currentKitSlot(void)
{
	return menu_loadedPresetNr[CURRENT_KIT] & 0x7F;
}
/* ---------------------------------------------------------------------- */
void lsGlue_reloadCurrentKit(void)
{
	menu_lsReloadCurrentKit();
}
/* ---------------------------------------------------------------------- */
void lsGlue_restoreParam(uint16_t idx, uint8_t value)
{
	menu_lsRestoreParam(idx, value);
}
/* ---------------------------------------------------------------------- */
void lsGlue_paramName(uint16_t idx, char *row16)
{
	menu_lsParamName(idx, row16);
}
