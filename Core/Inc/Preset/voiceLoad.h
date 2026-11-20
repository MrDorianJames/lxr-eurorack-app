/*
 * voiceLoad.h
 *
 * Glitch-free loading of a voice sound while the module is playing.
 *
 * A requested sound is staged and applied from the main loop as soon as
 * the voice is quiet (its volume envelope has finished or decayed to near
 * silence), so a sound never changes in the middle of a hit. If the voice
 * never goes quiet (very long decays, constant retriggering) the sound is
 * applied after VOICELOAD_TIMEOUT_MS anyway, which is how stock kit loads
 * always behave.
 *
 * While a sound is being applied, the SysTick trigger handler postpones a
 * note for that voice by one tick (1 ms), so a hit can never start with a
 * half-applied sound.
 */

#ifndef INC_PRESET_VOICELOAD_H_
#define INC_PRESET_VOICELOAD_H_

#include <stdint.h>
#include "Preset/voicePreset.h"

#define VOICELOAD_TIMEOUT_MS	500

/* Stage a sound for a voice (0..5). A newer request replaces an older one
 * that hasn't been applied yet. Returns 0 if the sound type doesn't fit. */
uint8_t voiceLoad_request(uint8_t voice, VoiceType type, const uint8_t *sound, uint32_t nowMs);

/* Drop a staged sound, e.g. when a whole kit is loaded instead. */
void voiceLoad_cancel(uint8_t voice);
void voiceLoad_cancelAll(void);

uint8_t voiceLoad_isPending(uint8_t voice);

/* Call regularly from the main loop. */
void voiceLoad_tick(uint32_t nowMs);

/* Read by the SysTick trigger handler: 1 while a voice is being changed. */
uint8_t voiceLoad_isApplying(uint8_t voice);

/* Engine glue (voiceLoad_engine.c): 1 if the voice is silent right now. */
uint8_t voiceLoad_isVoiceQuiet(uint8_t voice);

#endif /* INC_PRESET_VOICELOAD_H_ */
