#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Parameters.h"
#include "Preset/voicePreset.h"
#include "menu/CcNr2Text.h"
#include "menu/menu.h"
extern const ModTargetVoiceOffset modTargetVoiceOffsets[6];
static int fails=0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)

/* kit-level parameters: deliberately not part of any voice sound */
static int isKitLevel(int p){
  if(p==PAR_NONE||p==PAR_NRPN_DATA_ENTRY_COARSE||p==PAR_NRPN_FINE||p==PAR_NRPN_COARSE||p==PAR_BANK_CHANGE) return 1;
  if(p>=PAR_VOL1&&p<=PAR_VOL6) return 1;            /* mix: mixer page */
  if(p>=PAR_PAN1&&p<=PAR_PAN3) return 1;
  if(p>=PAR_PAN4&&p<=PAR_PAN6) return 1;
  if(p>=PAR_AUDIO_OUT1&&p<=PAR_AUDIO_OUT6) return 1;
  if(p>=PAR_CV_1VOCT_1&&p<=PAR_CV_1VOCT_6) return 1;
  if(p==PAR_UNUSED_7||p==PAR_VOICE_DECIMATION_ALL||p==PAR_MORPH_KIT_DEST) return 1;
  if(p>=PAR_FX_TYPE&&p<=PAR_FX_DELAY_TYPE) return 1;
  if(p>=PAR_CV_DEST_1&&p<END_OF_SOUND_PARAMETERS) return 1;
  return 0;
}
static uint8_t randTarget(int v){ int s=modTargetVoiceOffsets[v].start,e=modTargetVoiceOffsets[v].end; return (uint8_t)(s+rand()%(e-s+1)); }

int main(void){
  /* 1. coverage: every kit parameter owned by exactly one voice or kit-level */
  int owner[END_OF_SOUND_PARAMETERS]; for(int i=0;i<END_OF_SOUND_PARAMETERS;i++) owner[i]=-1;
  for(int v=0;v<NUM_VOICES;v++) for(int k=0;k<voicePreset_numParams(v);k++){
    int p=voicePreset_paramIndex(v,k);
    CHECK(p>0 && p<END_OF_SOUND_PARAMETERS,"voice %d slot %d has bad index %d",v,k,p);
    CHECK(owner[p]==-1,"param %d in voice %d and voice %d",p,owner[p],v);
    CHECK(!isKitLevel(p),"kit-level param %d listed in voice %d",p,v);
    owner[p]=v;
  }
  int unowned=0; for(int p=0;p<END_OF_SOUND_PARAMETERS;p++) if(owner[p]==-1 && !isKitLevel(p)){ printf("  unclassified param %d\n",p); unowned++; }
  CHECK(unowned==0,"%d parameters belong to nothing",unowned);
  int nVoice=0,nKit=0; for(int p=0;p<END_OF_SOUND_PARAMETERS;p++){ if(owner[p]>=0) nVoice++; else nKit++; }
  printf("coverage: %d voice params + %d kit-level = %d (END_OF_SOUND_PARAMETERS=%d)\n",nVoice,nKit,nVoice+nKit,END_OF_SOUND_PARAMETERS);

  srand(1234);
  for(int iter=0;iter<2000;iter++){
    uint8_t kit[END_OF_SOUND_PARAMETERS],orig[END_OF_SOUND_PARAMETERS];
    for(int p=0;p<END_OF_SOUND_PARAMETERS;p++) kit[p]=rand()&127;
    for(int v=0;v<6;v++){ kit[PAR_VEL_DEST_1+v]= (rand()%4==0)?0:randTarget(v);
      int tv=(rand()%2)? v : rand()%6; kit[PAR_VOICE_LFO1+v]=tv+1; kit[PAR_TARGET_LFO1+v]=(rand()%4==0)?0:randTarget(tv);
      kit[PAR_RETRIGGER_LFO1+v]=rand()%7; }
    memcpy(orig,kit,sizeof kit);

    /* 2. extract + apply to the same voice is lossless */
    for(int v=0;v<6;v++){ uint8_t s[VOICE_PARAMS_MAX]; voicePreset_extract(kit,v,s);
      CHECK(voicePreset_apply(kit,v,voicePreset_typeOfVoice(v),s),"apply refused v%d",v); }
    CHECK(memcmp(kit,orig,sizeof kit)==0,"same-voice round trip changed the kit (iter %d)",iter);

    /* 3. move drum 1 -> drum 3 */
    uint8_t s[VOICE_PARAMS_MAX]; voicePreset_extract(orig,0,s);
    memcpy(kit,orig,sizeof kit); voicePreset_apply(kit,2,VOICE_TYPE_DRUM,s);
    for(int p=0;p<END_OF_SOUND_PARAMETERS;p++) if(owner[p]!=2) CHECK(kit[p]==orig[p],"move touched param %d outside voice 3",p);
    for(int k=0;k<VOICE_PARAMS_DRUM;k++){ int p1=voicePreset_paramIndex(0,k),p3=voicePreset_paramIndex(2,k);
      if(p1==PAR_VEL_DEST_1||p1==PAR_TARGET_LFO1||p1==PAR_VOICE_LFO1||p1==PAR_RETRIGGER_LFO1) continue;
      CHECK(kit[p3]==orig[p1],"plain param slot %d not copied",k); }
    /* velocity target: same logical target on the new voice */
    CHECK(getModTargetGapIndex(kit[PAR_VEL_DEST_3])==getModTargetGapIndex(orig[PAR_VEL_DEST_1]),"vel dest not remapped");
    CHECK(kit[PAR_VEL_DEST_3]==0 || (kit[PAR_VEL_DEST_3]>=modTargetVoiceOffsets[2].start && kit[PAR_VEL_DEST_3]<=modTargetVoiceOffsets[2].end),"vel dest outside voice 3 range");
    /* LFO pointing at itself follows the sound; pointing elsewhere stays */
    if(orig[PAR_VOICE_LFO1]==1){ CHECK(kit[PAR_VOICE_LFO3]==3,"self LFO not moved to voice 3");
      CHECK(getModTargetGapIndex(kit[PAR_TARGET_LFO3])==getModTargetGapIndex(orig[PAR_TARGET_LFO1]),"self LFO target not remapped"); }
    else { CHECK(kit[PAR_VOICE_LFO3]==orig[PAR_VOICE_LFO1] && kit[PAR_TARGET_LFO3]==orig[PAR_TARGET_LFO1],"foreign LFO target changed"); }
    if(orig[PAR_RETRIGGER_LFO1]==1) CHECK(kit[PAR_RETRIGGER_LFO3]==3,"self retrigger not moved");
    else CHECK(kit[PAR_RETRIGGER_LFO3]==orig[PAR_RETRIGGER_LFO1],"foreign retrigger changed");

    /* 4. incompatible types are refused and leave the kit alone */
    memcpy(kit,orig,sizeof kit);
    CHECK(voicePreset_apply(kit,5,VOICE_TYPE_DRUM,s)==0,"drum sound accepted by hihat");
    CHECK(memcmp(kit,orig,sizeof kit)==0,"refused apply still changed kit");
  }
  /* 5. damaged sound data (e.g. a corrupt file) never puts invalid LFO or velocity values in the kit */
  for(int iter=0; iter<500; iter++){
    uint8_t kit[END_OF_SOUND_PARAMETERS]={0}; uint8_t s[VOICE_PARAMS_MAX];
    for(int v=0; v<6; v++){
      for(int k=0;k<VOICE_PARAMS_MAX;k++) s[k]=(uint8_t)rand();
      voicePreset_apply(kit, v, voicePreset_typeOfVoice(v), s);
      CHECK(kit[PAR_VOICE_LFO1+v]>=1 && kit[PAR_VOICE_LFO1+v]<=6, "LFO voice %d out of range", kit[PAR_VOICE_LFO1+v]);
      CHECK(kit[PAR_RETRIGGER_LFO1+v]<=6, "LFO retrigger %d out of range", kit[PAR_RETRIGGER_LFO1+v]);
      int tv=kit[PAR_VOICE_LFO1+v]-1, t=kit[PAR_TARGET_LFO1+v];
      CHECK(t==0 || tv!=v || (t>=modTargetVoiceOffsets[v].start && t<=modTargetVoiceOffsets[v].end), "self LFO target %d outside voice range", t);
      int vd=kit[PAR_VEL_DEST_1+v];
      CHECK(vd==0 || (vd>=modTargetVoiceOffsets[v].start && vd<=modTargetVoiceOffsets[v].end), "velocity target %d outside voice range", vd);
    }
  }
  printf("%s (%d failures)\n", fails? "TESTS FAILED":"ALL TESTS PASSED", fails);
  return fails!=0;
}
