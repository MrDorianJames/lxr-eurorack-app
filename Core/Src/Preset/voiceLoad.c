/*
 * voiceLoad.c
 *
 * Glitch-free loading of a voice sound while playing. See voiceLoad.h.
 */

#include "Preset/voiceLoad.h"
#include "menu/menu.h"
#include <string.h>

/* from PresetManager.h, which isn't included here because it pulls in the
 * SD card driver headers (keeps this file testable on a PC) */
void preset_sendParameter(uint16_t idx);

typedef struct
{
	uint8_t		pending;
	VoiceType	type;
	uint32_t	requestMs;
	uint8_t		sound[VOICE_PARAMS_MAX];
} StagedSound;

static StagedSound voiceLoad_staged[NUM_VOICES];

/* read from the SysTick interrupt, so volatile */
static volatile uint8_t voiceLoad_applying[NUM_VOICES];

/* keep the compiler from moving memory accesses across the guard flag */
#define VOICELOAD_BARRIER() __asm volatile ("" ::: "memory")

/* ---------------------------------------------------------------------- */
uint8_t voiceLoad_request(uint8_t voice, VoiceType type, const uint8_t *sound, uint32_t nowMs)
{
	if(voice >= NUM_VOICES || !sound) return 0;
	if(voicePreset_typeOfVoice(voice) != type) return 0;

	StagedSound *s = &voiceLoad_staged[voice];
	memcpy(s->sound, sound, voicePreset_numParams(voice));
	s->type = type;
	s->requestMs = nowMs;
	s->pending = 1;
	return 1;
}
/* ---------------------------------------------------------------------- */
void voiceLoad_cancel(uint8_t voice)
{
	if(voice < NUM_VOICES) voiceLoad_staged[voice].pending = 0;
}
/* ---------------------------------------------------------------------- */
void voiceLoad_cancelAll(void)
{
	for(uint8_t v=0; v<NUM_VOICES; v++) voiceLoad_staged[v].pending = 0;
}
/* ---------------------------------------------------------------------- */
uint8_t voiceLoad_isPending(uint8_t voice)
{
	return (voice < NUM_VOICES) ? voiceLoad_staged[voice].pending : 0;
}
/* ---------------------------------------------------------------------- */
uint8_t voiceLoad_isApplying(uint8_t voice)
{
	return (voice < NUM_VOICES) ? voiceLoad_applying[voice] : 0;
}
/* ---------------------------------------------------------------------- */
/* returns 1 if the sound was applied */
static uint8_t voiceLoad_apply(uint8_t voice, uint8_t force)
{
	StagedSound *s = &voiceLoad_staged[voice];

	/* raise the guard first: from here on SysTick postpones notes for this
	 * voice. Then check again, because a note may have started between the
	 * quiet check and raising the guard. */
	voiceLoad_applying[voice] = 1;
	VOICELOAD_BARRIER();

	if(!force && !voiceLoad_isVoiceQuiet(voice))
	{
		VOICELOAD_BARRIER();
		voiceLoad_applying[voice] = 0;
		return 0;					/* a hit just started, try again later */
	}

	voicePreset_apply(activeParameters, voice, s->type, s->sound);

	const uint8_t n = voicePreset_numParams(voice);
	for(uint8_t k=0; k<n; k++)
	{
		preset_sendParameter(voicePreset_paramIndex(voice, k));
	}

	s->pending = 0;
	VOICELOAD_BARRIER();
	voiceLoad_applying[voice] = 0;
	return 1;
}
/* ---------------------------------------------------------------------- */
void voiceLoad_tick(uint32_t nowMs)
{
	for(uint8_t v=0; v<NUM_VOICES; v++)
	{
		StagedSound *s = &voiceLoad_staged[v];
		if(!s->pending) continue;

		const uint8_t timedOut = (uint32_t)(nowMs - s->requestMs) >= VOICELOAD_TIMEOUT_MS;
		if(timedOut)
		{
			voiceLoad_apply(v, 1);
		}
		else if(voiceLoad_isVoiceQuiet(v))
		{
			voiceLoad_apply(v, 0);
		}
	}
}
