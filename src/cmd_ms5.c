#include "cmd_common.h"
#include "firmware.h"

#include <string.h>
#include <math.h>


void CmdHelpMD5(void) {
	puts("");
	puts("Usage: md5");
	puts("");
	puts("Prints out the MD5 digests of components of the firmware.");
}


//MD5 sine table
static const uint32_t k[] = {
	0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE,
	0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
	0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE,
	0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
	0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA,
	0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
	0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED,
	0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
	0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C,
	0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
	0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05,
	0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
	0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039,
	0x655b59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
	0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
	0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391
};

//MD5 rotate table
static const int s[] = {
	7, 12, 17, 22,   7, 12, 17, 22,   7, 12, 17, 22,   7, 12, 17, 22,
	5,  9, 14, 20,   5,  9, 14, 20,   5,  9, 14, 20,   5,  9, 14, 20, 
	4, 11, 16, 23,   4, 11, 16, 23,   4, 11, 16, 23,   4, 11, 16, 23,
	6, 10, 15, 21,   6, 10, 15, 21,   6, 10, 15, 21,   6, 10, 15, 21
};


static uint32_t RotL(uint32_t v1, int amt){
	return (v1 << amt) | (v1 >> (32 - amt));
}

static void ComputeMd5(const unsigned char *buf, unsigned int len, unsigned char *pDigest) {
	unsigned int processedLen = len + 1; // added 1-bit
	
	//compute padding size (to 56 bytes mod 64)
	if ((processedLen % 64) < 56) {
		processedLen += 56 - (processedLen % 64);
	} else if ((processedLen % 64) > 56) {
		processedLen += 64 + 56 - (processedLen % 64);
	}
	processedLen += 8;
	unsigned char *processed = calloc(processedLen, 1);
	memcpy(processed, buf, len);
	processed[len] = 0x80;
	
	uint64_t nBitsSrc = len * 8;
	for (int i = 0; i < 8; i++) {
		processed[processedLen - 8 + i] = (nBitsSrc >> (8 * i)) & 0xFF;
	}
	
	//initial MD5 state
	unsigned int a0 = 0x67452301;
	unsigned int b0 = 0xEFCDAB89;
	unsigned int c0 = 0x98BADCFE;
	unsigned int d0 = 0x10325476;
	
	for (unsigned int n = 0; n < processedLen; n += 64) {
		uint32_t chunk[16];
		const unsigned char *chunksrc = processed + n;
		for (int i = 0; i < 16; i++) {
			chunk[i] = (chunksrc[i * 4 + 0] << 0) | (chunksrc[i * 4 + 1] << 8)
				| (chunksrc[i * 4 + 2] << 16) | (chunksrc[i * 4 + 3] << 24);
		}
		
		uint32_t a = a0;
		uint32_t b = b0;
		uint32_t c = c0;
		uint32_t d = d0;
		for (int i = 0; i < 64; i++) {
			
			uint32_t f = 0, g = 0;
			switch ((i >> 4)) {
				case 0:
					f = (b & c) | ((~b) & d);
					g = i;
					break;
				case 1:
					f = (d & b) | ((~d) & c);
					g = 5 * i + 1;
					break;
				case 2:
					f = b ^ c ^ d;
					g = 3 * i + 5;
					break;
				case 3:
					f = c ^ (b | (~d));
					g = 7 * i;
					break;
			}
			
			uint32_t tmp = d;
			d = c;
			c = b;
			b = b + RotL(a + f + k[i] + chunk[g % 16], s[i]);
			a = tmp;
		}
		a0 += a;
		b0 += b;
		c0 += c;
		d0 += d;
	}
	free(processed);
	
	//write digest
	for (int i = 0; i < 4; i++) *(pDigest++) = (a0 >> (8 * i)) & 0xFF;
	for (int i = 0; i < 4; i++) *(pDigest++) = (b0 >> (8 * i)) & 0xFF;
	for (int i = 0; i < 4; i++) *(pDigest++) = (c0 >> (8 * i)) & 0xFF;
	for (int i = 0; i < 4; i++) *(pDigest++) = (d0 >> (8 * i)) & 0xFF;
}

static void PrintDigest(const unsigned char *digest) {
	for (int i = 0; i < 16; i++) printf("%02X", digest[i]);
}

void CmdProcMD5(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	(void) argc;
	(void) argv;
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
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
	
	//digest buffers
	unsigned char dFw[16], dStatic9c[16], dStatic9u[16], dStatic7c[16], dStatic7u[16];
	unsigned char dSecondary9c[16], dSecondary9u[16], dSecondary7c[16], dSecondary7u[16];
	unsigned char dRsrcu[16], dRsrcc[16];
	
	ComputeMd5(buffer, size, dFw);
	ComputeMd5(buffer + arm9StaticRomAddr, arm9StaticSize, dStatic9c);
	ComputeMd5(buffer + arm7StaticRomAddr, arm7StaticSize, dStatic7c);
	ComputeMd5(buffer + arm9SecondaryRomAddr, arm9SecondarySize, dSecondary9c);
	ComputeMd5(buffer + arm7SecondaryRomAddr, arm7SecondarySize, dSecondary7c);
	ComputeMd5(buffer + rsrcRomAddr, rsrcSize, dRsrcc);
	ComputeMd5(arm9Static, arm9StaticUncompressed, dStatic9u);
	ComputeMd5(arm7Static, arm7StaticUncompressed, dStatic7u);
	ComputeMd5(arm9Secondary, arm9SecondaryUncompressed, dSecondary9u);
	ComputeMd5(arm7Secondary, arm7SecondaryUncompressed, dSecondary7u);
	ComputeMd5(rsrc, rsrcUncompressed, dRsrcu);
	
	
	puts("");
	printf("Raw Digest              : "); PrintDigest(dFw);          puts("");
	puts("");
	printf("Compressed Modules\n");
	if (arm9Static    != NULL) { printf("  ARM9 Static Digest    : "); PrintDigest(dStatic9c);    puts(""); }
	if (arm7Static    != NULL) { printf("  ARM7 Static Digest    : "); PrintDigest(dStatic7c);    puts(""); }
	if (arm9Secondary != NULL) { printf("  ARM9 Secondary Digest : "); PrintDigest(dSecondary9c); puts(""); }
	if (arm7Secondary != NULL) { printf("  ARM7 Secondary Digest : "); PrintDigest(dSecondary7c); puts(""); }
	if (rsrc          != NULL) { printf("  Resources Digest      : "); PrintDigest(dRsrcc);       puts(""); }
	puts("");
	printf("Uncompressed Modules\n");
	if (arm9Static    != NULL) { printf("  ARM9 Static Digest    : "); PrintDigest(dStatic9u);    puts(""); }
	if (arm7Static    != NULL) { printf("  ARM7 Static Digest    : "); PrintDigest(dStatic7u);    puts(""); }
	if (arm9Secondary != NULL) { printf("  ARM9 Secondary Digest : "); PrintDigest(dSecondary9u); puts(""); }
	if (arm7Secondary != NULL) { printf("  ARM7 Secondary Digest : "); PrintDigest(dSecondary7u); puts(""); }
	if (rsrc          != NULL) { printf("  Resources Digest      : "); PrintDigest(dRsrcu);       puts(""); }
	
	if (arm9Static == NULL || arm7Static == NULL || arm9Secondary == NULL || arm7Secondary == NULL || rsrc == NULL) {
		puts("");
		printf("One or more modules failed to decompress.\n");
		goto End;
	}
	
End:
	if (arm9Static    != NULL) free(arm9Static);
	if (arm7Static    != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc          != NULL) free(rsrc);
}
