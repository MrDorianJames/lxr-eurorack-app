/*
 * lsMenu.c
 *
 * The Load/Save menu. See lsMenu.h for how it's used.
 * Everything that touches the rest of the firmware goes through the
 * lsGlue_ functions (lsMenu_glue.c), so this file can be tested on a PC.
 */

#include "menu/lsMenu.h"
#include "Preset/voicePreset.h"
#include "Preset/voiceStore.h"
#include "Preset/voiceLoad.h"
#include "Preset/undo.h"
#include <stdio.h>
#include <string.h>

typedef enum
{
	LS_CLOSED = 0,
	LS_OVERVIEW,
	LS_LOAD_VOICE,
	LS_SAVE_VOICE,
	LS_CONFIRM_OVERWRITE,
	LS_MORPH_PROMPT,
	LS_MESSAGE,
	LS_STOCK,			/* a stock kit page opened from the overview */
	LS_KIT_VOICE,		/* load one voice out of a saved kit */
	LS_CONFIRM_REVERT_VOICE,
	LS_CONFIRM_REVERT_KIT,
	LS_RESTORE			/* message return target only: back to what was there before (undo) */
} LsState;

/* overview page 1: sv vc, sv kit, ld vc, ld kit
 * overview page 2: ld kvc (voice from kit), rv vc, rv kit */
enum { COL_SAVE_VOICE = 0, COL_SAVE_KIT, COL_LOAD_VOICE, COL_LOAD_KIT,
	   COL_KIT_VOICE, COL_REVERT_VOICE, COL_REVERT_KIT, NUM_COLS };
#define COLS_PER_PAGE		4
#define NUM_KIT_SLOTS_LS	128

#define CURSOR_KNOB_STEPS	6	/* knob steps per cursor position */
#define SUFFIX_KNOB_STEPS	3	/* knob steps per number */
#define MSG_SAVED_MS		1200
#define MSG_ERROR_MS		2500
#define MSG_KIT_LOADED_MS	1000
#define MSG_VOICE_LOADED_MS	1000
#define MSG_REVERTED_MS		1000
#define MSG_UNDO_MS			1200

static LsState	ls_state = LS_CLOSED;
static uint8_t	ls_column;

/* load voice: remembered position per voice type */
static uint8_t	ls_browseSlot[NUM_VOICE_TYPES];
static uint8_t	ls_shownPending;
static uint8_t	ls_loadWatch;			/* a voice load was requested: confirm when applied */
static uint8_t	ls_loadVoice;
static char		ls_loadText[17];		/* second line of the "Voice loaded" message */

/* load voice from kit: kit slot being browsed */
static uint8_t	ls_kitSlot;
static uint8_t	ls_kitSlotSet;			/* 0: start on the current kit */

/* revert voice: the sound each voice got from its last voice load or save */
static uint8_t	ls_snap[NUM_VOICES][VOICE_PARAMS_MAX];
static uint8_t	ls_snapValid[NUM_VOICES];

/* undo message: the state to go back to */
static LsState	ls_toastPrev;

/* save voice as */
static uint8_t	ls_saveVoice;			/* 0..5, fixed while the save screen is open */
static uint8_t	ls_saveSlot;
static char		ls_baseName[VOICE_NAME_LEN];
static uint8_t	ls_suffix;				/* 0 = no number */
static uint8_t	ls_cursor;
static int8_t	ls_knobAcc[4];
static uint8_t	ls_answerYes;			/* overwrite / morph prompts */

/* message screen */
static char		ls_msgTitle[17];
static char		ls_msg[17];
static uint32_t	ls_msgUntil;
static LsState	ls_msgReturn;

/* name each voice got from a loaded or saved voice preset */
static char		ls_voiceName[NUM_VOICES][VOICE_NAME_LEN];
static uint8_t	ls_voiceNameValid[NUM_VOICES];

/* ===================================================================== */
/* helpers                                                                */
/* ===================================================================== */
static uint8_t ls_currentVoice(void)
{
	const uint8_t v = lsGlue_activeVoice();
	return (v >= 5) ? 5 : v;		/* closed and open hihat share voice 6's sound */
}
/* --------------------------------------------------------------------- */
static const char* ls_voiceLabel(uint8_t voice)
{
	static const char* const labels[NUM_VOICES] = { "Drm1", "Drm2", "Drm3", "Snr ", "Cym ", "Hat " };
	return labels[voice < NUM_VOICES ? voice : 5];
}
/* --------------------------------------------------------------------- */
/* copy a string into a 16 character row, space padded */
static void ls_row(char *row, const char *text)
{
	memset(row, ' ', 16);
	const size_t n = strlen(text);
	memcpy(row, text, n > 16 ? 16 : n);
	row[16] = 0;
}
/* --------------------------------------------------------------------- */
static char ls_upper(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c; }
static char ls_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
/* --------------------------------------------------------------------- */
/* name with the number added: "DRUMATIX" + 2 -> "DRUMATI2", "KICK" + 2 -> "KICK2" */
static void ls_composeName(const char *base, uint8_t suffix, char *out)
{
	char digits[4] = {0};
	uint8_t nDigits = 0;
	if(suffix > 0)
	{
		nDigits = (uint8_t)snprintf(digits, sizeof(digits), "%u", (unsigned)suffix);
	}

	uint8_t len = VOICE_NAME_LEN;
	while(len > 0 && (base[len-1] == ' ' || base[len-1] == 0)) len--;
	if(len > VOICE_NAME_LEN - nDigits) len = (uint8_t)(VOICE_NAME_LEN - nDigits);

	memset(out, ' ', VOICE_NAME_LEN);
	memcpy(out, base, len);
	memcpy(&out[len], digits, nDigits);
}
/* --------------------------------------------------------------------- */
/* smallest number (1-99) whose name isn't used yet for this voice type */
static uint8_t ls_nextUnusedSuffix(VoiceType type, const char *base)
{
	char candidate[VOICE_NAME_LEN];
	for(uint8_t n = 1; n < 100; n++)
	{
		ls_composeName(base, n, candidate);
		uint8_t taken = 0;
		for(uint16_t s = 0; s < VOICE_SLOTS && !taken; s++)
		{
			const char *name = voiceStore_name(type, (uint8_t)s);
			if(name && memcmp(name, candidate, VOICE_NAME_LEN) == 0) taken = 1;
		}
		if(!taken) return n;
	}
	return 99;
}
/* --------------------------------------------------------------------- */
/* the knob edited the name directly: the number becomes part of it */
static void ls_commitName(void)
{
	char composed[VOICE_NAME_LEN];
	ls_composeName(ls_baseName, ls_suffix, composed);
	memcpy(ls_baseName, composed, VOICE_NAME_LEN);
	ls_suffix = 0;
}
/* --------------------------------------------------------------------- */
static int16_t ls_findUsed(VoiceType type, int16_t from, int8_t dir)
{
	for(int16_t s = from; s >= 0 && s < VOICE_SLOTS; s += dir)
	{
		if(voiceStore_isUsed(type, (uint8_t)s)) return s;
	}
	return -1;
}
/* --------------------------------------------------------------------- */
/* knob steps -> whole steps, keeping the remainder */
static int8_t ls_knobSteps(uint8_t pot, int8_t diff, int8_t stepsPerUnit)
{
	ls_knobAcc[pot] = (int8_t)(ls_knobAcc[pot] + diff);
	int8_t units = 0;
	while(ls_knobAcc[pot] >= stepsPerUnit)  { ls_knobAcc[pot] = (int8_t)(ls_knobAcc[pot] - stepsPerUnit); units++; }
	while(ls_knobAcc[pot] <= -stepsPerUnit) { ls_knobAcc[pot] = (int8_t)(ls_knobAcc[pot] + stepsPerUnit); units--; }
	return units;
}
/* --------------------------------------------------------------------- */
static void ls_showMessage(const char *title, const char *text, uint32_t ms, LsState returnTo)
{
	ls_row(ls_msgTitle, title);
	ls_row(ls_msg, text);
	ls_msgUntil = lsGlue_nowMs() + ms;
	ls_msgReturn = returnTo;
	ls_state = LS_MESSAGE;
	lsGlue_requestRepaint();
}

/* ===================================================================== */
/* opening the screens                                                    */
/* ===================================================================== */
static void ls_openOverview(void)
{
	ls_state = LS_OVERVIEW;
	lsGlue_enterOverviewPage();
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_openLoadVoice(void)
{
	const VoiceType type = voicePreset_typeOfVoice(ls_currentVoice());
	if(!voiceStore_isUsed(type, ls_browseSlot[type]))
	{
		int16_t s = ls_findUsed(type, ls_browseSlot[type], 1);
		if(s < 0) s = ls_findUsed(type, ls_browseSlot[type], -1);
		if(s >= 0) ls_browseSlot[type] = (uint8_t)s;
	}
	ls_column = COL_LOAD_VOICE;
	ls_state = LS_LOAD_VOICE;
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_openSaveVoice(void)
{
	ls_saveVoice = ls_currentVoice();
	const VoiceType type = voicePreset_typeOfVoice(ls_saveVoice);

	if(ls_voiceNameValid[ls_saveVoice])
	{
		memcpy(ls_baseName, ls_voiceName[ls_saveVoice], VOICE_NAME_LEN);
	}
	else
	{
		lsGlue_currentKitName(ls_baseName);
	}
	for(uint8_t i = 0; i < VOICE_NAME_LEN; i++)
	{
		if(ls_baseName[i] == 0) ls_baseName[i] = ' ';
	}

	const int16_t freeSlot = voiceStore_nextFree(type, 0);
	ls_saveSlot = (freeSlot >= 0) ? (uint8_t)freeSlot : 0;
	ls_suffix = 0;
	ls_cursor = 0;
	memset(ls_knobAcc, 0, sizeof(ls_knobAcc));
	ls_column = COL_SAVE_VOICE;
	ls_state = LS_SAVE_VOICE;
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_openSaveKit(void)
{
	ls_column = COL_SAVE_KIT;
	if(lsGlue_isMorphActive())
	{
		ls_answerYes = 0;
		ls_state = LS_MORPH_PROMPT;
		lsGlue_requestRepaint();
	}
	else
	{
		ls_state = LS_STOCK;
		lsGlue_enterStockKitPage(1, 0);
	}
}
/* --------------------------------------------------------------------- */
static void ls_openLoadKit(void)
{
	ls_column = COL_LOAD_KIT;
	ls_state = LS_STOCK;
	lsGlue_enterStockKitPage(0, 0);
}
/* --------------------------------------------------------------------- */
static void ls_openKitVoice(void)
{
	if(!ls_kitSlotSet)
	{
		ls_kitSlot = lsGlue_currentKitSlot();
		ls_kitSlotSet = 1;
	}
	ls_column = COL_KIT_VOICE;
	ls_state = LS_KIT_VOICE;
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_openConfirm(uint8_t col, LsState state)
{
	ls_column = col;
	ls_answerYes = 0;					/* default: no */
	ls_state = state;
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_openColumn(uint8_t col)
{
	switch(col)
	{
	case COL_SAVE_VOICE:	ls_openSaveVoice();	break;
	case COL_SAVE_KIT:		ls_openSaveKit();	break;
	case COL_LOAD_VOICE:	ls_openLoadVoice();	break;
	case COL_LOAD_KIT:		ls_openLoadKit();	break;
	case COL_KIT_VOICE:		ls_openKitVoice();	break;
	case COL_REVERT_VOICE:	ls_openConfirm(COL_REVERT_VOICE, LS_CONFIRM_REVERT_VOICE); break;
	default:				ls_openConfirm(COL_REVERT_KIT, LS_CONFIRM_REVERT_KIT); break;
	}
}

/* ===================================================================== */
/* actions                                                                */
/* ===================================================================== */
/* a voice got a new sound from this menu: it's what "revert voice" goes
 * back to, and the voice's undo steps no longer apply */
static void ls_rememberSound(uint8_t voice, const uint8_t *sound)
{
	memcpy(ls_snap[voice], sound, voicePreset_numParams(voice));
	ls_snapValid[voice] = 1;
}
/* --------------------------------------------------------------------- */
/* request a glitch-free voice load; "Voice loaded" + text once applied */
static uint8_t ls_requestVoice(uint8_t voice, const uint8_t *sound, const char *text)
{
	if(!voiceLoad_request(voice, voicePreset_typeOfVoice(voice), sound, lsGlue_nowMs())) return 0;
	ls_rememberSound(voice, sound);
	undo_clearVoice(voice);
	ls_loadWatch = 1;
	ls_loadVoice = voice;
	ls_row(ls_loadText, text);
	return 1;
}
/* --------------------------------------------------------------------- */
/* a different kit is in the edit buffer now */
static void ls_resetForKit(void)
{
	memset(ls_voiceNameValid, 0, sizeof(ls_voiceNameValid));
	memset(ls_snapValid, 0, sizeof(ls_snapValid));
	voiceLoad_cancelAll();				/* a staged voice must not override the new kit */
	undo_clear();
	ls_loadWatch = 0;
	ls_kitSlotSet = 0;
}
/* --------------------------------------------------------------------- */
static void ls_doSave(void)
{
	const VoiceType type = voicePreset_typeOfVoice(ls_saveVoice);
	uint8_t sound[VOICE_PARAMS_MAX];
	char name[VOICE_NAME_LEN];

	ls_composeName(ls_baseName, ls_suffix, name);
	voicePreset_extract(lsGlue_kitParameters(), ls_saveVoice, sound);

	if(voiceStore_save(type, ls_saveSlot, name, sound) == VOICESTORE_OK)
	{
		memcpy(ls_voiceName[ls_saveVoice], name, VOICE_NAME_LEN);
		ls_voiceNameValid[ls_saveVoice] = 1;
		ls_rememberSound(ls_saveVoice, sound);	/* revert goes back to what was saved */
		char text[17];
		snprintf(text, sizeof(text), "Saved to %u", (unsigned)ls_saveSlot);
		char title[17];
		snprintf(title, sizeof(title), "Save Vc as  %s", ls_voiceLabel(ls_saveVoice));
		ls_showMessage(title, text, MSG_SAVED_MS, LS_OVERVIEW);
	}
	else
	{
		/* back to the save screen so it can be tried again */
		char title[17];
		snprintf(title, sizeof(title), "Save Vc as  %s", ls_voiceLabel(ls_saveVoice));
		ls_showMessage(title, "SD card error", MSG_ERROR_MS, LS_SAVE_VOICE);
	}
}
/* --------------------------------------------------------------------- */
static void ls_doLoad(void)
{
	const uint8_t voice = ls_currentVoice();
	const VoiceType type = voicePreset_typeOfVoice(voice);
	const uint8_t slot = ls_browseSlot[type];
	const uint8_t *params = voiceStore_params(type, slot);
	if(!params) return;

	char text[17];
	snprintf(text, sizeof(text), "%3u %.8s", (unsigned)slot, voiceStore_name(type, slot));
	if(ls_requestVoice(voice, params, text))
	{
		memcpy(ls_voiceName[voice], voiceStore_name(type, slot), VOICE_NAME_LEN);
		ls_voiceNameValid[voice] = 1;
	}
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
/* load the selected voice's sound out of the browsed kit slot */
static void ls_doKitVoice(void)
{
	const uint8_t voice = ls_currentVoice();
	uint8_t sound[VOICE_PARAMS_MAX];
	char name[9], text[17];

	voicePreset_extract(lsGlue_kitParamsOfSlot(ls_kitSlot), voice, sound);
	lsGlue_kitName(ls_kitSlot, name);
	name[8] = 0;
	snprintf(text, sizeof(text), "%3u %s", (unsigned)ls_kitSlot, name);
	if(ls_requestVoice(voice, sound, text))
	{
		memcpy(ls_voiceName[voice], name, VOICE_NAME_LEN);	/* save voice suggests the kit's name */
		ls_voiceNameValid[voice] = 1;
	}
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
/* back to the sound of the last voice load/save, or else to the voice as
 * it is in the saved kit */
static void ls_doRevertVoice(void)
{
	const uint8_t voice = ls_currentVoice();
	uint8_t sound[VOICE_PARAMS_MAX];

	if(ls_snapValid[voice]) memcpy(sound, ls_snap[voice], voicePreset_numParams(voice));
	else voicePreset_extract(lsGlue_kitParamsOfSlot(lsGlue_currentKitSlot()), voice, sound);

	char title[17];
	snprintf(title, sizeof(title), "Revert Vc   %s", ls_voiceLabel(voice));
	if(voiceLoad_request(voice, voicePreset_typeOfVoice(voice), sound, lsGlue_nowMs()))
	{
		undo_clearVoice(voice);
		ls_loadWatch = 0;
		ls_showMessage(title, "Voice reverted", MSG_REVERTED_MS, LS_CLOSED);
	}
	else
	{
		ls_showMessage(title, "not possible", MSG_ERROR_MS, LS_OVERVIEW);
	}
}
/* --------------------------------------------------------------------- */
/* reload the current kit as it was last loaded or saved */
static void ls_doRevertKit(void)
{
	const uint8_t slot = lsGlue_currentKitSlot();
	char name[9], text[17];

	lsGlue_reloadCurrentKit();
	ls_resetForKit();
	lsGlue_kitName(slot, name);
	name[8] = 0;
	snprintf(text, sizeof(text), "%3u %s", (unsigned)slot, name);
	ls_showMessage("Kit reverted", text, MSG_REVERTED_MS, LS_CLOSED);
}
/* --------------------------------------------------------------------- */
static void ls_browseKit(int8_t steps)
{
	int16_t s = (int16_t)(ls_kitSlot + steps);
	if(s < 0) s = 0;
	if(s > NUM_KIT_SLOTS_LS - 1) s = NUM_KIT_SLOTS_LS - 1;
	ls_kitSlot = (uint8_t)s;
}
/* --------------------------------------------------------------------- */
/* the undo message ends: back to the screen that was there before */
static void ls_endToast(void)
{
	ls_state = ls_toastPrev;
	if(ls_state == LS_STOCK && !lsGlue_isOnStockPage()) ls_state = LS_CLOSED;
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
static void ls_browse(int8_t steps)
{
	const VoiceType type = voicePreset_typeOfVoice(ls_currentVoice());
	while(steps != 0)
	{
		const int8_t dir = (steps > 0) ? 1 : -1;
		const int16_t s = ls_findUsed(type, (int16_t)(ls_browseSlot[type] + dir), dir);
		if(s < 0) break;			/* stop at the first / last sound */
		ls_browseSlot[type] = (uint8_t)s;
		steps = (int8_t)(steps - dir);
	}
}

/* ===================================================================== */
/* public                                                                 */
/* ===================================================================== */
/* The stock kit pages can leave by themselves (after "kit saved" the
 * firmware returns to the voice page), so check the stock page is really
 * still there before trusting LS_STOCK. */
static void ls_syncStock(void)
{
	if(ls_state == LS_STOCK && !lsGlue_isOnStockPage()) ls_state = LS_CLOSED;
}
/* --------------------------------------------------------------------- */
uint8_t lsMenu_isActive(void)
{
	return (ls_state != LS_CLOSED) && (ls_state != LS_STOCK);
}
/* --------------------------------------------------------------------- */
uint8_t lsMenu_isOpen(void)
{
	return ls_state != LS_CLOSED;
}
/* --------------------------------------------------------------------- */
void lsMenu_forceClose(void)
{
	ls_state = LS_CLOSED;
	ls_loadWatch = 0;
}
/* --------------------------------------------------------------------- */
void lsMenu_onLoadSaveButton(void)
{
	lsMenu_dismissToast();
	ls_syncStock();
	switch(ls_state)
	{
	case LS_CLOSED:
		ls_column = COL_SAVE_VOICE;		/* always opens on save voice */
		ls_openOverview();
		break;
	case LS_OVERVIEW:
		ls_state = LS_CLOSED;
		ls_loadWatch = 0;
		lsGlue_leaveMenu();
		break;
	default:							/* any expanded view, prompt or stock page */
		ls_openOverview();
		break;
	}
}
/* --------------------------------------------------------------------- */
void lsMenu_onKitLoaded(uint8_t slot)
{
	ls_resetForKit();

	/* loaded from our menu's kit list: confirm briefly, then to the Perform page */
	ls_syncStock();
	if(ls_state == LS_STOCK)
	{
		char name[9], text[17];
		lsGlue_kitName(slot, name);
		name[8] = 0;
		snprintf(text, sizeof(text), "%3u %s", (unsigned)slot, name);
		ls_showMessage("Kit loaded", text, MSG_KIT_LOADED_MS, LS_CLOSED);
	}
}
/* --------------------------------------------------------------------- */
void lsMenu_handleEncoder(int8_t inc, uint8_t clicked)
{
	switch(ls_state)
	{
	case LS_OVERVIEW:
		if(clicked) { ls_openColumn(ls_column); return; }
		{
			int16_t c = (int16_t)(ls_column + inc);
			if(c < 0) c = 0;
			if(c > NUM_COLS - 1) c = NUM_COLS - 1;
			ls_column = (uint8_t)c;
		}
		break;

	case LS_LOAD_VOICE:
		if(clicked) { ls_doLoad(); return; }
		ls_browse(inc);
		break;

	case LS_KIT_VOICE:
		if(clicked) { ls_doKitVoice(); return; }
		ls_browseKit(inc);
		break;

	case LS_CONFIRM_REVERT_VOICE:
	case LS_CONFIRM_REVERT_KIT:
		if(clicked)
		{
			if(!ls_answerYes) { ls_openOverview(); return; }
			if(ls_state == LS_CONFIRM_REVERT_VOICE) ls_doRevertVoice();
			else ls_doRevertKit();
			return;
		}
		else if(inc != 0)
		{
			ls_answerYes = (inc > 0);
		}
		break;

	case LS_SAVE_VOICE:
		if(clicked)
		{
			const VoiceType type = voicePreset_typeOfVoice(ls_saveVoice);
			if(voiceStore_isUsed(type, ls_saveSlot))
			{
				ls_answerYes = 0;		/* default: don't overwrite */
				ls_state = LS_CONFIRM_OVERWRITE;
			}
			else
			{
				ls_doSave();
				return;
			}
		}
		else
		{
			int16_t s = (int16_t)(ls_saveSlot + inc);
			if(s < 0) s = 0;
			if(s > VOICE_SLOTS - 1) s = VOICE_SLOTS - 1;
			ls_saveSlot = (uint8_t)s;
		}
		break;

	case LS_CONFIRM_OVERWRITE:
		if(clicked)
		{
			if(ls_answerYes) { ls_doSave(); return; }
			ls_state = LS_SAVE_VOICE;
		}
		else if(inc != 0)
		{
			ls_answerYes = (inc > 0);
		}
		break;

	case LS_MORPH_PROMPT:
		if(clicked)
		{
			ls_state = LS_STOCK;
			lsGlue_enterStockKitPage(1, ls_answerYes);
			return;
		}
		else if(inc != 0)
		{
			ls_answerYes = (inc > 0);
		}
		break;

	default:
		break;
	}
	lsGlue_requestRepaint();
}
/* --------------------------------------------------------------------- */
int8_t lsMenu_handleKnob(uint8_t potNr, int8_t diff)
{
	ls_syncStock();
	if(potNr > 3 || diff == 0) return (ls_state == LS_CLOSED || ls_state == LS_STOCK) ? -1 : 0;

	switch(ls_state)
	{
	case LS_CLOSED:
		return -1;

	case LS_STOCK:
		/* in the stock kit loader, knob 4 scrolls like on the overview */
		if(potNr == COL_LOAD_KIT && ls_column == COL_LOAD_KIT)
		{
			lsGlue_stockScroll(diff);
			return 1;
		}
		return -1;					/* e.g. the stock name editor on the kit save page */

	case LS_OVERVIEW:
	case LS_LOAD_VOICE:
	case LS_KIT_VOICE:
	{
		/* the knob under a column opens it; the voice lists scroll right away */
		const uint8_t page = (ls_state == LS_KIT_VOICE) ? 1 :
							 (ls_state == LS_LOAD_VOICE) ? 0 : (uint8_t)(ls_column / COLS_PER_PAGE);
		const uint8_t col = (uint8_t)(page * COLS_PER_PAGE + potNr);
		if(col >= NUM_COLS) return 0;
		if(col == COL_LOAD_VOICE)
		{
			if(ls_state != LS_LOAD_VOICE) ls_openLoadVoice();
			else ls_browse(diff);
			lsGlue_requestRepaint();
			return 1;
		}
		if(col == COL_KIT_VOICE)
		{
			if(ls_state != LS_KIT_VOICE) ls_openKitVoice();
			else ls_browseKit(diff);
			lsGlue_requestRepaint();
			return 1;
		}
		ls_openColumn(col);				/* any other knob opens its own column */
		return 1;
	}

	case LS_SAVE_VOICE:
	{
		switch(potNr)
		{
		case 0: /* cursor */
		{
			const int8_t steps = ls_knobSteps(0, diff, CURSOR_KNOB_STEPS);
			int16_t c = (int16_t)(ls_cursor + steps);
			if(c < 0) c = 0;
			if(c > VOICE_NAME_LEN - 1) c = VOICE_NAME_LEN - 1;
			ls_cursor = (uint8_t)c;
			break;
		}
		case 1: /* upper / lower case */
			ls_commitName();
			ls_baseName[ls_cursor] = (diff > 0) ? ls_lower(ls_baseName[ls_cursor]) : ls_upper(ls_baseName[ls_cursor]);
			break;
		case 2: /* letter */
		{
			ls_commitName();
			const uint8_t n = lsGlue_numNameChars();
			int16_t idx = 0;
			for(uint8_t i = 0; i < n; i++)
			{
				if(lsGlue_nameChar(i) == ls_baseName[ls_cursor]) { idx = i; break; }
			}
			idx = (int16_t)(idx + diff);
			if(idx < 0) idx = 0;
			if(idx > n - 1) idx = (int16_t)(n - 1);
			ls_baseName[ls_cursor] = lsGlue_nameChar((uint8_t)idx);
			break;
		}
		default: /* number at the end */
		{
			const int8_t steps = ls_knobSteps(3, diff, SUFFIX_KNOB_STEPS);
			if(steps == 0) break;
			if(ls_suffix == 0 && steps > 0)
			{
				ls_suffix = ls_nextUnusedSuffix(voicePreset_typeOfVoice(ls_saveVoice), ls_baseName);
			}
			else
			{
				int16_t s = (int16_t)(ls_suffix + steps);
				if(s < 0) s = 0;
				if(s > 99) s = 99;
				ls_suffix = (uint8_t)s;
			}
			break;
		}
		}
		lsGlue_requestRepaint();
		return 1;
	}

	default:						/* prompts and messages ignore the knobs */
		return 0;
	}
}
/* --------------------------------------------------------------------- */
void lsMenu_tick(uint32_t nowMs)
{
	if(ls_state == LS_MESSAGE && (int32_t)(nowMs - ls_msgUntil) >= 0)
	{
		if(ls_msgReturn == LS_RESTORE) ls_endToast();
		else if(ls_msgReturn == LS_OVERVIEW) ls_openOverview();
		else if(ls_msgReturn == LS_CLOSED)
		{
			ls_state = LS_CLOSED;		/* after a load: to the Perform page */
			lsGlue_leaveMenu();
		}
		else
		{
			ls_state = ls_msgReturn;
			if(ls_state == LS_STOCK && !lsGlue_isOnStockPage()) ls_state = LS_CLOSED;
			lsGlue_requestRepaint();
		}
	}
	else if(ls_loadWatch && ls_state != LS_MESSAGE && ls_state != LS_CLOSED && ls_state != LS_STOCK
			&& !voiceLoad_isPending(ls_loadVoice))
	{
		/* the requested sound has been applied */
		ls_loadWatch = 0;
		ls_showMessage("Voice loaded", ls_loadText, MSG_VOICE_LOADED_MS, LS_CLOSED);
	}
	else if(ls_state == LS_LOAD_VOICE || ls_state == LS_KIT_VOICE)
	{
		/* clear the loading marker once the sound has been applied */
		const uint8_t pending = voiceLoad_isPending(ls_currentVoice());
		if(pending != ls_shownPending) lsGlue_requestRepaint();
	}
}
/* --------------------------------------------------------------------- */
void lsMenu_repaint(void)
{
	char r0[17], r1[17], tmp[24];
	int8_t cursorCol = -1;

	switch(ls_state)
	{
	case LS_OVERVIEW:
	{
		static const char* const top[NUM_COLS] = { "sv", "sv",  "ld", "ld",  "ld",  "rv", "rv"  };
		static const char* const bot[NUM_COLS] = { "vc", "kit", "vc", "kit", "kvc", "vc", "kit" };
		const uint8_t page = (uint8_t)(ls_column / COLS_PER_PAGE);
		ls_row(r0, ""); ls_row(r1, "");
		for(uint8_t i = 0; i < COLS_PER_PAGE; i++)
		{
			const uint8_t c = (uint8_t)(page * COLS_PER_PAGE + i);
			if(c >= NUM_COLS) break;
			for(uint8_t k = 0; top[c][k]; k++) r0[i*4 + k] = (c == ls_column) ? ls_upper(top[c][k]) : top[c][k];
			for(uint8_t k = 0; bot[c][k]; k++) r1[i*4 + k] = (c == ls_column) ? ls_upper(bot[c][k]) : bot[c][k];
		}
		r0[15] = page ? '<' : '>';		/* more columns to the right / left */
		break;
	}

	case LS_LOAD_VOICE:
	{
		const uint8_t voice = ls_currentVoice();
		const VoiceType type = voicePreset_typeOfVoice(voice);
		snprintf(tmp, sizeof(tmp), "Load Voice  %s", ls_voiceLabel(voice));
		ls_row(r0, tmp);
		const char *name = voiceStore_name(type, ls_browseSlot[type]);
		ls_shownPending = voiceLoad_isPending(voice);
		if(name)
		{
			snprintf(tmp, sizeof(tmp), "%3u %.8s", (unsigned)ls_browseSlot[type], name);
			ls_row(r1, tmp);
			if(ls_shownPending) r1[15] = '*';	/* waiting for the voice to go quiet */
		}
		else
		{
			ls_row(r1, " no sounds saved");
		}
		break;
	}

	case LS_SAVE_VOICE:
	{
		char name[VOICE_NAME_LEN];
		const VoiceType type = voicePreset_typeOfVoice(ls_saveVoice);
		snprintf(tmp, sizeof(tmp), "Save Vc as  %s", ls_voiceLabel(ls_saveVoice));
		ls_row(r0, tmp);
		ls_composeName(ls_baseName, ls_suffix, name);
		snprintf(tmp, sizeof(tmp), "%3u %.8s %cok", (unsigned)ls_saveSlot, name,
				voiceStore_isUsed(type, ls_saveSlot) ? '*' : ' ');
		ls_row(r1, tmp);
		cursorCol = (int8_t)(4 + ls_cursor);
		break;
	}

	case LS_CONFIRM_OVERWRITE:
	{
		const char *old = voiceStore_name(voicePreset_typeOfVoice(ls_saveVoice), ls_saveSlot);
		snprintf(tmp, sizeof(tmp), "Replace %.8s", old ? old : "        ");
		ls_row(r0, tmp);
		ls_row(r1, ls_answerYes ? "          [Y/n]" : "          [y/N]");
		break;
	}

	case LS_KIT_VOICE:
	{
		const uint8_t voice = ls_currentVoice();
		char name[9];
		snprintf(tmp, sizeof(tmp), "Kit Voice   %s", ls_voiceLabel(voice));
		ls_row(r0, tmp);
		lsGlue_kitName(ls_kitSlot, name);
		name[8] = 0;
		snprintf(tmp, sizeof(tmp), "%3u %s", (unsigned)ls_kitSlot, name);
		ls_row(r1, tmp);
		ls_shownPending = voiceLoad_isPending(voice);
		if(ls_shownPending) r1[15] = '*';		/* waiting for the voice to go quiet */
		break;
	}

	case LS_CONFIRM_REVERT_VOICE:
		snprintf(tmp, sizeof(tmp), "Revert Vc   %s", ls_voiceLabel(ls_currentVoice()));
		ls_row(r0, tmp);
		ls_row(r1, ls_answerYes ? "          [Y/n]" : "          [y/N]");
		break;

	case LS_CONFIRM_REVERT_KIT:
	{
		char name[9];
		lsGlue_kitName(lsGlue_currentKitSlot(), name);
		name[8] = 0;
		ls_row(r0, "Revert kit");
		snprintf(tmp, sizeof(tmp), "%s  %s", name, ls_answerYes ? "[Y/n]" : "[y/N]");
		ls_row(r1, tmp);
		break;
	}

	case LS_MORPH_PROMPT:
		ls_row(r0, "Save kit with");
		ls_row(r1, ls_answerYes ? "morph?    [Y/n]" : "morph?    [y/N]");
		break;

	case LS_MESSAGE:
		memcpy(r0, ls_msgTitle, 17);
		memcpy(r1, ls_msg, 17);
		break;

	default:
		return;
	}

	lsGlue_show(r0, r1, 1, cursorCol);
}
/* --------------------------------------------------------------------- */
void lsMenu_undo(void)
{
	ls_syncStock();
	if(ls_state == LS_MESSAGE && ls_msgReturn != LS_RESTORE) return;	/* a load/save message is up */
	const LsState prev = (ls_state == LS_MESSAGE) ? ls_toastPrev : ls_state;

	const uint8_t voice = ls_currentVoice();
	UndoStep step;
	char title[17], text[17];

	if(undo_pop(voice, &step))
	{
		lsGlue_restoreParam(step.param, step.oldValue);
		snprintf(title, sizeof(title), "Undo        %s",
				 step.owner == VOICEPRESET_KIT_LEVEL ? "Kit " : ls_voiceLabel(step.owner));
		lsGlue_paramName(step.param, text);
		text[16] = 0;
	}
	else
	{
		snprintf(title, sizeof(title), "Undo        %s", ls_voiceLabel(voice));
		snprintf(text, sizeof(text), "nothing to undo");
	}
	ls_showMessage(title, text, MSG_UNDO_MS, LS_RESTORE);
	ls_toastPrev = prev;
}
/* --------------------------------------------------------------------- */
uint8_t lsMenu_isToast(void)
{
	return ls_state == LS_MESSAGE && ls_msgReturn == LS_RESTORE;
}
/* --------------------------------------------------------------------- */
void lsMenu_dismissToast(void)
{
	if(lsMenu_isToast()) ls_endToast();
}
