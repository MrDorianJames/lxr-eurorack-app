/*
 * cfgMenu.c
 *
 * Config menu with routing and info pages. See cfgMenu.h.
 * Firmware access goes through cfgGlue_ (cfgMenu_glue.c), so this file
 * can be tested on a PC.
 */

#include "menu/cfgMenu.h"
#include "globalMix.h"
#include "customVersion.h"
#include <stdio.h>
#include <string.h>

typedef enum { CFG_OVERVIEW = 0, CFG_ROUTING, CFG_INFO } CfgScreen;

#define LIST_KNOB_STEPS		3	/* knob steps per list entry */
#define NUM_ROUTE_ITEMS		GLOBALMIX_ROUTES
#define NUM_CFG_COLUMNS		4	/* scr cv rtg nfo */
#define COL_ROUTING			2
#define COL_INFO			3

/* sub-page marker: built-in filled down triangle (WS0010 Western European
 * font table II, code 0xF5) */
#define SUBPAGE_MARK		" \xF5"

static CfgScreen cfg_screen;
static uint8_t	 cfg_column;			/* overview */
static uint8_t	 cfg_item;				/* routing 0..6 */
static uint8_t	 cfg_editing;			/* encoder edits the selected item */
static int8_t	 cfg_knobAcc[4];

static const char* const routeLabels[NUM_ROUTE_ITEMS] = { "dr1","dr2","dr3","snr","cym","hat","fx" };
static const char* const routeValues[GLOBALMIX_ROUTE_TO_FX + 1] = { "kt","St1","St2","L1","R1","L2","R2","FX" };

/* ===================================================================== */
static void cfg_row(char *row) { memset(row, ' ', 16); row[16] = 0; }
static void cfg_put(char *row, uint8_t col, const char *text, uint8_t upper)
{
	for(uint8_t i = 0; text[i] && col + i < 16; i++)
	{
		char c = text[i];
		if(upper && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
		row[col + i] = c;
	}
}
/* --------------------------------------------------------------------- */
static int8_t cfg_steps(uint8_t pot, int8_t diff)
{
	cfg_knobAcc[pot] = (int8_t)(cfg_knobAcc[pot] + diff);
	int8_t n = 0;
	while(cfg_knobAcc[pot] >=  LIST_KNOB_STEPS) { cfg_knobAcc[pot] = (int8_t)(cfg_knobAcc[pot] - LIST_KNOB_STEPS); n++; }
	while(cfg_knobAcc[pot] <= -LIST_KNOB_STEPS) { cfg_knobAcc[pot] = (int8_t)(cfg_knobAcc[pot] + LIST_KNOB_STEPS); n--; }
	return n;
}
static uint8_t cfg_clamp(int16_t v, int16_t lo, int16_t hi) { return (uint8_t)(v < lo ? lo : (v > hi ? hi : v)); }

/* ===================================================================== */
/* editing                                                                */
/* ===================================================================== */
static void cfg_changeRoute(uint8_t item, int8_t steps)
{
	if(item >= NUM_ROUTE_ITEMS || steps == 0) return;
	/* voices can also be fixed to the FX bus; the FX bus itself can't */
	const uint8_t maxRoute = (item == GLOBALMIX_ROUTE_FX_BUS) ? GLOBALMIX_NUM_OUTPUTS : GLOBALMIX_ROUTE_TO_FX;
	globalMix.route[item] = cfg_clamp((int16_t)(globalMix.route[item] + steps), 0, maxRoute);
	cfgGlue_sendParameter(globalMix_routeParam(item));
}
/* --------------------------------------------------------------------- */
static void cfg_open(CfgScreen s)
{
	cfg_screen = s;
	cfg_item = 0;
	cfg_editing = 0;
	memset(cfg_knobAcc, 0, sizeof(cfg_knobAcc));
}

/* ===================================================================== */
/* public                                                                 */
/* ===================================================================== */
void cfgMenu_onEnter(void)
{
	cfg_screen = CFG_OVERVIEW;
	cfg_column = 0;
	cfg_editing = 0;
}
/* --------------------------------------------------------------------- */
uint8_t cfgMenu_isActive(void)
{
	return cfgGlue_isConfigPage();
}
/* --------------------------------------------------------------------- */
uint8_t cfgMenu_onEditButton(void)
{
	if(!cfgGlue_isConfigPage() || cfg_screen == CFG_OVERVIEW) return 0;
	cfg_screen = CFG_OVERVIEW;
	cfg_editing = 0;
	cfgGlue_requestRepaint();
	return 1;
}
/* --------------------------------------------------------------------- */
void cfgMenu_handleEncoder(int8_t inc, uint8_t clicked)
{
	switch(cfg_screen)
	{
	case CFG_OVERVIEW:
		if(clicked)
		{
			if(cfg_column == 0) cfgGlue_setScreensaver(!cfgGlue_getScreensaver());
			else if(cfg_column == 1) cfgGlue_setCvGlobal(!cfgGlue_getCvGlobal());
			else if(cfg_column == COL_ROUTING) cfg_open(CFG_ROUTING);
			else cfg_open(CFG_INFO);
		}
		else cfg_column = cfg_clamp((int16_t)(cfg_column + inc), 0, NUM_CFG_COLUMNS - 1);
		break;

	case CFG_INFO:
		break;

	case CFG_ROUTING:
		if(clicked) cfg_editing = !cfg_editing;
		else if(cfg_editing) cfg_changeRoute(cfg_item, inc);
		else cfg_item = cfg_clamp((int16_t)(cfg_item + inc), 0, NUM_ROUTE_ITEMS - 1);
		break;

	}
	cfgGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
uint8_t cfgMenu_handleKnob(uint8_t potNr, int8_t diff)
{
	if(potNr > 3 || diff == 0) return 0;

	switch(cfg_screen)
	{
	case CFG_INFO:
		break;

	case CFG_OVERVIEW:
		if(potNr == 0)      { cfg_column = 0; cfgGlue_setScreensaver(diff > 0); }
		else if(potNr == 1) { cfg_column = 1; cfgGlue_setCvGlobal(diff > 0); }
		else                { cfg_column = potNr; cfg_open(potNr == COL_ROUTING ? CFG_ROUTING : CFG_INFO); }
		break;

	case CFG_ROUTING:
	{
		const uint8_t item = (uint8_t)((cfg_item / 4) * 4 + potNr);
		const int8_t steps = cfg_steps(potNr, diff);
		if(item < NUM_ROUTE_ITEMS)
		{
			cfg_item = item;
			cfg_changeRoute(item, steps);
		}
		break;
	}

	}
	cfgGlue_requestRepaint();
	return 1;
}
/* --------------------------------------------------------------------- */
void cfgMenu_repaint(void)
{
	char r0[17], r1[17];
	int8_t cursorCol = -1;
	cfg_row(r0); cfg_row(r1);

	switch(cfg_screen)
	{
	case CFG_OVERVIEW:
	{
		static const char* const labels[NUM_CFG_COLUMNS] = { "scr","cv","rtg","nfo" };
		for(uint8_t c = 0; c < NUM_CFG_COLUMNS; c++) cfg_put(r0, (uint8_t)(c * 4), labels[c], c == cfg_column);
		cfg_put(r1, 0, cfgGlue_getScreensaver() ? " on" : "off", 0);
		cfg_put(r1, 4, cfgGlue_getCvGlobal() ? "glo" : "kit", 0);
		cfg_put(r1, 8, SUBPAGE_MARK, 0);
		cfg_put(r1, 12, SUBPAGE_MARK, 0);
		break;
	}

	case CFG_INFO:
	{
		char line[24];
		cfg_put(r0, 0, CUSTOM_FW_REVISION, 0);
		snprintf(line, sizeof(line), "stock V%s", cfgGlue_stockVersion());
		cfg_put(r1, 0, line, 0);
		break;
	}

	case CFG_ROUTING:
	{
		const uint8_t page = cfg_item / 4;
		for(uint8_t c = 0; c < 4; c++)
		{
			const uint8_t item = (uint8_t)(page * 4 + c);
			if(item >= NUM_ROUTE_ITEMS) break;
			cfg_put(r0, (uint8_t)(c * 4), routeLabels[item], item == cfg_item);
			cfg_put(r1, (uint8_t)(c * 4), routeValues[globalMix.route[item]], 0);
		}
		r0[15] = page ? '<' : '>';
		if(cfg_editing) cursorCol = (int8_t)((cfg_item % 4) * 4);
		break;
	}

	}
	cfgGlue_show(r0, r1, cfg_editing ? 1 : 0, cursorCol);
}
