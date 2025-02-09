#pragma once

#include <stdint.h>

#include "compression.h"



// ----- FLASH structures

typedef struct FlashHeader_ {
	uint16_t arm9SecondaryRomAddr;          // +0x000 ARM9 Secondary Module ROM Address
	uint16_t arm7SecondaryRomAddr;          // +0x002 ARM7 Secondary Module ROM Address
	uint16_t secondaryCrc;                  // +0x004 Secondary Module (ARM9+ARM7) CRC
	uint16_t staticCrc;                     // +0x006 Static Module (ARM9+ARM7) CRC
	uint32_t blowfishKey;                   // +0x008 Static Module blowfish key
	uint16_t arm9StaticRomAddr;             // +0x00C ARM9 Static Module ROM Address
	uint16_t arm9StaticRamAddr;             // +0x00E ARM9 Static Module RAM Address
	uint16_t arm7StaticRomAddr;             // +0x010 ARM7 Static Module ROM Address
	uint16_t arm7StaticRamAddr;             // +0x012 ARM7 Static Module RAM Address
	uint16_t arm9RomAddrScale : 3;          // +0x014 ARM9 Static Module ROM Address Scale
	uint16_t arm9RamAddrScale : 3;          // +0x014 ARM9 Static Module RAM Address Scale
	uint16_t arm7RomAddrScale : 3;          // +0x014 ARM7 Static Module ROM Address Scale
	uint16_t arm7RamAddrScale : 3;          // +0x014 ARM7 Static Module RAM Address Scale
	uint16_t arm7RamLocation  : 1;          // +0x014 ARM7 Static Module RAM Address Selection (0=WRAM, 1=Main RAM)
	uint16_t flashCapacity    : 3;          // +0x014 Flash memory capacity (dubious)
	uint16_t resourceRomAddr;               // +0x016 Resource Pack ROM Address
	union {
		struct {
			unsigned char timestamp[5];     // +0x018 Firmware build time
			unsigned char ipl2Type;         // +0x01D IPL2 type
			uint16_t pad1E;                 // +0x1E
		};
		uint32_t unscrambleKey[2];          // +0x018 Unscramble key
	};
	uint16_t nvramUserConfigAddr;           // +0x020 NVRAM User Config Address
	uint16_t field22;
	uint16_t field24;
	uint16_t resourceCrc;                   // +0x026 Resource Pack CRC
	uint16_t field28;
} FlashHeader;

//some of these bits are conjecture
#define IPL2_TYPE_NORMAL               0xFF // Indicates Original DS
#define IPL2_TYPE_CPU_NTR              0x80 // Indicates use of CPU-NTR on USG firmware and display?
#define IPL2_TYPE_EXTENDED             0x40 // Indicates presence of extended settings
#define IPL2_TYPE_USG                  0x20 // Indicates DS Lite type
#define IPL2_TYPE_TWL                  0x10 // Indicates DSi type
#define IPL2_TYPE_KOREAN               0x04 // Indicates Korean language support
#define IPL2_TYPE_CHINESE              0x02 // Indicates Chinese language support
#define IPL2_TYPE_EXT_LANGUAGE         0x01 // Indicates extended language support

typedef struct FlashRfBbInfo_ {
	uint16_t crc;                           // +0x02A CRC of table
	uint16_t tableSize;                     // +0x02C Size of table
	uint8_t vendor;                         // +0x02E Vendor ID
	uint8_t module;                         // +0x02F Module ID
	uint8_t serial[6];                      // +0x030 Serial Number
	uint8_t macAddr[6];                     // +0x036 MAC Address
	uint16_t allowedChannel;                // +0x03C Allowed channels
	uint16_t macFlags;                      // +0x03E MAC operation flags
	uint8_t rfType;                         // +0x040 RF type
	uint8_t rfRegisterBits;                 // +0x041 Width of RF register
	uint8_t rfInitRegisterCount;            // +0x042 Number of RF registers to initialize
	uint8_t rfChannelRegisterCount;         // +0x043 Number of RF registers per channel config
	uint16_t macInitRegs[16];               // +0x044 MAC init regs
	uint8_t bbInitRegs[0x69];               // +0x064 BBP init regs
	uint8_t pad_;
} FlashRfBbInfo;

typedef struct FlashDate_ {
	uint8_t month;
	uint8_t day;
} FlashDate;

typedef struct FlashUserConfigData_ {
	uint8_t version;                        // +0x000  (5=original retail)
	uint8_t pad1;                           // +0x001
	
	//ownerInfo
	uint8_t favoriteColor;                  // +0x002
	FlashDate birthday;                     // +0x003
	uint8_t pad5;                           // +0x005
	uint16_t nickname[10];                  // +0x006
	uint8_t nicknameLength;                 // +0x01A
	uint8_t pad1B;                          // +0x01B
	uint16_t comment[26];                   // +0x01C
	uint8_t commentLength;                  // +0x050
	uint8_t pad51;                          // +0x051
	
	//alarm
	uint8_t alarmHour;                      // +0x052
	uint8_t alarmMinute;                    // +0x053
	uint8_t alarmSecond;                    // +0x054
	uint8_t pad55;                          // +0x055
	uint16_t alarmEnableWeek;               // +0x056
	
	//tp
	uint16_t tp1;                           // +0x058
	uint16_t tp2;                           // +0x05A
	uint8_t tp3;                            // +0x05C
	uint8_t tp4;                            // +0x05D
	uint16_t tp5;                           // +0x05E
	uint16_t tp6;                           // +0x060
	uint8_t tp7;                            // +0x062
	uint8_t tp8;                            // +0x063
	
	//option
	uint16_t language         : 3;          // +0x064
	uint16_t gbaMode          : 1;
	uint16_t backlight        : 2;
	uint16_t autoboot         : 1;
	uint16_t pad64_4          : 4;
	uint16_t hasFavoriteColor : 1;
	uint16_t hasTpCalib       : 1;
	uint16_t hasLanguage      : 1;
	uint16_t hasRtc           : 1;
	uint16_t hasNickname      : 1;
	uint8_t timezone;                       // +0x066
	uint8_t rtcClockAdjust;                 // +0x067
	uint64_t rtcOffset;                     // +0x068
	
	uint16_t saveCount;                     // +0x070
	uint16_t crc;                           // +0x072
	
	//extended settings
	uint8_t exVersion;                      // +0x074
	uint8_t exLanguage;                     // +0x075
	uint16_t languages;                     // +0x076
	unsigned char pad78[0x86];              // +0x078
	uint16_t exCrc;                         // +0x0FE
} FlashUserConfigData;

#define FLASH_NCD_SIZE                      0x74 // size of user config
#define FLASH_NCD_EX_SIZE                   0x8C // size of extended portion of user config


typedef struct FlashConnection_ {
	unsigned char ispID[32];                // +0x000
	unsigned char ispPass[32];              // +0x020
	unsigned char ssid[32];                 // +0x040
	unsigned char ssidAoss[32];             // +0x060
	unsigned char wepKey[4][16];            // +0x080
	uint32_t ipAddr;                        // +0x0C0
	uint32_t gateway;                       // +0x0C4
	uint32_t dns[2];                        // +0x0C8
	uint8_t subnetMask;                     // +0x0D0
	unsigned char wepKey2[4][5];            // +0x0D1
	uint8_t authType;                       // +0x0E5
	uint8_t wepMode         : 2;            // +0x0E6 (0=None, 1=WEP 40, 2=WEP 104, 3=WEP 128)
	uint8_t wepKeyNotation  : 6;            // +0x0E6 (0=hex, 1=ASCII)
	uint8_t setType;                        // +0x0E7 (0=Manual, 1=AOSS, 2=Rakuraku, FF=unset)
	uint8_t fieldE8;                        // +0x0E8 (TWL)
	uint8_t fieldE9;                        // +0x0E9 (TWL)
	uint16_t mtu;                           // +0x0EA (TWL)
	unsigned char fieldEC[3];               // +0x0EC
	uint8_t state;                          // +0x0EF
	uint32_t dwcUserIdLo;                   // +0x0F0 DWC user ID (32)             | 43-bit
	uint16_t dwcUserIdHi : 11;              // +0x0F4 DWC user ID (11)             /
	uint16_t dwcUnattestedUserIdLo : 5;     // +0x0F4 DWC unattested user ID (5)   | 43-bit
	uint16_t dwcUnattestedUserIdMid1;       // +0x0F6 DWC unattested user ID (16)  |
	uint16_t dwcUnattestedUserIdMid2;       // +0x0F8 DWC unattested user ID (16)  |
	uint16_t dwcUnattestedUserIdHi : 6;     // +0x0FA DWC unattested user ID (6)   /
	uint16_t pass : 10;                     // +0x0FA
	uint16_t rand;                          // +0x0FC
	uint16_t crc;                           // +0x0FE
} FlashConnSetting;

typedef struct FlashConnectionEx_ {
	FlashConnSetting base;                  // +0x000
	unsigned char psk[32];                  // +0x100
	unsigned char passphrase[64];           // +0x120
	unsigned char field160[0x21];           // +0x160
	uint8_t wpaMode;                        // +0x181 (0=No/WEP, 4=WPA TKIP, 5=WPA2 TKIP, 6=WPA AES, 7=WPA2 AES)
	uint8_t proxyEnable;                    // +0x182
	uint8_t proxyAuthenticate;              // +0x183
	char proxyName[48];                     // +0x184
	unsigned char field1B4[52];             // +0x1B4
	uint16_t proxyPort;                     // +0x1E8
	unsigned char field1EA[20];             // +0x1EA
	uint16_t exCrc;                         // +0x1FE
} FlashConnExSetting;


// ----- Common routines

uint16_t ComputeCrc(const void *p, unsigned int length, uint16_t init);
uint16_t ComputeStaticCrc(const void *arm9Static, unsigned int arm9StaticSize, const void *arm7Static, unsigned int arm7StaticSize);
uint16_t ComputeSecondaryCrc(const void *arm9Secondary, unsigned int arm9SecondarySize, const void *arm7Secondary, unsigned int arm7SecondarySize);
void UpdateFirmwareModuleChecksums(unsigned char *buffer, unsigned int size);


// ----- RF routines

const char *GetRfType(int type);
int IsValidRfType(int type);


// ----- unpack routines

unsigned char *UncompressLZBlowfish(const unsigned char *buffer, unsigned int size, unsigned int romAddr, uint32_t *pSize, uint32_t *pUncompressed);
unsigned char *GetArm9StaticInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed);
unsigned char *GetArm7StaticInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed);
unsigned char *GetArm9SecondaryInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType);
unsigned char *GetArm7SecondaryInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType);
unsigned char *GetResourcesPackInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType);

void GetSecondaryResourceLoadAddresses(
	const unsigned char *arm9Static,
	unsigned int arm9StaticUncompressed,
	const unsigned char *arm7Static,
	unsigned int arm7StaticUncompressed,
	uint32_t *pArm9SecondaryLoadAddr,
	uint32_t *pArm7SecondaryLoadAddr,
	uint32_t *pRsrcLoadAddr
);

int HasTwlSettings(int ipl2Type);
int HasExConfig(int ipl2Type);
