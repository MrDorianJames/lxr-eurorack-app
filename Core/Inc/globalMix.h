/*
 * globalMix.h
 *
 * Global routing and mix settings (config menu: rtg and mix pages).
 *
 * Routing: each voice and the FX bus is either "kt" (use what the kit says,
 * stock behaviour, including a kit that sends the voice to FX) or fixed to a
 * physical output, or (voices only) fixed to the FX bus. A fixed choice
 * always wins, so kit loads no longer move it.
 *
 * Mix: per voice, volume and pan come from the kit (stock) or from fixed
 * global values that kit loads and morph leave alone.
 *
 * The override is applied in midiParser_ccHandler, the one place every
 * real parameter change passes through (kit loads, morph, menu edits).
 * Modulation is centred on the overridden value.
 *
 * Stored after the stock block in glo.cfg. Stock firmware reads exactly
 * its own block and ignores the rest, so the file stays loadable on both.
 */

#ifndef INC_GLOBALMIX_H_
#define INC_GLOBALMIX_H_

#include <stdint.h>

#define GLOBALMIX_NUM_VOICES	6
#define GLOBALMIX_ROUTES		7		/* 6 voices + FX bus */
#define GLOBALMIX_ROUTE_FX_BUS	6		/* index of the FX bus in route[] */

#define GLOBALMIX_KT			0		/* route: follow the kit */
#define GLOBALMIX_NUM_OUTPUTS	6		/* St1 St2 L1 R1 L2 R2 */
#define GLOBALMIX_OUT_FX		6		/* kit routing value "FX" */
#define GLOBALMIX_ROUTE_TO_FX	7		/* route[] value: voice fixed to the FX bus */

#define GLOBALMIX_SRC_KIT		0
#define GLOBALMIX_SRC_GLOBAL	1

#define GLOBALMIX_EXT_LEN		30		/* bytes appended to glo.cfg */

typedef struct
{
	uint8_t route[GLOBALMIX_ROUTES];	/* 0 = kt, 1..6 = output St1..R2, 7 = FX (voices only) */
	uint8_t src[GLOBALMIX_NUM_VOICES];	/* volume/pan source */
	uint8_t vol[GLOBALMIX_NUM_VOICES];	/* 0..127 */
	uint8_t pan[GLOBALMIX_NUM_VOICES];	/* 0..127, 63 = centre */
} GlobalMix;

extern GlobalMix globalMix;

/* everything "kt"/"kit": identical to stock behaviour */
void globalMix_setDefaults(void);

/* value the engine should get for a parameter, given the kit's value */
uint8_t globalMix_filter(uint16_t paramIdx, uint8_t kitValue);

/* 1 if the global settings currently replace this parameter's kit value */
uint8_t globalMix_isOverridden(uint16_t paramIdx, uint8_t kitValue);

/* kit parameter indices */
uint16_t globalMix_volParam(uint8_t voice);
uint16_t globalMix_panParam(uint8_t voice);
uint16_t globalMix_routeParam(uint8_t route);	/* 0..5 voices, 6 FX bus */

/* glo.cfg extension block; deserialize falls back to defaults if the block
 * is missing (older or stock-written file) or damaged */
void globalMix_serialize(uint8_t *buf);
uint8_t globalMix_deserialize(const uint8_t *buf, uint16_t len);

/* re-send all affected parameters (after loading settings) */
void globalMix_applyAll(void);

#endif /* INC_GLOBALMIX_H_ */
