/* Host test: glitch-free voice loading (voiceLoad.c) against a simulated engine. */
#include <stdio.h>
#include <string.h>
#include "Parameters.h"
#include "Preset/voicePreset.h"
#include "Preset/voiceLoad.h"
#include "menu/menu.h"

static int fails = 0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

/* --- the firmware's kit buffers ------------------------------------------ */
uint8_t activeParameters[NUM_PARAMS];
uint8_t mainParameters[NUM_KIT_SLOTS][END_OF_SOUND_PARAMETERS];

/* --- simulated engine ---------------------------------------------------- */
static uint8_t quiet[NUM_VOICES];
static int quietCalls;               /* to simulate a trigger between two checks */
static int triggerOnCall = -1; static uint8_t triggerVoice;
uint8_t voiceLoad_isVoiceQuiet(uint8_t voice)
{
	quietCalls++;
	if(quietCalls == triggerOnCall) quiet[triggerVoice] = 0;   /* a note starts right now */
	return quiet[voice];
}

/* record every parameter sent to the engine, and the guard flag at that time */
static int sent[256]; static int numSent; static int sentWithoutGuard; static int sentVoice;
void preset_sendParameter(uint16_t idx)
{
	CHECK(idx < 256, "sent out-of-range index %u", idx);
	if(idx < 256) sent[idx]++;
	numSent++;
	if(!voiceLoad_isApplying((uint8_t)sentVoice)) sentWithoutGuard++;
}
static void resetSends(int voice){ memset(sent,0,sizeof sent); numSent=0; sentWithoutGuard=0; sentVoice=voice; }

static void soundFor(uint8_t voice, uint8_t seed, uint8_t *s)
{ for(int k=0;k<voicePreset_numParams(voice);k++) s[k]=(uint8_t)((seed+k*5)&127); }

int main(void)
{
	uint8_t snd[VOICE_PARAMS_MAX], before[NUM_PARAMS];
	for(int p=0;p<NUM_PARAMS;p++) activeParameters[p]=(uint8_t)(p&127);
	for(int v=0;v<NUM_VOICES;v++){ activeParameters[PAR_VEL_DEST_1+v]=0; activeParameters[PAR_VOICE_LFO1+v]=(uint8_t)(v+1); activeParameters[PAR_TARGET_LFO1+v]=0; activeParameters[PAR_RETRIGGER_LFO1+v]=0; }

	/* 1. quiet voice: applied on the next tick, only that voice changes */
	memset(quiet,1,sizeof quiet); soundFor(0,10,snd); memcpy(before,activeParameters,sizeof before); resetSends(0);
	CHECK(voiceLoad_request(0, VOICE_TYPE_DRUM, snd, 1000), "request accepted");
	CHECK(voiceLoad_isPending(0), "pending after request");
	voiceLoad_tick(1001);
	CHECK(!voiceLoad_isPending(0), "applied on the first quiet tick");
	CHECK(numSent == voicePreset_numParams(0), "sent %d params, expected %d", numSent, voicePreset_numParams(0));
	for(int k=0;k<voicePreset_numParams(0);k++) CHECK(sent[voicePreset_paramIndex(0,k)]==1, "param slot %d sent once", k);
	CHECK(sentWithoutGuard == 0, "%d params sent without the trigger guard raised", sentWithoutGuard);
	CHECK(!voiceLoad_isApplying(0), "guard lowered after applying");
	int otherChanged=0;
	for(int p=0;p<NUM_PARAMS;p++){ int mine=0; for(int k=0;k<voicePreset_numParams(0);k++) if(voicePreset_paramIndex(0,k)==p) mine=1; if(!mine && activeParameters[p]!=before[p]) otherChanged++; }
	CHECK(otherChanged==0, "%d parameters outside voice 1 changed", otherChanged);
	CHECK(activeParameters[PAR_COARSE1]==snd[1] && activeParameters[PAR_OSC_WAVE_DRUM1]==snd[0], "voice 1 has the new sound");

	/* 2. busy voice: waits, then gives up waiting after the timeout */
	quiet[3]=0; soundFor(3,20,snd); resetSends(3);
	voiceLoad_request(3, VOICE_TYPE_SNARE, snd, 5000);
	voiceLoad_tick(5001); voiceLoad_tick(5000+VOICELOAD_TIMEOUT_MS-1);
	CHECK(voiceLoad_isPending(3) && numSent==0, "busy voice not changed before the timeout");
	voiceLoad_tick(5000+VOICELOAD_TIMEOUT_MS);
	CHECK(!voiceLoad_isPending(3) && numSent==voicePreset_numParams(3), "applied at the timeout");
	CHECK(sentWithoutGuard == 0, "timeout path also raises the guard");

	/* 3. busy voice that goes quiet before the timeout */
	quiet[4]=0; soundFor(4,30,snd); resetSends(4);
	voiceLoad_request(4, VOICE_TYPE_CYMBAL, snd, 8000);
	voiceLoad_tick(8100); CHECK(voiceLoad_isPending(4), "still busy");
	quiet[4]=1; voiceLoad_tick(8150);
	CHECK(!voiceLoad_isPending(4) && numSent==voicePreset_numParams(4), "applied once quiet");

	/* 4. a trigger lands between the quiet check and raising the guard */
	quiet[5]=1; soundFor(5,40,snd); resetSends(5);
	voiceLoad_request(5, VOICE_TYPE_HIHAT, snd, 9000);
	quietCalls=0; triggerOnCall=2; triggerVoice=5;    /* 1st check quiet, 2nd sees the new hit */
	voiceLoad_tick(9001);
	CHECK(voiceLoad_isPending(5) && numSent==0, "backed off when a hit started during the swap");
	CHECK(!voiceLoad_isApplying(5), "guard lowered after backing off");
	triggerOnCall=-1; quiet[5]=1; voiceLoad_tick(9050);
	CHECK(!voiceLoad_isPending(5) && numSent==voicePreset_numParams(5), "applied at the next quiet moment");

	/* 5. a newer request replaces an older one */
	quiet[1]=0; uint8_t a[VOICE_PARAMS_MAX], b[VOICE_PARAMS_MAX]; soundFor(1,50,a); soundFor(1,60,b); resetSends(1);
	voiceLoad_request(1, VOICE_TYPE_DRUM, a, 10000);
	voiceLoad_request(1, VOICE_TYPE_DRUM, b, 10010);
	quiet[1]=1; voiceLoad_tick(10020);
	CHECK(activeParameters[PAR_OSC_WAVE_DRUM2]==b[0] && activeParameters[PAR_COARSE2]==b[1], "latest sound wins");
	CHECK(numSent==voicePreset_numParams(1), "applied once, not twice");

	/* 6. wrong type, bad voice and cancel */
	CHECK(!voiceLoad_request(5, VOICE_TYPE_DRUM, a, 0), "drum sound refused for the hihat");
	CHECK(!voiceLoad_request(6, VOICE_TYPE_DRUM, a, 0), "voice 7 refused");
	quiet[2]=0; voiceLoad_request(2, VOICE_TYPE_DRUM, a, 11000); voiceLoad_cancel(2);
	resetSends(2); voiceLoad_tick(11000+VOICELOAD_TIMEOUT_MS*2);
	CHECK(!voiceLoad_isPending(2) && numSent==0, "cancelled sound never applied");
	voiceLoad_request(2, VOICE_TYPE_DRUM, a, 12000); voiceLoad_request(4, VOICE_TYPE_CYMBAL, snd, 12000); voiceLoad_cancelAll();
	CHECK(!voiceLoad_isPending(2) && !voiceLoad_isPending(4), "cancel all");

	/* 7. the millisecond counter wrapping around (after ~49 days) */
	quiet[0]=0; resetSends(0); soundFor(0,70,snd);
	voiceLoad_request(0, VOICE_TYPE_DRUM, snd, 0xFFFFFF00u);
	voiceLoad_tick(0xFFFFFF10u); CHECK(voiceLoad_isPending(0), "no early timeout before wrap");
	voiceLoad_tick((uint32_t)(0xFFFFFF00u + VOICELOAD_TIMEOUT_MS));
	CHECK(!voiceLoad_isPending(0), "timeout works across the wrap");

	printf("%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
	return fails != 0;
}
