#include "cmd_common.h"
#include "firmware.h"

void CmdHelpFix(void) {
	puts("");
	puts("Usage: fix");
	puts("");
	puts("Fixes incorrect fields in the firmware. The CRCs of the static, secondary, and");
	puts("resource modules are corrected. The CRCs of the wireless initialization tables,");
	puts("user configuration, and wireless connection settings are corrected.");
}

static void UpgradeNcdVersion(FlashUserConfigData *ncd) {
	if (ncd->version == 5) return; // correct
	
	printf("Unspported user configuration version %d.\n", ncd->version);
}

void CmdProxFix(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	FlashHeader *hdr = (FlashHeader *) buffer;
	FlashRfBbInfo *wl = (FlashRfBbInfo *) (buffer + 0x2A);
	
	uint32_t arm9SecondaryRomAddr, arm9SecondarySize, arm9SecondaryRamAddr, arm9SecondaryUncompressed;
	uint32_t arm7SecondaryRomAddr, arm7SecondarySize, arm7SecondaryRamAddr, arm7SecondaryUncompressed;
	uint32_t    arm9StaticRomAddr,    arm9StaticSize,    arm9StaticRamAddr,    arm9StaticUncompressed;
	uint32_t    arm7StaticRomAddr,    arm7StaticSize,    arm7StaticRamAddr,    arm7StaticUncompressed;
	uint32_t          rsrcRomAddr,          rsrcSize,          rsrcRamAddr,          rsrcUncompressed;
	
	//unpack firmware and data headers
	CxCompressionType type9, type7, typeRsrc;
	unsigned char *arm9Static = GetArm9StaticInfo(buffer, size, &arm9StaticRomAddr, &arm9StaticRamAddr, &arm9StaticSize, &arm9StaticUncompressed);
	unsigned char *arm7Static = GetArm7StaticInfo(buffer, size, &arm7StaticRomAddr, &arm7StaticRamAddr, &arm7StaticSize, &arm7StaticUncompressed);
	unsigned char *arm9Secondary = GetArm9SecondaryInfo(buffer, size, &arm9SecondaryRomAddr, &arm9SecondaryRamAddr, &arm9SecondarySize, &arm9SecondaryUncompressed, &type9);
	unsigned char *arm7Secondary = GetArm7SecondaryInfo(buffer, size, &arm7SecondaryRomAddr, &arm7SecondaryRamAddr, &arm7SecondarySize, &arm7SecondaryUncompressed, &type7);
	unsigned char *rsrc = GetResourcesPackInfo(buffer, size, &rsrcRomAddr, &rsrcRamAddr, &rsrcSize, &rsrcUncompressed, &typeRsrc);
	
	
	//1. correct static module CRC
	if (arm9Static != NULL && arm7Static != NULL) {
		uint16_t staticCrc = hdr->staticCrc;
		uint16_t staticCrc2 = ComputeStaticCrc(arm9Static, arm9StaticUncompressed, arm7Static, arm7StaticUncompressed);
		if (staticCrc != staticCrc2) {
			hdr->staticCrc = staticCrc2;
			printf("Corrected static module CRC (%04X -> %04X)\n", staticCrc, staticCrc2);
		}
	} else {
		puts("Could not decompress static modules.");
	}
	
	//2. correct secondary module CRC
	if (arm9Secondary != NULL && arm7Secondary != NULL) {
		uint16_t secondaryCrc = hdr->secondaryCrc;
		uint16_t secondaryCrc2 = ComputeSecondaryCrc(arm9Secondary, arm9SecondaryUncompressed, arm7Secondary, arm7SecondaryUncompressed);
		if (secondaryCrc != secondaryCrc2) {
			hdr->secondaryCrc = secondaryCrc2;
			printf("Corrected secondary module CRC (%04X -> %04X)\n", secondaryCrc, secondaryCrc2);
		}
	} else {
		puts("Could not decompress secondary modules.");
	}
	
	//3. correct resources CRC
	if (rsrc != NULL) {
		uint16_t rsrcCrc = hdr->resourceCrc;
		uint16_t rsrcCrc2 = ComputeCrc(rsrc, rsrcUncompressed, 0xFFFF);
		if (rsrcCrc != rsrcCrc2) {
			hdr->resourceCrc = rsrcCrc2;
			printf("Corrected resources pack CRC (%04X -> %04X)\n", rsrcCrc, rsrcCrc2);
		}
	} else {
		puts("Could not decompress resources pack.");
	}
	
	//4. correct wireless table CRC
	{
		uint16_t wlCrc  = wl->crc;
		uint16_t wlCrc2 = ComputeCrc(buffer + 0x2A + 2, wl->tableSize, 0);
		if (wl->tableSize < (0x200 - 0x2E) && wlCrc != wlCrc2) {
			wl->crc = wlCrc2;
			printf("Corrected wireless init CRC (%04X -> %04X)\n", wlCrc, wlCrc2);
		}
	}
	
	int hasExConfig = HasExConfig(hdr->ipl2Type);
	int hasTwlConfig = HasTwlSettings(hdr->ipl2Type);
	unsigned int ncdAddr = hdr->nvramUserConfigAddr * 8;
	
	//5. correct connection settings CRCs
	if (ncdAddr >= 0x400) {
		unsigned int connAddr = ncdAddr - 0x400;
		for (int i = 0; i < 3; i++) {
			FlashConnSetting *conn = (FlashConnSetting *) (buffer + connAddr + i * 0x100);
			if (conn->setType == 0xFF) continue;
			
			//check CRC
			uint16_t crc = conn->crc;
			uint16_t crc2 = ComputeCrc(conn, sizeof(FlashConnSetting)-2, 0);
			if (crc != crc2) {
				conn->crc = crc2;
				printf("Corrected connection %d CRC (%04X -> %04X)\n", i, crc, crc2);
			}
		}
		
		if (hasTwlConfig && connAddr >= 0x600) {
			unsigned int connExAddr = connAddr - 0x600;
			for (int i = 0; i < 3; i++) {
				FlashConnExSetting *conn = (FlashConnExSetting *) (buffer + connExAddr + i * 0x200);
				if (conn->base.setType == 0xFF) continue;
				
				uint16_t crc = conn->base.crc;
				uint16_t crc2 = ComputeCrc(&conn->base, sizeof(FlashConnSetting)-2, 0);
				if (crc != crc2) {
					conn->base.crc = crc2;
					printf("Corrected connection %d CRC (%04X -> %04X)\n", i + 3, crc, crc2);
				}
				
				uint16_t exCrc = conn->exCrc;
				uint16_t exCrc2 = ComputeCrc(&conn->base + 1, sizeof(FlashConnExSetting)-sizeof(FlashConnSetting)-2, 0);
				if (exCrc != exCrc2) {
					printf("Corrected connection %d CRC (%04X -> %04X)\n", i + 3, exCrc, exCrc2);
				}
			}
		}
	}
	
	//6. correct user data CRCs
	if (ncdAddr < size && (ncdAddr + 0x200) < size) {
		for (int i = 0; i < 2; i++) {
			FlashUserConfigData *ncd = (FlashUserConfigData *) (buffer + ncdAddr + i * 0x100);
			
			if (ncd->version != 5) {
				UpgradeNcdVersion(ncd);
				if (ncd->version != 5) continue;
			}
			
			uint16_t crc = ncd->crc;
			uint16_t crc2 = ComputeCrc(ncd, FLASH_NCD_SIZE-4, 0xFFFF);
			if (crc != crc2) {
				ncd->crc = crc2;
				printf("Corrected user config %d CRC (%04X -> %04X)\n", i, crc, crc2);
			}
			
			if (hasExConfig) {
				if (ncd->exVersion != 1) {
					ncd->exVersion = 1;
				}
				
				uint16_t exCrc = ncd->exCrc;
				uint16_t exCrc2 = ComputeCrc(&ncd->exVersion, FLASH_NCD_EX_SIZE-2, 0xFFFF);
				if (exCrc != exCrc2) {
					ncd->exCrc = exCrc2;
					printf("Corrected user config %d CRC (%04X -> %04X)\n", i, exCrc, exCrc2);
				}
			}
		}
	}
	
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
