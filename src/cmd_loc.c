#include "cmd_common.h"
#include "firmware.h"

void CmdHelpLoc(void) {
	puts("");
	puts("Usage: loc <address>");
	puts("");
	puts("Locates the module that will be loaded at the specified address in RAM, and at");
	puts("what offset it appears.");
}

void CmdProcLoc(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	if (argc < 2) {
		CmdHelpLoc();
		return;
	}
	
	const char *straddr = argv[1];
	uint32_t addr = ParseArgNumber(straddr);
	
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
	
	//locate the load addresses for secondary modules and resources pack
	GetSecondaryResourceLoadAddresses(arm9Static, arm9StaticUncompressed, arm7Static, arm7StaticUncompressed, &arm9SecondaryRamAddr, &arm7SecondaryRamAddr, &rsrcRamAddr);
	
	if (arm9StaticRamAddr && addr >= arm9StaticRamAddr && addr < (arm9StaticRamAddr + arm9StaticUncompressed)) {
		printf("%08X: ARM9 Static Module + 0x%X\n", addr, addr - arm9StaticRamAddr);
	} else if (arm7StaticRamAddr && addr >= arm7StaticRamAddr && addr < (arm7StaticRamAddr + arm7StaticUncompressed)) {
		printf("%08X: ARM7 Static Module + 0x%X\n", addr, addr - arm7StaticRamAddr);
	} else if (arm9SecondaryRamAddr && addr >= arm9SecondaryRamAddr && addr < (arm9SecondaryRamAddr + arm9SecondaryUncompressed)) {
		printf("%08X: ARM9 Secondary Module + 0x%X\n", addr, addr - arm9SecondaryRamAddr);
	} else if (arm7SecondaryRamAddr && addr >= arm7SecondaryRamAddr && addr < (arm7SecondaryRamAddr + arm7SecondaryUncompressed)) {
		printf("%08X: ARM7 Secondary Module + 0x%X\n", addr, addr - arm7SecondaryRamAddr);
	} else if (rsrcRamAddr && addr >= rsrcRamAddr && addr < (rsrcRamAddr + rsrcUncompressed)) {
		printf("%08X: Resources Pack + 0x%X\n", addr, addr - rsrcRamAddr);
	} else {
		printf("No matches for address %08X.\n", addr);
	}
	
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
