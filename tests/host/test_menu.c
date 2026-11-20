/* Host test: the Load/Save menu (lsMenu.c) driven like a user would, with
 * the real voice storage (simulated SD card) and voice loading code. */
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "Parameters.h"
#include "menu/menu.h"
#include "menu/lsMenu.h"
#include "Preset/voicePreset.h"
#include "Preset/voiceStore.h"
#include "Preset/voiceLoad.h"
#include "Preset/undo.h"

static int fails = 0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

/* ---- firmware state the menu works with -------------------------------- */
FATFS SDFatFS; char SDPath[4] = "";
uint8_t activeParameters[NUM_PARAMS];
uint8_t mainParameters[NUM_KIT_SLOTS][END_OF_SOUND_PARAMETERS];
static uint8_t activeVoice = 0, morphActive = 0;
static uint32_t now = 1000;
static char kitName[9] = "DRUMATIX";

/* ---- simulated engine for voiceLoad -------------------------------------- */
static uint8_t quiet = 1; static int paramsSent = 0;
uint8_t voiceLoad_isVoiceQuiet(uint8_t v){ (void)v; return quiet; }
void preset_sendParameter(uint16_t idx){ (void)idx; paramsSent++; }

/* ---- glue: record what the menu asks the firmware to do ------------------ */
static char shown[2][17]; static int curRow = -1, curCol = -1;
static int overviewEntered, stockEntered, stockSave = -1, stockMorph = -1, leftMenu, stockScrolled;
static uint8_t onStockPage;
void lsGlue_show(const char *r0, const char *r1, int8_t row, int8_t col)
{ memcpy(shown[0], r0, 16); memcpy(shown[1], r1, 16); shown[0][16] = shown[1][16] = 0; curRow = row; curCol = col; }
void lsGlue_requestRepaint(void){ lsMenu_repaint(); }
void lsGlue_enterOverviewPage(void){ overviewEntered++; onStockPage = 0; }
void lsGlue_enterStockKitPage(uint8_t save, uint8_t morph){ stockEntered++; stockSave = save; stockMorph = morph; onStockPage = 1; }
void lsGlue_leaveMenu(void){ leftMenu++; onStockPage = 0; }
void lsGlue_stockScroll(int8_t inc){ stockScrolled += inc; }
uint8_t lsGlue_isOnStockPage(void){ return onStockPage; }
uint8_t lsGlue_activeVoice(void){ return activeVoice; }
void lsGlue_currentKitName(char *n){ memcpy(n, kitName, 8); }
void lsGlue_kitName(uint8_t slot, char *n){ (void)slot; memcpy(n, "TECHNO1 ", 8); }
uint8_t lsGlue_isMorphActive(void){ return morphActive; }
uint8_t* lsGlue_kitParameters(void){ return activeParameters; }
uint32_t lsGlue_nowMs(void){ return now; }
static const char nameChars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
uint8_t lsGlue_numNameChars(void){ return (uint8_t)(sizeof(nameChars) - 1); }
char lsGlue_nameChar(uint8_t i){ return nameChars[i]; }
static uint8_t curKit = 5; static int reloads, restored;
const uint8_t* lsGlue_kitParamsOfSlot(uint8_t slot){ return mainParameters[slot]; }
uint8_t lsGlue_currentKitSlot(void){ return curKit; }
void lsGlue_reloadCurrentKit(void){ memcpy(activeParameters, mainParameters[curKit], END_OF_SOUND_PARAMETERS); reloads++; }
void lsGlue_restoreParam(uint16_t idx, uint8_t v){ activeParameters[idx] = v; restored++; }
void lsGlue_paramName(uint16_t idx, char *row){ (void)idx; memcpy(row, "Osc     Coarse  ", 17); }
static int soundIs(const uint8_t *kit, uint8_t voice)
{ uint8_t a[VOICE_PARAMS_MAX], b[VOICE_PARAMS_MAX]; voicePreset_extract(activeParameters, voice, a); voicePreset_extract(kit, voice, b); return memcmp(a, b, voicePreset_numParams(voice)) == 0; }

/* ---- user actions --------------------------------------------------------- */
static void pressLoadSave(void){ lsMenu_onLoadSaveButton(); }
static void turn(int8_t n){ lsMenu_handleEncoder(n, 0); }
static void push(void){ lsMenu_handleEncoder(0, 1); }
static int  knob(uint8_t pot, int steps){ int r = 0; while(steps){ int8_t d = steps > 0 ? 1 : -1; r = lsMenu_handleKnob(pot, d); steps -= d; } return r; }
/* same order as the firmware's main loop: loader first, then the menu */
static void wait(uint32_t ms){ now += ms; voiceLoad_tick(now); lsMenu_tick(now); }
static int  screenIs(const char *a, const char *b){ return strcmp(shown[0], a) == 0 && strcmp(shown[1], b) == 0; }
#define SCREEN(a,b) CHECK(screenIs(a,b), "screen is\n   [%s]\n   [%s]\n expected\n   [%s]\n   [%s]", shown[0], shown[1], a, b)

int main(void)
{
	static BYTE work[4096];
	f_mkfs("", FM_ANY | FM_SFD, 0, work, sizeof work); f_mount(&SDFatFS, SDPath, 1);
	voiceStore_init();
	for(int p = 0; p < NUM_PARAMS; p++) activeParameters[p] = (uint8_t)(p & 127);
	for(int v = 0; v < 6; v++){ activeParameters[PAR_VEL_DEST_1+v] = 0; activeParameters[PAR_VOICE_LFO1+v] = (uint8_t)(v+1); activeParameters[PAR_TARGET_LFO1+v] = 0; activeParameters[PAR_RETRIGGER_LFO1+v] = 0; }

	/* 1. one press opens the overview with save voice selected */
	CHECK(!lsMenu_isOpen(), "closed at start");
	pressLoadSave();
	CHECK(lsMenu_isActive() && overviewEntered == 1, "overview open");
	SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");
	turn(1);  SCREEN("sv  SV  ld  ld >", "vc  KIT vc  kit ");
	turn(2);  SCREEN("sv  sv  ld  LD >", "vc  kit vc  KIT ");
	turn(1);  SCREEN("LD  rv  rv     <", "KVC vc  kit     ");   /* page 2 */
	turn(5);  SCREEN("ld  rv  RV     <", "kvc vc  KIT     ");   /* stops at the last column */
	turn(-9); SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");

	/* 2. push opens save voice as: kit name, next empty slot, cursor on the name */
	push();
	SCREEN("Save Vc as  Drm1", "  0 DRUMATIX  ok");
	CHECK(curRow == 1 && curCol == 4, "cursor on first letter (row %d col %d)", curRow, curCol);

	/* 3. knob 4 adds the next unused number, push saves */
	knob(3, 3);
	SCREEN("Save Vc as  Drm1", "  0 DRUMATI1  ok");
	push();
	SCREEN("Save Vc as  Drm1", "Saved to 0      ");   /* title still shows the voice */
	CHECK(voiceStore_isUsed(VOICE_TYPE_DRUM, 0) && memcmp(voiceStore_name(VOICE_TYPE_DRUM, 0), "DRUMATI1", 8) == 0, "saved as DRUMATI1 in slot 0");
	{ uint8_t snd[VOICE_PARAMS_MAX]; voicePreset_extract(activeParameters, 0, snd);
	  CHECK(memcmp(voiceStore_params(VOICE_TYPE_DRUM, 0), snd, VOICE_PARAMS_DRUM) == 0, "saved sound is voice 1's sound"); }
	wait(1300);
	SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");    /* back to the overview */

	/* 4. quickest path: press, push, knob 4, push -> next name and next empty slot */
	push(); SCREEN("Save Vc as  Drm1", "  1 DRUMATI1  ok");   /* name now comes from the saved preset */
	knob(3, 3); SCREEN("Save Vc as  Drm1", "  1 DRUMATI2  ok");
	push(); wait(1300);
	CHECK(memcmp(voiceStore_name(VOICE_TYPE_DRUM, 1), "DRUMATI2", 8) == 0, "saved DRUMATI2 in slot 1");

	/* 5. name editing with knobs 1-3 */
	push(); SCREEN("Save Vc as  Drm1", "  2 DRUMATI2  ok");
	knob(0, 6);  CHECK(curCol == 5, "cursor moved one letter (col %d)", curCol);
	knob(1, 1);  SCREEN("Save Vc as  Drm1", "  2 DrUMATI2  ok");   /* lower case */
	knob(1, -1); SCREEN("Save Vc as  Drm1", "  2 DRUMATI2  ok");   /* upper case */
	knob(2, 1);  SCREEN("Save Vc as  Drm1", "  2 DSUMATI2  ok");   /* next letter */
	knob(0, -60); CHECK(curCol == 4, "cursor stops at the first letter");
	knob(0, 600); CHECK(curCol == 11, "cursor stops at the last letter");

	/* 6. overwriting asks first, No is the default */
	turn(-2); SCREEN("Save Vc as  Drm1", "  0 DSUMATI2 *ok");   /* '*' = slot in use */
	push();   SCREEN("Replace DRUMATI1", "          [y/N] ");
	push();   SCREEN("Save Vc as  Drm1", "  0 DSUMATI2 *ok");   /* No: nothing saved */
	CHECK(memcmp(voiceStore_name(VOICE_TYPE_DRUM, 0), "DRUMATI1", 8) == 0, "No kept the old sound");
	push(); turn(1); SCREEN("Replace DRUMATI1", "          [Y/n] ");
	push(); CHECK(memcmp(voiceStore_name(VOICE_TYPE_DRUM, 0), "DSUMATI2", 8) == 0, "Yes replaced it");
	wait(1300);

	/* 7. load voice: knob 3 opens it, encoder scrolls, push loads */
	CHECK(knob(2, 1) == 1, "knob 3 handled");
	SCREEN("Load Voice  Drm1", "  0 DSUMATI2    ");
	turn(1); SCREEN("Load Voice  Drm1", "  1 DRUMATI2    ");
	turn(5); SCREEN("Load Voice  Drm1", "  1 DRUMATI2    ");   /* stops at the last sound */
	knob(2, -1); SCREEN("Load Voice  Drm1", "  0 DSUMATI2    ");
	quiet = 0; push();
	SCREEN("Load Voice  Drm1", "  0 DSUMATI2   *");   /* waiting for the voice to go quiet */
	CHECK(voiceLoad_isPending(0) && paramsSent == 0, "staged, not applied while the voice sounds");
	quiet = 1; wait(1);
	SCREEN("Voice loaded    ", "  0 DSUMATI2    ");   /* confirmed once applied */
	CHECK(!voiceLoad_isPending(0) && paramsSent == voicePreset_numParams(0), "applied once quiet");
	wait(500); SCREEN("Voice loaded    ", "  0 DSUMATI2    ");
	wait(600); CHECK(!lsMenu_isOpen() && leftMenu == 1, "then to the Perform page");
	pressLoadSave(); knob(2, 1); SCREEN("Load Voice  Drm1", "  0 DSUMATI2    ");   /* list remembers the slot */

	/* 8. the list follows the selected voice */
	activeVoice = 3; lsMenu_repaint(); SCREEN("Load Voice  Snr ", " no sounds saved");
	activeVoice = 6; lsMenu_repaint(); SCREEN("Load Voice  Hat ", " no sounds saved");   /* open hat = hat */
	activeVoice = 2; lsMenu_repaint(); SCREEN("Load Voice  Drm3", "  0 DSUMATI2    ");   /* drums share sounds */
	activeVoice = 0;

	/* 9. a loaded voice's name becomes the save name */
	knob(0, 1); SCREEN("Save Vc as  Drm1", "  2 DSUMATI2  ok");

	/* 10. Load/Save steps back one level, then leaves the menu */
	pressLoadSave(); SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");   /* back from save voice */
	pressLoadSave(); CHECK(!lsMenu_isOpen() && leftMenu == 2, "left the menu to the Perform page");

	/* 11. kit columns use the stock pages; knob 4 scrolls the stock kit list */
	pressLoadSave(); knob(3, 1);
	CHECK(stockEntered == 1 && stockSave == 0 && !lsMenu_isActive() && lsMenu_isOpen(), "stock kit loader open");
	CHECK(knob(3, 4) == 1 && stockScrolled == 4, "knob 4 scrolls the kit list");
	/* loading a kit from the list confirms it, then goes to the Perform page */
	lsMenu_onKitLoaded(12);
	SCREEN("Kit loaded      ", " 12 TECHNO1     ");
	CHECK(lsMenu_isActive(), "message shown over the kit list");
	wait(500); SCREEN("Kit loaded      ", " 12 TECHNO1     ");
	wait(600);
	CHECK(!lsMenu_isOpen() && leftMenu == 3, "to the Perform page after the message");
	pressLoadSave(); knob(3, 1);
	CHECK(stockEntered == 2 && lsMenu_isOpen(), "kit list reopened");
	CHECK(lsMenu_handleKnob(0, 1) == -1, "other knobs left to the stock page");
	pressLoadSave(); SCREEN("sv  sv  ld  LD >", "vc  kit vc  KIT ");   /* stepping back keeps your column */
	turn(-2); push();
	CHECK(stockEntered == 3 && stockSave == 1 && stockMorph == 0, "save kit opens the stock save page");
	CHECK(lsMenu_handleKnob(0, 1) == -1, "stock name editor keeps its knobs");
	pressLoadSave();

	/* 12. saving a kit with morph up asks first */
	morphActive = 1; knob(1, 1);
	SCREEN("Save kit with   ", "morph?    [y/N] ");
	turn(1); SCREEN("Save kit with   ", "morph?    [Y/n] ");
	push(); CHECK(stockEntered == 4 && stockSave == 1 && stockMorph == 1, "morphed kit save");
	pressLoadSave(); morphActive = 0;

	/* 13. loading a kit forgets voice names and drops staged sounds */
	knob(2, 1); quiet = 0; push(); CHECK(voiceLoad_isPending(0), "staged");
	lsMenu_onKitLoaded(4);
	CHECK(!voiceLoad_isPending(0), "kit load cancels a staged voice");
	CHECK(strncmp(shown[0], "Kit loaded", 10) != 0, "no message when the kit wasn't loaded from our kit list");
	knob(0, 1); SCREEN("Save Vc as  Drm1", "  2 DRUMATIX  ok");   /* back to the kit's name */
	quiet = 1; wait(10);
	CHECK(strncmp(shown[0], "Voice loaded", 12) != 0, "no 'Voice loaded' for a load the kit cancelled");

	/* 13b. after a kit save the firmware returns to the voice page by itself:
	 *      the next press must open a fresh overview on save voice */
	pressLoadSave(); pressLoadSave();                     /* leave whatever was open */
	pressLoadSave(); turn(1); push();                     /* open the stock kit save page */
	CHECK(stockSave == 1 && lsMenu_isOpen(), "kit save page open");
	onStockPage = 0;                                      /* "kit saved", back to the voice page */
	CHECK(lsMenu_handleKnob(0, 1) == -1, "knobs back to normal on the voice page");
	pressLoadSave(); SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");
	pressLoadSave();

	/* 14. opening from outside the menu always starts on save voice */
	pressLoadSave(); pressLoadSave(); CHECK(!lsMenu_isOpen(), "left (save screen -> overview -> out)");
	pressLoadSave(); SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");
	turn(3); pressLoadSave(); pressLoadSave(); SCREEN("SV  sv  ld  ld >", "VC  kit vc  kit ");

	/* 15. another mode button closes the menu */
	lsMenu_forceClose(); CHECK(!lsMenu_isOpen() && lsMenu_handleKnob(0, 1) == -1, "closed by another mode");

	/* 16. a card error goes back to the save screen */
	pressLoadSave(); push();
	extern int ramdisk_failWrites; ramdisk_failWrites = 1; push(); ramdisk_failWrites = 0;
	SCREEN("Save Vc as  Drm1", "SD card error   ");
	wait(2600); SCREEN("Save Vc as  Drm1", "  2 DRUMATIX  ok");
	CHECK(!voiceStore_isUsed(VOICE_TYPE_DRUM, 2), "nothing saved on error");
	push(); wait(1300); CHECK(voiceStore_isUsed(VOICE_TYPE_DRUM, 2), "retry after the error works");

	/* 17. page 2: the knob under a column opens it */
	lsMenu_forceClose();
	CHECK(!lsMenu_isOpen(), "closed before page 2 tests");
	pressLoadSave(); turn(4); SCREEN("LD  rv  rv     <", "KVC vc  kit     ");
	knob(1, 1); SCREEN("Revert Vc   Drm1", "          [y/N] ");
	pressLoadSave(); SCREEN("ld  RV  rv     <", "kvc VC  kit     ");
	CHECK(knob(3, 1) == 0, "knob 4 does nothing on page 2");

	/* 18. load voice from kit: browse the kit slots, push loads the voice */
	for(int p = 0; p < END_OF_SOUND_PARAMETERS; p++){ mainParameters[5][p] = activeParameters[p]; mainParameters[7][p] = activeParameters[p]; }
	for(uint8_t k = 0; k < voicePreset_numParams(0); k++){ const uint16_t i = voicePreset_paramIndex(0, k); if(i != PAR_TARGET_LFO1 && i != PAR_VEL_DEST_1 && i != PAR_VOICE_LFO1) mainParameters[7][i] = (uint8_t)(mainParameters[7][i] ^ 1); }
	CHECK(!soundIs(mainParameters[7], 0), "kit 7 has a different drum 1");
	knob(0, 1); SCREEN("Kit Voice   Drm1", "  5 TECHNO1     ");   /* starts on the current kit */
	knob(0, 2); SCREEN("Kit Voice   Drm1", "  7 TECHNO1     ");
	turn(-1); turn(1); SCREEN("Kit Voice   Drm1", "  7 TECHNO1     ");
	undo_record(voicePreset_paramIndex(0, 0), 9, now); undo_record(voicePreset_paramIndex(1, 0), 9, now);
	const uint8_t stepsBefore = undo_count();
	push(); wait(10);
	SCREEN("Voice loaded    ", "  7 TECHNO1     ");
	CHECK(soundIs(mainParameters[7], 0), "drum 1 has kit 7's sound");
	CHECK(soundIs(mainParameters[5], 1), "drum 2 untouched");
	CHECK(undo_count() == stepsBefore - 1, "voice load drops that voice's undo steps only");
	wait(1100); CHECK(!lsMenu_isOpen() && leftMenu > 0, "then to the Perform page");
	pressLoadSave(); knob(0, 1); SCREEN("Save Vc as  Drm1", "  3 TECHNO1   ok");   /* save suggests the kit name */
	pressLoadSave(); pressLoadSave();

	/* 19. revert voice: asks first, then back to the last loaded sound */
	activeParameters[voicePreset_paramIndex(0, 0)] ^= 0x10;
	CHECK(!soundIs(mainParameters[7], 0), "voice edited");
	pressLoadSave(); turn(5); SCREEN("ld  RV  rv     <", "kvc VC  kit     ");
	push(); SCREEN("Revert Vc   Drm1", "          [y/N] ");
	push(); SCREEN("ld  RV  rv     <", "kvc VC  kit     ");    /* no: nothing changes */
	CHECK(!soundIs(mainParameters[7], 0), "still edited after no");
	push(); turn(1); SCREEN("Revert Vc   Drm1", "          [Y/n] ");
	push(); SCREEN("Revert Vc   Drm1", "Voice reverted  ");
	wait(10); CHECK(soundIs(mainParameters[7], 0), "back to the sound loaded from kit 7");
	wait(1100); CHECK(!lsMenu_isOpen(), "then to the Perform page");

	/* 20. revert kit: reload the current kit; then revert voice uses the kit */
	activeParameters[voicePreset_paramIndex(1, 0)] ^= 0x10;
	pressLoadSave(); turn(6); SCREEN("ld  rv  RV     <", "kvc vc  KIT     ");
	push(); SCREEN("Revert kit      ", "TECHNO1   [y/N] ");
	turn(1); push();
	SCREEN("Kit reverted    ", "  5 TECHNO1     ");
	CHECK(reloads == 1 && memcmp(activeParameters, mainParameters[5], END_OF_SOUND_PARAMETERS) == 0, "kit reloaded");
	CHECK(undo_count() == 0, "kit revert clears the undo history");
	wait(1100); CHECK(!lsMenu_isOpen(), "then to the Perform page");
	activeParameters[voicePreset_paramIndex(0, 0)] ^= 0x10;
	pressLoadSave(); turn(5); push(); turn(1); push(); wait(10);
	CHECK(soundIs(mainParameters[5], 0), "without a voice load, revert goes to the saved kit");
	wait(1100);

	/* 21. undo: restores the old value and shows it briefly over any screen */
	{
		const uint16_t i0 = voicePreset_paramIndex(0, 0), i1 = voicePreset_paramIndex(1, 0);
		uint16_t kitParam = 1;
		while(voicePreset_ownerOfParam(kitParam) != VOICEPRESET_KIT_LEVEL) kitParam++;
		activeVoice = 0;
		lsMenu_undo(); SCREEN("Undo        Drm1", "nothing to undo ");
		CHECK(lsMenu_isToast() && lsMenu_isActive(), "message shown");
		wait(1300); CHECK(!lsMenu_isOpen() && !lsMenu_isToast(), "gone again, nothing else changed");

		activeParameters[i0] = 20; undo_record(i0, 11, now);
		activeParameters[i1] = 20; undo_record(i1, 12, now);
		now += 2000; undo_record(kitParam, 13, now);
		restored = 0;
		lsMenu_undo(); SCREEN("Undo        Kit ", "Osc     Coarse  ");
		CHECK(activeParameters[kitParam] == 13, "kit-wide step undone first");
		lsMenu_undo(); SCREEN("Undo        Drm1", "Osc     Coarse  ");
		CHECK(activeParameters[i0] == 11 && activeParameters[i1] == 20, "drum 1 step undone, drum 2's kept");
		lsMenu_undo(); SCREEN("Undo        Drm1", "nothing to undo ");
		lsMenu_dismissToast(); CHECK(!lsMenu_isOpen(), "dismissed");
		activeVoice = 1; lsMenu_undo(); SCREEN("Undo        Drm2", "Osc     Coarse  ");
		CHECK(activeParameters[i1] == 12 && restored == 3, "drum 2 step undone");
		wait(1300); activeVoice = 0;

		/* over the overview: back to the same column afterwards */
		pressLoadSave(); turn(2); undo_record(i0, 30, now); lsMenu_undo();
		SCREEN("Undo        Drm1", "Osc     Coarse  ");
		wait(1300); SCREEN("sv  sv  LD  ld >", "vc  kit VC  kit ");
		/* over a stock kit page: back to it */
		knob(3, 1); CHECK(onStockPage && !lsMenu_isActive(), "on the stock page");
		undo_record(i0, 31, now); lsMenu_undo(); CHECK(lsMenu_isActive(), "message over the stock page");
		lsMenu_dismissToast(); CHECK(!lsMenu_isActive() && lsMenu_isOpen(), "back on the stock page");
		/* the Load/Save button during the message steps back as usual */
		undo_record(i0, 32, now); lsMenu_undo(); pressLoadSave();
		SCREEN("sv  sv  ld  LD >", "vc  kit vc  KIT ");
		pressLoadSave();
		/* a load/save message isn't replaced */
		pressLoadSave(); knob(3, 1); lsMenu_onKitLoaded(3); lsMenu_undo();
		SCREEN("Kit loaded      ", "  3 TECHNO1     ");
		CHECK(undo_count() == 0, "kit load clears the undo history");
		wait(1100);
	}

	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
