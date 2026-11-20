/*
 * cfgMenu.h
 *
 * The config menu (Shift + Edit; Shift + Load/Save still works too).
 *
 *   scr cv  rtg nfo        screensaver, CV routing kit/global,
 *    on kit  v   v         routing page, firmware info
 *                          (v = the display's filled down triangle,
 *                           marking a sub-page)
 *
 * Encoder turns pick a column; push toggles scr/cv or opens rtg/nfo.
 * Knobs 1/2 change scr/cv, knobs 3/4 open rtg/nfo.
 * Edit steps back from a sub-page; on the overview it leaves the menu as
 * stock (settings are saved to glo.cfg when the menu is left).
 *
 * Routing:  DR1 dr2 dr3 snr >   /   cym hat fx      <
 *           kt  L1  kt  St1          St1 kt  St2
 *   kt = follow the kit (including FX), or fixed: St1 St2 L1 R1 L2 R2,
 *   or FX (voices only: through the effect). A fixed choice always wins.
 *   Each knob edits the value under it; or turn the encoder to pick an
 *   item, push to edit it with the encoder, push again.
 *
 * The mixer is in the Perform menu (mixMenu.h).
 */

#ifndef INC_MENU_CFGMENU_H_
#define INC_MENU_CFGMENU_H_

#include <stdint.h>

void    cfgMenu_onEnter(void);			/* config opened: start on the overview */
uint8_t cfgMenu_isActive(void);
void    cfgMenu_repaint(void);
void    cfgMenu_handleEncoder(int8_t inc, uint8_t clicked);
uint8_t cfgMenu_handleKnob(uint8_t potNr, int8_t diff);	/* returns 1 to repaint */
uint8_t cfgMenu_onEditButton(void);		/* 1 if it stepped back (handled) */

/* ---- provided by cfgMenu_glue.c (firmware) or a test ---------------- */
void    cfgGlue_show(const char *row0, const char *row1, int8_t cursorRow, int8_t cursorCol);
void    cfgGlue_requestRepaint(void);
uint8_t cfgGlue_isConfigPage(void);
uint8_t cfgGlue_kitValue(uint16_t paramIdx);
void    cfgGlue_setKitValue(uint16_t paramIdx, uint8_t value);
void    cfgGlue_sendParameter(uint16_t paramIdx);
uint8_t cfgGlue_getScreensaver(void);
void    cfgGlue_setScreensaver(uint8_t on);
uint8_t cfgGlue_getCvGlobal(void);
void    cfgGlue_setCvGlobal(uint8_t global);
const char* cfgGlue_stockVersion(void);	/* e.g. "1.51" */

#endif /* INC_MENU_CFGMENU_H_ */
