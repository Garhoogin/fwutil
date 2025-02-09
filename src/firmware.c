#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "firmware.h"
#include "compression.h"
#include "blowfish.h"

uint16_t ComputeCrc(const void *p, unsigned int length, uint16_t init) {
	const uint16_t tbl[] = {
		0x0000, 0xCC01, 0xD801, 0x1400,
		0xF001, 0x3C00, 0x2800, 0xE401,
		0xA001, 0x6C00, 0x7800, 0xB401,
		0x5000, 0x9C01, 0x8801, 0x4400
	};
	
	uint16_t r = init;
	const unsigned char *pp = (const unsigned char *) p;
	for (unsigned int i = 0; i < length; i++) {
		uint16_t c = (tbl[*pp & 0x0F] ^ (r >> 4)) ^ tbl[r & 0x0F];
		r = (tbl[*pp >> 4] ^ (c >> 4)) ^ tbl[c & 0xF];
		pp++;
	}
	return r;
}

uint16_t ComputeStaticCrc(const void *arm9Static, unsigned int arm9StaticSize, const void *arm7Static, unsigned int arm7StaticSize) {
	uint16_t crc = ComputeCrc(arm9Static, arm9StaticSize, 0xFFFF);
	return ComputeCrc(arm7Static, arm7StaticSize, crc);
}

uint16_t ComputeSecondaryCrc(const void *arm9Secondary, unsigned int arm9SecondarySize, const void *arm7Secondary, unsigned int arm7SecondarySize) {
	uint16_t crc = ComputeCrc(arm9Secondary, arm9SecondarySize, 0xFFFF);
	return ComputeCrc(arm7Secondary, arm7SecondarySize, crc);
}

void UpdateFirmwareModuleChecksums(unsigned char *buffer, unsigned int size) {
	//header
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
	
	//static module checksum
	if (arm9Static != NULL && arm7Static != NULL) {
		uint16_t sum = ComputeStaticCrc(arm9Static, arm9StaticUncompressed, arm7Static, arm7StaticUncompressed);
		hdr->staticCrc = sum;
	}
	
	//secondary module checksum
	if (arm9Secondary != NULL && arm7Secondary != NULL) {
		uint16_t sum = ComputeSecondaryCrc(arm9Secondary, arm9SecondaryUncompressed, arm7Secondary, arm7SecondaryUncompressed);
		hdr->secondaryCrc = sum;
	}
	
	//rsources checksum
	if (rsrc != NULL) {
		uint16_t sum = ComputeCrc(rsrc, rsrcUncompressed, 0xFFFF);
		hdr->resourceCrc = sum;
	}
	
	if (arm9Static != NULL) free(arm9Static);
	if (arm7Static != NULL) free(arm7Static);
	if (arm9Secondary != NULL) free(arm9Secondary);
	if (arm7Secondary != NULL) free(arm7Secondary);
	if (rsrc != NULL) free(rsrc);
}


// ----- RF utilities


const char *GetRfType(int type) {
	switch (type) {
		case 1:
			return "MAX2822";
		case 2:
			return "RF2958";
		case 3:
			return "MM3156";
		case 4:
			return "TEST";
		case 5:
			return "MTMBBP ES1";
		case 6:
			return "MM3218";
	}
	return "(unknown)";
}

int IsValidRfType(int type) {
	if (type == 1 || type == 2 || type == 3 || type == 5 || type == 6) return 1;
	return 0;
}





// ----- unpack routines


/*
 * Searches for a byte pattern in the firmware image.
 */
static unsigned char *SearchPattern(const unsigned char *buf, unsigned int size, const unsigned char *search, unsigned int searchlen) {
	if (searchlen > size) return NULL;
	
	for (unsigned int i = 0; i < (size - searchlen); i++) {
		if (memcmp(buf + i, search, searchlen) == 0) return (unsigned char *) (buf + i);
	}
	return NULL;
}

/*
 *  Searches for the decode_ash routine in the firmware image.
 */
static unsigned char *SearchDecodeAsh(const unsigned char *buf, unsigned int size) {
	//signature of decode_ash in newer firmware versions
	const unsigned char signature[] = {
		0xF0, 0x5F, 0x2D, 0xE9, // PUSH    { R4-R12, LR }
		0x04, 0xD0, 0x4D, 0xE2, // SUB     SP, #4
		0x04, 0x40, 0x91, 0xE5, // LDR     R4, [R1, #4]
		0x64, 0x58, 0x24, 0xE0, // EOR     R5, R4, R4, ROR #16
		0xFF, 0x58, 0xC5, 0xE3, // BIC     R5, R5, #0x00FF000000
		0x64, 0x44, 0xA0, 0xE1, // MOV     R4, R4, ROR #8
		0x25, 0x44, 0x24, 0xE0, // EOR     R4, R4, R5, LSR #8
		0xFF, 0x44, 0xC4, 0xE3, // BIC     R4, R4, #0xFF000000
	};
	unsigned char *p = SearchPattern(buf, size, signature, sizeof(signature));
	if (p != NULL) return p;
	
	//original decode_ash in older firmware versions
	const unsigned char signature2[] = {
		0xF0, 0x4F, 0x2D, 0xE9, // PUSH    { R4-R11, LR }
		0x0C, 0xD0, 0x4D, 0xE2, // SUB     SP, #0xC
		0x01, 0x90, 0xA0, 0xE1, // MOV     sb, R1
		0x05, 0x10, 0xD9, 0xE5, // LDRB    R1, [sb, #5]
		0x06, 0x20, 0xD9, 0xE5, // LDRB    R2, [sb, #6]
		0x08, 0x30, 0xD9, 0xE5, // LDRB    R3, [sb, #8]
		0x01, 0x48, 0xA0, 0xE1, // MOV     R4, R1, LSL #16
		0x02, 0x54, 0x84, 0xE1, // ORR     R5, R4, R2, LSL #8
	};
	p = SearchPattern(buf, size, signature2, sizeof(signature2));
	if (p != NULL) return p;
	
	return NULL;
}


// ----- decompression routines

typedef struct StreamState_ {
	const unsigned char *buf;
	unsigned int pos;
	unsigned int size;
} StreamState;

static int ReadBlowfishCallback(void *arg) {
	BfStream *stream = (BfStream *) arg;
	unsigned char b = BfDecryptStreamNext(stream);
	if (stream->error) return CX_STREAM_EOF;
	return b;
}

static int ReadNormalCallback(void *arg) {
	StreamState *stream = (StreamState *) arg;
	if (stream->pos >= stream->size) return CX_STREAM_EOF;
	
	return stream->buf[stream->pos++];
}

unsigned char *UncompressLZBlowfish(const unsigned char *buffer, unsigned int size, unsigned int romAddr, uint32_t *pSize, uint32_t *pUncompressed) {
	if (buffer == NULL || romAddr >= size) {
		return NULL;
	}
	
	unsigned int uncompSize;
	BfStream *stream = BfDecryptStreamInit(buffer + romAddr, size - romAddr, buffer);
	unsigned char *uncomp = CxDecompressLZStream(&uncompSize, ReadBlowfishCallback, stream);
	
	if (uncomp != NULL) {
		*pUncompressed = uncompSize;
		*pSize = (stream->srcpos + 7) & ~7; // source size (round up to multiple of blowfish block size)
	}
	BfDecryptStreamEnd(stream);
	return uncomp;
}

unsigned char *UncompressLZNormal(const unsigned char *buffer, unsigned int size, unsigned int romAddr, uint32_t *pSize, uint32_t *pUncompressed) {
	if (buffer == NULL || romAddr >= size) {
		return NULL;
	}
	
	unsigned int uncompSize;
	StreamState stream;
	stream.buf = buffer;
	stream.pos = romAddr;
	stream.size = size;
	
	unsigned char *uncomp = CxDecompressLZStream(&uncompSize, ReadNormalCallback, &stream);
	if (uncomp != NULL) {
		*pUncompressed = uncompSize;
		*pSize = (stream.pos - romAddr + 7) & ~7; // source size (round up to multiple of blowfish block size)
	}
	return uncomp;
}

unsigned char *GetArm9StaticInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed) {
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	*pRomAddr      = (4 * hdr->arm9StaticRomAddr) << hdr->arm9RomAddrScale;
	*pRamAddr      = 0x02800000 - ((hdr->arm9StaticRamAddr * 4) << hdr->arm9RamAddrScale);
	*pSize         = 0;
	*pUncompressed = 0;
	return UncompressLZBlowfish(buffer, size, *pRomAddr, pSize, pUncompressed);
}

unsigned char *GetArm7StaticInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed) {
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	*pRomAddr      = (4 * hdr->arm7StaticRomAddr) << hdr->arm7RomAddrScale;
	*pRamAddr      = (hdr->arm7RamLocation ? 0x02800000 : 0x03810000) - ((hdr->arm7StaticRamAddr * 4) << hdr->arm7RamAddrScale);
	*pSize         = 0;
	*pUncompressed = 0;
	return UncompressLZBlowfish(buffer, size, *pRomAddr, pSize, pUncompressed);
}

static unsigned char *UncompressLZOrASH(const unsigned char *buffer, unsigned int size, unsigned int romAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType) {
	//bounds check
	if (romAddr > size) return NULL;
	if ((size - romAddr) < 4) return NULL;
	
	const unsigned char *p = buffer + romAddr;
	if (*p == 0x10) {
		//try decoding LZ
		unsigned char *decp = UncompressLZNormal(buffer, size, romAddr, pSize, pUncompressed);
		if (decp != NULL) {
			*pType = CX_COMPRESSION_LZ;
			return decp;
		}
	}
	
	if ((size - romAddr) < 0xC) return NULL;
	
	uint32_t header = *(const uint32_t *) (buffer + romAddr);
	unsigned int compSize = (header & 0x00FFFFFF) >> 2;
	if ((romAddr + compSize) > size) return NULL;
	if ((romAddr + compSize) < romAddr) return NULL;
	
	unsigned int uncompSize;
	unsigned char *decp = CxDecompressAsh(buffer + romAddr, compSize, &uncompSize);
	*pUncompressed = uncompSize;
	*pSize = compSize;
	*pType = CX_COMPRESSION_ASH;
	return decp;
}

unsigned char *GetArm9SecondaryInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType) {
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	*pRomAddr      = (4 * hdr->arm9SecondaryRomAddr) * 2;
	*pRamAddr      = 0;
	*pSize         = 0;
	*pUncompressed = 0;
	
	unsigned char *uncomp = NULL;
	uncomp = UncompressLZOrASH(buffer, size, *pRomAddr, pSize, pUncompressed, pType);
	return uncomp;
}

unsigned char *GetArm7SecondaryInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType) {
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	*pRomAddr      = (4 * hdr->arm7SecondaryRomAddr) * 2;
	*pRamAddr      = 0;
	*pSize         = 0;
	*pUncompressed = 0;
	
	unsigned char *uncomp = NULL;
	uncomp = UncompressLZOrASH(buffer, size, *pRomAddr, pSize, pUncompressed, pType);
	return uncomp;
}

unsigned char *GetResourcesPackInfo(const unsigned char *buffer, unsigned int size, uint32_t *pRomAddr, uint32_t *pRamAddr, uint32_t *pSize, uint32_t *pUncompressed, CxCompressionType *pType) {
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	
	*pRomAddr      = (4 * hdr->resourceRomAddr) * 2;
	*pRamAddr      = 0;
	*pSize         = 0;
	*pUncompressed = 0;
	
	unsigned char *uncomp = NULL;
	uncomp = UncompressLZOrASH(buffer, size, *pRomAddr, pSize, pUncompressed, pType);
	return uncomp;
}

void GetSecondaryResourceLoadAddresses(
	const unsigned char *arm9Static,
	unsigned int arm9StaticUncompressed,
	const unsigned char *arm7Static,
	unsigned int arm7StaticUncompressed,
	uint32_t *pArm9SecondaryLoadAddr,
	uint32_t *pArm7SecondaryLoadAddr,
	uint32_t *pRsrcLoadAddr
) {
	//locate the load addresses of the ARM9 secondary module and resources pack by locating the
	//decode_ash function in the ARM9 static module. We look for thumb calls to this function
	//and use that those to identify the load addresses.
	if (arm9Static != NULL) {
		unsigned char *pDecodeAsh = SearchDecodeAsh(arm9Static, arm9StaticUncompressed);
		if (pDecodeAsh != NULL) {
			
			//get offset to decode_ash
			uint32_t ofsDecodeAsh = pDecodeAsh - arm9Static;
			
			uint32_t rsrcRamAddr = 0x00000000;
			uint32_t arm9SecondaryRamAddr = 0x00000000;
			
			//search for thumb BL decode_ash
			for (unsigned int i = 4; i < ((arm9StaticUncompressed & ~1) - 2); i += 2) {
				uint32_t rel = ((((ofsDecodeAsh - (i + 4)) + 2) & ~3) >> 1) & 0x3FFFFF;
				
				uint16_t u1 = *(uint16_t *) (arm9Static + i + 0);
				uint16_t u2 = *(uint16_t *) (arm9Static + i + 2);
				if ((u1 & 0xF800) != 0xF000) continue;
				if ((u2 & 0xF800) != 0xE800) continue;
				
				if ((u1 & 0x7FF) != ((rel >> 11) & 0x7FF)) continue;
				if ((u2 & 0x7FF) != ((rel >>  0) & 0x7FF)) continue;
				
				// case 1: (resources pack)
				// LDR     R0, =rsrcTarget
				// LDR     R1, =0x02200000
				// BLX     decode_ash
				
				// case 2: (ARM9 Secondary)
				// LDR     R0, =Arm9SecondaryBase
				// LDR     R1, =0x022C0000
				// BLX     decode_ash
				
				// case 3: (ARM7 Secondary)
				// LDR     R0, [R1, #0x0C]
				// LDR     R1, [R1, #0x10]
				// BLX     decode_ash
				
				uint16_t instr1 = *(uint16_t *) (arm9Static + i - 4);
				uint16_t instr2 = *(uint16_t *) (arm9Static + i - 2);
				(void) instr2;
				
				if ((instr1 & 0xFF00) == 0x4800) {        // LDR R0, [PC, #X]
					unsigned int pool = (i + ((instr1 & 0x00FF) << 2)) & ~3;
					uint32_t addr = *(uint32_t *) (arm9Static + pool);
					//printf("Case 1 at %08X (load to %08X)\n", i, *(uint32_t *) (arm9Static + pool));	
					
					if (arm9SecondaryRamAddr == 0) arm9SecondaryRamAddr = addr;
					else                           rsrcRamAddr = addr;
				} else if ((instr1 & 0xFFC7) == 0x68C0) { // LDR R0, [R1, #0xC]
					
				}
			}
			
			*pArm9SecondaryLoadAddr = arm9SecondaryRamAddr;
			*pRsrcLoadAddr = rsrcRamAddr;
		} else {
			//puts("Could not locate decode_ash.");
		}
	}
	
	//the ARM7 secondary load address is determined by the ARM7. 
	if (arm7Static != NULL) {
		//the ARM7 secondary laod addess is located at 027FF86C. 
		for (unsigned int i = 0; i < ((arm7StaticUncompressed & ~1) - 4); i += 2) {
			uint16_t instr1 = *(uint16_t *) (arm7Static + i + 0);
			uint16_t instr2 = *(uint16_t *) (arm7Static + i + 2);
			uint16_t instr3 = *(uint16_t *) (arm7Static + i + 4);
			(void) instr3;
			
			if ((instr1 & 0xF800) != 0x4800) continue;  // LDR R2, =Arm7SecondaryLoadAddr
			if ((instr2 & 0xF800) != 0x4800) continue;  // LDR R1, =0x027FF86C
			
			unsigned int pool = ((i + 2) & ~3) + 4 + ((instr2 & 0xFF) << 2);
			uint32_t val = *(uint32_t *) (arm7Static + pool);
			if (val != 0x027FF86C) continue;
			
			unsigned int pool2 = ((i + 0) & ~3) + 4 + ((instr1 & 0xFF) << 2);
			uint32_t val2 = *(uint32_t *) (arm7Static + pool2);
			
			*pArm7SecondaryLoadAddr = val2;
		}
	}
}


// ----- IPL2 type functions


int HasTwlSettings(int ipl2Type) {
	if (ipl2Type == IPL2_TYPE_NORMAL) return 0; // original DS
	if (ipl2Type & IPL2_TYPE_TWL) return 1;
	return 0;
}

int HasExConfig(int ipl2Type) {
	if (ipl2Type == IPL2_TYPE_NORMAL) return 0; // original DS
	if (ipl2Type & (IPL2_TYPE_EXTENDED | IPL2_TYPE_TWL)) return 1;
	return 0;
}

