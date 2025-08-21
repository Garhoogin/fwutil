#include "cmd_common.h"
#include "firmware.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


static char *gFirmwarePath = NULL;
static unsigned char *gFirmware = NULL;
static unsigned int gFirmwareSize = 0;
static int gQuit = 0;


const char *GetCurrentFilePath(void) {
	return gFirmwarePath;
}

unsigned char *GetFirmwareImage(unsigned int *pSize) {
	*pSize = gFirmwareSize;
	return gFirmware;
}

int LoadFirmwareImage(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (fp == NULL) {
		printf("Could not open file '%s' for read acces.\n", path);
		return 0;
	}
	
	fseek(fp, 0, SEEK_END);
	unsigned int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *buf = malloc(size);
	fread(buf, size, 1, fp);
	fclose(fp);
	
	if (size < (4 * 1024)) {
		printf("Invalid firmware image size (%d bytes).\n", size);
		free(buf);
		return 0;
	}
	
	if (gFirmwarePath != NULL) {
		free(gFirmwarePath);
	}
	if (gFirmware != NULL) {
		free(gFirmware);
	}
	
	gFirmwarePath = strdup(path);
	gFirmware = buf;
	gFirmwareSize = size;
	printf("Loaded %s.\n", gFirmwarePath);
	return 1;
}

int RequireFirmwareImage(void) {
	if (gFirmware == NULL) {
		puts("No valid firmware image loaded.");
		puts("Load a firmware image using the 'load' command.");
		return 0;
	}
	
	return 1;
}

int IsExiting(void) {
	return gQuit;
}

void DoExit(void) {
	gQuit = 1;
}

uint64_t ParseArgNumberULLEx(const char *arg, unsigned int defRadix) {
	if (*arg == '\0') return 0;
	
	//parse number. Assume hexadecimal by default.
	int radix = defRadix;
	uint64_t val = 0;
	
	if (gFirmware != NULL && gFirmwareSize >= 0x200 && arg[0] == '$') {
		//pseudo-variable expansion
		arg++;
		
		FlashHeader *hdr = (FlashHeader *) gFirmware;
		uint32_t arm9StaticRomAddr = (hdr->arm9StaticRomAddr * 4) << hdr->arm9RomAddrScale;
		uint32_t arm7StaticRomAddr = (hdr->arm7StaticRomAddr * 4) << hdr->arm7RomAddrScale;
		uint32_t arm9SecondaryRomAddr = (hdr->arm9SecondaryRomAddr * 4) * 2;
		uint32_t arm7SecondaryRomAddr = (hdr->arm7SecondaryRomAddr * 4) * 2;
		uint32_t rsrcRomAddr = (hdr->resourceRomAddr * 4) * 2;
		
		uint32_t ncdaddr = (hdr->nvramUserConfigAddr * 4) * 2;
		uint32_t connaddr = ncdaddr - 0x400;
		uint32_t connexaddr = connaddr - 0x600;
		
		if (stricmp(arg, "arm9")    == 0) return arm9StaticRomAddr;
		if (stricmp(arg, "arm7")    == 0) return arm7StaticRomAddr;
		if (stricmp(arg, "arm9s")   == 0) return arm9SecondaryRomAddr;
		if (stricmp(arg, "arm7s")   == 0) return arm7SecondaryRomAddr;
		if (stricmp(arg, "rsrc")    == 0) return rsrcRomAddr;
		if (stricmp(arg, "ncd")     == 0) return ncdaddr;
		if (stricmp(arg, "ncd0")    == 0) return ncdaddr;
		if (stricmp(arg, "ncd1")    == 0) return ncdaddr + 0x100;
		if (stricmp(arg, "conn")    == 0) return connaddr;
		if (stricmp(arg, "conn0")   == 0) return connaddr + 0x000;
		if (stricmp(arg, "conn1")   == 0) return connaddr + 0x100;
		if (stricmp(arg, "conn2")   == 0) return connaddr + 0x200;
		if (stricmp(arg, "connex")  == 0) return connexaddr;
		if (stricmp(arg, "connex0") == 0) return connexaddr + 0x000;
		if (stricmp(arg, "connex1") == 0) return connexaddr + 0x200;
		if (stricmp(arg, "connex2") == 0) return connexaddr + 0x400;
		
		return val;
	}
	
	if (arg[0] == '0') {
		if (arg[1] == 'x' || arg[1] == 'X') {
			radix = 16; // hexadecimal
			arg += 2;
		} else if (arg[1] == 'n' || arg[1] == 'N') {
			radix = 10; // decimal
			arg += 2;
		} else if (arg[1] == 'o' || arg[1] == 'O') {
			radix = 8; // octal
			arg += 2;
		}
	}
	
	char c;
	while ((c = *(arg++)) != '\0') {
		int digit = -1;
		if (c >= '0' && c <= '9') digit = (c - '0') + 0x0;
		if (c >= 'A' && c <= 'F') digit = (c - 'A') + 0xA;
		if (c >= 'a' && c <= 'f') digit = (c - 'a') + 0xA;
		
		//chech valid digit
		if (digit == -1 || digit >= radix) {
			return val;
		}
		
		val *= (unsigned int) radix;
		val += (unsigned int) digit;
	}
	
	return val;
}

uint64_t ParseArgNumberULL(const char *arg) {
	return ParseArgNumberULLEx(arg, 16);
}

uint32_t ParseArgNumber(const char *arg) {
	return (uint32_t) ParseArgNumberULL(arg);
}
