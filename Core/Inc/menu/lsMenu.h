/*
 * lsMenu.h
 *
 * The Load/Save menu, opened with the Load/Save button. Two overview pages:
 *
 *   SV  sv  ld  ld  >     save voice, save kit, load voice, load kit
 *   VC  kit vc  kit       (the selected column is in capitals)
 *
 *   ld  rv  rv      <     load voice from a kit, revert voice, revert kit
 *   kvc vc  kit
 *
 * - The page always opens with "save voice" selected.
 * - Data encoder: turn to pick a column (past "ld kit" to page 2), push
 *   to open it.
 * - Knobs 1-4: turning the knob under a column opens it directly
 *   (the voice and kit lists start scrolling straight away).
 * - Load/Save button: steps back one level (expanded view -> overview ->
 *   leave the menu to the Perform page).
 * - After loading a voice or a kit, "Voice loaded" / "Kit loaded" shows
 *   for a second, then the menu closes to the Perform page.
 * - Shift + Load/Save: undo the last edit of the selected voice (or of the
 *   kit-wide settings), shown briefly as "Undo  Drm1 / <parameter>". Any
 *   knob, the encoder or a button ends the message early. Loading a kit
 *   clears the undo history, loading a voice clears that voice's steps.
 *
 * Save voice as:  knob 1 cursor, knob 2 upper/lower case, knob 3 letter,
 *                 knob 4 adds a number to the end of the name; encoder
 *                 turn picks the slot (starts on the next empty one),
 *                 push saves (asks before overwriting).
 * Load voice:     encoder or knob 3 scrolls the saved sounds for the
 *                 selected voice's type, push loads (glitch-free, see
 *                 voiceLoad.h).
 * Save/load kit:  the stock kit pages. Saving a kit while Morph is up
 *                 first asks whether to save the morphed sound.
 * Kit voice:      encoder or knob 1 scrolls the kit slots, push loads the
 *                 selected voice's sound from that kit (glitch-free).
 * Revert voice:   asks first, then the voice goes back to the sound of its
 *                 last voice load/save, or else to how it is in the saved
 *                 kit. Revert kit reloads the current kit as last saved.
 */

#ifndef INC_MENU_LSMENU_H_
#define INC_MENU_LSMENU_H_

#include <stdint.h>

/* ---- called by the firmware -------------------------------------------- */

/* Load/Save button pressed without Shift */
void lsMenu_onLoadSaveButton(void);

/* another mode button was pressed: forget the menu state */
void lsMenu_forceClose(void);

/* 1 while one of this menu's own screens is shown (it draws the display
 * and gets the encoder and knobs) */
uint8_t lsMenu_isActive(void);

/* 1 while the menu or a stock kit page opened from it is in use */
uint8_t lsMenu_isOpen(void);

void lsMenu_repaint(void);
void lsMenu_handleEncoder(int8_t inc, uint8_t clicked);

/* returns -1 if the knob isn't handled here (stock code continues),
 * otherwise 1 to request a repaint, 0 for none */
int8_t lsMenu_handleKnob(uint8_t potNr, int8_t diff);

/* main loop: message timeouts and the "loading" marker */
void lsMenu_tick(uint32_t nowMs);

/* a whole kit was loaded (slot 0..127): voices no longer carry a preset
 * name; when loaded from this menu's kit list, "Kit loaded" shows briefly */
void lsMenu_onKitLoaded(uint8_t slot);

/* Shift + Load/Save: undo one step (see above) */
void lsMenu_undo(void);

/* 1 while the undo message is shown; it's drawn over every other screen */
uint8_t lsMenu_isToast(void);

/* end the undo message now (knob, encoder or button input) */
void lsMenu_dismissToast(void);

/* ---- provided by lsMenu_glue.c (firmware) or a test ---------------------- */

/* show two lines of 16 characters; cursorCol < 0 hides the cursor */
void lsGlue_show(const char *row0, const char *row1, int8_t cursorRow, int8_t cursorCol);
void lsGlue_requestRepaint(void);

/* page and mode switching */
void lsGlue_enterOverviewPage(void);
void lsGlue_enterStockKitPage(uint8_t save, uint8_t morph);
void lsGlue_leaveMenu(void);			/* to the Perform page */
void lsGlue_stockScroll(int8_t inc);		/* scroll the stock kit list */
uint8_t lsGlue_isOnStockPage(void);		/* 1 while a stock kit load/save page is shown */

/* data from the rest of the firmware */
uint8_t lsGlue_activeVoice(void);			/* 0..6 (6 = open hihat) */
void    lsGlue_currentKitName(char *name8);	/* 8 characters, space padded */
void    lsGlue_kitName(uint8_t slot, char *name8);	/* name of a kit slot, same format */
uint8_t lsGlue_isMorphActive(void);
uint8_t* lsGlue_kitParameters(void);		/* the edit buffer */
const uint8_t* lsGlue_kitParamsOfSlot(uint8_t slot);	/* a saved kit (0..127) */
uint8_t lsGlue_currentKitSlot(void);
void    lsGlue_reloadCurrentKit(void);		/* load the current kit's slot again */
void    lsGlue_restoreParam(uint16_t idx, uint8_t value);	/* set + send one kit parameter */
void    lsGlue_paramName(uint16_t idx, char *row16);	/* "Osc     Coarse  ", 16 chars + 0 */
uint32_t lsGlue_nowMs(void);

/* name characters, in the order the knob scrolls through them */
uint8_t lsGlue_numNameChars(void);
char    lsGlue_nameChar(uint8_t index);

#endif /* INC_MENU_LSMENU_H_ */
