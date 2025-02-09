#include "cmd_common.h"
#include "firmware.h"

#include <string.h>
#include <ctype.h>

//simple file format for backing up user config data from a firmware image
typedef struct FirmwareSettings_ {
	uint16_t wlTableSize;                // size of wireless table
	unsigned char wlTable[512];          // wireless initialization table
	
	uint16_t connSettingSize;            // size of connection settings
	unsigned char connSetting[0x400];    // connection settings
	
	uint16_t connExSettingSize;          // size of extra connection settings
	unsigned char connExSetting[0x600];  // extended connection settings
	
	uint16_t userConfigSize;             // size of user configg
	unsigned char userConfig[0x200];     // user config
} FirmwareSettings;

void CmdHelpClean(void) {
	puts("");
	puts("Usage: clean");
	puts("");
	puts("Cleans the firmware image of the wireless initialization tables, user config,");
	puts("and wireless connection settings. The cleaned information can optionally be");
	puts("written to a file to restore later.");
}


void CmdProcClean(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	char fname[1024];
	printf("Save config path (enter to skip): ");
	fgets(fname, sizeof(fname), stdin);
	
	int len = strlen(fname);
	for (int i = 0; i < len; i++) {
		if (fname[i] == '\n' || fname[i] == '\r') {
			fname[i] = '\0';
			break;
		}
	}
	
	//wireless table
	unsigned char *wlTable = buffer + 0x2A;
	unsigned int wlTableSize = 0x200 - 0x2A;
	
	//user config
	unsigned int ncdOffset = (hdr->nvramUserConfigAddr * 4) * 2;
	unsigned char *ncd = NULL;
	unsigned int ncdSize = 0;
	if (ncdOffset < size && (ncdOffset + 0x200) <= size) {
		ncd = buffer + ncdOffset;
		ncdSize = 0x200;
	}
	
	//connection settings
	unsigned char *connSettings = NULL;
	unsigned int connSettingsSize = 0;
	unsigned char *connExSettings = NULL;
	unsigned int connExSettingsSize = 0;
	if (ncdOffset >= 0x400 && ncd != NULL) {
		connSettings = ncd - 0x400;
		connSettingsSize = 0x400;
	}
	if (HasTwlSettings(hdr->ipl2Type) && ncdOffset >= 0xA00 && connSettings != NULL) {
		connExSettings = connSettings - 0x600;
		connExSettingsSize = 0x600;
	}
	
	if (fname[0] != '\0') {
		//write
		FILE *fp = fopen(fname, "wb");
		if (fp != NULL) {
			FirmwareSettings *settings = calloc(1, sizeof(FirmwareSettings));
			
			settings->wlTableSize = wlTableSize;
			settings->connSettingSize = connSettingsSize;
			settings->connExSettingSize = connExSettingsSize;
			settings->userConfigSize = ncdSize;
			
			if (wlTable != NULL) memcpy(settings->wlTable, wlTable, wlTableSize);
			if (connSettings != NULL) memcpy(settings->connSetting, connSettings, connSettingsSize);
			if (connExSettings != NULL) memcpy(settings->connExSetting, connExSettings, connExSettingsSize);
			if (ncd != NULL) memcpy(settings->userConfig, ncd, ncdSize);
			
			fwrite(settings, sizeof(*settings), 1, fp);
			fclose(fp);
			free(settings);
		}
	}
	
	//erase
	if (wlTable        != NULL) { memset(wlTable,        0xFF, wlTableSize       ); printf("Erased wireless init table.\n");       }
	if (connSettings   != NULL) { memset(connSettings,   0xFF, connSettingsSize  ); printf("Erased connection settings.\n");       }
	if (connExSettings != NULL) { memset(connExSettings, 0xFF, connExSettingsSize); printf("Erased extra connection settings.\n"); }
	
	if (ncd != NULL) {
		for (int i = 0; i < 2; i++) {
			FlashUserConfigData *ncdi = &((FlashUserConfigData *) ncd)[i];
			memset(ncdi, 0, FLASH_NCD_SIZE + FLASH_NCD_EX_SIZE);
			
			ncdi->saveCount = i & 0x7F;
			ncdi->version = 5;
			ncdi->crc = ComputeCrc(ncdi, FLASH_NCD_SIZE-4, 0xFFFF);
			if (HasExConfig(hdr->ipl2Type)) {
				int isKOR = (hdr->ipl2Type != IPL2_TYPE_NORMAL) && (hdr->ipl2Type & IPL2_TYPE_KOREAN);
				int isCHN = (hdr->ipl2Type != IPL2_TYPE_NORMAL) && (hdr->ipl2Type & IPL2_TYPE_CHINESE);
				int isUSG = (hdr->ipl2Type != IPL2_TYPE_NORMAL) && (hdr->ipl2Type & IPL2_TYPE_USG);
				
				//Korean or Chinese USG: fill by FF
				if (isKOR || (isCHN && !isUSG)) memset(&ncdi->exVersion, 0xFF, FLASH_NCD_EX_SIZE);
				ncdi->exVersion = 1;
				ncdi->exLanguage = 1; // default to English
				
				//language mask:
				//Korean:  Korean,  Spanish, German,  French, English, Japanese
				//Chinese: Chinese, Spanish, Italian, German, French,  English
				ncdi->languages = isKOR ? 0x00AF : 0x007E;
				
				ncdi->exCrc = ComputeCrc(&ncdi->exVersion, FLASH_NCD_EX_SIZE-2, 0xFFFF);
			} else {
				//no ex config: zero only main buffer
				memset(&ncdi->exVersion, 0xFF, FLASH_NCD_EX_SIZE);
			}
		}
		printf("Erased user configuration.\n");
	}
}

void CmdHelpRestore(void) {
	puts("");
	puts("Usage: restore <filename>");
	puts("");
	puts("Restores configuration information from a file written by the 'clean' command.");
	puts("You wil be prompted whether to restore the wireless initialization, user");
	puts("configuration information, and wireless connection settings.");
}

void CmdProcRestore(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	if (argc < 2) {
		CmdHelpRestore();
		return;
	}
	
	const char *fname = argv[1];
	FILE *fp = fopen(fname, "rb");
	if (fp == NULL) {
		printf("Could not open '%s' for read access.\n", fname);
		return;
	}
	
	unsigned int settingsSize;
	fseek(fp, 0, SEEK_END);
	settingsSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *settingsbuf = malloc(settingsSize);
	fread(settingsbuf, settingsSize, 1, fp);
	fclose(fp);
	
	if (settingsSize != sizeof(FirmwareSettings)) {
		printf("Invalid settings file.\n");
		free(settingsbuf);
		return;
	}
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	FirmwareSettings *settings = (FirmwareSettings *) settingsbuf;
	
	
	//wireless table
	unsigned char *wlTable = buffer + 0x2A;
	unsigned int wlTableSize = 0x200 - 0x2A;
	
	//user config
	unsigned int ncdOffset = (hdr->nvramUserConfigAddr * 4) * 2;
	unsigned char *ncd = NULL;
	unsigned int ncdSize = 0;
	if (ncdOffset < size && (ncdOffset + 0x200) <= size) {
		ncd = buffer + ncdOffset;
		ncdSize = 0x200;
	}
	
	//connection settings
	unsigned char *connSettings = NULL;
	unsigned int connSettingsSize = 0;
	unsigned char *connExSettings = NULL;
	unsigned int connExSettingsSize = 0;
	if (ncdOffset >= 0x400 && ncd != NULL) {
		connSettings = ncd - 0x400;
		connSettingsSize = 0x400;
	}
	if (HasTwlSettings(hdr->ipl2Type) && ncdOffset >= 0xA00 && connSettings != NULL) {
		connExSettings = connSettings - 0x600;
		connExSettingsSize = 0x600;
	}
	
	char textbuffer[1024];
	if (wlTable != NULL) {
		printf("Restore wireless init table? (y/n) ");
		fgets(textbuffer, sizeof(textbuffer), stdin);
		
		if (toupper(textbuffer[0]) == 'Y') {
			if (settings->wlTableSize < wlTableSize) wlTableSize = settings->wlTableSize;
			memcpy(wlTable, settings->wlTable, wlTableSize);
		}
	}
	if (ncd != NULL) {
		printf("Restore user config? (y/n) ");
		fgets(textbuffer, sizeof(textbuffer), stdin);
		
		if (toupper(textbuffer[0]) == 'Y') {
			if (settings->userConfigSize < ncdSize) ncdSize = settings->userConfigSize;
			memcpy(ncd, settings->userConfig, settings->userConfigSize);
		}
	}
	if (connSettings != NULL) {
		printf("Restore connection settings? (y/n) ");
		fgets(textbuffer, sizeof(textbuffer), stdin);
		
		if (toupper(textbuffer[0]) == 'Y') {
			if (settings->connSettingSize < connSettingsSize) connSettingsSize = settings->connSettingSize;
			memcpy(connSettings, settings->connSetting, settings->connSettingSize);
		}
	}
	if (connExSettings != NULL) {
		printf("Restore extended connection settings? (y/n) ");
		fgets(textbuffer, sizeof(textbuffer), stdin);
		
		if (toupper(textbuffer[0]) == 'Y') {
			if (settings->connExSettingSize < connExSettingsSize) connExSettingsSize = settings->connExSettingSize;
			memcpy(connExSettings, settings->connExSetting, connExSettingsSize);
		}
	}
	
	free(settingsbuf);
}
