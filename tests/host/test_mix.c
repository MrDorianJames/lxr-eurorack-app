/* Host test: the mixer in the Perform menu (mixMenu.c) with global mix (globalMix.c). */
#include <stdio.h>
#include <string.h>
#include "Parameters.h"
#include "globalMix.h"
#include "menu/cfgMenu.h"
#include "menu/mixMenu.h"

static int fails = 0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

/* ---- simulated firmware --------------------------------------------- */
static uint8_t kit[512];					/* kit edit buffer */
static uint8_t engine[512];					/* what the sound engine got */
static uint8_t screensaver = 1, cvGlobal = 0, onConfig = 1;
static int sends;
/* the firmware hook in midiParser_ccHandler, simulated */
void preset_sendParameter(uint16_t idx) { engine[idx] = globalMix_filter(idx, kit[idx]); sends++; }

static char shown[2][17]; static int curCol = -1;
void cfgGlue_show(const char *a, const char *b, int8_t row, int8_t col){ (void)row; memcpy(shown[0],a,16); memcpy(shown[1],b,16); shown[0][16]=shown[1][16]=0; curCol=col; }
void cfgGlue_requestRepaint(void){ if(mixMenu_isActive()) mixMenu_repaint(); }
uint8_t cfgGlue_isConfigPage(void){ return 0; }
static uint8_t onPerf = 1;
uint8_t mixGlue_isPerfPage(void){ return onPerf; }
uint8_t cfgGlue_kitValue(uint16_t i){ return kit[i]; }
void cfgGlue_setKitValue(uint16_t i, uint8_t v){ kit[i] = v; }
void cfgGlue_sendParameter(uint16_t i){ preset_sendParameter(i); }
uint8_t cfgGlue_getScreensaver(void){ return screensaver; }
void cfgGlue_setScreensaver(uint8_t on){ screensaver = on; }
uint8_t cfgGlue_getCvGlobal(void){ return cvGlobal; }
void cfgGlue_setCvGlobal(uint8_t g){ cvGlobal = g; }
const char* cfgGlue_stockVersion(void){ return "1.51"; }

static void turn(int8_t n){ mixMenu_handleEncoder(n, 0); }
static void push(void){ mixMenu_handleEncoder(0, 1); }
static void knob(uint8_t p, int n){ while(n){ int8_t d = n > 0 ? 1 : -1; mixMenu_handleKnob(p, d); n -= d; } }
static void loadKit(void){ for(int i = 0; i < 512; i++) preset_sendParameter((uint16_t)i); }  /* a kit load sends everything */
#define SCREEN(a,b) CHECK(!strcmp(shown[0],a) && !strcmp(shown[1],b), "screen\n   [%s]\n   [%s]\n expected\n   [%s]\n   [%s]", shown[0], shown[1], a, b)

int main(void)
{
	globalMix_setDefaults();
	for(int v = 0; v < 6; v++){ kit[PAR_AUDIO_OUT1+v] = 0; kit[PAR_VOL1+v] = (uint8_t)(90+v); kit[globalMix_panParam((uint8_t)v)] = 63; }
	kit[PAR_AUDIO_OUT1+3] = GLOBALMIX_OUT_FX;   /* the snare is sent through the FX */
	kit[PAR_FX_ROUTING] = 1;                    /* FX bus returns on St2 */
	loadKit();

	globalMix_setDefaults();
	for(int v = 0; v < 6; v++){ kit[PAR_AUDIO_OUT1+v] = 0; kit[PAR_VOL1+v] = (uint8_t)(90+v); kit[globalMix_panParam((uint8_t)v)] = 63; }
	loadKit();

		/* 1. opening the mixer */
	CHECK(!mixMenu_isActive(), "closed at start");
	mixMenu_open();
	CHECK(mixMenu_isActive(), "open on the Perform page");
	SCREEN("VOL dr1 dr2 dr3>", "     90  91  92 ");
	knob(1, 5); SCREEN("vol DR1 dr2 dr3>", "     95  91  92 ");
	CHECK(kit[PAR_VOL1] == 95 && engine[PAR_VOL1] == 95, "kit volume edited");
	knob(0, 3); SCREEN("PAN dr1 dr2 dr3>", "      0   0   0 ");
	knob(2, -5); SCREEN("pan dr1 DR2 dr3>", "      0  -5   0 ");
	knob(0, 3); SCREEN("SRC dr1 dr2 dr3>", "    kit kit kit ");

	/* switching to glo starts from the current values, nothing jumps */
	knob(1, 3); SCREEN("src DR1 dr2 dr3>", "    glo kit kit ");
	CHECK(globalMix.vol[0] == 95 && engine[PAR_VOL1] == 95, "glo starts at the kit value");
	knob(0, -6); knob(1, -15);
	SCREEN("vol DR1 dr2 dr3>", "     80  91  92 ");
	CHECK(engine[PAR_VOL1] == 80 && kit[PAR_VOL1] == 95, "glo edit changes the engine, not the kit");

	/* kit loads and edits on the Mix page no longer change a glo voice */
	kit[PAR_VOL1] = 20; kit[PAR_PAN1] = 0; loadKit();
	CHECK(engine[PAR_VOL1] == 80 && engine[PAR_PAN1] == 63, "glo vol/pan kept on kit load");
	CHECK(engine[PAR_VOL2] == 91, "kit voice follows the kit");

	/* second screen: same attribute, voices 4-6 */
	turn(7); SCREEN("vol snr cym HAT<", "     93  94  95 ");
	knob(1, 1000); CHECK(kit[PAR_VOL4] == 127, "volume capped at 127");
	knob(1, -1000); CHECK(kit[PAR_VOL4] == 0, "volume stops at 0");
	turn(-1); push(); turn(2); push();     /* to the attribute column, encoder edit VOL -> SRC */
	SCREEN("SRC snr cym hat<", "    kit kit kit ");

	/* 2. glo edits are flagged for saving; kit edits are not (they belong to the kit) */
	mixMenu_takeChanged();
	turn(-7); knob(0, -6);    /* back to page 1, attribute VOL */
	SCREEN("VOL dr1 dr2 dr3>", "     80  91  92 ");
	knob(1, -1);              /* dr1 is glo: global change */
	CHECK(mixMenu_takeChanged() == 1, "glo edit needs saving");
	CHECK(mixMenu_takeChanged() == 0, "flag cleared after taking it");
	knob(2, 1);               /* dr2 is kit: kit edit */
	CHECK(mixMenu_takeChanged() == 0, "kit edit doesn't touch global settings");
	knob(0, 6); knob(3, 3);   /* SRC of dr3 -> glo */
	CHECK(mixMenu_takeChanged() == 1, "SRC change needs saving");

	/* 3. only active while the Perform page is shown; close resets it */
	onPerf = 0; CHECK(!mixMenu_isActive(), "inactive on other pages");
	onPerf = 1; CHECK(mixMenu_isActive(), "active again on the Perform page");
	mixMenu_close(); CHECK(!mixMenu_isActive(), "closed");
	mixMenu_open(); SCREEN("VOL dr1 dr2 dr3>", "     79  92  92 ");   /* reopens on VOL; dr3 glo started from its kit level */

	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
