/*
 * undo.c
 *
 * Undo history. See undo.h.
 */

#include "Preset/undo.h"
#include "Preset/voicePreset.h"
#include "Parameters.h"
#include <string.h>

static UndoStep undo_steps[UNDO_STEPS];	/* oldest first */
static uint8_t  undo_num;
static uint32_t undo_lastMs;

/* ---------------------------------------------------------------------- */
void undo_record(uint16_t param, uint8_t oldValue, uint32_t nowMs)
{
	/* kit sound parameters only: not the CV mod matrix block (it starts below
	 * 256 and isn't restored the same way) or the global settings after it */
	if(param == PAR_NONE || param >= PAR_CV_DEST_1 || param >= 256) return;

	const uint8_t merge = undo_num > 0
		&& undo_steps[undo_num - 1].param == param
		&& (uint32_t)(nowMs - undo_lastMs) < UNDO_MERGE_MS;
	undo_lastMs = nowMs;
	if(merge) return;	/* same knob turn: keep the value from before it started */

	if(undo_num == UNDO_STEPS)
	{
		memmove(&undo_steps[0], &undo_steps[1], (UNDO_STEPS - 1) * sizeof(UndoStep));
		undo_num--;
	}
	undo_steps[undo_num].param = param;
	undo_steps[undo_num].oldValue = oldValue;
	undo_steps[undo_num].owner = voicePreset_ownerOfParam(param);
	undo_num++;
}
/* ---------------------------------------------------------------------- */
uint8_t undo_pop(uint8_t voice, UndoStep *out)
{
	for(int16_t i = (int16_t)undo_num - 1; i >= 0; i--)
	{
		const uint8_t owner = undo_steps[i].owner;
		if(owner != voice && owner != VOICEPRESET_KIT_LEVEL) continue;
		*out = undo_steps[i];
		memmove(&undo_steps[i], &undo_steps[i + 1], (size_t)(undo_num - 1 - i) * sizeof(UndoStep));
		undo_num--;
		undo_lastMs = 0;		/* the next edit starts a new step */
		return 1;
	}
	return 0;
}
/* ---------------------------------------------------------------------- */
void undo_clear(void) { undo_num = 0; }
/* ---------------------------------------------------------------------- */
void undo_clearVoice(uint8_t voice)
{
	uint8_t j = 0;
	for(uint8_t i = 0; i < undo_num; i++)
	{
		if(undo_steps[i].owner != voice) undo_steps[j++] = undo_steps[i];
	}
	undo_num = j;
}
/* ---------------------------------------------------------------------- */
uint8_t undo_count(void) { return undo_num; }
