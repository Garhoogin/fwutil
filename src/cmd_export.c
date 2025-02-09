#include "cmd_common.h"
#include "firmware.h"
#include "blowfish.h"
#include "compression.h"

#include <string.h>

void CmdHelpExport(void) {
	puts("");
	puts("Usage: export <module> <filename> [flags...]");
	puts("");
	puts("Use one of the following for the module parameter:");
	puts("  arm9   ARM9 Static module");
	puts("  arm7   ARM7 static module");
	puts("  arm9s  ARM9 Secondary module");
	puts("  arm7s  ARM7 Secondary module");
	puts("  rsrc   Resources pack");
	puts("");
	puts("Flags:");
	puts("  -e     Do not decrypt the module (ARM7 and ARM9 static)");
	puts("  -c     Do not decompress the module");
}

void CmdProcExport(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	if (argc < 3) {
		CmdHelpExport();
		return;
	}
	
	int decrypt = 1, decompress = 1;
	const char *modname = argv[1];
	const char *filename = argv[2];
	
	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-e") == 0) {
			decrypt = 0;
			decompress = 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			decompress = 0;
		} else {
			printf("Unrecognized flag %s.\n", argv[i]);
		}
	}
	
	unsigned char *result = NULL;
	unsigned int resultSize = 0;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	//decompress module
	CxCompressionType type9, type7, typeRsrc;
	if (strcmp(modname, "arm9") == 0) {
		uint32_t arm9StaticRomAddr, arm9StaticSize, arm9StaticRamAddr, arm9StaticUncompressed;
		unsigned char *arm9Static = GetArm9StaticInfo(buffer, size, &arm9StaticRomAddr, &arm9StaticRamAddr, &arm9StaticSize, &arm9StaticUncompressed);
		
		if (decompress) {
			result = arm9Static;
			resultSize = arm9StaticUncompressed;
		} else {
			free(arm9Static);
			resultSize = arm9StaticSize;
			result = malloc(resultSize);
			memcpy(result, buffer + arm9StaticRomAddr, resultSize);
			
			if (decrypt) {
				//decrypt
				BfDecrypt(result, resultSize, buffer);
			}
		}
	} else if (strcmp(modname, "arm7") == 0) {
		uint32_t arm7StaticRomAddr, arm7StaticSize, arm7StaticRamAddr, arm7StaticUncompressed;
		unsigned char *arm7Static = GetArm7StaticInfo(buffer, size, &arm7StaticRomAddr, &arm7StaticRamAddr, &arm7StaticSize, &arm7StaticUncompressed);
		
		if (decompress) {
			result = arm7Static;
			resultSize = arm7StaticUncompressed;
		} else {
			free(arm7Static);
			resultSize = arm7StaticSize;
			result = malloc(resultSize);
			memcpy(result, buffer + arm7StaticRomAddr, resultSize);
			
			if (decrypt) {
				//decrypt
				BfDecrypt(result, resultSize, buffer);
			}
		}
	} else if (strcmp(modname, "arm9s") == 0) {
		uint32_t arm9SecondaryRomAddr, arm9SecondarySize, arm9SecondaryRamAddr, arm9SecondaryUncompressed;
		unsigned char *arm9Secondary = GetArm9SecondaryInfo(buffer, size, &arm9SecondaryRomAddr, &arm9SecondaryRamAddr, &arm9SecondarySize, &arm9SecondaryUncompressed, &type9);
		
		if (decompress) {
			//write decompressed
			result = arm9Secondary;
			resultSize = arm9SecondaryUncompressed;
		} else {
			//copy compressed
			free(arm9Secondary);
			resultSize = arm9SecondarySize;
			result = malloc(resultSize);
			memcpy(result, buffer + arm9SecondaryRomAddr, resultSize);
		}
	} else if (strcmp(modname, "arm7s") == 0) {
		uint32_t arm7SecondaryRomAddr, arm7SecondarySize, arm7SecondaryRamAddr, arm7SecondaryUncompressed;
		unsigned char *arm7Secondary = GetArm7SecondaryInfo(buffer, size, &arm7SecondaryRomAddr, &arm7SecondaryRamAddr, &arm7SecondarySize, &arm7SecondaryUncompressed, &type7);
		
		if (decompress) {
			//write decompressed
			result = arm7Secondary;
			resultSize = arm7SecondaryUncompressed;
		} else {
			//copy compressed
			free(arm7Secondary);
			resultSize = arm7SecondarySize;
			result = malloc(resultSize);
			memcpy(result, buffer + arm7SecondaryRomAddr, resultSize);
		}
	} else if (strcmp(modname, "rsrc") == 0) {
		uint32_t rsrcRomAddr, rsrcSize, rsrcRamAddr, rsrcUncompressed;
		unsigned char *rsrc = GetResourcesPackInfo(buffer, size, &rsrcRomAddr, &rsrcRamAddr, &rsrcSize, &rsrcUncompressed, &typeRsrc);
		
		if (decompress) {
			//write decompressed
			result = rsrc;
			resultSize = rsrcUncompressed;
		} else {
			//copy compressed
			free(rsrc);
			resultSize = rsrcSize;
			result = malloc(resultSize);
			memcpy(result, buffer + rsrcRomAddr, resultSize);
		}
	} else {
		printf("Unknown module name '%s'.\n", modname);
		return;
	}
	
	if (result == NULL) {
		puts("Could not extract module.");
		return;
	}
	
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		printf("Could not open '%s' for write access.\n", filename);
		free(result);
		return;
	}
	
	fwrite(result, resultSize, 1, fp);
	fclose(fp);
	
	free(result);
}

void CmdHelpImport(void) {
	puts("");
	puts("Usage: import <module> <filename> [flags...]");
	puts("");
	puts("Import a module into this firmware image. The source file may be uncompressed,");
	puts("compressed, or compressed and encrypted. Only the ARM9 and ARM7 static modules");
	puts("are encrypted.");
	puts("");
	puts("Use one of the following for the module parameter:");
	puts("  arm9   ARM9 Static module");
	puts("  arm7   ARM7 static module");
	puts("  arm9s  ARM9 Secondary module");
	puts("  arm7s  ARM7 Secondary module");
	puts("  rsrc   Resources pack");
	puts("");
	puts("Flags:");
	puts("  -c     Imported module is compressed.");
	puts("  -e     Imported module is compressed and encrypted.");
}

void CmdProcImport(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	if (argc < 2) {
		CmdHelpImport();
		return;
	}
	
	int decrypt = 1, decompress = 1;
	const char *modname = argv[1];
	const char *filename = argv[2];
	
	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-e") == 0) {
			decrypt = 0;
			decompress = 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			decompress = 0;
		} else {
			printf("Unrecognized flag %s.\n", argv[i]);
		}
	}
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Could not open '%s' for read access.\n", filename);
		return;
	}
	
	unsigned int inSize, padSize;
	unsigned char *inbuf;
	fseek(fp, 0, SEEK_END);
	inSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	//calculate padded size
	if (!decompress) {
		//pad size: for compressed (regardless of encryption), pad to 8.
		padSize = (inSize + 7) & ~7;
	} else {
		//else: no padding
		padSize = inSize;
	}
	
	inbuf = calloc(padSize, 1);
	fread(inbuf, inSize, 1, fp);
	fclose(fp);
	inSize = padSize;
	
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
	
	//can discard uncompressed binaries
	if (arm9Static != NULL) free(arm9Static);
	if (arm7Static != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc != NULL) free(rsrc);
	
	//pull out compressed+encrypted modules
	arm9Static = malloc(arm9StaticSize);
	arm7Static = malloc(arm7StaticSize);
	arm9Secondary = malloc(arm9SecondarySize);
	arm7Secondary = malloc(arm7SecondarySize);
	rsrc = malloc(rsrcSize);
	
	memcpy(arm9Static, buffer + arm9StaticRomAddr, arm9StaticSize);
	memcpy(arm7Static, buffer + arm7StaticRomAddr, arm7StaticSize);
	memcpy(arm9Secondary, buffer + arm9SecondaryRomAddr, arm9SecondarySize);
	memcpy(arm7Secondary, buffer + arm7SecondaryRomAddr, arm7SecondarySize);
	memcpy(rsrc, buffer + rsrcRomAddr, rsrcSize);
	
	//get module name
	const char *const modnames[] = { "arm9", "arm7", "arm9s", "arm7s", "rsrc" };
	int modno = -1;
	for (int i = 0; i < 5; i++) {
		if (strcmp(modname, modnames[i]) == 0) {
			modno = i;
			break;
		}
	}
	
	uint32_t modSizes[]          = { arm9StaticSize, arm7StaticSize, arm9SecondarySize, arm7SecondarySize, rsrcSize };
	unsigned char *mods[]        = { arm9Static,     arm7Static,     arm9Secondary,     arm7Secondary,     rsrc     };
	CxCompressionType modComps[] = { CX_COMPRESSION_NONE, CX_COMPRESSION_NONE, type9, type7, typeRsrc };
	
	//replace module
	{
		free(mods[modno]);
		modSizes[modno] = 0;
		
		if (decompress) {
			//source file is uncompressed, unencrypted.
			if (modno == 0 || modno == 1) {
				//ARM9 or ARM7 static: must be LZ compressed, then encrypted
				unsigned int compSize;
				unsigned char *comp = CxCompressLZ(inbuf, inSize, &compSize);
				comp = CxPadCompressed(comp, compSize, 8, &compSize);
				free(inbuf);
				
				//encrypt
				BfEncrypt(comp, compSize, buffer);
				
				mods[modno] = comp;
				modSizes[modno] = compSize;
			} else {
				//secondary/resource: must be either LZ or ASH compressed
				unsigned char *comp = NULL;
				unsigned int compSize = 0;
				if (modComps[modno] == CX_COMPRESSION_ASH) {
					//ASH compress
					printf("Compressing...\n");
					comp = CxCompressAshFirmware(inbuf, inSize, &compSize);
					printf("Done.\n");
				} else {
					//LZ compress
					comp = CxCompressLZ(inbuf, inSize, &compSize);
				}
				comp = CxPadCompressed(comp, compSize, 8, &compSize);
				free(inbuf);
				
				mods[modno] = comp;
				modSizes[modno] = compSize;
			}
		} else if (decrypt) {
			//source file is compressed, unencrypted.
			if (modno == 0 || modno == 1) {
				//static modules: must be encrypted
				BfEncrypt(inbuf, inSize, buffer);
			} else {
				//secondary/resource modules: not encrypted
			}
			mods[modno] = inbuf;
			modSizes[modno] = inSize;
		} else {
			//source file is compressed, encrypted.
			mods[modno] = inbuf;
			modSizes[modno] = inSize;
		}
	}
	
	//check size
	uint32_t totalSize = 0;
	for (int i = 0; i < 5; i++) {
		totalSize += modSizes[i];
		totalSize = (totalSize + 7) & ~7;
	}
	
	//get max address
	uint32_t maxAddr = hdr->nvramUserConfigAddr * 8 - 0x400;
	if (HasTwlSettings(hdr->ipl2Type)) maxAddr -= 0x600;
	if (maxAddr > size) maxAddr = size;
	
	if ((totalSize + 0x200) >= maxAddr) {
		printf("The module is too large.\n");
		goto End;
	}
	
	//write modules
	uint32_t curOffs = 0x200;
	for (int i = 0; i < 5; i++) {
		//write offset
		switch (i) {
			case 0: hdr->arm9StaticRomAddr    = curOffs / 8; hdr->arm9RomAddrScale = 1; break;
			case 1: hdr->arm7StaticRomAddr    = curOffs / 8; hdr->arm7RomAddrScale = 1; break;
			case 2: hdr->arm9SecondaryRomAddr = curOffs / 8; break;
			case 3: hdr->arm7SecondaryRomAddr = curOffs / 8; break;
			case 4: hdr->resourceRomAddr      = curOffs / 8; break;
		}
		
		//get source
		unsigned char *src = mods[i];
		unsigned int modSize = modSizes[i];
		memcpy(buffer + curOffs, src, modSize);
		
		curOffs += modSize;
		curOffs = (curOffs + 7) & ~7;
	}
	
	UpdateFirmwareModuleChecksums(buffer, size);
	
End:
	for (int i = 0; i < 5; i++) {
		if (mods[i] != NULL) free(mods[i]);
	}
}

