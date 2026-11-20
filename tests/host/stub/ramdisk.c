/* Simulated SD card for host tests: FatFs disk I/O on a RAM buffer. */
#include <string.h>
#include "ff.h"
#include "diskio.h"

#define RAMDISK_SECTORS 16384u              /* 8 MB */
static BYTE ramdisk[RAMDISK_SECTORS * 512u];
int ramdisk_failWrites = 0;                 /* tests set this to simulate a card error */

DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_status(BYTE pdrv)     { (void)pdrv; return 0; }

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
	(void)pdrv;
	if(sector + count > RAMDISK_SECTORS) return RES_PARERR;
	memcpy(buff, &ramdisk[sector * 512u], count * 512u);
	return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
	(void)pdrv;
	if(ramdisk_failWrites) return RES_ERROR;
	if(sector + count > RAMDISK_SECTORS) return RES_PARERR;
	memcpy(&ramdisk[sector * 512u], buff, count * 512u);
	return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
	(void)pdrv;
	switch(cmd)
	{
	case CTRL_SYNC:        return RES_OK;
	case GET_SECTOR_COUNT: *(DWORD*)buff = RAMDISK_SECTORS; return RES_OK;
	case GET_SECTOR_SIZE:  *(WORD*)buff = 512; return RES_OK;
	case GET_BLOCK_SIZE:   *(DWORD*)buff = 1; return RES_OK;
	default:               return RES_PARERR;
	}
}

DWORD get_fattime(void) { return ((DWORD)(2026 - 1980) << 25) | (11u << 21) | (1u << 16); }
