#include "cmd_common.h"
#include "firmware.h"

typedef enum SerialType_ {
	SERIAL_TYPE_RETAIL,
	SERIAL_TYPE_IS_NITRO_CAPTURE,
	SERIAL_TYPE_IS_NITRO_EMULATOR,
	SERIAL_TYPE_UNKNOWN
} SerialType;

void CmdHelpInfo(void) {
	puts("");
	puts("Usage: info");
	puts("");
	puts("Prints basic information about a firmware image.");
	puts("");
	puts("The ROM offsets and sizes for the firmware's modules are listed, as well as their");
	puts("destination addresses and the size of the module once uncompressed. If the module");
	puts("cannot be decompressed, its size is listed as 0. Verify the state of the modules");
	puts("using the verify command.");
}

static const char *GetIpl2TypeString(int type) {
	if (type == IPL2_TYPE_NORMAL) type = 0;
	
	const char *typestr = "DS";
	if (type & IPL2_TYPE_USG) {
		if (type & IPL2_TYPE_CPU_NTR) {
			typestr = "DS Lite with CPU-NTR";
		} else {
			typestr = "DS Lite";
		}
	} else if (type & IPL2_TYPE_TWL) {
		typestr = "DSi";
		type &= ~(IPL2_TYPE_EXT_LANGUAGE | IPL2_TYPE_CHINESE | IPL2_TYPE_KOREAN);
	}
	
	const char *region = "World";
	if (type & IPL2_TYPE_EXT_LANGUAGE) {
		if (type & IPL2_TYPE_CHINESE) {
			region = "iQue";
		} else if (type & IPL2_TYPE_KOREAN) {
			region = "Korea";
		}
	}
	
	static char buffer[64];
	sprintf(buffer, "%s (%s)", typestr, region);
	return buffer;
}

static SerialType DecodeSerial(const uint8_t *raw, uint64_t *pSerial) {
	//IS-NITRO-EMULATOR and IS-NITRO-CAPTURE devices arrange the serial number in
	//opposite endiannesses. We need to detect the endianness.
	uint64_t be = 0, le = 0;
	for (int i = 0; i < 5; i++) {
		le |= ((uint64_t) raw[i]) << (     i * 8);
		be |= ((uint64_t) raw[i]) << (32 - i * 8);
	}

	//low 36 bits are the decimal date
	uint64_t leDate = le & 0x0FFFFFFFFFull;
	uint64_t beDate = be & 0x0FFFFFFFFFull;
	uint64_t serial64 = 0xFFFFFFFFFFull;

	if (beDate <= 99999999ull) serial64 = be;
	if (leDate <= 99999999ull) serial64 = le;
	if (serial64 == 0xFFFFFFFFFFull) {
		*pSerial = 0;
		return SERIAL_TYPE_RETAIL;
	}
	
	SerialType type = SERIAL_TYPE_RETAIL;
	int typeCode = (serial64 >> 36) & 0xF;
	switch (typeCode) {
		case 0xF:
			type = SERIAL_TYPE_IS_NITRO_EMULATOR;
			break;
		case 0xE:
			type = SERIAL_TYPE_IS_NITRO_CAPTURE;
			break;
		default:
			type = SERIAL_TYPE_UNKNOWN;
			break;
	}

	//decoded
	*pSerial = serial64 & 0x0FFFFFFFFFull;
	return type;
}

static void PrintSerial(const uint8_t *pRawSerial) {
	//serial info
	uint64_t serialNo;
	SerialType serialType = DecodeSerial(pRawSerial, &serialNo);

	if (serialType == SERIAL_TYPE_RETAIL) {
		//retail devices have no serial number
		printf("Retail (no serial)\n");
		return;
	}

	switch (serialType) {
		case SERIAL_TYPE_IS_NITRO_EMULATOR: printf("IS-NITRO-EMULATOR "); break;
		case SERIAL_TYPE_IS_NITRO_CAPTURE:  printf("IS-NITRO-CAPTURE "); break;
		default:                            printf("Unknown Device "); break;
	}
	
	printf("%08d\n", (int) serialNo);
}

void CmdProcInfo(int argc, const char **argv) {
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
	
	//locate the load addresses for secondary modules and resources pack
	GetSecondaryResourceLoadAddresses(arm9Static, arm9StaticUncompressed, arm7Static, arm7StaticUncompressed, &arm9SecondaryRamAddr, &arm7SecondaryRamAddr, &rsrcRamAddr);
	
	//print firmware info
	printf("\n");
	
	printf("Firmware Info:\n");
	printf("  Build date            : 20%02X/%02X/%02X %02X:%02X\n", hdr->timestamp[4], hdr->timestamp[3], hdr->timestamp[2], hdr->timestamp[1], hdr->timestamp[0]);
	printf("  IPL2 type             : %s\n", GetIpl2TypeString(hdr->ipl2Type));
	printf("  Extended settings     : %s\n", (hdr->ipl2Type != 0xFF && (hdr->ipl2Type & 0x40)) ? "yes" : "no");
	printf("  Flash capacity        : %d KB\n", 128 << hdr->flashCapacity);
	printf("\n");
	
	
	//Module info: ARM9,ARM7 static modules, ARM9,ARM7 secondary modules, resource blob
	printf("Module Info:              Offset   Size     Address  Uncompressed\n");
	printf("  ARM9 Static Module    : %08X %08X %08X %08X\n", arm9StaticRomAddr, arm9StaticSize, arm9StaticRamAddr, arm9StaticUncompressed);
	printf("  ARM7 Static Module    : %08X %08X %08X %08X\n", arm7StaticRomAddr, arm7StaticSize, arm7StaticRamAddr, arm7StaticUncompressed);
	printf("  ARM9 Secondary Module : %08X %08X %08X %08X\n", arm9SecondaryRomAddr, arm9SecondarySize, arm9SecondaryRamAddr, arm9SecondaryUncompressed);
	printf("  ARM7 Secondary Module : %08X %08X %08X %08X\n", arm7SecondaryRomAddr, arm7SecondarySize, arm7SecondaryRamAddr, arm7SecondaryUncompressed);
	printf("  Resources Pack        : %08X %08X %08X %08X\n", rsrcRomAddr, rsrcSize, rsrcRamAddr, rsrcUncompressed);
	printf("\n");
	
	uint16_t channels = wl->allowedChannel;
	
	//Wireless info: RFU type, allowed channels, register values
	printf("Wireless Info:\n");
	printf("  RF Type               : %s\n", GetRfType(wl->rfType));
	printf("  Module Vendor         : %02X %02X\n", wl->module, wl->vendor);
	printf("  Serial                : "); PrintSerial(wl->serial);
	printf("  MAC Address           : %02X-%02X-%02X-%02X-%02X-%02X\n", wl->macAddr[0], wl->macAddr[1], wl->macAddr[2], wl->macAddr[3], wl->macAddr[4], wl->macAddr[5]);
	printf("  Allowed Channels      : "); for (int i = 0; i < 16; i++) if (channels & (1 << i)) printf("%d ", i); printf("\n");
	printf("\n");
	
	
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
