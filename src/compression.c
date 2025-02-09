#include "compression.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>



static void *CxiShrink(void *block, unsigned int to) {
	void *newblock = realloc(block, to);
	if (newblock == NULL) {
		//alloc fail, return old block
		return block;
	}
	return newblock;
}



// ----- LZ decompression routines

unsigned char *CxDecompressLZ(const unsigned char *buffer, unsigned int size, unsigned int *uncompressedSize) {
	if (size < 4) return NULL;

	//find the length of the decompressed buffer.
	uint32_t length = (*(uint32_t *) buffer) >> 8;

	//create a buffer for the decompressed buffer
	unsigned char *result = (unsigned char *) malloc(length);
	if (result == NULL) return NULL;
	*uncompressedSize = length;

	//initialize variables
	uint32_t offset = 4;
	uint32_t dstOffset = 0;
	while (1) {
		uint8_t head = buffer[offset];
		offset++;
		//loop 8 times
		for (int i = 0; i < 8; i++) {
			int flag = head >> 7;
			head <<= 1;

			if (!flag) {
				result[dstOffset] = buffer[offset];
				dstOffset++, offset++;
				if (dstOffset == length) return result;
			} else {
				uint8_t high = buffer[offset++];
				uint8_t low = buffer[offset++];

				//length of uncompressed chunk and offset
				uint32_t offs = (((high & 0xF) << 8) | low) + 1;
				uint32_t len = (high >> 4) + 3;
				for (uint32_t j = 0; j < len; j++) {
					result[dstOffset] = result[dstOffset - offs];
					dstOffset++;
					if (dstOffset == length) return result;
				}
			}
		}
	}
	return result;
}

unsigned char *CxDecompressLZStream(unsigned int *uncompressedSize, CxStreamReadCallback callback, void *arg) {
	int b = callback(arg);
	if (b != 0x10) return NULL;
	
	unsigned int length = 0;
	for (int i = 0; i < 3; i++) {
		b = callback(arg);
		if (b == CX_STREAM_EOF) return NULL;
		
		length |= b << (i * 8);
	}
	
	unsigned char *result = (unsigned char *) malloc(length);
	*uncompressedSize = length;
	
	//initialize variables
	uint32_t dstOffset = 0;
	while (1) {
		if ((b = callback(arg)) == CX_STREAM_EOF) goto Error;
		uint8_t head = b;
		//printf("Out pos: %08X\n", dstOffset);
		
		//loop 8 times
		for (int i = 0; i < 8; i++) {
			int flag = head >> 7;
			head <<= 1;

			if (!flag) {
				if ((b = callback(arg)) == CX_STREAM_EOF) goto Error;
				
				result[dstOffset] = b;
				dstOffset++;
			} else {
				if ((b = callback(arg)) == CX_STREAM_EOF) goto Error;
				uint8_t high = b;
				if ((b = callback(arg)) == CX_STREAM_EOF) goto Error;
				uint8_t low = b;

				//length of uncompressed chunk and offset
				uint32_t offs = (((high & 0xF) << 8) | low) + 1;
				uint32_t len = (high >> 4) + 3;
				
				if (offs > dstOffset)           goto Error; // reference underflow
				if ((dstOffset + len) > length) goto Error; // reference overflow
				if (offs == 1)                  goto Error; // BIOS uses SVC UnCompLZShort
				
				for (uint32_t j = 0; j < len; j++) {
					result[dstOffset] = result[dstOffset - offs];
					dstOffset++;
				}
			}
			if (dstOffset == length) return result;
		}
	}
	return result;
	
Error:
	if (result != NULL) free(result);
	return NULL;
}



// ----- ASH decompression routines

typedef struct BIT_READER_8_ {
	const unsigned char *start;
	const unsigned char *end;
	const unsigned char *pos;
	unsigned char current;
	uint8_t nBitsBuffered;
	uint8_t error;
	uint8_t beBits;
	uint32_t nBitsRead;
} BIT_READER_8;

#define TREE_RIGHT    0x80000000
#define TREE_LEFT     0x40000000
#define TREE_VAL_MASK 0x3FFFFFFF

static uint32_t BigToLittle32(uint32_t x) {
	return (x >> 24) | (x << 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8);
}

unsigned char CxiReverseByte(unsigned char x) {
	unsigned char out = 0;
	for (int i = 0; i < 8; i++) out |= ((x >> i) & 1) << (7 - i);
	return out;
}

void CxiInitBitReader(BIT_READER_8 *reader, const unsigned char *pos, const unsigned char *end, int beBits) {
	reader->pos = pos;
	reader->end = end;
	reader->start = pos;
	reader->beBits = beBits;
	reader->nBitsBuffered = 8;
	reader->nBitsRead = 0;
	reader->current = *pos;
	reader->error = 0;
	if (reader->beBits) reader->current = CxiReverseByte(reader->current);
}

uint32_t CxiConsumeBit(BIT_READER_8 *reader) {
	if (reader->pos >= reader->end) {
		//error
		reader->error = 1;
		return 0;
	}

	unsigned char byteVal = reader->current;
	reader->nBitsBuffered--;
	reader->nBitsRead++;

	if (reader->nBitsBuffered > 0) {
		reader->current >>= 1;
	} else {
		reader->pos++;
		if (reader->pos < reader->end) {
			reader->nBitsBuffered = 8;
			reader->current = *reader->pos;
			if (reader->beBits) reader->current = CxiReverseByte(reader->current);
		}
	}

	return byteVal & 1;
}

uint32_t CxiConsumeBits(BIT_READER_8 *bitReader, unsigned int nBits) {
	uint32_t string = 0, i = 0;
	for (i = 0; i < nBits; i++) {
		if (bitReader->pos >= bitReader->end) {
			//error
			bitReader->error = 1;
			return string;
		}

		bitReader->nBitsBuffered--;
		bitReader->nBitsRead++;
		if (bitReader->beBits) {
			string <<= 1;
			string |= (bitReader->current & 1);
		} else {
			string |= (bitReader->current & 1) << i;
		}

		if (bitReader->nBitsBuffered > 0) {
			bitReader->current >>= 1;
		} else {
			bitReader->pos++;
			if (bitReader->pos < bitReader->end) {
				bitReader->nBitsBuffered = 8;
				bitReader->current = *bitReader->pos;
				if (bitReader->beBits) bitReader->current = CxiReverseByte(bitReader->current);
			}
		}
	}

	return string;
}

uint32_t CxAshReadTree(BIT_READER_8 *reader, int width, uint32_t *leftTree, uint32_t *rightTree) {
	uint32_t *workmem = (uint32_t *) calloc(2 * (1 << width), sizeof(uint32_t));
	uint32_t *work = workmem;

	uint32_t r23 = (1 << width);
	uint32_t symRoot = 0;
	uint32_t nNodes = 0;
	do {
		int bit = CxiConsumeBit(reader);
		if (reader->error) goto Error;

		if (bit) {
			if (r23 >= (2 * (1u << width)) || nNodes >= (2 * (1u << width))) goto Error;

			*(work++) = r23 | TREE_RIGHT;
			*(work++) = r23 | TREE_LEFT;
			nNodes += 2;
			r23++;
		} else {
			if (nNodes == 0) goto Error;

			symRoot = CxiConsumeBits(reader, width);
			if (reader->error) goto Error;
			do {
				uint32_t nodeval = *--work;
				uint32_t idx = nodeval & TREE_VAL_MASK;
				nNodes--;
				if (nodeval & TREE_RIGHT) {
					rightTree[idx] = symRoot;
					symRoot = idx;
				} else {
					leftTree[idx] = symRoot;
					break;
				}
			} while (nNodes > 0);
		}
	} while (nNodes > 0);

	free(workmem);
	return symRoot;

Error:
	free(workmem);
	return UINT32_MAX;
}

unsigned char *CxDecompressAsh(const unsigned char *buffer, unsigned int size, unsigned int *uncompressedSize) {
	if (size < 0xC) {
		*uncompressedSize = 0;
		return NULL;
	}
	
	int symBits = 9, distBits = 11;
	uint32_t uncompSize = BigToLittle32(*(uint32_t *) (buffer + 4)) & 0x00FFFFFF;
	uint32_t outSize = uncompSize;
	
	BIT_READER_8 reader, reader2;
	const unsigned char *endp = buffer + size;
	uint32_t offsDstStream = BigToLittle32(*(const uint32_t *) (buffer + 0x8));
	if (offsDstStream >= size) {
		//must reserve at least some space to write a minimal tree there
		*uncompressedSize = 0;
		return NULL;
	}
	
	CxiInitBitReader(&reader, buffer + offsDstStream, endp, 1);
	CxiInitBitReader(&reader2, buffer + 0xC, endp, 1);
	
	uint8_t *outbuf = calloc(uncompSize, 1);
	uint8_t *destp = outbuf;

	uint32_t symMax = (1 << symBits);
	uint32_t distMax = (1 << distBits);
	
	uint32_t *symLeftTree = calloc(2 * symMax - 1, sizeof(uint32_t));
	uint32_t *symRightTree = calloc(2 * symMax - 1, sizeof(uint32_t));
	uint32_t *distLeftTree = calloc(2 * distMax - 1, sizeof(uint32_t));
	uint32_t *distRightTree = calloc(2 * distMax - 1, sizeof(uint32_t));

	uint32_t symRoot, distRoot;
	symRoot = CxAshReadTree(&reader2, symBits, symLeftTree, symRightTree);
	distRoot = CxAshReadTree(&reader, distBits, distLeftTree, distRightTree);
	if (symRoot == UINT32_MAX || distRoot == UINT32_MAX) goto Error;

	//main uncompress loop
	do {
		uint32_t sym = symRoot;
		while (sym >= symMax) {
			if (!CxiConsumeBit(&reader2)) {
				sym = symLeftTree[sym];
			} else {
				sym = symRightTree[sym];
			}
		}

		if (sym < 0x100) {
			*(destp++) = sym;
			uncompSize--;
		} else {
			uint32_t distsym = distRoot;
			while (distsym >= distMax) {
				if (!CxiConsumeBit(&reader)) {
					distsym = distLeftTree[distsym];
				} else {
					distsym = distRightTree[distsym];
				}
			}

			uint32_t copylen = (sym - 0x100) + 3;
			const uint8_t *srcp = destp - distsym - 1;
			
			if (copylen > uncompSize) goto Error;
			//if ((distsym + 1) >= (size_t) (destp - outbuf)) goto Error;
			
			uncompSize -= copylen;
			while (copylen--) {
				*(destp++) = *(srcp++);
			}
		}
	} while (uncompSize > 0);

	if (0) {	
Error:
		//free output
		if (outbuf != NULL) free(outbuf);
		outbuf = NULL;
		outSize = 0;
	}

	if (symLeftTree != NULL) free(symLeftTree);
	if (symLeftTree != NULL) free(symRightTree);
	if (symLeftTree != NULL) free(distLeftTree);
	if (symLeftTree != NULL) free(distRightTree);

	*uncompressedSize = outSize;
	return outbuf;
}


unsigned char *CxPadCompressed(unsigned char *comp, unsigned int size, unsigned int boundary, unsigned int *pOutSize) {
	//check if size is already aligned to boundary
	if ((size % boundary) == 0) {
		*pOutSize = size;
		return comp;
	}
	
	//realloc
	unsigned outSize = (size + boundary - 1) / boundary * boundary;
	unsigned char *out = realloc(comp, outSize);
	if (out == NULL) {
		//error
		free(comp);
		return NULL;
	}
	
	*pOutSize = outSize;
	return out;
}


// ----- LZ Compression Routines


#define LZ_MIN_DISTANCE        0x01   // minimum distance per LZ encoding
#define LZ_MIN_SAFE_DISTANCE   0x02   // minimum safe distance per BIOS LZ bug
#define LZ_MAX_DISTANCE      0x1000   // maximum distance per LZ encoding
#define LZ_MIN_LENGTH          0x03   // minimum length per LZ encoding
#define LZ_MAX_LENGTH          0x12   // maximum length per LZ encoding


//struct for mapping an LZ graph
typedef struct CxiLzNode_ {
	uint16_t distance;         // distance of node if reference
	uint16_t length;           // length of node
	uint32_t weight;           // weight of node
} CxiLzNode;

//struct for representing tokenized LZ data
typedef struct CxiLzToken_ {
	uint8_t isReference;
	union {
		uint8_t symbol;
		struct {
			int16_t length;
			int16_t distance;
		};
	};
} CxiLzToken;

//struct for keeping track of LZ sliding window
typedef struct CxiLzState_ {
	const unsigned char *buffer;
	unsigned int size;
	unsigned int pos;
	unsigned int minLength;
	unsigned int maxLength;
	unsigned int minDistance;
	unsigned int maxDistance;
	unsigned int symLookup[512];
	unsigned int *chain;
} CxiLzState;

static unsigned int CxiLzHash3(const unsigned char *p) {
	unsigned char c0 = p[0];         // A
	unsigned char c1 = p[0] ^ p[1];  // A ^ B
	unsigned char c2 = p[0] ^ p[2];  // (A ^ B) ^ (B ^ C)
	return (c0 ^ (c1 << 1) ^ (c2 << 2) ^ (c2 >> 7)) & 0x1FF;
}

static void CxiLzStateInit(CxiLzState *state, const unsigned char *buffer, unsigned int size, unsigned int minLength, unsigned int maxLength, unsigned int minDistance, unsigned int maxDistance) {
	state->buffer = buffer;
	state->size = size;
	state->pos = 0;
	state->minLength = minLength;
	state->maxLength = maxLength;
	state->minDistance = minDistance;
	state->maxDistance = maxDistance;

	for (unsigned int i = 0; i < 512; i++) {
		//init symbol lookup to empty
		state->symLookup[i] = UINT_MAX;
	}

	state->chain = (unsigned int *) calloc(state->maxDistance, sizeof(unsigned int));
	for (unsigned int i = 0; i < state->maxDistance; i++) {
		state->chain[i] = UINT_MAX;
	}
}

static void CxiLzStateFree(CxiLzState *state) {
	free(state->chain);
}

static unsigned int CxiLzStateGetChainIndex(CxiLzState *state, unsigned int index) {
	return (state->pos - index) % state->maxDistance;
}

static unsigned int CxiLzStateGetChain(CxiLzState *state, int index) {
	unsigned int chainIndex = CxiLzStateGetChainIndex(state, index);

	return state->chain[chainIndex];
}

static void CxiLzStatePutChain(CxiLzState *state, unsigned int index, unsigned int data) {
	unsigned int chainIndex = CxiLzStateGetChainIndex(state, index);

	state->chain[chainIndex] = data;
}

static void CxiLzStateSlideByte(CxiLzState *state) {
	if (state->pos >= state->size) return; // cannot slide

	//only update search structures when we have enough space left to necessitate searching.
	if ((state->size - state->pos) >= 3) {
		//fetch next 3 bytes' hash
		unsigned int next = CxiLzHash3(state->buffer + state->pos);

		//get the distance back to the next byte before sliding. If it exists in the window,
		//we'll have nextDelta less than UINT_MAX. We'll take this first occurrence and it 
		//becomes the offset from the current byte. Bear in mind the chain is 0-indexed starting
		//at a distance of 1. 
		unsigned int nextDelta = state->symLookup[next];
		if (nextDelta != UINT_MAX) {
			nextDelta++;
			if (nextDelta >= state->maxDistance) {
				nextDelta = UINT_MAX;
			}
		}
		CxiLzStatePutChain(state, 0, nextDelta);

		//increment symbol lookups
		for (int i = 0; i < 512; i++) {
			if (state->symLookup[i] != UINT_MAX) {
				state->symLookup[i]++;
				if (state->symLookup[i] > state->maxDistance) state->symLookup[i] = UINT_MAX;
			}
		}
		state->symLookup[next] = 0; // update entry for the current byte to the start of the chain
	}

	state->pos++;
}

static void CxiLzStateSlide(CxiLzState *state, unsigned int nSlide) {
	while (nSlide--) CxiLzStateSlideByte(state);
}

static unsigned int CxiCompareMemory(const unsigned char *b1, const unsigned char *b2, unsigned int nMax) {
	//compare nAbsoluteMax bytes, do not perform any looping.
	unsigned int nSame = 0;
	while (nMax > 0) {
		if (*(b1++) != *(b2++)) break;
		nMax--;
		nSame++;
	}
	return nSame;
}

static int CxiLzConfirmMatch(const unsigned char *buffer, unsigned int size, unsigned int pos, unsigned int distance, unsigned int length) {
	(void) size;

	//compare string match
	return memcmp(buffer + pos, buffer + pos - distance, length) == 0;
}

static unsigned int CxiLzSearch(CxiLzState *state, unsigned int *pDistance) {
	unsigned int nBytesLeft = state->size - state->pos;
	if (nBytesLeft < 3 || nBytesLeft < state->minLength) {
		*pDistance = 0;
		return 1;
	}

	unsigned int firstMatch = state->symLookup[CxiLzHash3(state->buffer + state->pos)];
	if (firstMatch == UINT_MAX) {
		//return byte literal
		*pDistance = 0;
		return 1;
	}

	unsigned int distance = firstMatch + 1;
	unsigned int bestLength = 1, bestDistance = 0;

	unsigned int nMaxCompare = state->maxLength;
	if (nMaxCompare > nBytesLeft) nMaxCompare = nBytesLeft;

	//search backwards
	const unsigned char *curp = state->buffer + state->pos;
	while (distance <= state->maxDistance) {
		//check only if distance is at least minDistance
		if (distance >= state->minDistance) {
			unsigned int matchLen = CxiCompareMemory(curp - distance, curp, nMaxCompare);

			if (matchLen > bestLength) {
				bestLength = matchLen;
				bestDistance = distance;
				if (bestLength == nMaxCompare) break;
			}
		}

		if (distance == state->maxDistance) break;
		unsigned int next = CxiLzStateGetChain(state, distance);
		if (next == UINT_MAX) break;
		distance += next;
	}

	if (bestLength < state->minLength) {
		bestLength = 1;
		distance = 0;
	}
	*pDistance = bestDistance;
	return bestLength;
}

static inline int CxiLzNodeIsReference(const CxiLzNode *node) {
	return node->length >= LZ_MIN_LENGTH;
}

//length of compressed data output by LZ token
static inline unsigned int CxiLzTokenCost(unsigned int length) {
	unsigned int nBytesToken;
	if (length >= LZ_MIN_LENGTH) {
		nBytesToken = 2;
	} else {
		nBytesToken = 1;
	}
	return 1 + nBytesToken * 8;
}


unsigned char *CxCompressLZ(const unsigned char *buffer, unsigned int size, unsigned int *compressedSize) {
	CxiLzState state;
	CxiLzStateInit(&state, buffer, size, LZ_MIN_LENGTH, LZ_MAX_LENGTH, LZ_MIN_SAFE_DISTANCE, LZ_MAX_DISTANCE);

	//create node list and fill in the maximum string reference sizes
	CxiLzNode *nodes = (CxiLzNode *) calloc(size, sizeof(CxiLzNode));
	unsigned int pos = 0;
	while (pos < size) {
		unsigned int dst;
		unsigned int len = CxiLzSearch(&state, &dst);

		//store longest found match
		nodes[pos].length = len;
		nodes[pos].distance = dst;

		pos++;
		CxiLzStateSlide(&state, 1);
	}
	CxiLzStateFree(&state);

	//work backwards from the end of file
	pos = size;
	while (pos--) {
		//get node at pos
		CxiLzNode *node = nodes + pos;

		//search for largest LZ string match
		unsigned int len = nodes[pos].length;
		unsigned int dist = nodes[pos].distance;

		//if node takes us to the end of file, set weight to cost of this node.
		if ((pos + len) == size) {
			//token takes us to the end of the file, its weight equals this token cost.
			node->length = len;
			node->distance = dist;
			node->weight = CxiLzTokenCost(len);
		} else {
			//else, search LZ matches from here down.
			unsigned int weightBest = UINT_MAX;
			unsigned int lenBest = 1;
			while (len) {
				//measure cost
				unsigned int weightNext = nodes[pos + len].weight;
				unsigned int weight = CxiLzTokenCost(len) + weightNext;
				if (weight < weightBest) {
					lenBest = len;
					weightBest = weight;
				}

				//decrement length w.r.t. length discontinuity
				len--;
				if (len != 0 && len < LZ_MIN_LENGTH) len = 1;
			}

			//put node
			node->length = lenBest;
			node->distance = dist;
			node->weight = weightBest;
		}
	}

	//from here on, we have a direct path to the end of file. All we need to do is traverse it.

	//get max compressed size
	unsigned int maxCompressed = 4 + size + (size + 7) / 8;

	//encode LZ data
	unsigned char *buf = (unsigned char *) calloc(maxCompressed, 1);
	unsigned char *bufpos = buf;
	*(uint32_t *) (bufpos) = (size << 8) | 0x10;
	bufpos += 4;

	CxiLzNode *curnode = &nodes[0];

	unsigned int srcpos = 0;
	while (srcpos < size) {
		uint8_t head = 0;
		unsigned char *headpos = bufpos++;

		for (unsigned int i = 0; i < 8 && srcpos < size; i++) {
			unsigned int length = curnode->length;
			unsigned int distance = curnode->distance;

			if (CxiLzNodeIsReference(curnode)) {
				//node is reference
				uint16_t enc = (distance - LZ_MIN_DISTANCE) | ((length - LZ_MIN_LENGTH) << 12);
				*(bufpos++) = (enc >> 8) & 0xFF;
				*(bufpos++) = (enc >> 0) & 0xFF;
				head |= 1 << (7 - i);
			} else {
				//node is literal byte
				*(bufpos++) = buffer[srcpos];
			}

			srcpos += length; //remember: nodes correspond to byte positions
			curnode += length;
		}

		//put head byte
		*headpos = head;
	}

	//nodes no longer needed
	free(nodes);

	unsigned int outSize = bufpos - buf;
	*compressedSize = outSize;
	return CxiShrink(buf, outSize); //reduce buffer size
}


// ----- ASH compression routines


static uint32_t LittleToBig(uint32_t i)  {
	return ((i >> 24) | (i << 24) | ((i & 0x00FF0000) >> 8) | ((i & 0x0000FF00) << 8));
}

#define min(a,b)    (((a)<(b))?(a):(b))
#define max(a,b)    (((a)>(b))?(a):(b))


typedef struct CxiHuffNode_ {
	uint16_t sym;
	uint16_t symMin; //had space to spare, maybe make searches a little simpler
	uint16_t symMax;
	uint16_t nRepresent;
	int freq;
	struct CxiHuffNode_ *left;
	struct CxiHuffNode_ *right;
} CxiHuffNode;



typedef struct BITSTREAM_ {
	uint32_t *bits;
	int nWords;
	int nBitsInLastWord;
	int nWordsAlloc;
	int length;
} BITSTREAM;



// ----- bit stream

static void CxiBitStreamCreate(BITSTREAM *stream) {
	stream->nWords = 0;
	stream->length = 0;
	stream->nBitsInLastWord = 32;
	stream->nWordsAlloc = 16;
	stream->bits = (uint32_t *) calloc(stream->nWordsAlloc, 4);
}

static void CxiBitStreamFree(BITSTREAM *stream) {
	free(stream->bits);
}

static void CxiBitStreamWrite(BITSTREAM *stream, int bit) {
	if (stream->nBitsInLastWord == 32) {
		stream->nBitsInLastWord = 0;
		stream->nWords++;
		if (stream->nWords > stream->nWordsAlloc) {
			int newAllocSize = (stream->nWordsAlloc + 2) * 3 / 2;
			stream->bits = realloc(stream->bits, newAllocSize * 4);
			stream->nWordsAlloc = newAllocSize;
		}
		stream->bits[stream->nWords - 1] = 0;
	}

	stream->bits[stream->nWords - 1] |= (bit << (31 - stream->nBitsInLastWord));
	stream->nBitsInLastWord++;
	stream->length++;
}

static void *CxiBitStreamGetBytes(BITSTREAM *stream, int wordAlign, int beBytes, int beBits, unsigned int *size) {
	//allocate buffer
	unsigned int outSize = stream->nWords * 4;
	if (!wordAlign) {
		//nBitsInLast word is 32 if last word is full, 0 if empty.
		if (stream->nBitsInLastWord <= 24) outSize--;
		if (stream->nBitsInLastWord <= 16) outSize--;
		if (stream->nBitsInLastWord <=  8) outSize--;
		if (stream->nBitsInLastWord <=  0) outSize--;
	}
	unsigned char *outbuf = (unsigned char *) calloc(outSize, 1);
	if (outbuf == NULL) return NULL;

	//this function handles converting byte and bit orders from the internal
	//representation. Internally, we store the bit sequence as an array of
	//words, where the first bits are inserted at the most significant bit.
	//

	for (unsigned int i = 0; i < outSize; i++) {
		int byteShift = 8 * ((beBytes) ? (3 - (i % 4)) : (i % 4));
		uint32_t word = stream->bits[i / 4];
		uint8_t byte = (word >> byteShift) & 0xFF;

		//if little endian bit order, swap here
		if (!beBits) {
			uint8_t temp = byte;
			byte = 0;
			for (int j = 0; j < 8; j++) byte |= ((temp >> j) & 1) << (7 - j);
		}
		outbuf[i] = byte;
	}

	*size = outSize;
	return outbuf;
}

static void CxiBitStreamWriteBitsBE(BITSTREAM *stream, uint32_t bits, int nBits) {
	for (int i = 0; i < nBits; i++) CxiBitStreamWrite(stream, (bits >> (nBits - 1 - i)) & 1);
}


static unsigned int CxiSearchLZ(const unsigned char *buffer, unsigned int size, unsigned int curpos, unsigned int minDistance, unsigned int maxDistance, unsigned int maxLength, unsigned int *pDistance) {
	//nProcessedBytes = curpos
	unsigned int nBytesLeft = size - curpos;

	//the maximum distance we can search backwards is limited by how far into the buffer we are. It won't
	//make sense to a decoder to copy bytes from before we've started.
	if (maxDistance > curpos) maxDistance = curpos;

	//keep track of the biggest match and where it was
	unsigned int biggestRun = 0, biggestRunIndex = 0;

	//the longest string we can match, including repetition by overwriting the source.
	unsigned int nMaxCompare = maxLength;
	if (nMaxCompare > nBytesLeft) nMaxCompare = nBytesLeft;

	//begin searching backwards.
	for (unsigned int j = minDistance; j <= maxDistance; j++) {
		//compare up to 0xF bytes
		unsigned int nMatched = CxiCompareMemory(buffer - j, buffer, nMaxCompare);
		if (nMatched > biggestRun) {
			biggestRun = nMatched;
			biggestRunIndex = j;
			if (biggestRun == nMaxCompare) break;
		}
	}

	*pDistance = biggestRunIndex;
	return biggestRun;
}

static unsigned int CxiSearchLZRestricted(const unsigned char *buffer, unsigned int size, unsigned int curpos, const unsigned int *distances, int nDistances, unsigned int maxLength, unsigned int *pDistance) {
	if (nDistances == 0) {
		*pDistance = 0;
		return 0;
	}
	
	//nProcessedBytes = curpos
	unsigned int nBytesLeft = size - curpos;

	//the maximum distance we can search backwards is limited by how far into the buffer we are. It won't
	//make sense to a decoder to copy bytes from before we've started.
	unsigned int maxDistance = distances[nDistances - 1];
	if (maxDistance > curpos) maxDistance = curpos;

	//keep track of the biggest match and where it was
	unsigned int biggestRun = 0, biggestRunIndex = 0;

	//the longest string we can match, including repetition by overwriting the source.
	unsigned int nMaxCompare = maxLength;
	if (nMaxCompare > nBytesLeft) nMaxCompare = nBytesLeft;

	//begin searching backwards.
	for (int i = 0; i < nDistances; i++) {
		unsigned int j = distances[i];
		if (j > maxDistance) break;
		
		unsigned int nMatched = CxiCompareMemory(buffer - j, buffer, nMaxCompare);
		if (nMatched > biggestRun) {
			biggestRun = nMatched;
			biggestRunIndex = j;
			if (biggestRun == nMaxCompare) break;
		}
	}

	*pDistance = biggestRunIndex;
	return biggestRun;
}



// ----- Huffman tree code

#define ISLEAF(n) ((n)->left==NULL&&(n)->right==NULL)

static void CxiHuffmanInit(CxiHuffNode *nodes, unsigned int nNodes) {
	memset(nodes, 0, nNodes * 2 * sizeof(CxiHuffNode));
	for (unsigned int i = 0; i < nNodes; i++) {
		nodes[i].symMin = nodes[i].symMax = nodes[i].sym = i;
		nodes[i].nRepresent = 1;
	}
}

static int CxiHuffmanNodeComparator(const void *p1, const void *p2) {
	return ((CxiHuffNode *) p2)->freq - ((CxiHuffNode *) p1)->freq;
}

static void CxiHuffmanMakeShallowFirst(CxiHuffNode *node) {
	if (ISLEAF(node)) return;
	if (node->left->nRepresent > node->right->nRepresent) {
		CxiHuffNode *left = node->left;
		node->left = node->right;
		node->right = left;
	}
	CxiHuffmanMakeShallowFirst(node->left);
	CxiHuffmanMakeShallowFirst(node->right);
}

static int CxiHuffmanHasSymbol(CxiHuffNode *node, uint16_t sym) {
	if (ISLEAF(node)) return node->sym == sym;
	if (sym < node->symMin || sym > node->symMax) return 0;
	CxiHuffNode *left = node->left;
	CxiHuffNode *right = node->right;
	return CxiHuffmanHasSymbol(left, sym) || CxiHuffmanHasSymbol(right, sym);
}

static void CxiHuffmanWriteSymbol(BITSTREAM *bits, uint16_t sym, CxiHuffNode *tree) {
	if (ISLEAF(tree)) return;
	
	CxiHuffNode *left = tree->left;
	CxiHuffNode *right = tree->right;
	if (CxiHuffmanHasSymbol(left, sym)) {
		CxiBitStreamWrite(bits, 0);
		CxiHuffmanWriteSymbol(bits, sym, left);
	} else {
		CxiBitStreamWrite(bits, 1);
		CxiHuffmanWriteSymbol(bits, sym, right);
	}
}

static void CxiHuffmanConstructTree(CxiHuffNode *nodes, int nNodes) {
	//sort by frequency, then cut off the remainder (freq=0).
	qsort(nodes, nNodes, sizeof(CxiHuffNode), CxiHuffmanNodeComparator);
	for (int i = 0; i < nNodes; i++) {
		if (nodes[i].freq == 0) {
			nNodes = i;
			break;
		}
	}

	//unflatten the histogram into a huffman tree. 
	int nRoots = nNodes;
	int nTotalNodes = nNodes;
	while (nRoots > 1) {
		//copy bottom two nodes to just outside the current range
		CxiHuffNode *srcA = nodes + nRoots - 2;
		CxiHuffNode *destA = nodes + nTotalNodes;
		memcpy(destA, srcA, sizeof(CxiHuffNode));

		CxiHuffNode *left = destA;
		CxiHuffNode *right = nodes + nRoots - 1;
		CxiHuffNode *branch = srcA;

		branch->freq = left->freq + right->freq;
		branch->sym = 0;
		branch->left = left;
		branch->right = right;
		branch->symMin = min(left->symMin, right->symMin);
		branch->symMax = max(right->symMax, left->symMax);
		branch->nRepresent = left->nRepresent + right->nRepresent; //may overflow for root, but the root doesn't really matter for this

		nRoots--;
		nTotalNodes++;
		qsort(nodes, nRoots, sizeof(CxiHuffNode), CxiHuffmanNodeComparator);
	}

	//just to be sure, make sure the shallow node always comes first
	CxiHuffmanMakeShallowFirst(nodes);
}

static unsigned int CxiHuffmanGetNodeDepth(CxiHuffNode *tree, unsigned int sym) {
	if (tree->left == NULL) {
		//this is the node
		return 0;
	}
	
	if (CxiHuffmanHasSymbol(tree->left, sym)) {
		return CxiHuffmanGetNodeDepth(tree->left, sym) + 1;
	} else if (CxiHuffmanHasSymbol(tree->right, sym)) {
		return CxiHuffmanGetNodeDepth(tree->right, sym) + 1;
	} else {
		//!!!
		return 0;
	}
}

typedef struct CxiHuffSymbolInfo_ {
	uint16_t sym;
	uint16_t depth;
} CxiHuffSymbolInfo;

static int CxiHuffmanCountSymbolsOver(CxiHuffNode *tree, unsigned int nMin) {
	if (tree->left == NULL) {
		return tree->sym >= nMin;
	}
	return CxiHuffmanCountSymbolsOver(tree->left, nMin) + CxiHuffmanCountSymbolsOver(tree->right, nMin);
}

static CxiHuffSymbolInfo *CxiHuffmanEnumerateSymbolInfoInternal(CxiHuffNode *tree, CxiHuffSymbolInfo *buf, int depth, unsigned int nMin) {
	if (tree->left == NULL) {
		if (tree->sym >= nMin) {
			buf->sym = tree->sym;
			buf->depth = depth;
			buf++;
		}
	} else {
		//run recursion
		buf = CxiHuffmanEnumerateSymbolInfoInternal(tree->left, buf, depth + 1, nMin);
		buf = CxiHuffmanEnumerateSymbolInfoInternal(tree->right, buf, depth + 1, nMin);
	}
	return buf;
}

static int CxiHuffSymbolInfoComparator(const void *e1, const void *e2) {
	const CxiHuffSymbolInfo *d1 = (const CxiHuffSymbolInfo *) e1;
	const CxiHuffSymbolInfo *d2 = (const CxiHuffSymbolInfo *) e2;
	
	if (d1->sym < d2->sym) return -1;
	if (d1->sym > d2->sym) return +1;
	return 0;
}

static CxiHuffSymbolInfo *CxiHuffmanEnumerateSymbolInfo(CxiHuffNode *tree, int *pCount, unsigned int nMin) {
	int nNode = CxiHuffmanCountSymbolsOver(tree, nMin);
	CxiHuffSymbolInfo *buf = (CxiHuffSymbolInfo *) calloc(nNode, sizeof(CxiHuffSymbolInfo));
	if (buf == NULL) return NULL;
	
	CxiHuffmanEnumerateSymbolInfoInternal(tree, buf, 0, nMin);
	
	//sort
	qsort(buf, nNode, sizeof(*buf), CxiHuffSymbolInfoComparator);
	
	*pCount = nNode;
	return buf;
}


// ----- ASH code

static void CxiAshEnsureTreeElements(CxiHuffNode *nodes, int nNodes, int nMinNodes) {
	//count nodes
	int nPresent = 0;
	for (int i = 0; i < nNodes; i++) {
		if (nodes[i].freq) nPresent++;
	}
	
	//have sufficient nodes?
	if (nPresent >= nMinNodes) return;
	
	//add dummy nodes
	for (int i = 0; i < nNodes; i++) {
		if (nodes[i].freq == 0) {
			nodes[i].freq = 1;
			nPresent++;
			if (nPresent >= nMinNodes) return;
		}
	}
}

static void CxiAshWriteTree(BITSTREAM *stream, CxiHuffNode *nodes, int nBits) {
	if (nodes->left != NULL) {
		//
		CxiBitStreamWrite(stream, 1);
		CxiAshWriteTree(stream, nodes->left, nBits);
		CxiAshWriteTree(stream, nodes->right, nBits);
	} else {
		//write value
		CxiBitStreamWrite(stream, 0);
		CxiBitStreamWriteBitsBE(stream, nodes->sym, nBits);
	}
}

static CxiLzToken *CxiAshTokenize(const unsigned char *buffer, unsigned int size, int nSymBits, int nDstBits, unsigned int *pnTokens) {
	unsigned int nTokens = 0, tokenBufferSize = 16;
	CxiLzToken *tokenBuffer = (CxiLzToken *) calloc(tokenBufferSize, sizeof(CxiLzToken));
	if (tokenBuffer == NULL) return NULL;
	
	//
	unsigned int curpos = 0;
	while (curpos < size) {
		//ensure buffer capacity
		if (nTokens + 1 > tokenBufferSize) {
			tokenBufferSize = (tokenBufferSize + 2) * 3 / 2;
			tokenBuffer = (CxiLzToken *) realloc(tokenBuffer, tokenBufferSize * sizeof(CxiLzToken));
		}
		
		//search backwards
		unsigned int length, distance;
		length = CxiSearchLZ(buffer, size, curpos, 1, (1 << nDstBits), (1 << nSymBits) - 1 - 0x100 + 3, &distance);
		
		CxiLzToken *token = &tokenBuffer[nTokens++];
		if (length >= 3) {
			token->isReference = 1;
			token->length = length;
			token->distance = distance;
			
			buffer += length;
			curpos += length;
		} else  {
			token->isReference = 0;
			token->symbol = *(buffer++);
			curpos++;
		}
	}
	
	*pnTokens = nTokens;
	tokenBuffer = realloc(tokenBuffer, nTokens * sizeof(CxiLzToken));
	return tokenBuffer;
}

static void CxiAshGenHuffman(const CxiLzToken *tokens, unsigned int nTokens, CxiHuffNode *symNodes, unsigned int nSymNodes, CxiHuffNode *dstNodes, unsigned int nDstNodes) {
	CxiHuffmanInit(symNodes, nSymNodes);
	CxiHuffmanInit(dstNodes, nDstNodes);
	
	//construct frequency distribution
	for (unsigned int i = 0; i < nTokens; i++) {
		const CxiLzToken *token = &tokens[i];
		if (token->isReference) {
			symNodes[token->length - 3 + 0x100].freq++;
			dstNodes[token->distance - 1].freq++;
		} else {
			symNodes[token->symbol].freq++;
		}
	}
	
	//pre-tree construction: ensure at least two nodes are used
	CxiAshEnsureTreeElements(symNodes, nSymNodes, 2);
	CxiAshEnsureTreeElements(dstNodes, nDstNodes, 2);
	
	//construct trees
	CxiHuffmanConstructTree(symNodes, nSymNodes);
	CxiHuffmanConstructTree(dstNodes, nDstNodes);
}

static unsigned int CxiAshRoundDown(unsigned int sym, unsigned int *vals, unsigned int nVals, int *pIndex) {
	//if sym = 0, return 0 (no element can be equal)
	if (sym == 0) {
		*pIndex = -1;
		return 0;
	}
	
	//check lowest value <=
	unsigned int idxLo = 0, idxHi = nVals;
	unsigned int lo = 0;
	int loIndex = -1;
	while ((idxHi - idxLo) > 0) {
		unsigned int idxMed = (idxLo + idxHi) / 2;
		unsigned int valMed = vals[idxMed];
		
		//check low or high
		if (sym < valMed) {
			idxHi = idxMed;
		} else if (sym > valMed) {
			idxLo = idxMed + 1;
			loIndex = idxMed;
			lo = valMed;
		} else {
			//exact match
			*pIndex = idxMed;
			return sym;
		}
	}
	
	//if lo == 0, no match found, so return 1 with index of -1 (1 is implicitly in the list)
	if (lo == 0) lo = 1;
	*pIndex = loIndex;
	return lo;
}

static CxiLzToken *CxiAshRetokenize(const unsigned char *buffer, unsigned int size, int nSymBits, int nDstBits, CxiHuffNode *symNodes, CxiHuffNode *dstNodes, unsigned int *pnTokens) {
	//allocate graph
	CxiLzNode *nodes = (CxiLzNode *) calloc(size, sizeof(CxiLzNode));
	if (nodes == NULL) return NULL;
	
	//get a list of allowed distances
	int nLenNodesAvailable, nDstNodesAvailable;
	CxiHuffSymbolInfo *lenInfo = CxiHuffmanEnumerateSymbolInfo(symNodes, &nLenNodesAvailable, 0x100);
	CxiHuffSymbolInfo *dstInfo = CxiHuffmanEnumerateSymbolInfo(dstNodes, &nDstNodesAvailable,     0);

	//create array of symbol lengths
	unsigned int minDstCost = UINT_MAX;
	unsigned int *symDepths = (unsigned int *) calloc(1 << nSymBits, sizeof(unsigned int));
	unsigned int *dstDepths = (unsigned int *) calloc(1 << nDstBits, sizeof(unsigned int));
	for (int i = 0; i < (1 << nSymBits); i++) {
		if (CxiHuffmanHasSymbol(symNodes, i)) symDepths[i] = CxiHuffmanGetNodeDepth(symNodes, i);
	}
	for (int i = 0; i < (1 << nDstBits); i++) {
		if (CxiHuffmanHasSymbol(dstNodes, i)) dstDepths[i] = CxiHuffmanGetNodeDepth(dstNodes, i);
	}
	for (int i = 0; i < (1 << nDstBits); i++) {
		if (dstDepths[i] && dstDepths[i] < minDstCost) minDstCost = dstDepths[i];
	}
	
	//create array of allowed lengths
	unsigned int *lens = (unsigned int *) calloc(nLenNodesAvailable, sizeof(unsigned int));
	for (int i = 0; i < nLenNodesAvailable; i++) lens[i] = lenInfo[i].sym - 0x100 + 3;
	
	//create array of allowed distances
	unsigned int *dsts = (unsigned int *) calloc(nDstNodesAvailable, sizeof(unsigned int));
	for (int i = 0; i < nDstNodesAvailable; i++) dsts[i] = dstInfo[i].sym + 1;
	
	//scan backwards from end of file
	unsigned int pos = size;
	while (pos-- > 0) {
		//search LZ
		unsigned int length = 0, distance = 0;
		if (nLenNodesAvailable > 0) {
			unsigned int maxCompare = lens[nLenNodesAvailable - 1];
			if ((size - pos) < maxCompare) {
				int lengthIndex; //dummy
				maxCompare = size - pos;
				maxCompare = CxiAshRoundDown(maxCompare, lens, nLenNodesAvailable, &lengthIndex);
			}
			length = CxiSearchLZRestricted(buffer + pos, size, pos, dsts, nDstNodesAvailable, maxCompare, &distance);
		}
		
		//check: length must be in the allowed lengths list.
		int lengthIndex = -1;
		if (length >= 3) {
			length = CxiAshRoundDown(length, lens, nLenNodesAvailable, &lengthIndex);
		} else {
			length = 1;
		}
		
		//NOTE: all byte values that appear in the file will have a symbol associated since they must appear at least once.
		//thus we do not need to check that any byte value exists.
		
		//check length (should store reference?)
		unsigned int weight = 0;
		if (length < 3) {
			//byte literal (can't go lower)
			length = 1;
			
			//compute cost of byte literal
			weight = symDepths[buffer[pos]];
			if ((pos + 1) < size) {
				//add next weight
				weight += nodes[pos + 1].weight;
			}
		} else {
			//get cost of selected distance
			unsigned int dstCost = dstDepths[distance - 1];
			
			//scan size down
			unsigned int weightBest = UINT_MAX, lengthBest = length; 
			while (length) {
				unsigned int thisWeight;
				
				//compute weight of this length value
				unsigned int thisLengthWeight;
				if (length > 1) {
					//length > 1: symbol (use cost of length symbol)
					thisLengthWeight = lenInfo[lengthIndex].depth;
				} else {
					//length == 1: byte literal (use cost of byte literal)
					thisLengthWeight = symDepths[buffer[pos]];
				}
				
				//takes us to end of file? 
				if ((pos + length) == size) {
					//cost is just this node's weight
					thisWeight = thisLengthWeight;
				} else {
					//cost is this node's weight plus the weight of the next node
					CxiLzNode *next = nodes + pos + length;
					thisWeight = thisLengthWeight + next->weight;
				}
				
				//check if this node has better total weight. We allow the match to be equal since we're scanning
				//sizes down, we'll prefer shorter lengths. This pushes the distribution of lengths to the lower
				//end allowing for better Huffman coding. This also emphasizes byte literals, which have no distance
				//cost. We add the minimum distance cost here to offset this (lack of) cost.
				if (thisWeight <= weightBest || (length == 1 && thisWeight <= (weightBest + minDstCost))) {
					weightBest = thisWeight;
					lengthBest = length;
				}
				
				//decrement length (with respect to allowed length encodings)
				lengthIndex--;
				if (lengthIndex >= 0) {
					//length in list
					length = lens[lengthIndex];
				} else if (lengthIndex == -1) {
					//decrement to 1
					length = 1;
				} else {
					//end
					break;
				}
			}
			
			length = lengthBest;
			if (length < 3) {
				//byte literal (distance cost is thus now zero since we have no distance component)
				length = 1;
				dstCost = 0;
			} else {
				//we ended up selecting an LZ copy-able length. but did we select the most optimal distance
				//encoding?
				//search possible distances where we can match the string at. We'll take the lowest-cost one.
				for (int i = 0; i < nDstNodesAvailable; i++) {
					unsigned int dst = dsts[i];
					if (dst > pos) break;

					//matching distance, check the cost
					if (dstInfo[i].depth < dstCost) {
						//check matching LZ string...
						if (CxiLzConfirmMatch(buffer, size, pos, dst, length)) {
							dstCost = dstInfo[i].depth;
							distance = dst;
						}
					}
				}
			}
			weight = weightBest + dstCost;
		}
		
		//write node
		if (length >= 3) {
			nodes[pos].distance = distance;
			nodes[pos].length = length;
		} else {
			nodes[pos].length = 1;
		}
		nodes[pos].weight = weight;
	}
	
	free(lens);
	free(dsts);
	free(lenInfo);
	free(dstInfo);
	free(symDepths);
	free(dstDepths);
	
	//convert graph into node array
	unsigned int nTokens = 0;
	pos = 0;
	while (pos < size) {
		CxiLzNode *node = nodes + pos;
		nTokens++;
		
		pos += node->length;
	}
	
	CxiLzToken *tokens = (CxiLzToken *) calloc(nTokens, sizeof(CxiLzToken));
	if (tokens == NULL) {
		free(nodes);
		return NULL;
	}
	
	{
		pos = 0;
		unsigned int i = 0;
		while (pos < size) {
			CxiLzNode *node = nodes + pos;
			
			CxiLzToken *pToken = &tokens[i++];
			if (node->length >= 3) {
				pToken->isReference = 1;
				pToken->length = node->length;
				pToken->distance = node->distance;
			} else {
				pToken->isReference = 0;
				pToken->symbol = buffer[pos];
			}
			
			pos += node->length;
		}
		free(nodes);
	}
	
	*pnTokens = nTokens;
	return tokens;
}

unsigned char *CxCompressAsh(const unsigned char *buffer, unsigned int size, int nSymBits, int nDstBits, unsigned int nPasses, unsigned int *compressedSize) {
	//allocate tree structures
	int nSymNodes = (1 << nSymBits);
	int nDstNodes = (1 << nDstBits);
	CxiHuffNode *symNodes = (CxiHuffNode *) calloc(nSymNodes * 2, sizeof(CxiHuffNode));
	CxiHuffNode *dstNodes = (CxiHuffNode *) calloc(nDstNodes * 2, sizeof(CxiHuffNode));
	if (symNodes == NULL || dstNodes == NULL) {
		if (symNodes != NULL) free(symNodes);
		if (dstNodes != NULL) free(dstNodes);
		return NULL;
	}
	
	//tokenize
	unsigned int nTokens = 0;
	CxiLzToken *tokens = CxiAshTokenize(buffer, size, nSymBits, nDstBits, &nTokens);
	if (tokens == NULL) {
		free(symNodes);
		free(dstNodes);
		return NULL;
	}
	
	CxiAshGenHuffman(tokens, nTokens, symNodes, nSymNodes, dstNodes, nDstNodes);
	
	// ----------------------------------------------------------------------------------------------
	//    Herein lies the really expensive operations (both memory and time).
	// ----------------------------------------------------------------------------------------------
	
	//iterate on adjusting the frequency distribution and traversing the encoding space
	for (unsigned int i = 0; i < nPasses; i++) {
		//discard tokenized sequence. 
		free(tokens);

		//re-tokenize
		tokens = CxiAshRetokenize(buffer, size, nSymBits, nDstBits, symNodes, dstNodes, &nTokens);
		if (tokens == NULL) {
			free(symNodes);
			free(dstNodes);
			return NULL;
		}
		
		//regenerate huffman tree due to changes in frequency distribution
		CxiAshGenHuffman(tokens, nTokens, symNodes, nSymNodes, dstNodes, nDstNodes);
	}
	
	// ----------------------------------------------------------------------------------------------
	//    End of super intense operations
	// ----------------------------------------------------------------------------------------------
	
	//init streams
	BITSTREAM symStream, dstStream;
	CxiBitStreamCreate(&symStream);
	CxiBitStreamCreate(&dstStream);
	
	//first, write huffman trees.
	CxiAshWriteTree(&symStream, symNodes, nSymBits);
	CxiAshWriteTree(&dstStream, dstNodes, nDstBits);
	
	//write data stream
	for (unsigned int i = 0; i < nTokens; i++) {
		CxiLzToken *token = &tokens[i];
		
		if (token->isReference) {
			CxiHuffmanWriteSymbol(&symStream, token->length - 3 + 0x100, symNodes);
			CxiHuffmanWriteSymbol(&dstStream, token->distance - 1, dstNodes);
		} else {
			CxiHuffmanWriteSymbol(&symStream, token->symbol, symNodes);
		}
	}
	
	//free node and tree structs
	free(tokens);
	free(symNodes);
	free(dstNodes);
	
	//encode data output
	unsigned int symStreamSize = 0;
	unsigned int dstStreamSize = 0;
	void *symBytes = CxiBitStreamGetBytes(&symStream, 1, 1, 1, &symStreamSize);
	void *dstBytes = CxiBitStreamGetBytes(&dstStream, 1, 1, 1, &dstStreamSize);
	
	//write data out
	unsigned char *out = (unsigned char *) calloc(0xC + symStreamSize + dstStreamSize, 1);
	{
		//write header
		uint32_t header[3];
		header[0] = 0x30485341;    // 'ASH0'
		header[1] = LittleToBig(size);
		header[2] = LittleToBig(0xC + symStreamSize);
		memcpy(out, header, sizeof(header));
		
		//write streams
		memcpy(out + sizeof(header), symBytes, symStreamSize);
		memcpy(out + sizeof(header) + symStreamSize, dstBytes, dstStreamSize);
	}
	free(symBytes);
	free(dstBytes);
	
	//free stuff
	CxiBitStreamFree(&symStream);
	CxiBitStreamFree(&dstStream);
	
	*compressedSize = 0xC + symStreamSize + dstStreamSize;
	return out;
}


