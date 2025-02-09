#pragma once

#include <stdint.h>

#define CX_STREAM_EOF -1 // EOF return value for CxStreamReadCallback

typedef enum CxCompressionType_ {
	CX_COMPRESSION_NONE,
	CX_COMPRESSION_LZ,
	CX_COMPRESSION_ASH
} CxCompressionType;

typedef int (*CxStreamReadCallback) (void *pArg);

unsigned char *CxDecompressLZ(const unsigned char *buffer, unsigned int size, unsigned int *uncompressedSize);

unsigned char *CxDecompressLZStream(unsigned int *uncompressedSize, CxStreamReadCallback callback, void *arg);

unsigned char *CxDecompressAsh(const unsigned char *buffer, unsigned int size, unsigned int *uncompressedSize);

unsigned char *CxCompressLZ(const unsigned char *buffer, unsigned int size, unsigned int *compressedSize);

unsigned char *CxCompressAsh(const unsigned char *buffer, unsigned int size, int nSymBits, int nDstBits, unsigned int nPasses, unsigned int *compressedSize);

static inline unsigned char *CxCompressAshFirmware(const unsigned char *buffer, unsigned int size, unsigned int *compressedSize) {
	unsigned char *out = CxCompressAsh(buffer, size, 9, 11, 2, compressedSize);
	if (out != NULL) {
		uint32_t hdr = (*compressedSize << 2) | 0x80000000;
		out[0] = (hdr >>  0) & 0xFF;
		out[1] = (hdr >>  8) & 0xFF;
		out[2] = (hdr >> 16) & 0xFF;
		out[3] = (hdr >> 24) & 0xFF;
		
		out[4] |= 0x80;
	}
	return out;
}

unsigned char *CxPadCompressed(unsigned char *comp, unsigned int size, unsigned int boundary, unsigned int *pOutSize);
