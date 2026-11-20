/*
 * globalMix.c
 *
 * Global routing and mix settings. See globalMix.h.
 */

#include "globalMix.h"
#include "Parameters.h"
#include <string.h>

/* from PresetManager.h (not included: it pulls in the SD card headers) */
void preset_sendParameter(uint16_t idx);

GlobalMix globalMix;

static const uint16_t globalMix_panParams[GLOBALMIX_NUM_VOICES] =
	{ PAR_PAN1, PAR_PAN2, PAR_PAN3, PAR_PAN4, PAR_PAN5, PAR_PAN6 };

/* ---------------------------------------------------------------------- */
void globalMix_setDefaults(void)
{
	memset(&globalMix, 0, sizeof(globalMix));
	for(uint8_t v = 0; v < GLOBALMIX_NUM_VOICES; v++)
	{
		globalMix.vol[v] = 100;
		globalMix.pan[v] = 63;
	}
}
/* ---------------------------------------------------------------------- */
uint16_t globalMix_volParam(uint8_t voice)  { return (uint16_t)(PAR_VOL1 + voice); }
uint16_t globalMix_panParam(uint8_t voice)  { return globalMix_panParams[voice]; }
uint16_t globalMix_routeParam(uint8_t route)
{
	return (route == GLOBALMIX_ROUTE_FX_BUS) ? (uint16_t)PAR_FX_ROUTING : (uint16_t)(PAR_AUDIO_OUT1 + route);
}
/* ---------------------------------------------------------------------- */
/* returns 1 and sets *out if the parameter is replaced */
static uint8_t globalMix_lookup(uint16_t idx, uint8_t kitValue, uint8_t *out)
{
	(void)kitValue;
	if(idx >= PAR_AUDIO_OUT1 && idx < PAR_AUDIO_OUT1 + GLOBALMIX_NUM_VOICES)
	{
		const uint8_t r = globalMix.route[idx - PAR_AUDIO_OUT1];
		if(r == GLOBALMIX_KT) return 0;			/* follow the kit (which may say FX) */
		*out = (uint8_t)(r - 1);
		return 1;
	}
	if(idx == PAR_FX_ROUTING)
	{
		const uint8_t r = globalMix.route[GLOBALMIX_ROUTE_FX_BUS];
		if(r == GLOBALMIX_KT) return 0;
		*out = (uint8_t)(r - 1);
		return 1;
	}
	if(idx >= PAR_VOL1 && idx < PAR_VOL1 + GLOBALMIX_NUM_VOICES)
	{
		const uint8_t v = (uint8_t)(idx - PAR_VOL1);
		if(globalMix.src[v] != GLOBALMIX_SRC_GLOBAL) return 0;
		*out = globalMix.vol[v];
		return 1;
	}
	for(uint8_t v = 0; v < GLOBALMIX_NUM_VOICES; v++)
	{
		if(idx == globalMix_panParams[v])
		{
			if(globalMix.src[v] != GLOBALMIX_SRC_GLOBAL) return 0;
			*out = globalMix.pan[v];
			return 1;
		}
	}
	return 0;
}
/* ---------------------------------------------------------------------- */
uint8_t globalMix_filter(uint16_t paramIdx, uint8_t kitValue)
{
	uint8_t out;
	return globalMix_lookup(paramIdx, kitValue, &out) ? out : kitValue;
}
/* ---------------------------------------------------------------------- */
uint8_t globalMix_isOverridden(uint16_t paramIdx, uint8_t kitValue)
{
	uint8_t out;
	return globalMix_lookup(paramIdx, kitValue, &out);
}
/* ---------------------------------------------------------------------- */
void globalMix_serialize(uint8_t *buf)
{
	buf[0]='L'; buf[1]='X'; buf[2]='R'; buf[3]='G';
	buf[4] = 1;		/* version */
	memcpy(&buf[5],  globalMix.route, GLOBALMIX_ROUTES);
	memcpy(&buf[12], globalMix.src,   GLOBALMIX_NUM_VOICES);
	memcpy(&buf[18], globalMix.vol,   GLOBALMIX_NUM_VOICES);
	memcpy(&buf[24], globalMix.pan,   GLOBALMIX_NUM_VOICES);
}
/* ---------------------------------------------------------------------- */
uint8_t globalMix_deserialize(const uint8_t *buf, uint16_t len)
{
	globalMix_setDefaults();
	if(!buf || len < GLOBALMIX_EXT_LEN) return 0;
	if(buf[0]!='L' || buf[1]!='X' || buf[2]!='R' || buf[3]!='G' || buf[4]!=1) return 0;

	for(uint8_t i = 0; i < GLOBALMIX_ROUTES; i++)
	{
		const uint8_t maxRoute = (i == GLOBALMIX_ROUTE_FX_BUS) ? GLOBALMIX_NUM_OUTPUTS : GLOBALMIX_ROUTE_TO_FX;
		if(buf[5+i] > maxRoute) return 0;
	}
	for(uint8_t v = 0; v < GLOBALMIX_NUM_VOICES; v++)
	{
		if(buf[12+v] > GLOBALMIX_SRC_GLOBAL || buf[18+v] > 127 || buf[24+v] > 127) return 0;
	}
	memcpy(globalMix.route, &buf[5],  GLOBALMIX_ROUTES);
	memcpy(globalMix.src,   &buf[12], GLOBALMIX_NUM_VOICES);
	memcpy(globalMix.vol,   &buf[18], GLOBALMIX_NUM_VOICES);
	memcpy(globalMix.pan,   &buf[24], GLOBALMIX_NUM_VOICES);
	return 1;
}
/* ---------------------------------------------------------------------- */
void globalMix_applyAll(void)
{
	for(uint8_t r = 0; r < GLOBALMIX_ROUTES; r++) preset_sendParameter(globalMix_routeParam(r));
	for(uint8_t v = 0; v < GLOBALMIX_NUM_VOICES; v++)
	{
		preset_sendParameter(globalMix_volParam(v));
		preset_sendParameter(globalMix_panParam(v));
	}
}
