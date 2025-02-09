#include "cmd_common.h"
#include "firmware.h"

void CmdHelpMap(void) {
	puts("");
	puts("Usage: map");
	puts("");
	puts("Prints a map of the firmware image's address space.");
}

typedef struct Region_ {
	uint32_t start;
	uint32_t size;
	const char *name;
} Region;

static int RegionComparator(const void *e1, const void *e2) {
	const Region *r1 = (const Region *) e1;
	const Region *r2 = (const Region *) e2;
	
	if (r1->start < r2->start) return -1;
	if (r1->start > r2->start) return  1;
	return 0;
}

void CmdProcMap(int argc, const char **argv) {
	//map out the firmware address regions.
	if (!RequireFirmwareImage()) return;
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
		if (arm9Static == NULL)    printf("The ARM9 static module could not be decompressed.\n");
		if (arm7Static == NULL)    printf("The ARM7 static module could not be decompressed.\n");
		if (arm9Secondary == NULL) printf("The ARM9 secondary module could not be decompressed.\n");
		if (arm7Secondary == NULL) printf("The ARM7 secondary module could not be decompressed.\n");
		if (rsrc == NULL)          printf("The resources pack could not be decompressed.\n");
		goto End;
	}
	
	unsigned int ncdRomAddr = hdr->nvramUserConfigAddr * 4 * 2;
	unsigned int ncdSize = 0x200;
	unsigned int connSize = HasTwlSettings(hdr->ipl2Type) ? 0xA00 : 0x400;
	unsigned int connRomAddr = ncdRomAddr - connSize;
	
	Region regions[] = {
		{ 0,                    0x200,             "Header"              },
		{ arm9StaticRomAddr,    arm9StaticSize,    "ARM9 Static"         },
		{ arm7StaticRomAddr,    arm7StaticSize,    "ARM7 Static"         },
		{ arm9SecondaryRomAddr, arm9SecondarySize, "ARM9 Secondary"      },
		{ arm7SecondaryRomAddr, arm7SecondarySize, "ARM7 Secondary"      },
		{ rsrcRomAddr,          rsrcSize,          "Resources Pack"      },
		{ connRomAddr,          connSize,          "Connection Settings" },
		{ ncdRomAddr,           ncdSize,           "User Configuration"  }
	};
	qsort(regions, sizeof(regions) / sizeof(Region), sizeof(Region), RegionComparator);
	
	const char *fmtJct = " +----------------------------------------+-- %08X\n";
	puts("");
	
	unsigned int i = 0;
	uint32_t curAddr = 0;
	for (i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
		Region *rgn = &regions[i];
		printf(fmtJct, curAddr);
		
		char linebuf[64] = { 0 };
		if (curAddr < rgn->start) {
			//free space
			int len = snprintf(linebuf, sizeof(linebuf), "Free Space (%d bytes)", rgn->start - curAddr);
			printf(" |");
			for (int i = 0; i < (40 - len) / 2; i++) putchar(' ');
			printf(linebuf);
			for (int i = 0; i < (40 - (40 - len) / 2 - len); i++) putchar(' ');
			printf("|\n");
			curAddr = rgn->start;
			printf(fmtJct, curAddr);
		}
		
		//free space
		int len = snprintf(linebuf, sizeof(linebuf), "%s (%d bytes)", rgn->name, rgn->size);
		printf(" |");
		for (int i = 0; i < (40 - len) / 2; i++) putchar(' ');
		printf(linebuf);
		for (int i = 0; i < (40 - (40 - len) / 2 - len); i++) putchar(' ');
		printf("|\n");
		
		curAddr = rgn->start + rgn->size;
	}
	printf(fmtJct, curAddr);
	
	
End:
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
