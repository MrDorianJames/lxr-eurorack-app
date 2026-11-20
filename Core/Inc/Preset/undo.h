/*
 * undo.h
 *
 * Undo history for parameter edits made with the knobs and encoder.
 *
 * Each edit records the parameter's value from before the edit. A burst of
 * changes to the same parameter (one knob turn) is one step: a new step
 * starts with a different parameter or after a short pause. Undo restores
 * the most recent step of the selected voice or a kit-wide parameter (FX,
 * morph...), so undoing on the kick never touches the hats.
 *
 * Loading a kit clears the history; loading a sound into a voice clears
 * that voice's steps (the old values belong to the old sound).
 */

#ifndef INC_PRESET_UNDO_H_
#define INC_PRESET_UNDO_H_

#include <stdint.h>

#define UNDO_STEPS			64
#define UNDO_MERGE_MS		1500	/* same parameter within this time = same step */

typedef struct
{
	uint16_t param;
	uint8_t  oldValue;
	uint8_t  owner;		/* voice 0..5 or VOICEPRESET_KIT_LEVEL */
} UndoStep;

/* before a parameter changes; ignored for parameters undo doesn't cover */
void    undo_record(uint16_t param, uint8_t oldValue, uint32_t nowMs);

/* take the newest step of 'voice' (0..5) or a kit-wide one; 0 if none */
uint8_t undo_pop(uint8_t voice, UndoStep *out);

void    undo_clear(void);
void    undo_clearVoice(uint8_t voice);
uint8_t undo_count(void);

#endif /* INC_PRESET_UNDO_H_ */
