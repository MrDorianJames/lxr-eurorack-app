/* Host test: config menu (cfgMenu.c) and global routing/mix (globalMix.c). The mixer has its own test (test_mix.c). */
#include <stdio.h>
#include <string.h>
#include "Parameters.h"
#include "globalMix.h"
#include "menu/cfgMenu.h"

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
void cfgGlue_requestRepaint(void){ cfgMenu_repaint(); }
uint8_t cfgGlue_isConfigPage(void){ return onConfig; }
uint8_t cfgGlue_kitValue(uint16_t i){ return kit[i]; }
void cfgGlue_setKitValue(uint16_t i, uint8_t v){ kit[i] = v; }
void cfgGlue_sendParameter(uint16_t i){ preset_sendParameter(i); }
uint8_t cfgGlue_getScreensaver(void){ return screensaver; }
void cfgGlue_setScreensaver(uint8_t on){ screensaver = on; }
uint8_t cfgGlue_getCvGlobal(void){ return cvGlobal; }
void cfgGlue_setCvGlobal(uint8_t g){ cvGlobal = g; }
const char* cfgGlue_stockVersion(void){ return "1.51"; }

static void turn(int8_t n){ cfgMenu_handleEncoder(n, 0); }
static void push(void){ cfgMenu_handleEncoder(0, 1); }
static void knob(uint8_t p, int n){ while(n){ int8_t d = n > 0 ? 1 : -1; cfgMenu_handleKnob(p, d); n -= d; } }
static void loadKit(void){ for(int i = 0; i < 512; i++) preset_sendParameter((uint16_t)i); }  /* a kit load sends everything */
#define SCREEN(a,b) CHECK(!strcmp(shown[0],a) && !strcmp(shown[1],b), "screen\n   [%s]\n   [%s]\n expected\n   [%s]\n   [%s]", shown[0], shown[1], a, b)

int main(void)
{
	globalMix_setDefaults();
	for(int v = 0; v < 6; v++){ kit[PAR_AUDIO_OUT1+v] = 0; kit[PAR_VOL1+v] = (uint8_t)(90+v); kit[globalMix_panParam((uint8_t)v)] = 63; }
	kit[PAR_AUDIO_OUT1+3] = GLOBALMIX_OUT_FX;   /* the snare is sent through the FX */
	kit[PAR_FX_ROUTING] = 1;                    /* FX bus returns on St2 */
	loadKit();

	/* 1. defaults behave exactly like stock */
	for(int i = 0; i < 512; i++) CHECK(engine[i] == kit[i], "default changes param %d", i);

	/* 2. overview */
	cfgMenu_onEnter(); cfgMenu_repaint();
	SCREEN("SCR cv  rtg nfo ", " on kit  \xF5   \xF5  ");
	turn(1); SCREEN("scr CV  rtg nfo ", " on kit  \xF5   \xF5  ");
	push();  SCREEN("scr CV  rtg nfo ", " on glo  \xF5   \xF5  ");
	CHECK(cvGlobal == 1, "cv toggled");
	knob(0, -1); SCREEN("SCR cv  rtg nfo ", "off glo  \xF5   \xF5  ");
	knob(0, 1); knob(1, -1); CHECK(screensaver == 1 && cvGlobal == 0, "knobs set scr/cv");

	/* 3. routing page */
	knob(2, 1);
	SCREEN("DR1 dr2 dr3 snr>", "kt  kt  kt  kt  ");
	knob(1, 3*3);   /* dr2: kt -> St1 -> St2 -> L1 */
	SCREEN("dr1 DR2 dr3 snr>", "kt  L1  kt  kt  ");
	CHECK(engine[PAR_AUDIO_OUT2] == 2, "dr2 now on L1 (engine %d)", engine[PAR_AUDIO_OUT2]);
	CHECK(engine[PAR_AUDIO_OUT4] == GLOBALMIX_OUT_FX, "snr on kt follows its kit, which says FX");
	knob(3, 3);     /* snr fixed to St1, while its kit says FX */
	CHECK(engine[PAR_AUDIO_OUT4] == 0, "fixed output wins over the kit's FX (engine %d)", engine[PAR_AUDIO_OUT4]);
	turn(10);       /* to the FX bus item on the second screen */
	SCREEN("cym hat FX     <", "kt  kt  kt      ");
	push(); turn(5); push();   /* encoder edit: kt -> L2 (kt St1 St2 L1 R1 L2) */
	SCREEN("cym hat FX     <", "kt  kt  L2      ");
	CHECK(engine[PAR_FX_ROUTING] == 4, "FX bus on L2 (engine %d)", engine[PAR_FX_ROUTING]);
	knob(0, -30); CHECK(globalMix.route[4] == 0, "cym stops at kt");
	knob(2, 300); CHECK(globalMix.route[6] == 6, "fx bus stops at R2 (no FX option for itself)");
	knob(1, 300); SCREEN("cym HAT fx     <", "kt  FX  R2      ");
	CHECK(engine[PAR_AUDIO_OUT6] == GLOBALMIX_OUT_FX, "hat fixed to FX goes to the FX bus (engine %d)", engine[PAR_AUDIO_OUT6]);
	kit[PAR_AUDIO_OUT6] = 0; preset_sendParameter(PAR_AUDIO_OUT6);
	CHECK(engine[PAR_AUDIO_OUT6] == GLOBALMIX_OUT_FX, "fixed FX holds when the kit says St1");
	knob(1, -300); CHECK(globalMix.route[5] == 0, "hat back to kt");
	knob(2, -3);  /* back to L2 */

	/* kit loads no longer move fixed outputs */
	kit[PAR_AUDIO_OUT2] = 5; kit[PAR_AUDIO_OUT1] = 5; kit[PAR_FX_ROUTING] = 0; loadKit();
	CHECK(engine[PAR_AUDIO_OUT2] == 2, "fixed dr2 kept on kit load");
	CHECK(engine[PAR_AUDIO_OUT1] == 5, "kt dr1 follows the kit");
	CHECK(engine[PAR_FX_ROUTING] == 4, "fixed FX bus kept on kit load");

	/* 4. Edit steps back to the overview, then leaves */
	CHECK(cfgMenu_onEditButton() == 1, "edit handled in sub-page");
	SCREEN("scr cv  RTG nfo ", " on kit  \xF5   \xF5  ");
	CHECK(cfgMenu_onEditButton() == 0, "edit on the overview leaves (stock)");

	/* 4b. firmware info in the 4th column */
	turn(1); SCREEN("scr cv  rtg NFO ", " on kit  \xF5   \xF5  ");
	turn(5); SCREEN("scr cv  rtg NFO ", " on kit  \xF5   \xF5  ");   /* last column */
	push();  CHECK(!strncmp(shown[0], "custom fw r", 11) && !strncmp(shown[1], "stock V1.51", 11), "info screen [%s] [%s]", shown[0], shown[1]);
	knob(2, 5); turn(3); CHECK(!strncmp(shown[1], "stock V1.51", 11), "info screen ignores knobs and encoder");
	CHECK(cfgMenu_onEditButton() == 1, "edit steps back from info");
	SCREEN("scr cv  rtg NFO ", " on kit  \xF5   \xF5  ");
	knob(3, 1); CHECK(!strncmp(shown[0], "custom fw r", 11), "knob 4 opens info");
	cfgMenu_onEditButton();
	turn(-1); SCREEN("scr cv  RTG nfo ", " on kit  \xF5   \xF5  ");

	/* 6. settings survive a save and load, and stay stock-compatible */
	uint8_t stock[31], file[31 + GLOBALMIX_EXT_LEN];
	for(int i = 0; i < 31; i++) stock[i] = (uint8_t)(i * 7);
	memcpy(file, stock, 31); globalMix_serialize(&file[31]);
	CHECK(memcmp(file, stock, 31) == 0, "stock part unchanged (stock firmware reads only this)");
	GlobalMix saved = globalMix;
	globalMix_setDefaults();
	CHECK(globalMix_deserialize(&file[31], GLOBALMIX_EXT_LEN) == 1, "extension read back");
	CHECK(memcmp(&saved, &globalMix, sizeof saved) == 0, "settings identical after reload");

	/* a glo.cfg written by stock firmware has no extension: defaults */
	CHECK(globalMix_deserialize(&file[31], 0) == 0 && globalMix.route[1] == 0 && globalMix.src[0] == 0, "no extension -> stock behaviour");
	/* damaged extension: defaults, never out-of-range values */
	uint8_t bad[GLOBALMIX_EXT_LEN]; memcpy(bad, &file[31], sizeof bad); bad[5] = 99;
	CHECK(globalMix_deserialize(bad, sizeof bad) == 0 && globalMix.route[0] == 0, "bad route rejected");
	memcpy(bad, &file[31], sizeof bad); bad[5+6] = 7;
	CHECK(globalMix_deserialize(bad, sizeof bad) == 0, "FX bus routed to FX rejected");
	memcpy(bad, &file[31], sizeof bad); bad[5+0] = 7;
	CHECK(globalMix_deserialize(bad, sizeof bad) == 1 && globalMix.route[0] == 7, "voice routed to FX accepted");
	memcpy(bad, &file[31], sizeof bad); bad[0] = 'X';
	CHECK(globalMix_deserialize(bad, sizeof bad) == 0, "bad magic rejected");

	/* 6b. isOverridden drives the '*' marker on the voice pages */
	globalMix_setDefaults();
	CHECK(!globalMix_isOverridden(PAR_AUDIO_OUT1, 2) && !globalMix_isOverridden(PAR_VOL1, 90), "nothing marked by default");
	globalMix.route[0] = 3;
	CHECK(globalMix_isOverridden(PAR_AUDIO_OUT1, 0) && globalMix_isOverridden(PAR_AUDIO_OUT1, GLOBALMIX_OUT_FX), "fixed route marked (also over FX)");
	CHECK(!globalMix_isOverridden(PAR_AUDIO_OUT2, 0), "other voice not marked");
	globalMix.src[4] = GLOBALMIX_SRC_GLOBAL;
	CHECK(globalMix_isOverridden(PAR_VOL5, 10) && globalMix_isOverridden(PAR_PAN5, 10), "glo voice: volume and pan marked");
	CHECK(!globalMix_isOverridden(PAR_FILTER_FREQ_5, 10) && !globalMix_isOverridden(PAR_VOL4, 10), "sound and other voices not marked");
	globalMix.route[GLOBALMIX_ROUTE_FX_BUS] = 2;
	CHECK(globalMix_isOverridden(PAR_FX_ROUTING, 0), "fixed FX bus output marked on the FX page");
	CHECK(!globalMix_isOverridden(PAR_NONE, 0) && !globalMix_isOverridden(400, 0), "unknown parameters never marked");

	/* 7. applyAll re-sends every affected parameter */
	globalMix_deserialize(&file[31], GLOBALMIX_EXT_LEN); sends = 0; globalMix_applyAll();
	CHECK(sends == 7 + 12, "applyAll sent %d", sends);

	/* 8. not active outside the config page */
	onConfig = 0; CHECK(!cfgMenu_isActive() && cfgMenu_onEditButton() == 0, "inactive elsewhere");

	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
