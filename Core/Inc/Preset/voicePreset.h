/*
 * voicePreset.h
 *
 * Per-voice parameter tables for the LXR Eurorack module.
 *
 * Kit parameters are stored grouped by parameter type (all COARSE values,
 * then all FILTER_FREQ values, ...), so one voice's parameters are scattered
 * across the kit array. The tables here list, for each voice, which kit
 * parameter indices make up "the sound" of that voice, in a fixed
 * voice-relative order. Position k means the same thing on every voice of
 * the same type, so a sound can move between drum voices 1-3.
 *
 * Deliberately NOT part of a voice sound (they belong to the kit / patch):
 *   volume and pan (mix settings, see the mixer page),
 *   audio output routing, 1V/oct CV enable, CV modulation routing,
 *   FX bus settings, morph target, global decimation.
 */

#ifndef INC_PRESET_VOICEPRESET_H_
#define INC_PRESET_VOICEPRESET_H_

#include <stdint.h>

/* the four voice architectures */
typedef enum
{
	VOICE_TYPE_DRUM = 0,	/* voices 1-3 */
	VOICE_TYPE_SNARE,		/* voice 4 (snare/clap) */
	VOICE_TYPE_CYMBAL,		/* voice 5 */
	VOICE_TYPE_HIHAT,		/* voice 6 (closed + open hat) */
	NUM_VOICE_TYPES
} VoiceType;

#define NUM_VOICES 6

/* number of parameters that make up one voice sound, per type */
#define VOICE_PARAMS_DRUM	34
#define VOICE_PARAMS_SNARE	33
#define VOICE_PARAMS_CYMBAL	34
#define VOICE_PARAMS_HIHAT	34

/* largest of the above, used to size buffers */
#define VOICE_PARAMS_MAX	34

/* voice is 0 based (0..5) */
VoiceType voicePreset_typeOfVoice(uint8_t voice);

/* number of sound parameters for a voice (0 based) */
uint8_t voicePreset_numParams(uint8_t voice);

/* kit parameter index of voice-relative parameter k */
uint16_t voicePreset_paramIndex(uint8_t voice, uint8_t k);

/* voice (0..5) a kit parameter belongs to, including volume, pan, output
 * and 1V/oct; VOICEPRESET_KIT_LEVEL for kit-wide parameters (FX, morph...) */
#define VOICEPRESET_KIT_LEVEL 0xFF
uint8_t voicePreset_ownerOfParam(uint16_t paramIdx);

/* true if two voices can exchange sounds (same architecture) */
uint8_t voicePreset_compatible(uint8_t voiceA, uint8_t voiceB);

/*
 * Copy one voice's sound out of a kit parameter array into a
 * voice-relative buffer (length voicePreset_numParams(voice)).
 * Modulation targets that point at the voice itself are stored
 * voice-relative, so they still point at the voice after a move.
 */
void voicePreset_extract(const uint8_t *kitParams, uint8_t voice, uint8_t *sound);

/*
 * Write a voice-relative sound into a kit parameter array for the given
 * voice. Only that voice's parameters are touched. Returns 0 if the
 * sound's type doesn't match the voice.
 */
uint8_t voicePreset_apply(uint8_t *kitParams, uint8_t voice, VoiceType soundType, const uint8_t *sound);

#endif /* INC_PRESET_VOICEPRESET_H_ */
