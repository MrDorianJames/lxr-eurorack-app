/*
 * customVersion.h
 *
 * Revision of the custom firmware (voice presets, new Load/Save menu, ...)
 * on top of Erica Synths' firmware. Shown on the "Loading voices" boot
 * screen, so it's always clear which build is running. The stock version
 * number (FIRMWARE_VERSION) and the update image name are left unchanged,
 * because the bootloader looks for LXRE*.img files.
 */
#ifndef INC_CUSTOMVERSION_H_
#define INC_CUSTOMVERSION_H_

#define CUSTOM_FW_REVISION	"custom fw r17"	/* max 16 characters */

#endif /* INC_CUSTOMVERSION_H_ */
