#include "cmd_common.h"
#include "firmware.h"

void CmdHelpLoad(void) {
	puts("");
	puts("Usage: load <file name>");
	puts("");
	puts("Loads a firmare image from the given file path.");
}

void CmdProcLoad(int argc, const char **argv) {
	if (argc < 2) {
		CmdHelpLoad();
		return;
	}
	
	const char *filename = argv[1];
	LoadFirmwareImage(filename);
}

void CmdHelpSave(void) {
	puts("");
	puts("Usage: save [file name]");
	puts("");
	puts("Saves the working firmware image. A file name may optionally be specified.");
}

void CmdProcSave(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;
	
	const char *outpath = GetCurrentFilePath();
	if (argc >= 2) {
		outpath = argv[1];
	}
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	FILE *fp = fopen(outpath, "wb");
	if (fp == NULL) {
		printf("Could not open '%s' for write access.\n", outpath);
	}
	
	fwrite(buffer, size, 1, fp);
	fclose(fp);
}
