/*
 * voicePreset.c
 *
 * Per-voice parameter tables and voice-relative extract/apply.
 * See voicePreset.h for the overview.
 */

#include "Preset/voicePreset.h"
#include "Parameters.h"
#include "menu/CcNr2Text.h"

/* -------------------------------------------------------------------------
 * Voice-relative parameter tables.
 * The ORDER of each table is the order used in voice preset files, so it
 * must never change for an existing file format version. Add new
 * parameters only at the end and bump the voice file version.
 * ---------------------------------------------------------------------- */

#define DRUM_TABLE(n) { \
	PAR_OSC_WAVE_DRUM##n, PAR_COARSE##n, PAR_FINE##n, PAR_MOD_WAVE_DRUM##n, PAR_PWM_##n, \
	PAR_FILTER_FREQ_##n, PAR_RESO_##n, PAR_FILTER_TYPE_##n, PAR_FILTER_DRIVE_##n, \
	PAR_VELOA##n, PAR_VELOD##n, PAR_VOL_SLOPE##n, \
	PAR_MOD_EG##n, PAR_MODAMNT##n, PAR_PITCH_SLOPE##n, \
	PAR_FMAMNT##n, PAR_FM_FREQ##n, PAR_MIX_MOD_##n, \
	PAR_DRIVE##n, PAR_VOICE_DECIMATION##n, \
	PAR_VOLUME_MOD_ON_OFF##n, PAR_VELO_MOD_AMT_##n, PAR_VEL_DEST_##n, \
	PAR_TRANS##n##_VOL, PAR_TRANS##n##_WAVE, PAR_TRANS##n##_FREQ, \
	PAR_FREQ_LFO##n, PAR_AMOUNT_LFO##n, PAR_WAVE_LFO##n, PAR_VOICE_LFO##n, \
	PAR_TARGET_LFO##n, PAR_RETRIGGER_LFO##n, PAR_SYNC_LFO##n, PAR_OFFSET_LFO##n }

static const uint16_t drumParams[3][VOICE_PARAMS_DRUM] = {
	DRUM_TABLE(1), DRUM_TABLE(2), DRUM_TABLE(3)
};

static const uint16_t snareParams[VOICE_PARAMS_SNARE] = {
	PAR_OSC_WAVE_SNARE, PAR_COARSE4, PAR_FINE4, PAR_PWM_4,
	PAR_NOISE_FREQ1, PAR_MIX1,
	PAR_FILTER_FREQ_4, PAR_RESO_4, PAR_FILTER_TYPE_4, PAR_FILTER_DRIVE_4,
	PAR_VELOA4, PAR_VELOD4, PAR_VOL_SLOPE4, PAR_REPEAT4,
	PAR_MOD_EG4, PAR_MODAMNT4, PAR_PITCH_SLOPE4,
	PAR_SNARE_DISTORTION, PAR_VOICE_DECIMATION4,
	PAR_VOLUME_MOD_ON_OFF4, PAR_VELO_MOD_AMT_4, PAR_VEL_DEST_4,
	PAR_TRANS4_VOL, PAR_TRANS4_WAVE, PAR_TRANS4_FREQ,
	PAR_FREQ_LFO4, PAR_AMOUNT_LFO4, PAR_WAVE_LFO4, PAR_VOICE_LFO4,
	PAR_TARGET_LFO4, PAR_RETRIGGER_LFO4, PAR_SYNC_LFO4, PAR_OFFSET_LFO4
};

static const uint16_t cymbalParams[VOICE_PARAMS_CYMBAL] = {
	PAR_WAVE1_CYM, PAR_WAVE2_CYM, PAR_WAVE3_CYM, PAR_COARSE5, PAR_FINE5, PAR_PWM_5,
	PAR_MOD_OSC_F1_CYM, PAR_MOD_OSC_F2_CYM, PAR_MOD_OSC_GAIN1_CYM, PAR_MOD_OSC_GAIN2_CYM,
	PAR_FILTER_FREQ_5, PAR_RESO_5, PAR_FILTER_TYPE_5, PAR_FILTER_DRIVE_5,
	PAR_VELOA5, PAR_VELOD5, PAR_VOL_SLOPE5, PAR_REPEAT5,
	PAR_CYMBAL_DISTORTION, PAR_VOICE_DECIMATION5,
	PAR_VOLUME_MOD_ON_OFF5, PAR_VELO_MOD_AMT_5, PAR_VEL_DEST_5,
	PAR_TRANS5_VOL, PAR_TRANS5_WAVE, PAR_TRANS5_FREQ,
	PAR_FREQ_LFO5, PAR_AMOUNT_LFO5, PAR_WAVE_LFO5, PAR_VOICE_LFO5,
	PAR_TARGET_LFO5, PAR_RETRIGGER_LFO5, PAR_SYNC_LFO5, PAR_OFFSET_LFO5
};

static const uint16_t hihatParams[VOICE_PARAMS_HIHAT] = {
	PAR_WAVE1_HH, PAR_WAVE2_HH, PAR_WAVE3_HH, PAR_COARSE6, PAR_FINE6, PAR_PWM_6,
	PAR_MOD_OSC_F1, PAR_MOD_OSC_F2, PAR_MOD_OSC_GAIN1, PAR_MOD_OSC_GAIN2,
	PAR_FILTER_FREQ_6, PAR_RESO_6, PAR_FILTER_TYPE_6, PAR_FILTER_DRIVE_6,
	PAR_VELOA6, PAR_VELOD6_CLOSED, PAR_VELOD6_OPEN, PAR_VOL_SLOPE6,
	PAR_HAT_DISTORTION, PAR_VOICE_DECIMATION6,
	PAR_VOLUME_MOD_ON_OFF6, PAR_VELO_MOD_AMT_6, PAR_VEL_DEST_6,
	PAR_TRANS6_VOL, PAR_TRANS6_WAVE, PAR_TRANS6_FREQ,
	PAR_FREQ_LFO6, PAR_AMOUNT_LFO6, PAR_WAVE_LFO6, PAR_VOICE_LFO6,
	PAR_TARGET_LFO6, PAR_RETRIGGER_LFO6, PAR_SYNC_LFO6, PAR_OFFSET_LFO6
};

/* -------------------------------------------------------------------------
 * Self-reference encoding used inside a voice sound buffer.
 * Velocity and LFO targets are stored in the kit as absolute indices that
 * include the voice number. A sound that modulates ITSELF is stored
 * voice-relative so it keeps modulating itself in its new slot.
 * ---------------------------------------------------------------------- */
#define SELF_VOICE		0x00	/* LFO target voice: "this voice" (kit uses 1-6) */
#define SELF_RETRIG		0x80	/* LFO retrigger: "this voice" (kit uses 0=off, 1-6) */

/* ---------------------------------------------------------------------- */
/* Relative target -> absolute target on 'voice', or 0 (off) if the result
 * isn't one of that voice's own targets. Real sounds always map inside the
 * range; this only catches damaged data (the gap map also has two entries
 * per voice past the end of each voice's block in modTargets). */
static uint8_t voicePreset_targetFromRelative(uint8_t voice, uint8_t rel)
{
	const uint8_t t = getModTargetIdxFromGapIdx(voice, rel);
	if(t < modTargetVoiceOffsets[voice].start || t > modTargetVoiceOffsets[voice].end) return 0;
	return t;
}
/* ---------------------------------------------------------------------- */
VoiceType voicePreset_typeOfVoice(uint8_t voice)
{
	switch(voice)
	{
	case 0: case 1: case 2:	return VOICE_TYPE_DRUM;
	case 3:					return VOICE_TYPE_SNARE;
	case 4:					return VOICE_TYPE_CYMBAL;
	default:				return VOICE_TYPE_HIHAT;
	}
}
/* ---------------------------------------------------------------------- */
static const uint16_t* voicePreset_table(uint8_t voice)
{
	switch(voice)
	{
	case 0: case 1: case 2:	return drumParams[voice];
	case 3:					return snareParams;
	case 4:					return cymbalParams;
	default:				return hihatParams;
	}
}
/* ---------------------------------------------------------------------- */
uint8_t voicePreset_numParams(uint8_t voice)
{
	switch(voicePreset_typeOfVoice(voice))
	{
	case VOICE_TYPE_DRUM:	return VOICE_PARAMS_DRUM;
	case VOICE_TYPE_SNARE:	return VOICE_PARAMS_SNARE;
	case VOICE_TYPE_CYMBAL:	return VOICE_PARAMS_CYMBAL;
	default:				return VOICE_PARAMS_HIHAT;
	}
}
/* ---------------------------------------------------------------------- */
uint16_t voicePreset_paramIndex(uint8_t voice, uint8_t k)
{
	return voicePreset_table(voice)[k];
}
/* ---------------------------------------------------------------------- */
uint8_t voicePreset_ownerOfParam(uint16_t paramIdx)
{
	static const uint16_t pans[NUM_VOICES] = { PAR_PAN1, PAR_PAN2, PAR_PAN3, PAR_PAN4, PAR_PAN5, PAR_PAN6 };
	if(paramIdx >= PAR_VOL1 && paramIdx < PAR_VOL1 + NUM_VOICES) return (uint8_t)(paramIdx - PAR_VOL1);
	if(paramIdx >= PAR_AUDIO_OUT1 && paramIdx < PAR_AUDIO_OUT1 + NUM_VOICES) return (uint8_t)(paramIdx - PAR_AUDIO_OUT1);
	if(paramIdx >= PAR_CV_1VOCT_1 && paramIdx < PAR_CV_1VOCT_1 + NUM_VOICES) return (uint8_t)(paramIdx - PAR_CV_1VOCT_1);
	for(uint8_t v = 0; v < NUM_VOICES; v++)
	{
		if(paramIdx == pans[v]) return v;
		const uint16_t *t = voicePreset_table(v);
		const uint8_t n = voicePreset_numParams(v);
		for(uint8_t k = 0; k < n; k++)
		{
			if(t[k] == paramIdx) return v;
		}
	}
	return VOICEPRESET_KIT_LEVEL;
}
/* ---------------------------------------------------------------------- */
uint8_t voicePreset_compatible(uint8_t voiceA, uint8_t voiceB)
{
	return voicePreset_typeOfVoice(voiceA) == voicePreset_typeOfVoice(voiceB);
}
/* ---------------------------------------------------------------------- */
/* position of a parameter inside a voice table, 0xFF if not present */
static uint8_t voicePreset_find(uint8_t voice, uint16_t param)
{
	const uint16_t *t = voicePreset_table(voice);
	const uint8_t n = voicePreset_numParams(voice);
	for(uint8_t k=0; k<n; k++)
	{
		if(t[k] == param) return k;
	}
	return 0xFF;
}
/* ---------------------------------------------------------------------- */
void voicePreset_extract(const uint8_t *kitParams, uint8_t voice, uint8_t *sound)
{
	const uint16_t *t = voicePreset_table(voice);
	const uint8_t n = voicePreset_numParams(voice);
	const uint8_t self = (uint8_t)(voice + 1); /* kit voice numbers are 1 based */

	for(uint8_t k=0; k<n; k++)
	{
		sound[k] = kitParams[t[k]];
	}

	/* velocity destination always targets the voice itself -> store relative */
	const uint8_t kVel = voicePreset_find(voice, (uint16_t)(PAR_VEL_DEST_1 + voice));
	sound[kVel] = getModTargetGapIndex(kitParams[PAR_VEL_DEST_1 + voice]);

	/* LFO: only rewrite when it points at the voice itself */
	const uint8_t kVoice = voicePreset_find(voice, (uint16_t)(PAR_VOICE_LFO1 + voice));
	const uint8_t kTarg  = voicePreset_find(voice, (uint16_t)(PAR_TARGET_LFO1 + voice));
	const uint8_t kRetr  = voicePreset_find(voice, (uint16_t)(PAR_RETRIGGER_LFO1 + voice));

	if(kitParams[PAR_VOICE_LFO1 + voice] == self)
	{
		sound[kVoice] = SELF_VOICE;
		sound[kTarg]  = getModTargetGapIndex(kitParams[PAR_TARGET_LFO1 + voice]);
	}
	if(kitParams[PAR_RETRIGGER_LFO1 + voice] == self)
	{
		sound[kRetr] = SELF_RETRIG;
	}
}
/* ---------------------------------------------------------------------- */
uint8_t voicePreset_apply(uint8_t *kitParams, uint8_t voice, VoiceType soundType, const uint8_t *sound)
{
	if(soundType != voicePreset_typeOfVoice(voice)) return 0;

	const uint16_t *t = voicePreset_table(voice);
	const uint8_t n = voicePreset_numParams(voice);
	const uint8_t self = (uint8_t)(voice + 1);

	for(uint8_t k=0; k<n; k++)
	{
		kitParams[t[k]] = sound[k];
	}

	/* velocity destination: relative -> absolute for this voice */
	const uint8_t kVel = voicePreset_find(voice, (uint16_t)(PAR_VEL_DEST_1 + voice));
	kitParams[PAR_VEL_DEST_1 + voice] = voicePreset_targetFromRelative(voice, sound[kVel]);

	/* LFO self references */
	const uint8_t kVoice = voicePreset_find(voice, (uint16_t)(PAR_VOICE_LFO1 + voice));
	const uint8_t kTarg  = voicePreset_find(voice, (uint16_t)(PAR_TARGET_LFO1 + voice));
	const uint8_t kRetr  = voicePreset_find(voice, (uint16_t)(PAR_RETRIGGER_LFO1 + voice));

	if(sound[kVoice] == SELF_VOICE || sound[kVoice] > NUM_VOICES)
	{
		/* this voice (or a damaged value: fall back to this voice, target off) */
		const uint8_t valid = (sound[kVoice] == SELF_VOICE);
		kitParams[PAR_VOICE_LFO1 + voice]  = self;
		kitParams[PAR_TARGET_LFO1 + voice] = valid ? voicePreset_targetFromRelative(voice, sound[kTarg]) : 0;
	}
	else
	{
		/* aimed at a specific voice: keep it, but only with one of that voice's targets */
		const uint8_t tv = (uint8_t)(sound[kVoice] - 1);
		const uint8_t t = kitParams[PAR_TARGET_LFO1 + voice];
		if(t != 0 && (t < modTargetVoiceOffsets[tv].start || t > modTargetVoiceOffsets[tv].end))
		{
			kitParams[PAR_TARGET_LFO1 + voice] = 0;
		}
	}
	if(sound[kRetr] == SELF_RETRIG)
	{
		kitParams[PAR_RETRIGGER_LFO1 + voice] = self;
	}
	else if(sound[kRetr] > NUM_VOICES)
	{
		kitParams[PAR_RETRIGGER_LFO1 + voice] = 0;	/* damaged value: retrigger off */
	}
	return 1;
}
