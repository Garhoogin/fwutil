#include "cmd_common.h"
#include "firmware.h"

void CmdHelpVerify(void) {
	puts("");
	puts("Usage: verify");
	puts("");
	puts("Verifies the integrity of the firmware image. This command verifies the");
	puts("checksums and data validity of the firmware.");
}

static int VerifyArm7Accessible(uint32_t addr, uint32_t size) {
	//address must be in WRAM or main memory
	if (addr <  0x02000000) return 0;
	if (addr >= 0x04000000) return 0;
	if ((addr + size) < addr) return 0;
	if ((addr + size) >= 0x04000000) return 0;
	
	return 1;
}

static int VerifyArm9StaticAddress(uint32_t addr, uint32_t size) {
	//ARM9 static must be accessible from the ARM7.
	if (!VerifyArm7Accessible(addr, size)) return 0;
	
	return 1;
}

static int VerifyArm7StaticAddress(uint32_t addr, uint32_t size) {
	//ARM7 static must be accessible from the ARM7.
	if (!VerifyArm7Accessible(addr, size)) return 0;
	
	return 1;
}

void CmdProcVerify(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	//flash header
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
	
	int nErrors = 0;
	printf("\nError list:\n");
	
	//validate module data validity
	if (arm9Static == NULL)    { printf("  The ARM9 static module could not be decompressed.\n");    nErrors++; }
	if (arm7Static == NULL)    { printf("  The ARM7 static module could not be decompressed.\n");    nErrors++; }
	if (arm9Secondary == NULL) { printf("  The ARM9 secondary module could not be decompressed.\n"); nErrors++; }
	if (arm7Secondary == NULL) { printf("  The ARM7 secondary module could not be decompressed.\n"); nErrors++; }
	if (rsrc == NULL)          { printf("  The resources pack could not be decompressed.\n");        nErrors++; }
	
	//validate load addresses
	int arm9StaticLoadOK = VerifyArm9StaticAddress(arm9StaticRamAddr, arm9StaticUncompressed);
	int arm7StaticLoadOK = VerifyArm7StaticAddress(arm7StaticRamAddr, arm7StaticUncompressed);
	if (arm9Static != NULL && !arm9StaticLoadOK) { printf("  Invalid load address for ARM9 static module.\n"); nErrors++; }
	if (arm7Static != NULL && !arm7StaticLoadOK) { printf("  Invalid load address for ARM7 static module.\n"); nErrors++; }
	
	//get checksums from header
	uint16_t staticCrc = hdr->staticCrc, secondaryCrc = hdr->secondaryCrc, rsrcCrc = hdr->resourceCrc;
	uint16_t staticCrc2 = ComputeStaticCrc(arm9Static, arm9StaticUncompressed, arm7Static, arm7StaticUncompressed);
	uint16_t secondaryCrc2 = ComputeSecondaryCrc(arm9Secondary, arm9SecondaryUncompressed, arm7Secondary, arm7SecondaryUncompressed);
	uint16_t rsrcCrc2 = ComputeCrc(rsrc, rsrcUncompressed, 0xFFFF);
	if (arm9Static != NULL && arm7Static != NULL && staticCrc != staticCrc2) { printf("  Checksum mismatch for static module: %04X (expected %04X)\n", staticCrc2, staticCrc); nErrors++; }
	if (arm9Secondary != NULL && arm7Secondary != NULL && secondaryCrc != secondaryCrc2) { printf("  Checksum mismatch for secondary module: %04X (expected %04X)\n", secondaryCrc2, secondaryCrc); nErrors++; }
	if (rsrc != NULL && rsrcCrc != rsrcCrc2) { printf("  Checksum mismatch for resources pack: %04X (expected %04X)\n", rsrcCrc2, rsrcCrc); nErrors++; }
	
	//validate wireless info
	int isValidChannels = ((wl->allowedChannel & 0x8001) == 0) && ((wl->allowedChannel & 0x7FFE) != 0);
	uint16_t wlCrc = 0;
	if ((wl->tableSize + 0x2C) <= 0x200 || wl->tableSize < sizeof(*wl)) {
		wlCrc = ComputeCrc(&wl->tableSize, wl->tableSize, 0);
	} else                            { printf("  Invalid wireless init table size.\n");             nErrors++; }
	if (wlCrc!= wl->crc)              { printf("  CRC mismatch for wireless initialization.\n");     nErrors++; };
	if (!IsValidRfType(wl->rfType))   { printf("  No valid wireless RF type specified.\n");          nErrors++; }
	if (!isValidChannels)             { printf("  Invalid wireless channel specification.\n");       nErrors++; }
	
	//error footer
	printf("\n%d error(s) found.\n\n", nErrors);
	
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
