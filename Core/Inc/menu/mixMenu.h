/*
 * mixMenu.h
 *
 * The mixer (global volume and pan), part of the Perform menu.
 *
 * Open it with knob 4 on Perform page 1 (under "mix"), or with
 * Shift + Perf from anywhere. Perf steps back to the Perform page.
 *
 *   VOL dr1 dr2 dr3 >   /   VOL snr cym hat <
 *       100  90  80             95 100  70
 *
 * Knob 1 picks VOL / PAN / SRC; knobs 2-4 edit the three voices on screen;
 * turn the encoder to reach the other three. SRC kit = level and pan from
 * the kit (stock), glo = fixed here, kept across kit loads. Or select an
 * item with the encoder, push to edit it with the encoder, push again.
 * Settings are saved with the global settings (glo.cfg).
 */

#ifndef INC_MENU_MIXMENU_H_
#define INC_MENU_MIXMENU_H_

#include <stdint.h>

void    mixMenu_open(void);
void    mixMenu_close(void);
uint8_t mixMenu_isActive(void);
void    mixMenu_repaint(void);
void    mixMenu_handleEncoder(int8_t inc, uint8_t clicked);
uint8_t mixMenu_handleKnob(uint8_t potNr, int8_t diff);	/* returns 1 to repaint */

/* 1 if global settings changed since the last call (they need saving) */
uint8_t mixMenu_takeChanged(void);

/* ---- provided by mixMenu_glue.c (firmware) or a test ---------------- */
uint8_t mixGlue_isPerfPage(void);
/* also uses cfgGlue_show, cfgGlue_requestRepaint, cfgGlue_kitValue,
 * cfgGlue_setKitValue and cfgGlue_sendParameter (cfgMenu.h) */

#endif /* INC_MENU_MIXMENU_H_ */
