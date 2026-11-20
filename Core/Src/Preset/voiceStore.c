/*
 * voiceStore.c
 *
 * Voice preset storage on the SD card with a RAM cache.
 * See voiceStore.h for folders, naming and the file format.
 */

#include "Preset/voiceStore.h"
#include "../../Middlewares/Third_Party/FatFs/src/ff.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * RAM cache: every voice file is read at boot. Lives in normal RAM (.bss),
 * not DTCM, which is already mostly used by the sound engine.
 * 4 types * 128 slots * 43 bytes = about 22 KB.
 * ---------------------------------------------------------------------- */
typedef struct
{
	uint8_t used;
	char	name[VOICE_NAME_LEN];
	uint8_t params[VOICE_PARAMS_MAX];
} VoiceSlot;

static VoiceSlot voiceStore_cache[NUM_VOICE_TYPES][VOICE_SLOTS];
static uint16_t	 voiceStore_skipped;

/* the firmware's mounted card (FATFS/App/fatfs.c) */
extern FATFS SDFatFS;
extern char  SDPath[4];

static const char* const voiceStore_folders[NUM_VOICE_TYPES] =
{
	"/voices/drum",
	"/voices/snare",
	"/voices/cymbal",
	"/voices/hihat"
};

static const uint8_t voiceStore_counts[NUM_VOICE_TYPES] =
{
	VOICE_PARAMS_DRUM, VOICE_PARAMS_SNARE, VOICE_PARAMS_CYMBAL, VOICE_PARAMS_HIHAT
};

/* filenames found while scanning a folder, read after the folder is closed
 * (FatFs here allows only one open file or folder at a time). Room for two
 * files per slot, so a leftover broken file can't hide a good one. */
#define VOICE_SCAN_MAX (VOICE_SLOTS * 2)
static char    voiceStore_scanNames[VOICE_SCAN_MAX][13];
static uint8_t voiceStore_scanSlots[VOICE_SCAN_MAX];

/* ---------------------------------------------------------------------- */
/* copy an 8.3 filename (max 12 characters) into a 13 byte buffer */
static void voiceStore_copyFileName(char *dst, const char *src)
{
	size_t n = strlen(src);
	if(n > 12) n = 12;
	memcpy(dst, src, n);
	dst[n] = 0;
}
/* ---------------------------------------------------------------------- */
static uint8_t voiceStore_validArgs(VoiceType type, uint8_t slot)
{
	return (type < NUM_VOICE_TYPES) && (slot < VOICE_SLOTS);
}
/* ---------------------------------------------------------------------- */
/* characters allowed in an 8.3 filename without long filename support */
static char voiceStore_fileChar(char c)
{
	if(c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
	if(c >= 'A' && c <= 'Z') return c;
	if(c >= '0' && c <= '9') return c;
	switch(c)
	{
	case '!': case '#': case '$': case '%': case '&': case '\'': case '(':
	case ')': case '-': case '@': case '^': case '_': case '`': case '{':
	case '}': case '~':
		return c;
	default:
		return '_';
	}
}
/* ---------------------------------------------------------------------- */
void voiceStore_makeFileName(char *buf, uint8_t slot, const char *name)
{
	uint8_t pos = 0;
	uint8_t maxChars;

	/* slot number, 2 digits below 100, 3 digits above (same as kits) */
	if(slot > 99)
	{
		buf[pos++] = (char)('0' + slot/100);
		buf[pos++] = (char)('0' + (slot/10)%10);
		buf[pos++] = (char)('0' + slot%10);
		maxChars = 4;
	}
	else
	{
		buf[pos++] = (char)('0' + slot/10);
		buf[pos++] = (char)('0' + slot%10);
		maxChars = 5;
	}
	buf[pos++] = '-';

	/* start of the name, without trailing spaces */
	uint8_t len = VOICE_NAME_LEN;
	while(len > 0 && (name[len-1] == ' ' || name[len-1] == 0)) len--;
	if(len > maxChars) len = maxChars;

	for(uint8_t i=0; i<len; i++)
	{
		buf[pos++] = voiceStore_fileChar(name[i]);
	}
	memcpy(&buf[pos], ".VCE", 5); /* includes the terminating 0 */
}
/* ---------------------------------------------------------------------- */
/* slot number from a filename like "07-KICK8.VCE", -1 if not a voice file */
static int16_t voiceStore_slotFromFileName(const char *fname)
{
	int16_t slot = 0;
	uint8_t i = 0;

	while(fname[i] >= '0' && fname[i] <= '9')
	{
		slot = (int16_t)(slot*10 + (fname[i] - '0'));
		i++;
		if(i > 3) return -1;
	}
	if(i < 2 || fname[i] != '-') return -1;

	/* extension must be .VCE (FatFs reports short names in upper case) */
	const char *dot = strrchr(fname, '.');
	if(!dot) return -1;
	if(!((dot[1]=='V'||dot[1]=='v') && (dot[2]=='C'||dot[2]=='c') &&
		 (dot[3]=='E'||dot[3]=='e') && dot[4]==0)) return -1;

	if(slot >= VOICE_SLOTS) return -1;
	return slot;
}
/* ---------------------------------------------------------------------- */
static void voiceStore_makePath(char *path, VoiceType type, const char *fname)
{
	strcpy(path, voiceStore_folders[type]);
	strcat(path, "/");
	strcat(path, fname);
}
/* ---------------------------------------------------------------------- */
/* read and check one file into a cache slot; returns 1 on success */
static uint8_t voiceStore_readFile(VoiceType type, uint8_t slot, const char *fname)
{
	char path[32];
	FIL file;
	UINT bytesRead;
	uint8_t buf[VOICE_FILE_HEADER_LEN + VOICE_PARAMS_MAX];
	const uint8_t count = voiceStore_counts[type];
	const UINT expected = (UINT)(VOICE_FILE_HEADER_LEN + count);

	voiceStore_makePath(path, type, fname);
	if(f_open(&file, path, FA_READ) != FR_OK) return 0;

	uint8_t ok = (f_size(&file) == expected);
	if(ok)
	{
		ok = (f_read(&file, buf, expected, &bytesRead) == FR_OK) && (bytesRead == expected);
	}
	f_close(&file);
	if(!ok) return 0;

	/* header checks */
	if(buf[0]!='L' || buf[1]!='X' || buf[2]!='R' || buf[3]!='V') return 0;
	if(buf[4] != VOICE_FILE_VERSION) return 0;
	if(buf[5] != (uint8_t)type) return 0;		/* e.g. a drum file in the snare folder */
	if(buf[6] != count) return 0;

	VoiceSlot *s = &voiceStore_cache[type][slot];
	memcpy(s->name, &buf[8], VOICE_NAME_LEN);
	memcpy(s->params, &buf[VOICE_FILE_HEADER_LEN], count);
	s->used = 1;
	return 1;
}
/* ---------------------------------------------------------------------- */
static uint16_t voiceStore_scanFolder(VoiceType type)
{
	DIR dir;
	FILINFO fno;
	uint16_t loaded = 0;
	uint16_t numNames = 0;

	/* pass 1: collect candidate filenames while the folder is open */
	if(f_opendir(&dir, voiceStore_folders[type]) != FR_OK) return 0;
	for(;;)
	{
		if(f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
		if(fno.fattrib & AM_DIR) continue;

		const int16_t slot = voiceStore_slotFromFileName(fno.fname);
		if(slot < 0) continue;

		if(numNames < VOICE_SCAN_MAX)
		{
			voiceStore_copyFileName(voiceStore_scanNames[numNames], fno.fname);
			voiceStore_scanSlots[numNames] = (uint8_t)slot;
			numNames++;
		}
		else
		{
			voiceStore_skipped++;
		}
	}
	f_closedir(&dir);

	/* pass 2: with the folder closed, the first VALID file wins each slot */
	for(uint16_t i=0; i<numNames; i++)
	{
		const uint8_t slot = voiceStore_scanSlots[i];
		if(voiceStore_cache[type][slot].used)
		{
			voiceStore_skipped++;			/* extra file for a filled slot */
		}
		else if(voiceStore_readFile(type, slot, voiceStore_scanNames[i]))
		{
			loaded++;
		}
		else
		{
			voiceStore_skipped++;			/* damaged or foreign file */
		}
	}
	return loaded;
}
/* ---------------------------------------------------------------------- */
uint16_t voiceStore_init(void)
{
	uint16_t loaded = 0;

	memset(voiceStore_cache, 0, sizeof(voiceStore_cache));
	voiceStore_skipped = 0;

	/* FR_EXIST just means the folder is already there */
	f_mkdir("/voices");
	for(uint8_t t=0; t<NUM_VOICE_TYPES; t++)
	{
		f_mkdir(voiceStore_folders[t]);
	}

	for(uint8_t t=0; t<NUM_VOICE_TYPES; t++)
	{
		loaded += voiceStore_scanFolder((VoiceType)t);
	}
	return loaded;
}
/* ---------------------------------------------------------------------- */
uint16_t voiceStore_numSkipped(void)
{
	return voiceStore_skipped;
}
/* ---------------------------------------------------------------------- */
uint8_t voiceStore_isUsed(VoiceType type, uint8_t slot)
{
	if(!voiceStore_validArgs(type, slot)) return 0;
	return voiceStore_cache[type][slot].used;
}
/* ---------------------------------------------------------------------- */
const char* voiceStore_name(VoiceType type, uint8_t slot)
{
	if(!voiceStore_isUsed(type, slot)) return 0;
	return voiceStore_cache[type][slot].name;
}
/* ---------------------------------------------------------------------- */
const uint8_t* voiceStore_params(VoiceType type, uint8_t slot)
{
	if(!voiceStore_isUsed(type, slot)) return 0;
	return voiceStore_cache[type][slot].params;
}
/* ---------------------------------------------------------------------- */
int16_t voiceStore_nextFree(VoiceType type, uint8_t from)
{
	if(type >= NUM_VOICE_TYPES) return -1;
	for(uint16_t i=0; i<VOICE_SLOTS; i++)
	{
		const uint8_t slot = (uint8_t)((from + i) % VOICE_SLOTS);
		if(!voiceStore_cache[type][slot].used) return slot;
	}
	return -1;
}
/* ---------------------------------------------------------------------- */
/* remove every file for 'slot' except 'keep' */
static void voiceStore_removeStale(VoiceType type, uint8_t slot, const char *keep)
{
	DIR dir;
	FILINFO fno;
	char stale[4][13];
	uint8_t numStale = 0;
	char path[32];

	/* collect first, delete after closing the folder */
	if(f_opendir(&dir, voiceStore_folders[type]) != FR_OK) return;
	for(;;)
	{
		if(f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
		if(fno.fattrib & AM_DIR) continue;
		if(voiceStore_slotFromFileName(fno.fname) != slot) continue;
		if(strcmp(fno.fname, keep) == 0) continue;
		if(numStale < 4)
		{
			voiceStore_copyFileName(stale[numStale], fno.fname);
			numStale++;
		}
	}
	f_closedir(&dir);

	for(uint8_t i=0; i<numStale; i++)
	{
		voiceStore_makePath(path, type, stale[i]);
		f_unlink(path);
	}
}
/* ---------------------------------------------------------------------- */
/* After a failed write FatFs keeps the file locked (f_close only releases
 * it when its final flush succeeds). With one open file allowed, that would
 * block every later card access, kits included. Remounting clears the lock;
 * then the half-written file is removed so it can't shadow a good sound. */
static void voiceStore_recoverAfterWriteError(const char *partialPath)
{
	f_mount(0, (TCHAR const*)SDPath, 0);
	if(f_mount(&SDFatFS, (TCHAR const*)SDPath, 1) == FR_OK)
	{
		f_unlink(partialPath);
	}
}
/* ---------------------------------------------------------------------- */
VoiceStoreResult voiceStore_save(VoiceType type, uint8_t slot, const char *name, const uint8_t *params)
{
	if(!voiceStore_validArgs(type, slot) || !name || !params) return VOICESTORE_ERR_ARG;

	const uint8_t count = voiceStore_counts[type];
	uint8_t buf[VOICE_FILE_HEADER_LEN + VOICE_PARAMS_MAX];
	char cleanName[VOICE_NAME_LEN];
	char fname[13];
	char path[32];
	FIL file;
	UINT written;

	/* names are stored space padded, never with zero bytes */
	for(uint8_t i=0; i<VOICE_NAME_LEN; i++)
	{
		cleanName[i] = (name[i] == 0) ? ' ' : name[i];
	}

	buf[0]='L'; buf[1]='X'; buf[2]='R'; buf[3]='V';
	buf[4] = VOICE_FILE_VERSION;
	buf[5] = (uint8_t)type;
	buf[6] = count;
	buf[7] = 0;
	memcpy(&buf[8], cleanName, VOICE_NAME_LEN);
	memcpy(&buf[VOICE_FILE_HEADER_LEN], params, count);

	voiceStore_makeFileName(fname, slot, cleanName);
	voiceStore_makePath(path, type, fname);

	/* write the new file first, so a failed write never loses the old one */
	if(f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		voiceStore_recoverAfterWriteError(path);
		return VOICESTORE_ERR_SD;
	}
	FRESULT res = f_write(&file, buf, (UINT)(VOICE_FILE_HEADER_LEN + count), &written);
	FRESULT resClose = f_close(&file);
	if(res != FR_OK || resClose != FR_OK || written != (UINT)(VOICE_FILE_HEADER_LEN + count))
	{
		voiceStore_recoverAfterWriteError(path);
		return VOICESTORE_ERR_SD;
	}

	/* then remove an older file for this slot with a different name */
	voiceStore_removeStale(type, slot, fname);

	VoiceSlot *s = &voiceStore_cache[type][slot];
	memcpy(s->name, cleanName, VOICE_NAME_LEN);
	memcpy(s->params, params, count);
	s->used = 1;
	return VOICESTORE_OK;
}
