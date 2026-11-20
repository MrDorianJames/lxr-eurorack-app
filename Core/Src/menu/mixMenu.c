/*
 * mixMenu.c
 *
 * The mixer in the Perform menu. See mixMenu.h.
 */

#include "menu/mixMenu.h"
#include "menu/cfgMenu.h"
#include "globalMix.h"
#include <stdio.h>
#include <string.h>

typedef enum { ATTR_VOL = 0, ATTR_PAN, ATTR_SRC, NUM_ATTRS } MixAttr;

#define LIST_KNOB_STEPS		3	/* knob steps per list entry */
#define NUM_MIX_ITEMS		8	/* attr + 3 voices, twice */

static uint8_t	mix_open;
static uint8_t	mix_item;
static uint8_t	mix_editing;
static MixAttr	mix_attr;
static int8_t	mix_knobAcc[4];
static uint8_t	mix_changed;

static const char* const voiceLabels[GLOBALMIX_NUM_VOICES] = { "dr1","dr2","dr3","snr","cym","hat" };
static const char* const attrLabels[NUM_ATTRS] = { "vol","pan","src" };

/* ===================================================================== */
static void mix_row(char *row) { memset(row, ' ', 16); row[16] = 0; }
static void mix_put(char *row, uint8_t col, const char *text, uint8_t upper)
{
	for(uint8_t i = 0; text[i] && col + i < 16; i++)
	{
		char c = text[i];
		if(upper && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
		row[col + i] = c;
	}
}
static int8_t mix_steps(uint8_t pot, int8_t diff)
{
	mix_knobAcc[pot] = (int8_t)(mix_knobAcc[pot] + diff);
	int8_t n = 0;
	while(mix_knobAcc[pot] >=  LIST_KNOB_STEPS) { mix_knobAcc[pot] = (int8_t)(mix_knobAcc[pot] - LIST_KNOB_STEPS); n++; }
	while(mix_knobAcc[pot] <= -LIST_KNOB_STEPS) { mix_knobAcc[pot] = (int8_t)(mix_knobAcc[pot] + LIST_KNOB_STEPS); n--; }
	return n;
}
static uint8_t mix_clamp(int16_t v, int16_t lo, int16_t hi) { return (uint8_t)(v < lo ? lo : (v > hi ? hi : v)); }

/* ===================================================================== */
static void mix_setSrc(uint8_t v, uint8_t src)
{
	if(globalMix.src[v] == src) return;
	if(src == GLOBALMIX_SRC_GLOBAL)
	{
		/* start from what's playing now, so nothing jumps */
		globalMix.vol[v] = cfgGlue_kitValue(globalMix_volParam(v));
		globalMix.pan[v] = cfgGlue_kitValue(globalMix_panParam(v));
	}
	globalMix.src[v] = src;
	mix_changed = 1;
	cfgGlue_sendParameter(globalMix_volParam(v));
	cfgGlue_sendParameter(globalMix_panParam(v));
}
/* --------------------------------------------------------------------- */
/* amount: raw steps for VOL/PAN, list steps for SRC */
static void mix_change(uint8_t v, int8_t amount)
{
	if(v >= GLOBALMIX_NUM_VOICES || amount == 0) return;
	if(mix_attr == ATTR_SRC)
	{
		mix_setSrc(v, amount > 0 ? GLOBALMIX_SRC_GLOBAL : GLOBALMIX_SRC_KIT);
		return;
	}
	const uint16_t param = (mix_attr == ATTR_VOL) ? globalMix_volParam(v) : globalMix_panParam(v);
	if(globalMix.src[v] == GLOBALMIX_SRC_GLOBAL)
	{
		uint8_t *val = (mix_attr == ATTR_VOL) ? &globalMix.vol[v] : &globalMix.pan[v];
		*val = mix_clamp((int16_t)(*val + amount), 0, 127);
		mix_changed = 1;
	}
	else
	{
		cfgGlue_setKitValue(param, mix_clamp((int16_t)(cfgGlue_kitValue(param) + amount), 0, 127));
	}
	cfgGlue_sendParameter(param);
}
static void mix_changeAttr(int8_t steps)
{
	mix_attr = (MixAttr)mix_clamp((int16_t)(mix_attr + steps), 0, NUM_ATTRS - 1);
}

/* ===================================================================== */
void mixMenu_open(void)
{
	mix_open = 1;
	mix_item = 0;
	mix_editing = 0;
	mix_attr = ATTR_VOL;
	memset(mix_knobAcc, 0, sizeof(mix_knobAcc));
	cfgGlue_requestRepaint();
}
void mixMenu_close(void)			{ mix_open = 0; mix_editing = 0; }
uint8_t mixMenu_isActive(void)		{ return mix_open && mixGlue_isPerfPage(); }
uint8_t mixMenu_takeChanged(void)	{ const uint8_t c = mix_changed; mix_changed = 0; return c; }
/* --------------------------------------------------------------------- */
void mixMenu_handleEncoder(int8_t inc, uint8_t clicked)
{
	if(clicked) mix_editing = !mix_editing;
	else if(mix_editing)
	{
		if(mix_item % 4 == 0) mix_changeAttr(inc);
		else mix_change((uint8_t)((mix_item / 4) * 3 + (mix_item % 4) - 1), inc);
	}
	else mix_item = mix_clamp((int16_t)(mix_item + inc), 0, NUM_MIX_ITEMS - 1);
	cfgGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
uint8_t mixMenu_handleKnob(uint8_t potNr, int8_t diff)
{
	if(potNr > 3 || diff == 0) return 0;
	const uint8_t page = mix_item / 4;
	mix_item = (uint8_t)(page * 4 + potNr);
	if(potNr == 0) mix_changeAttr(mix_steps(0, diff));
	else if(mix_attr == ATTR_SRC) mix_change((uint8_t)(page * 3 + potNr - 1), mix_steps(potNr, diff));
	else mix_change((uint8_t)(page * 3 + potNr - 1), diff);
	cfgGlue_requestRepaint();
	return 1;
}
/* --------------------------------------------------------------------- */
void mixMenu_repaint(void)
{
	char r0[17], r1[17], tmp[8];
	const uint8_t page = mix_item / 4;
	mix_row(r0); mix_row(r1);

	mix_put(r0, 0, attrLabels[mix_attr], mix_item % 4 == 0);
	for(uint8_t c = 1; c < 4; c++)
	{
		const uint8_t v = (uint8_t)(page * 3 + c - 1);
		mix_put(r0, (uint8_t)(c * 4), voiceLabels[v], (uint8_t)(page * 4 + c) == mix_item);
		const uint8_t glo = (globalMix.src[v] == GLOBALMIX_SRC_GLOBAL);
		if(mix_attr == ATTR_SRC)
		{
			mix_put(r1, (uint8_t)(c * 4), glo ? "glo" : "kit", 0);
		}
		else if(mix_attr == ATTR_VOL)
		{
			snprintf(tmp, sizeof(tmp), "%3u", (unsigned)(glo ? globalMix.vol[v] : cfgGlue_kitValue(globalMix_volParam(v))));
			mix_put(r1, (uint8_t)(c * 4), tmp, 0);
		}
		else
		{
			const int p = (int)(glo ? globalMix.pan[v] : cfgGlue_kitValue(globalMix_panParam(v))) - 63;
			if(p == 0) snprintf(tmp, sizeof(tmp), "  0");
			else snprintf(tmp, sizeof(tmp), "%+3d", p);
			mix_put(r1, (uint8_t)(c * 4), tmp, 0);
		}
	}
	r0[15] = page ? '<' : '>';
	cfgGlue_show(r0, r1, mix_editing ? 1 : 0, mix_editing ? (int8_t)((mix_item % 4) * 4) : -1);
}
