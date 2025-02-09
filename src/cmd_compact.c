#include "cmd_common.h"
#include "firmware.h"
#include "blowfish.h"

#include <string.h>

void CmdHelpCompact(void) {
	puts("");
	puts("Usage: compact");
	puts("");
	puts("Recompresses each of the firmware's modules. Recompressing the modules may free");
	puts("up space to be used for larger data.");
}

void CmdProcCompact(int argc, const char **argv) {
	//compact the firmware. We do this by recompressing the binaries and relocating
	//them to save as much space as possible. We will use the lower granularity
	//wherever appropriate to move all unused space to the end.
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
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
	
	if (arm9Static == NULL || arm7Static == NULL || arm9Secondary == NULL || arm7Secondary == NULL || rsrc == NULL) {
		if (arm9Static    == NULL) printf("The ARM9 static module could not be decompressed.\n");
		if (arm7Static    == NULL) printf("The ARM7 static module could not be decompressed.\n");
		if (arm9Secondary == NULL) printf("The ARM9 secondary module could not be decompressed.\n");
		if (arm7Secondary == NULL) printf("The ARM7 secondary module could not be decompressed.\n");
		if (rsrc          == NULL) printf("The resources pack could not be decompressed.\n");
		goto End;
	}
	puts("");
	
	//recompress each module
	
	unsigned int arm9StaticRecompSize, arm7StaticRecompSize, arm9SecondaryRecompSize, arm7SecondaryRecompSize, rsrcRecompSize;
	
	unsigned char *arm9StaticRecomp = CxCompressLZ(arm9Static, arm9StaticUncompressed, &arm9StaticRecompSize);
	arm9StaticRecomp = CxPadCompressed(arm9StaticRecomp, arm9StaticRecompSize, 8, &arm9StaticRecompSize);
	printf("ARM9 static   : %08X -> %08X\n", arm9StaticSize, arm9StaticRecompSize);
	
	unsigned char *arm7StaticRecomp = CxCompressLZ(arm7Static, arm7StaticUncompressed, &arm7StaticRecompSize);
	arm7StaticRecomp = CxPadCompressed(arm7StaticRecomp, arm7StaticRecompSize, 8, &arm7StaticRecompSize);
	printf("ARM7 static   : %08X -> %08X\n", arm7StaticSize, arm7StaticRecompSize);
	
	unsigned char *arm9SecondaryRecomp = CxCompressAshFirmware(arm9Secondary, arm9SecondaryUncompressed, &arm9SecondaryRecompSize);
	arm9SecondaryRecomp = CxPadCompressed(arm9SecondaryRecomp, arm9SecondaryRecompSize, 8, &arm9SecondaryRecompSize);
	printf("ARM9 secondary: %08X -> %08X\n", arm9SecondarySize, arm9SecondaryRecompSize);
	
	unsigned char *arm7SecondaryRecomp = CxCompressAshFirmware(arm7Secondary, arm7SecondaryUncompressed, &arm7SecondaryRecompSize);
	arm7SecondaryRecomp = CxPadCompressed(arm7SecondaryRecomp, arm7SecondaryRecompSize, 8, &arm7SecondaryRecompSize);
	printf("ARM7 secondary: %08X -> %08X\n", arm7SecondarySize, arm7SecondaryRecompSize);
	
	unsigned char *rsrcRecomp = CxCompressAshFirmware(rsrc, rsrcUncompressed, &rsrcRecompSize);
	rsrcRecomp = CxPadCompressed(rsrcRecomp, rsrcRecompSize, 8, &rsrcRecompSize);
	printf("Resources     : %08X -> %08X\n", rsrcSize, rsrcRecompSize);
	
	unsigned int size1 = arm9StaticSize + arm7StaticSize + arm9SecondarySize + arm7SecondarySize + rsrcSize;
	unsigned int size2 = arm9StaticRecompSize + arm7StaticRecompSize + arm9SecondaryRecompSize + arm7SecondaryRecompSize + rsrcRecompSize;
	puts("");
	printf("Total saved: %08X\n", size1 - size2);
	
	//encrypt modules
	BfEncrypt(arm9StaticRecomp, arm9StaticRecompSize, buffer);
	BfEncrypt(arm7StaticRecomp, arm7StaticRecompSize, buffer);
	
	//place updated module 
	unsigned int offs = 0x200;
	memcpy(buffer + offs, arm9StaticRecomp, arm9StaticRecompSize);
	hdr->arm9RomAddrScale = 1; // 8x
	hdr->arm9StaticRomAddr = (offs / 8);
	
	offs += arm9StaticRecompSize;
	memcpy(buffer + offs, arm7StaticRecomp, arm7StaticRecompSize);
	hdr->arm7RomAddrScale = 1; // 8x
	hdr->arm7StaticRomAddr = (offs / 8);
	
	offs += arm7StaticRecompSize;
	memcpy(buffer + offs, arm9SecondaryRecomp, arm9SecondaryRecompSize);
	hdr->arm9SecondaryRomAddr = (offs / 8);
	
	offs += arm9SecondaryRecompSize;
	memcpy(buffer + offs, arm7SecondaryRecomp, arm7SecondaryRecompSize);
	hdr->arm7SecondaryRomAddr = (offs / 8);
	
	offs += arm7SecondaryRecompSize;
	memcpy(buffer + offs, rsrcRecomp, rsrcRecompSize);
	hdr->resourceRomAddr = (offs / 8);
	
	
	free(arm9StaticRecomp);
	free(arm7StaticRecomp);
	free(arm9SecondaryRecomp);
	free(arm7SecondaryRecomp);
	free(rsrcRecomp);
	
End:
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
