/* Host test: voice preset storage on the firmware's real FatFs code,
 * configured exactly like the module, against a simulated 8 MB SD card. */
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "Preset/voiceStore.h"

extern int ramdisk_failWrites;
static int fails = 0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

/* same names the firmware uses for its mounted card */
FATFS SDFatFS;
char  SDPath[4] = "";

static int fileExists(const char *path){ FILINFO f; return f_stat(path, &f) == FR_OK; }

static void writeRaw(const char *path, const void *data, UINT len)
{
	FIL f; UINT w;
	if(f_open(&f, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) { printf("setup: cannot create %s\n", path); fails++; return; }
	f_write(&f, data, len, &w); f_close(&f);
}

static void fillParams(uint8_t *p, uint8_t seed){ for(int i=0;i<VOICE_PARAMS_MAX;i++) p[i]=(uint8_t)((seed*31+i*7)&127); }

static int sameSlot(VoiceType t, uint8_t slot, const char *name, uint8_t seed, uint8_t count)
{
	uint8_t p[VOICE_PARAMS_MAX]; fillParams(p, seed);
	const char *n = voiceStore_name(t, slot); const uint8_t *q = voiceStore_params(t, slot);
	return n && q && memcmp(n, name, 8) == 0 && memcmp(q, p, count) == 0;
}

int main(void)
{
	static BYTE work[4096];
	CHECK(f_mkfs("", FM_ANY | FM_SFD, 0, work, sizeof work) == FR_OK, "format simulated card");
	CHECK(f_mount(&SDFatFS, SDPath, 1) == FR_OK, "mount");

	/* a stock kit file that must never be touched */
	f_mkdir("/proj00");
	const char kitData[] = "pretend kit data";
	writeRaw("/proj00/01-TEST.SND", kitData, sizeof kitData);

	/* 1. empty card: folders are created, nothing loaded */
	CHECK(voiceStore_init() == 0, "empty card loads nothing");
	CHECK(fileExists("/voices/drum") && fileExists("/voices/snare") &&
	      fileExists("/voices/cymbal") && fileExists("/voices/hihat"), "voice folders created");
	CHECK(voiceStore_init() == 0, "second init with existing folders is fine");

	/* 2. filenames follow the kit scheme */
	char fn[13];
	voiceStore_makeFileName(fn, 7, "KICK808 ");  CHECK(strcmp(fn, "07-KICK8.VCE") == 0, "name slot 7: %s", fn);
	voiceStore_makeFileName(fn, 127, "CRASHY  "); CHECK(strcmp(fn, "127-CRAS.VCE") == 0, "name slot 127: %s", fn);
	voiceStore_makeFileName(fn, 0, "HH      ");   CHECK(strcmp(fn, "00-HH.VCE") == 0, "trailing spaces trimmed: %s", fn);
	voiceStore_makeFileName(fn, 5, "a*b?c/d ");   CHECK(strcmp(fn, "05-A_B_C.VCE") == 0, "illegal characters replaced: %s", fn);
	voiceStore_makeFileName(fn, 3, "        ");   CHECK(strcmp(fn, "03-.VCE") == 0, "blank name: %s", fn);

	/* 3. save one sound per type and read it all back after a "reboot" */
	uint8_t p[VOICE_PARAMS_MAX];
	fillParams(p, 1); CHECK(voiceStore_save(VOICE_TYPE_DRUM,   7,   "KICK808 ", p) == VOICESTORE_OK, "save drum");
	fillParams(p, 2); CHECK(voiceStore_save(VOICE_TYPE_SNARE,  0,   "SNAP    ", p) == VOICESTORE_OK, "save snare");
	fillParams(p, 3); CHECK(voiceStore_save(VOICE_TYPE_CYMBAL, 127, "CRASHY  ", p) == VOICESTORE_OK, "save cymbal");
	fillParams(p, 4); CHECK(voiceStore_save(VOICE_TYPE_HIHAT,  100, "TICKY   ", p) == VOICESTORE_OK, "save hihat");
	fillParams(p, 5); CHECK(voiceStore_save(VOICE_TYPE_DRUM,   5,   "a*b?c/d ", p) == VOICESTORE_OK, "save odd name");
	CHECK(fileExists("/voices/drum/07-KICK8.VCE"), "drum file on card");
	CHECK(fileExists("/voices/cymbal/127-CRAS.VCE"), "cymbal file on card");
	CHECK(fileExists("/voices/hihat/100-TICK.VCE"), "hihat file on card");

	CHECK(voiceStore_init() == 5, "reboot loads 5 sounds");
	CHECK(voiceStore_numSkipped() == 0, "nothing skipped");
	CHECK(sameSlot(VOICE_TYPE_DRUM, 7, "KICK808 ", 1, VOICE_PARAMS_DRUM), "drum restored");
	CHECK(sameSlot(VOICE_TYPE_SNARE, 0, "SNAP    ", 2, VOICE_PARAMS_SNARE), "snare restored");
	CHECK(sameSlot(VOICE_TYPE_CYMBAL, 127, "CRASHY  ", 3, VOICE_PARAMS_CYMBAL), "cymbal restored");
	CHECK(sameSlot(VOICE_TYPE_HIHAT, 100, "TICKY   ", 4, VOICE_PARAMS_HIHAT), "hihat restored");
	CHECK(sameSlot(VOICE_TYPE_DRUM, 5, "a*b?c/d ", 5, VOICE_PARAMS_DRUM), "odd name kept in full inside file");
	CHECK(!voiceStore_isUsed(VOICE_TYPE_DRUM, 6) && voiceStore_name(VOICE_TYPE_DRUM, 6) == 0, "empty slot reads empty");

	/* 4. saving a slot under a new name replaces the old file */
	fillParams(p, 6); CHECK(voiceStore_save(VOICE_TYPE_DRUM, 7, "BOOMY   ", p) == VOICESTORE_OK, "rename save");
	CHECK(fileExists("/voices/drum/07-BOOMY.VCE") && !fileExists("/voices/drum/07-KICK8.VCE"), "old file replaced");
	CHECK(voiceStore_init() == 5 && sameSlot(VOICE_TYPE_DRUM, 7, "BOOMY   ", 6, VOICE_PARAMS_DRUM), "renamed sound survives reboot");

	/* 5. damaged or stray files are skipped, never loaded or fatal */
	uint8_t raw[VOICE_FILE_HEADER_LEN + VOICE_PARAMS_MAX] = {0};
	memcpy(raw, "XXXX", 4); writeRaw("/voices/drum/09-BAD.VCE", raw, VOICE_FILE_HEADER_LEN + VOICE_PARAMS_DRUM);
	memcpy(raw, "LXRV", 4); raw[4]=1; raw[5]=VOICE_TYPE_DRUM; raw[6]=VOICE_PARAMS_DRUM;
	writeRaw("/voices/snare/03-WRONG.VCE", raw, VOICE_FILE_HEADER_LEN + VOICE_PARAMS_DRUM); /* drum file in snare folder */
	writeRaw("/voices/drum/10-SHORT.VCE", raw, 20);                                        /* cut off */
	raw[4]=9; writeRaw("/voices/drum/11-NEWER.VCE", raw, VOICE_FILE_HEADER_LEN + VOICE_PARAMS_DRUM); /* unknown version */
	raw[4]=1; writeRaw("/voices/drum/07-DUP.VCE", raw, VOICE_FILE_HEADER_LEN + VOICE_PARAMS_DRUM);   /* second file for slot 7 */
	writeRaw("/voices/drum/200-X.VCE", raw, 10);   /* slot out of range: ignored */
	writeRaw("/voices/drum/NOTES.TXT", raw, 10);   /* not a voice file: ignored */
	f_mkdir("/voices/drum/12-SUB.VCE");            /* a folder with a voice-like name: ignored */
	CHECK(voiceStore_init() == 5, "bad files don't load (loaded %u)", 5);
	CHECK(voiceStore_numSkipped() == 5, "5 bad files skipped, got %u", voiceStore_numSkipped());
	CHECK(!voiceStore_isUsed(VOICE_TYPE_DRUM, 9) && !voiceStore_isUsed(VOICE_TYPE_SNARE, 3) &&
	      !voiceStore_isUsed(VOICE_TYPE_DRUM, 10) && !voiceStore_isUsed(VOICE_TYPE_DRUM, 11), "bad slots stay empty");
	CHECK(voiceStore_isUsed(VOICE_TYPE_DRUM, 7), "slot 7 still has one valid sound");

	/* 6. a failed card write keeps the old file and the old cached sound */
	ramdisk_failWrites = 1;
	fillParams(p, 7); CHECK(voiceStore_save(VOICE_TYPE_SNARE, 0, "LOST    ", p) == VOICESTORE_ERR_SD, "write error reported");
	ramdisk_failWrites = 0;
	CHECK(sameSlot(VOICE_TYPE_SNARE, 0, "SNAP    ", 2, VOICE_PARAMS_SNARE), "cache unchanged after failed save");
	{ DIR d; CHECK(f_opendir(&d, "/voices/snare") == FR_OK, "card usable again after the error (no stuck lock)"); f_closedir(&d); }
	writeRaw("/proj00/02-AFTER.SND", kitData, sizeof kitData);
	CHECK(fileExists("/proj00/02-AFTER.SND"), "kit saving still works after a voice save error");
	f_unlink("/proj00/02-AFTER.SND");
	/* a broken leftover listed BEFORE the good file must not hide it */
	writeRaw("/voices/snare/00-AAAA.VCE", "x", 1);
	{ /* make sure the broken file really is listed first: move the good one behind it */
		uint8_t good[VOICE_FILE_HEADER_LEN + VOICE_PARAMS_MAX]; FIL g; UINT n;
		f_open(&g, "/voices/snare/00-SNAP.VCE", FA_READ); f_read(&g, good, sizeof good, &n); f_close(&g);
		f_unlink("/voices/snare/00-SNAP.VCE"); f_unlink("/voices/snare/00-AAAA.VCE");
		writeRaw("/voices/snare/00-AAAA.VCE", "x", 1);
		writeRaw("/voices/snare/00-SNAP.VCE", good, n);
		DIR d; FILINFO fi; int posBad = -1, posGood = -1, i = 0;
		f_opendir(&d, "/voices/snare");
		while(f_readdir(&d, &fi) == FR_OK && fi.fname[0]) { if(!strcmp(fi.fname,"00-AAAA.VCE")) posBad=i; if(!strcmp(fi.fname,"00-SNAP.VCE")) posGood=i; i++; }
		f_closedir(&d);
		CHECK(posBad >= 0 && posGood > posBad, "setup: broken file listed first (bad %d, good %d)", posBad, posGood);
	}
	CHECK(voiceStore_init() >= 5 && sameSlot(VOICE_TYPE_SNARE, 0, "SNAP    ", 2, VOICE_PARAMS_SNARE), "good file wins over broken leftover");
	f_unlink("/voices/snare/00-AAAA.VCE");
	CHECK(voiceStore_init() >= 5 && sameSlot(VOICE_TYPE_SNARE, 0, "SNAP    ", 2, VOICE_PARAMS_SNARE), "card unchanged after failed save");

	/* 7. bad arguments are refused */
	CHECK(voiceStore_save(VOICE_TYPE_DRUM, 128, "X       ", p) == VOICESTORE_ERR_ARG, "slot 128 refused");
	CHECK(voiceStore_save(NUM_VOICE_TYPES, 0, "X       ", p) == VOICESTORE_ERR_ARG, "bad type refused");

	/* 8. next free slot and a completely full folder */
	CHECK(voiceStore_nextFree(VOICE_TYPE_HIHAT, 100) == 101, "next free after 100");
	CHECK(voiceStore_nextFree(VOICE_TYPE_CYMBAL, 127) == 0, "next free wraps around");
	for(int s=0; s<VOICE_SLOTS; s++){ fillParams(p, (uint8_t)s); char n[9]; snprintf(n, 9, "HAT%-5d", s);
		CHECK(voiceStore_save(VOICE_TYPE_HIHAT, (uint8_t)s, n, p) == VOICESTORE_OK, "fill slot %d", s); }
	CHECK(voiceStore_nextFree(VOICE_TYPE_HIHAT, 0) == -1, "full folder has no free slot");
	uint16_t loaded = voiceStore_init();
	int hats = 0; for(int s=0;s<VOICE_SLOTS;s++) hats += voiceStore_isUsed(VOICE_TYPE_HIHAT,(uint8_t)s);
	CHECK(hats == VOICE_SLOTS, "all 128 hihat sounds reload (got %d, total %u)", hats, loaded);

	/* 9. the kit folder was never touched */
	FIL f; char back[sizeof kitData]; UINT r;
	CHECK(f_open(&f, "/proj00/01-TEST.SND", FA_READ) == FR_OK, "kit file still there");
	f_read(&f, back, sizeof back, &r); f_close(&f);
	CHECK(r == sizeof kitData && memcmp(back, kitData, sizeof kitData) == 0, "kit file unchanged");

	printf("cache size: %u bytes\n", (unsigned)(NUM_VOICE_TYPES * VOICE_SLOTS * (1 + VOICE_NAME_LEN + VOICE_PARAMS_MAX)));
	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
