#include "cmd_common.h"
#include "firmware.h"

void CmdHelpEB(void) {
	puts("");
	puts("Usage: eb <address> <bytes...>");
	puts("");
	puts("Bytes are separated by spaces. The byte values and address are interpreted in");
	puts("hexadecimal by default. Prefix with '0n' to input decimal, or '0o' for octal.");
	puts("Hexadecimal values may optionally be prefixed with '0x' or suffixed with 'h'.");
}

void CmdProcEB(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	if (argc < 2) {
		CmdHelpEB();
		return;
	}
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	uint32_t addr = ParseArgNumber(argv[1]);
	if (addr >= size) {
		printf("Address %08X is out of bounds.\n", addr);
		return;
	}
	
	int i;
	for (i = 2; i < argc && addr < size; i++) {
		uint8_t bval = ParseArgNumber(argv[i]);
		buffer[addr++] = bval;
	}
	
	if (i < argc) {
		puts("");
		printf("Write truncated to %d byte(s).\n", i - 2);
	}
}

void CmdHelpDB(void) {
	puts("");
	puts("Usage: db <address> [size]");
	puts("");
	puts("Dumps the bytes at the specified address. If size is not specified, then up to");
	puts("128 bytes are dumped. The address is interpreted in hexadecimal by default.");
	puts("Prefix with '0n' to input decimal, or '0o' for octal. Hexadecimal values may");
	puts("optionally be prefixed with '0x' or suffixed with 'h'.");
}

void CmdProcDB(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	if (argc < 2) {
		CmdHelpDB();
		return;
	}
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	uint32_t addr = ParseArgNumber(argv[1]);
	
	if (addr >= size) {
		printf("Address %08X is out of bounds.\n", addr);
		return;
	}
	
	uint32_t nBytes = 0x80;
	if (nBytes > (size - addr)) nBytes = size - addr;
	if (argc >= 3) {
		nBytes = ParseArgNumber(argv[2]);
	}
	
	if (nBytes > (size - addr)) {
		nBytes = size - addr;
		printf("Dump truncated to %d bytes.\n", nBytes);
	}
	
	puts("");
	printf("          |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F | 0123456789ABCDEF \n");
	printf("----------+-------------------------------------------------+------------------\n");
	
	uint32_t rowAddr = (addr & ~0xF);
	uint32_t rowLast = (addr + nBytes - 1) & ~0xF;
	while (rowAddr <= rowLast) {
		printf(" %08X | ", rowAddr);
		
		for (unsigned int i = 0; i < 16; i++) {
			uint32_t baddr = rowAddr + i;
			if (baddr >= addr && baddr < (addr + nBytes)) printf("%02X ", buffer[baddr]);
			else printf("   ");
		}
		
		printf("| ");
		for (unsigned int i = 0; i < 16; i++) {
			uint32_t baddr = rowAddr + i;
			if (baddr >= addr && baddr < (addr + nBytes)) {
				unsigned char b = buffer[baddr];
				if (b < 0x20 || b >= 0x7F) b = '.';
				printf("%c", b);
			}
			else printf(" ");
		}
		
		printf("\n");
		
		rowAddr += 0x10;
	}
}

