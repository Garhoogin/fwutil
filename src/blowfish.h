#pragma once

#include <stdint.h>

typedef struct BlowfishCtx_ {
 	unsigned int parr[18];
 	unsigned int sbox[4][256];
} BfBlowfishContext;

typedef struct BfStream_ {
	BfBlowfishContext ctx;
	const unsigned char *buffer;
	unsigned int size;
	unsigned int srcpos;
	uint32_t block[2];
	int error;
} BfStream;

void BfDecrypt(unsigned char *buf, unsigned int len, const unsigned char *fwHeader);
void BfEncrypt(unsigned char *buf, unsigned int len, const unsigned char *fwHeader);

BfStream *BfDecryptStreamInit(const unsigned char *buffer, unsigned int len, const unsigned char *fwHeader);
unsigned char BfDecryptStreamNext(BfStream *stream);
void BfDecryptStreamEnd(BfStream *stream);
