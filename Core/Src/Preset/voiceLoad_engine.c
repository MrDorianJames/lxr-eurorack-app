/*
 * voiceLoad_engine.c
 *
 * Sound engine glue for voiceLoad.c: is a voice silent right now?
 * Kept separate so the loading logic can be tested on a PC.
 */

#include "Preset/voiceLoad.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/SlopeEg2.h"

/* about -66 dB: inaudible, and well below where a decay starts */
#define VOICELOAD_QUIET_LEVEL	0.0005f

static uint8_t voiceLoad_egQuiet(const SlopeEg2 *eg, float level)
{
	if(eg->state == EG_STOPPED) return 1;
	/* attack or repeat means a hit is starting: never quiet */
	if(eg->state == EG_D && level < VOICELOAD_QUIET_LEVEL) return 1;
	return 0;
}

uint8_t voiceLoad_isVoiceQuiet(uint8_t voice)
{
	switch(voice)
	{
	case 0: case 1: case 2:
		return voiceLoad_egQuiet(&voiceArray[voice].oscVolEg, voiceArray[voice].egValueOscVol);
	case 3:
		return voiceLoad_egQuiet(&snareVoice.oscVolEg, snareVoice.egValueOscVol);
	case 4:
		return voiceLoad_egQuiet(&cymbalVoice.oscVolEg, cymbalVoice.egValueOscVol);
	case 5:
		return voiceLoad_egQuiet(&hatVoice.oscVolEg, hatVoice.egValueOscVol);
	default:
		return 1;
	}
}
