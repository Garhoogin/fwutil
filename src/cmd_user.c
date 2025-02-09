#include "cmd_common.h"
#include "firmware.h"

void CmdHelpUser(void) {
	puts("");
	puts("Usage: user");
	puts("");
	puts("Prints out the current user configuration settings. Only the current");
	puts("information is printed.");
	puts("The wireless connection settings are printed. When the advanced connection");
	puts("settings are present, those are printed too.");
}

static int GetEffectiveConfig(FlashUserConfigData *ncd) {
	
	int crc1OK = ComputeCrc(ncd, FLASH_NCD_SIZE-4, 0xFFFF) == ncd[0].crc;
	int crc2OK = ComputeCrc(ncd, FLASH_NCD_SIZE-4, 0xFFFF) == ncd[1].crc;
	if (!crc1OK && !crc2OK) return -1; // no good config data
	
	if (crc1OK && !crc2OK) return 0; // config 1 good
	if (crc2OK && !crc1OK) return 1; // config 2 good
	
	if (((ncd[0].saveCount + 1) & 0x7F) == ncd[1].saveCount) return 1; // config 2 is newer
	return 0; // config 1
}

static void PrintUcs2(const uint16_t *str, unsigned int len) {
	for (unsigned int i = 0; i < len; i++) {
		if (str[i] >= 0x20 && str[i] < 0x7F) putchar((char) str[i]);
		else putchar('?'); // out of ASCII plane
	}
}

static void PrintDate(const FlashDate *date) {
	const char *const monthNames[] = {
		"Jan", "Feb", "Mar", "Apr",
		"May", "Jun", "Jul", "Aug",
		"Sep", "Oct", "Nov", "Dec"
	};
	
	if (date->month == 0 || date->month > 12 || date->day == 0 || date->day > 31) {
		printf("Invalid date (%02d/%02d)", date->month, date->day);
	} else {
		printf("%s %d", monthNames[date->month - 1], date->day);
	}
}

static void PrintColor(int col) {
	const char *const colorNames[] = {
		"Gray",       "Brown",     "Red",       "Pink",
		"Orange",     "Yellow",    "Lime",      "Green",
		"Dark Green", "Sea Green", "Turquoise", "Blue",
		"Dark Blue",  "Purple",    "Violet",    "Magenta"
	};
	printf("%d (%s)", col, colorNames[col]);
}

static void PrintLanguage(FlashUserConfigData *ncd, int hasExConfig) {
	const char *const languages[] = {
		"Japanese", "English", "French", "German", "Italian", "Spanish",
		"Chinese", "Korean"
	};
	
	if (!hasExConfig) {
		int lang = ncd->language;
		if (lang > 5) printf("Invalid (%d)", lang);
		else printf("%s", languages[lang]);
	} else {
		int lang = ncd->exLanguage;
		if (lang > 7) printf("Invalid (%d)", lang);
		else printf("%s", languages[lang]);
	}
}

static void PrintSSID(const unsigned char *ssid) {
	//null-terminated SSIDs
	for (unsigned int i = 0; i < 32; i++) {
		if (ssid[i] == 0) break;
		
		if (ssid[i] >= 0x20 && ssid[i] < 0x7F) putchar((char) ssid[i]);
		else putchar('?');
	}
}

static void PrintWEP(FlashConnSetting *set) {
	if (set->wepMode == 0) {
		//no WEP mode
		printf("None");
		return;
	}
	
	unsigned int keylen = 0;
	switch (set->wepMode) {
		case 1:
			keylen = 40/8;
			break;
		case 2:
			keylen = 104/8;
			break;
		case 3:
			keylen = 128/8;
			break;
	}
	
	printf("WEP %d (", keylen * 8);
	for (unsigned int j = 0; j < keylen; j++) {
		printf("%02X", set->wepKey[0][j]);
	}
	printf(")");
}

static void PrintWPA(FlashConnExSetting *set) {
	if (set->wpaMode == 0 && set->base.wepMode == 0) {
		//no WEP mode
		printf("None");
		return;
	}
	
	const char *const wpaTypes[] = {
		"WPA-PSK (TKIP)",
		"WPA2-PSK (TKIP)",
		"WPA-PSK (AES)",
		"WPA2-PSK (AES)"
	};
	
	printf("%s (", wpaTypes[set->wpaMode - 4]);
	for (unsigned int i = 0; i < 64; i++) {
		char c = set->passphrase[i];
		if (!c) break;
		
		putchar(c);
	}
	printf(")");
}

static void PrintIP(uint32_t addr) {
	if (addr == 0) {
		printf("Auto-obtain");
	} else {
		printf("%3d.%3d.%3d.%3d", (addr >> 0) & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF);
	}
}

static void PrintNcd(FlashUserConfigData *ncd, int hasExConfig) {
	printf("User configuration\n");
	printf("  Nickname       : "); PrintUcs2(ncd->nickname, ncd->nicknameLength); puts("");
	printf("  Comment        : "); PrintUcs2(ncd->comment, ncd->commentLength); puts("");
	printf("  Birthday       : "); PrintDate(&ncd->birthday); puts("");
	printf("  Favorite Color : "); PrintColor(ncd->favoriteColor); puts("");
	printf("  Language       : "); PrintLanguage(ncd, hasExConfig); puts("");
}

static void PrintConnSetting(FlashConnSetting *set, int id) {
	printf("Connection %d\n", id + 1);
	if (set->setType == 0xFF) {
		printf("  Not configured.\n");
		puts("");
		return;
	}
	
	printf("  SSID     : "); PrintSSID(set->ssid); puts("");
	printf("  Security : "); PrintWEP(set); puts("");
	printf("  IP       : "); PrintIP(set->ipAddr); puts("");
	printf("  Gateway  : "); PrintIP(set->gateway); puts("");
	printf("  Subnet   : "); PrintIP(((1 << set->subnetMask) - 1)); puts("");
	printf("  DNS 1    : "); PrintIP(set->dns[0]); puts("");
	printf("  DNS 2    : "); PrintIP(set->dns[1]); puts("");
	
	
	uint64_t wfcId = (uint64_t) set->dwcUserIdLo;
	wfcId |= ((uint64_t) set->dwcUserIdHi) << 32;
	wfcId *= 1000ull;
	
	uint64_t wfcId2 = (uint64_t) set->dwcUnattestedUserIdLo;
	wfcId2 |= ((uint64_t) set->dwcUnattestedUserIdMid1) << 5;
	wfcId2 |= ((uint64_t) set->dwcUnattestedUserIdMid2) << 21;
	wfcId2 |= ((uint64_t) set->dwcUnattestedUserIdHi) << 37;
	wfcId2 *= 1000ull;
	
	printf("  WFC ID   : %016llu (%016llu)\n", wfcId, wfcId2);
	printf("  WFC Pass : %03X\n", set->pass);
	
	puts("");
}

static void PrintConnExSetting(FlashConnExSetting *set, int id) {
	printf("Connection %d\n", id + 1);
	if (set->base.setType == 0xFF) {
		printf("  Not configured.\n");
		puts("");
		return;
	}
	
	printf("  SSID     : "); PrintSSID(set->base.ssid); puts("");
	printf("  Security : "); if (set->wpaMode) PrintWPA(set); else PrintWEP(&set->base); puts("");
	printf("  IP       : "); PrintIP(set->base.ipAddr); puts("");
	printf("  Gateway  : "); PrintIP(set->base.gateway); puts("");
	printf("  Subnet   : "); PrintIP(((1 << set->base.subnetMask) - 1)); puts("");
	printf("  DNS 1    : "); PrintIP(set->base.dns[0]); puts("");
	printf("  DNS 2    : "); PrintIP(set->base.dns[1]); puts("");
	printf("  MTU      : %d\n", set->base.mtu);
	
	uint64_t wfcId = (uint64_t) set->base.dwcUserIdLo;
	wfcId |= ((uint64_t) set->base.dwcUserIdHi) << 32;
	wfcId *= 1000ull;
	
	uint64_t wfcId2 = (uint64_t) set->base.dwcUnattestedUserIdLo;
	wfcId2 |= ((uint64_t) set->base.dwcUnattestedUserIdMid1) << 5;
	wfcId2 |= ((uint64_t) set->base.dwcUnattestedUserIdMid2) << 21;
	wfcId2 |= ((uint64_t) set->base.dwcUnattestedUserIdHi) << 37;
	wfcId2 *= 1000ull;
	
	printf("  WFC ID   : %016llu (%016llu)\n", wfcId, wfcId2);
	printf("  WFC Pass : %03X\n", set->base.pass);
	
	puts("");
}

void CmdProcUser(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	int hasExConfig = HasExConfig(hdr->ipl2Type);
	int hasTwlConfig = HasTwlSettings(hdr->ipl2Type);
	
	//address of user config
	unsigned int ncdAddr = hdr->nvramUserConfigAddr * 8;
	if (ncdAddr >= size || (ncdAddr + 0x200) > size) {
		puts("Flash header user config address is invalid.");
		return;
	}
	
	FlashUserConfigData *ncd = (FlashUserConfigData *) (buffer + ncdAddr);
	
	//determine active configuration
	int effective = GetEffectiveConfig(ncd);
	if (effective == -1) {
		puts("User configuation is invalid.");
		return;
	}
	
	//print user config
	puts("");
	PrintNcd(&ncd[effective], hasExConfig);
	
	//print access point settings
	if (ncdAddr >= 0x400) {
		unsigned int connAddr = ncdAddr - 0x400;
		puts("");
		for (int i = 0; i < 3; i++) {
			PrintConnSetting((FlashConnSetting *) (buffer + connAddr + i * 0x100), i);
		}
		
		if (hasTwlConfig && connAddr >= 0x600) {
			unsigned int connExAddr = connAddr - 0x600;
			for (int i = 0; i < 3; i++) {
				PrintConnExSetting((FlashConnExSetting *) (buffer + connExAddr + i * 0x200), i + 3);
			}
		}
	}
}
