/* Host test: undo history (undo.c) and the parameter owner lookup. */
#include <stdio.h>
#include <string.h>
#include "Parameters.h"
#include "Preset/voicePreset.h"
#include "Preset/undo.h"

static int fails = 0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

int main(void)
{
	/* owner lookup */
	for(uint8_t v = 0; v < NUM_VOICES; v++)
	{
		for(uint8_t k = 0; k < voicePreset_numParams(v); k++)
			CHECK(voicePreset_ownerOfParam(voicePreset_paramIndex(v, k)) == v, "sound param of voice %d", v);
	}
	CHECK(voicePreset_ownerOfParam(PAR_VOL3) == 2 && voicePreset_ownerOfParam(PAR_PAN5) == 4, "vol/pan owners");
	CHECK(voicePreset_ownerOfParam(PAR_AUDIO_OUT6) == 5 && voicePreset_ownerOfParam(PAR_CV_1VOCT_1) == 0, "out/1voct owners");
	CHECK(voicePreset_ownerOfParam(PAR_FX_TYPE) == VOICEPRESET_KIT_LEVEL && voicePreset_ownerOfParam(PAR_MORPH_TARGET) == VOICEPRESET_KIT_LEVEL, "kit-level");

	UndoStep s;
	/* one knob turn = one step, keeping the value from before the turn */
	undo_record(PAR_FILTER_FREQ_1, 40, 1000);
	undo_record(PAR_FILTER_FREQ_1, 41, 1010);
	undo_record(PAR_FILTER_FREQ_1, 42, 1020);
	CHECK(undo_count() == 1, "burst merged (%u)", undo_count());
	/* a pause makes a new step */
	undo_record(PAR_FILTER_FREQ_1, 43, 1020 + UNDO_MERGE_MS);
	CHECK(undo_count() == 2, "pause starts a new step");
	/* other voices and kit-level edits */
	undo_record(PAR_COARSE4, 10, 4000);      /* snare */
	undo_record(PAR_FX_TYPE, 1, 4100);       /* kit level */
	undo_record(PAR_RESO_1, 70, 4200);       /* drum 1 */

	CHECK(undo_pop(0, &s) && s.param == PAR_RESO_1 && s.oldValue == 70, "newest drum-1 step first");
	CHECK(undo_pop(0, &s) && s.param == PAR_FX_TYPE && s.oldValue == 1, "kit-level steps belong to every voice");
	CHECK(undo_pop(0, &s) && s.param == PAR_FILTER_FREQ_1 && s.oldValue == 43, "skips the snare step");
	CHECK(undo_pop(0, &s) && s.param == PAR_FILTER_FREQ_1 && s.oldValue == 40, "value from before the first turn");
	CHECK(!undo_pop(0, &s), "drum 1 history empty");
	CHECK(undo_pop(3, &s) && s.param == PAR_COARSE4 && s.oldValue == 10, "snare step still there");
	CHECK(undo_count() == 0, "all taken");

	/* after an undo, turning the same knob again is a new step */
	undo_record(PAR_DRIVE1, 5, 9000); undo_pop(0, &s);
	undo_record(PAR_DRIVE1, 6, 9001);
	CHECK(undo_count() == 1, "new step after undo");
	undo_clear();

	/* things undo doesn't cover */
	undo_record(PAR_NONE, 1, 10000);
	undo_record(PAR_CV_DEST_1, 1, 10000);           /* mod matrix block (starts below 256) */
	undo_record(PAR_CV_DEST_1 + 5, 1, 10000);
	undo_record(END_OF_SOUND_PARAMETERS + 2, 1, 10000);   /* global settings */
	CHECK(undo_count() == 0, "not recorded");

	/* clearing one voice */
	undo_record(PAR_COARSE1, 1, 20000); undo_record(PAR_COARSE2, 2, 20000); undo_record(PAR_FX_P1, 3, 20000);
	undo_clearVoice(0);
	CHECK(undo_count() == 2, "only drum 1 removed");
	CHECK(undo_pop(0, &s) && s.param == PAR_FX_P1, "kit-level step left for drum 1");
	undo_clear();

	/* full history drops the oldest */
	for(int i = 0; i < UNDO_STEPS + 10; i++) undo_record((uint16_t)(PAR_COARSE1 + (i & 1)), (uint8_t)i, (uint32_t)(30000 + i * 10));
	CHECK(undo_count() == UNDO_STEPS, "capped at %d", UNDO_STEPS);
	uint8_t n = 0; uint8_t last = 255;
	while(undo_pop(0, &s)) { if(last != 255) CHECK(s.oldValue < last, "newest first"); last = s.oldValue; n++; }
	CHECK(n == UNDO_STEPS && last == 10, "oldest kept is #10 (got %u)", last);

	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
