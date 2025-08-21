#include "cmd_common.h"
#include "firmware.h"

#include <string.h>

typedef struct HelpEntry_ {
	const char *cmd;
	void (*helpProc) (void);
} HelpEntry;

static const HelpEntry sHelpEntries[] = {
	{ "help",    CmdHelpHelp    },
	{ "quit",    CmdHelpQuit    },
	{ "load",    CmdHelpLoad    },
	{ "save",    CmdHelpSave    },
	{ "info",    CmdHelpInfo    },
	{ "wl",      CmdHelpWl      },
	{ "verify",  CmdHelpVerify  },
	{ "map",     CmdHelpMap     },
	{ "compact", CmdHelpCompact },
	{ "md5",     CmdHelpMD5     },
	{ "clean",   CmdHelpClean   },
	{ "restore", CmdHelpRestore },
	{ "fix",     CmdHelpFix     },
	{ "import",  CmdHelpImport  },
	{ "export",  CmdHelpExport  },
	{ "user",    CmdHelpUser    },
	{ "loc",     CmdHelpLoc     },
	{ "eb",      CmdHelpEB      },
	{ "db",      CmdHelpDB      }
};

void CmdHelpHelp(void) {
	puts("");
	puts("Usage: help [command]");
	puts("");
	puts("Prints help for a given command. If no command is specified, a list of commands");
	puts("is printed.");
}

void CmdProcHelp(int argc, const char **argv) {
	(void) argc;
	(void) argv;
	
	if (argc >= 2) {
		const char *cmd = argv[1];
		
		for (unsigned int i = 0; i < sizeof(sHelpEntries) / sizeof(sHelpEntries[0]); i++) {
			if (stricmp(cmd, sHelpEntries[i].cmd) == 0) {
				if (sHelpEntries[i].helpProc != NULL) sHelpEntries[i].helpProc();
				else puts("Unimplemented.");
				return;
			}
		}
		
		printf("Unrecognized command '%s'.\n", cmd);
		return;
	}
	
	puts("");
	puts("Type help <command> for information about a command.");
	puts("");
	puts("File commands:");
	puts("  load         Load a firmware image.");
	puts("  save         Saves a firmware image to disk.");
	puts("");
	puts("Reporting commands:");
	puts("  info         Print basic information about a firmware image.");
	puts("  loc          Locate a module occupying an address.");
	puts("  map          Prints a map of the firmware address space.");
	puts("  md5          Calculates the MD5 sum of the firmware image.");
	puts("  user         Prints the user configuration information.");
	puts("  verify       Verify a firmware image.");
	puts("");
	puts("Manipulation commands:");
	puts("  clean        Cleans the user configuration and wireless configuration.");
	puts("  compact      Compacts the firmware image.");
	puts("  db           Dump bytes from the firmware image.");
	puts("  eb           Enter bytes into the firmware image.");
	puts("  export       Exports a firmware component.");
	puts("  fix          Fixes problems in the firmware image.");
	puts("  import       Import a firmware component.");
	puts("  restore      Restore firmware configuration from a file.");
	puts("  wl           Modify the firmware wireless information.");
}
