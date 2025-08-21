#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//
// Get the current open file path.
//
const char *GetCurrentFilePath(void);

//
// Get the currently open firmware image buffer.
//
unsigned char *GetFirmwareImage(unsigned int *pSize);

//
// Load a firmware image from a file path.
//
int LoadFirmwareImage(const char *path);

//
// Returns 1 if a firmware image is open, 0 otherwise.
//
int RequireFirmwareImage(void);

//
// Get the current command processor exiting status.
//
int IsExiting(void);

//
// Start the command processor exiting process.
//
void DoExit(void);

//
// Parse a number from an argument list.
//
uint32_t ParseArgNumber(const char *arg);


// ----- command procs

void CmdProcHelp(int argc, const char **argv);
void CmdProcInfo(int argc, const char **argv);
void CmdProcWl(int argc, const char **argv);
void CmdProcVerify(int argc, const char **argv);
void CmdProcMap(int argc, const char **argv);
void CmdProcLoad(int argc, const char **argv);
void CmdProcSave(int argc, const char **argv);
void CmdProcClean(int argc, const char **argv);
void CmdProcRestore(int argc, const char **argv);
void CmdProcExport(int argc, const char **argv);
void CmdProcImport(int argc, const char **argv);
void CmdProcLoc(int argc, const char **argv);
void CmdProcEB(int argc, const char **argv);
void CmdProcDB(int argc, const char **argv);
void CmdProcMD5(int argc, const char **argv);
void CmdProcUser(int argc, const char **argv);
void CmdProxFix(int argc, const char **argv);
void CmdProcCompact(int argc, const char **argv);
void CmdProcQuit(int argc, const char **argv);


// ----- command help procs
void CmdHelpHelp(void);
void CmdHelpInfo(void);
void CmdHelpWl(void);
void CmdHelpVerify(void);
void CmdHelpMap(void);
void CmdHelpLoad(void);
void CmdHelpSave(void);
void CmdHelpClean(void);
void CmdHelpRestore(void);
void CmdHelpExport(void);
void CmdHelpImport(void);
void CmdHelpLoc(void);
void CmdHelpEB(void);
void CmdHelpDB(void);
void CmdHelpMD5(void);
void CmdHelpUser(void);
void CmdHelpFix(void);
void CmdHelpCompact(void);
void CmdHelpQuit(void);

