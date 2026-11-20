/*
 * voiceStore.h
 *
 * Voice preset storage on the SD card, with a RAM cache.
 *
 * Folders (created at boot if missing, next to /proj00):
 *   /voices/drum    sounds for voices 1-3
 *   /voices/snare   voice 4
 *   /voices/cymbal  voice 5
 *   /voices/hihat   voice 6
 *
 * Files are named like kits: "07-KICK8.vce" (slot, dash, start of name),
 * slots 0..VOICE_SLOTS-1. The full 8 character name is stored inside.
 *
 * File format, version 1 (all single bytes):
 *   0..3   magic 'L' 'X' 'R' 'V'
 *   4      format version (1)
 *   5      voice type (VoiceType)
 *   6      parameter count
 *   7      reserved (0)
 *   8..15  name, 8 characters, space padded
 *   16..   parameters, voice-relative order (see voicePreset.c)
 *
 * Every voice file is read into RAM at boot, so loading a sound during a
 * performance never touches the SD card. Only saving writes to the card.
 *
 * All functions must be called from the main loop, never from an
 * interrupt. FatFs is configured for one open file or folder at a time,
 * so every function here closes what it opens before returning.
 */

#ifndef INC_PRESET_VOICESTORE_H_
#define INC_PRESET_VOICESTORE_H_

#include <stdint.h>
#include "Preset/voicePreset.h"

#define VOICE_SLOTS				128
#define VOICE_NAME_LEN			8

#define VOICE_FILE_VERSION		1
#define VOICE_FILE_HEADER_LEN	16

typedef enum
{
	VOICESTORE_OK = 0,
	VOICESTORE_ERR_ARG,			/* bad type, slot or name */
	VOICESTORE_ERR_SD,			/* card error while writing */
} VoiceStoreResult;

/* Create the folders if needed and read every voice file into RAM.
 * Returns the number of voice files loaded. Invalid files are skipped. */
uint16_t voiceStore_init(void);

/* number of files that were skipped as invalid during the last init */
uint16_t voiceStore_numSkipped(void);

uint8_t voiceStore_isUsed(VoiceType type, uint8_t slot);

/* 8 characters, not zero terminated; NULL if the slot is empty */
const char* voiceStore_name(VoiceType type, uint8_t slot);

/* voice-relative parameters; NULL if the slot is empty */
const uint8_t* voiceStore_params(VoiceType type, uint8_t slot);

/* first empty slot at or after 'from' (wrapping); -1 if all are used */
int16_t voiceStore_nextFree(VoiceType type, uint8_t from);

/* Write a sound to the card and update the cache. Any older file for the
 * same slot (with a different name) is removed. 'name' is 8 characters. */
VoiceStoreResult voiceStore_save(VoiceType type, uint8_t slot, const char *name, const uint8_t *params);

/* Build the 8.3 filename for a slot and name (buf: at least 13 bytes). */
void voiceStore_makeFileName(char *buf, uint8_t slot, const char *name);

#endif /* INC_PRESET_VOICESTORE_H_ */
