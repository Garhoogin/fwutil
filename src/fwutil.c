#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "compression.h"
#include "blowfish.h"
#include "firmware.h"
#include "cmd_common.h"


void CmdHelpQuit(void) {
	puts("");
	puts("Usage: quit");
	puts("");
	puts("Quits the program.");
}

void CmdProcQuit(int argc, const char **argv) {
	(void) argc;
	(void) argv;
	
	DoExit();
}


typedef struct CmdProcEntry_ {
	char *cmd;
	void (*proc) (int argc, const char **argv);
} CmdProcEntry;

static CmdProcEntry sProcTable[] = {
	{ "help",    CmdProcHelp    },
	{ "h",       CmdProcHelp    },
	
	{ "quit",    CmdProcQuit    },
	{ "q",       CmdProcQuit    },
	{ "exit",    CmdProcQuit    },
	
	{ "load",    CmdProcLoad    },
	{ "save",    CmdProcSave    },
	{ "info",    CmdProcInfo    },
	{ "verify",  CmdProcVerify  },
	{ "wl",      CmdProcWl      },
	{ "map",     CmdProcMap     },
	{ "compact", CmdProcCompact },
	
	{ "md5",     CmdProcMD5     },
	{ "clean",   CmdProcClean   },
	{ "restore", CmdProcRestore },
	{ "fix",     CmdProxFix     },
	{ "import",  CmdProcImport  },
	{ "export",  CmdProcExport  },
	{ "user",    CmdProcUser    },
	{ "loc",     CmdProcLoc     },
	{ "eb",      CmdProcEB      },
	{ "db",      CmdProcDB      }
};

static void CmdParse(const char *buffer, int *pArgc, char ***pArgv) {
	//empty command line?
	if (*buffer == '\0' || *buffer == '\n' || *buffer == '\r') {
		*pArgc = 0;
		*pArgv = NULL;
		return;
	}
	
	//count arguments
	const char *p = buffer;
	int quote = 0, nArg = 1, lastSpace = 0, nChars = 1;
	while (1) {
		char c = *(p++);
		if (c == '\0' || c == '\n' || c == '\r') break;
		
		int isWhiteSpace = (c == ' ' || c == '\t');
		
		if (c == '"') {
			quote = !quote;
		} else if (isWhiteSpace && !quote) {
			if (!lastSpace) {
				nArg++;
				nChars++; // null separator
			}
		} else {
			nChars++; // character in string
		}
		lastSpace = isWhiteSpace;
	}
	
	//fill arguments
	char **argv = (char **) calloc(nArg, sizeof(char *));
	char *argbuf = (char *) calloc(nChars, 1);
	char *argp = argbuf;
	int curArgv = 0;
	p = buffer;
	lastSpace = 0;
	quote = 0;
	argv[curArgv++] = argp; // first argument
	
	while (1) {
		char c = *(p++);
		if (c == '\0' || c == '\n' || c == '\r') {
			if (!lastSpace) {
				*(argp++) = '\0';
			}
			break;
		}
		
		int isWhiteSpace = (c == ' ' || c == '\t');
		
		if (c == '"') {
			quote = !quote;
		} else if (isWhiteSpace && !quote) {
			if (!lastSpace) {
				//put terminator and append to argument list
				*(argp++) = '\0';
				if (curArgv < nArg) argv[curArgv++] = argp;
			}
		} else {
			*(argp++) = c;
		}
		lastSpace = isWhiteSpace;
	}
	
	*pArgv = argv;
	*pArgc = nArg;
}

static void CmdFree(char **argv) {
	if (argv != NULL) {
		free(argv[0]);
		free(argv);
	}
}

static void CmdDispatch(int argc, const char **argv) {
	if (argc == 0) return;
	
	for (unsigned int i = 0; i < sizeof(sProcTable) / sizeof(sProcTable[0]); i++) {
		if (stricmp(sProcTable[i].cmd, argv[0]) == 0) {
			if (sProcTable[i].proc != NULL) sProcTable[i].proc(argc, argv);
			else puts("Unimplemented.");
			return;
		}
	}
	
	printf("Unrecognized command '%s'.\n", argv[0]);
}

static void CmdMain(const char *defpath) {
	puts("*******************************************************************************");
	puts("*                                                                             *");
	puts("*                        Nintendo DS Firmware Utility                         *");
	puts("*                                                                             *");
	puts("*******************************************************************************");
	puts("");
	if (defpath != NULL) {
		const char *loadargs[] = {
			"load", defpath
		};
		CmdProcLoad(2, loadargs);
		puts("");
	}
	puts("Type 'help' for a list of commands.");
	puts("");
	
	char buffer[1024];
	while (!IsExiting()) {
		printf("> ");
		fgets(buffer, sizeof(buffer), stdin);
		
		char **argv;
		int argc;
		CmdParse(buffer, &argc, &argv);
		CmdDispatch(argc, argv);
		CmdFree(argv);
		puts("");
	}
}

int main(int argc, char **argv) {
	CmdMain(argc >= 1 ? argv[1] : NULL);
	
	return 0;
}
