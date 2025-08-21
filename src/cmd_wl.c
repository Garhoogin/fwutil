#include "cmd_common.h"
#include "firmware.h"

#include <string.h>

static const char *const sMacInitRegisterNames[] = {
	"TXPE_OFF_DELAY",
	"TX_DELAY",
	"RX_DELAY",
	"TRX_PE_INTERVAL",
	"RDY_TIMEOUT",
	"RX_TIMEOUT",
	"MULTIACK_DELAY_TIME",
	"CCA_DELAY",
	"TIMEOUT_PARAM",
	"ACK_CCA_TIMEOUT",
	"TSF_TXOFFSET",
	"TSF_RXOFFSET",
	"WAKEUP_CTRL",
	"TBTT_ACT_TIME",
	"TMPTT_ACT_TIME",
	"RF_WAKEUP_TIME"
};

//named by RF2958 data sheet
static const char *const sRf2958RegisterNames[] = {
	"CFG1",   // 00
	"IFPLL1", // 01
	"IFPLL2", // 02
	"IFPLL3", // 03
	"RFPLL1", // 04
	"RFPLL2", // 05
	"RFPLL3", // 06
	"RFPLL4", // 07
	"CAL1",   // 08
	"TXRX1",  // 09
	"PCNT1",  // 0A
	"PCNT2",  // 0B
	"VCOT1",  // 0C
	"?",      // 0D (?)
	"?",      // 0E (?)
	"?",      // 0F (?)
	"?",      // 10 (?)
	"?",      // 11 (?)
	"?",      // 12 (?)
	"?",      // 13 (?)
	"?",      // 14 (?)
	"?",      // 15 (?)
	"?",      // 16 (?)
	"?",      // 17 (?)
	"?",      // 18 (?)
	"?",      // 19 (?)
	"?",      // 1A (?)
	"TEST",   // 1B
	"?",      // 1C (?)
	"?",      // 1D (?)
	"?",      // 1E (?)
	"RST",    // 1F,
	NULL
};

//names hypothesized
static const char *const sMm3156RegisterNames[] = {
	"?",      // 00
	"PC",     // 01
	"SC",     // 02
	"RC",     // 03
	"?",      // 04
	"?",      // 05
	"VER",    // 06
	"LNAR",   // 07
	"?",      // 08
	"?",      // 09
	"?",      // 0A
	"?",      // 0B
	"?",      // 0C
	"?",      // 0D
	"VCO2",   // 0E
	"VCO",    // 0F
	"?",      // 10
	"PLL",    // 11
	"?",      // 12
	"?",      // 13
	"?",      // 14
	"?",      // 15
	"?",      // 16
	"?",      // 17
	"TXAOF1", // 18
	"TXAOF2", // 19
	"?",      // 1A
	"?",      // 1B
	"?",      // 1C
	"?",      // 1D
	"?",      // 1E
	"?",      // 1F
	"?",      // 20
	"?",      // 21
	"?",      // 22
	"?",      // 23
	"?",      // 24
	"?",      // 25
	"?",      // 26
	"?",      // 27
	"?",      // 28
	NULL
};

//names hypothesized
static const char *const sBbpRegisterNames[] = {
	"VERSION",                  // 00
	"MCLK",                     // 01
	"MODU",                     // 02
	"?",                        // 03
	"SCRAMBLER_INIT_SHORT",     // 04
	"SCRAMBLER_INIT_LONG",      // 05
	"SCRAMBLER",                // 06
	"SYNC_LONG",                // 07
	"SYNC_SHORT",               // 08
	"?",                        // 09
	"?",                        // 0A
	"?",                        // 0B
	"?",                        // 0C
	"TX_PPDU_SIGNAL",           // 0D
	"TX_PPDU_SERVICE",          // 0E
	"TX_PPDU_LENGTH_H",         // 0F
	"TX_PPDU_LENGTH_L",         // 10
	"TX_PPDU_CRC_H",            // 11
	"TX_PPDU_CRC_L",            // 12
	"CCA_MODE",                 // 13
	"?",                        // 14
	"?",                        // 15
	"RX_PPDU_SERVICE",          // 16
	"RX_PPDU_LENGTH_H",         // 17
	"RX_PPDU_LENGTH_L",         // 18
	"RX_PPDU_CRC_V_H",          // 19
	"RX_PPDU_CRC_V_L",          // 1A
	"?",                        // 1B
	"?",                        // 1C
	"?",                        // 1D
	"AGC",                      // 1E
	"?",                        // 1F
	"?",                        // 20
	"?",                        // 21
	"?",                        // 22
	"?",                        // 23
	"?",                        // 24
	"?",                        // 25
	"TXVGA_TARGET",             // 26
	"?",                        // 27
	"?",                        // 28
	"?",                        // 29
	"?",                        // 2A
	"?",                        // 2B
	"?",                        // 2C
	"?",                        // 2D
	"ACG_LIMIT",                // 2E
	"?",                        // 2F
	"?",                        // 30
	"?",                        // 31
	"?",                        // 32
	"?",                        // 33
	"?",                        // 34
	"ED_THRESHOLD",             // 35
	"?",                        // 36
	"?",                        // 37
	"?",                        // 38
	"?",                        // 39
	"?",                        // 3A
	"?",                        // 3B
	"?",                        // 3C
	"?",                        // 3D
	"?",                        // 3E
	"?",                        // 3F
	"?",                        // 40
	"?",                        // 41
	"?",                        // 42
	"?",                        // 43
	"?",                        // 44
	"?",                        // 45
	"?",                        // 46
	"?",                        // 47
	"?",                        // 48
	"?",                        // 49
	"?",                        // 4A
	"?",                        // 4B
	"?",                        // 4C
	"?",                        // 4D
	"?",                        // 4E
	"?",                        // 4F
	"?",                        // 50
	"?",                        // 51
	"?",                        // 52
	"?",                        // 53
	"?",                        // 54
	"?",                        // 55
	"?",                        // 56
	"?",                        // 57
	"?",                        // 58
	"?",                        // 59
	"?",                        // 5A
	"?",                        // 5B
	"?",                        // 5C
	"?",                        // 5D
	"MDR_COUNT_H",              // 5E
	"MDR_COUNT_L",              // 5F
	"SFD_COUNT_H",              // 60
	"SFD_COUNT_L",              // 61
	"COUNT_CNT",                // 62
	"?",                        // 63
	"?",                        // 64
	"?",                        // 65
	"?",                        // 66
	"?",                        // 67
	"?",                        // 68
	NULL
};


static void WlCalcSum(FlashRfBbInfo *wl) {
	wl->crc = ComputeCrc((&wl->crc) + 1, wl->tableSize, 0);
}


// ----- RF2958 routines

static void Rf2958Print_CFG1(uint32_t reg) {
	const char *const refSel[] = { "/2", "/3", "/44", "/1" };
	printf("CFG1=%05X\n", reg);
	printf("    IF_VGA_REG_EN=%s\n", ((reg >>  0) & 1) ? "ON" : "OFF");
	printf("    IF_VCO_REG_EN=%s\n", ((reg >>  1) & 1) ? "ON" : "OFF");
	printf("    RF_VCO_REG_EN=%s\n", ((reg >>  2) & 1) ? "ON" : "OFF");
	printf("    HYBERNATE    =%s\n", ((reg >>  3) & 1) ? "ON" : "OFF");
	printf("    REF_SEL      =%s\n", refSel[(reg >> 14) & 3]);
}

static void Rf2958Print_IFPLL1(uint32_t reg) {
	printf("IFPLL1=%05X\n", reg);
	printf("    DAC1       =%d\n", ((reg >>  0) & 3));
	printf("    P1         =%s\n", ((reg >>  9) & 1) ? "Prescale 4/5" : "Prescale 8/9");
	printf("    LD_EN1     =%s\n", ((reg >> 10) & 1) ? "ON" : "OFF");
	printf("    AUTOCAL_EN1=%s\n", ((reg >> 11) & 1) ? "ON" : "OFF");
	printf("    PDP1       =%s\n", ((reg >> 12) & 1) ? "-" : "+");
	printf("    CPL1       =%s\n", ((reg >> 13) & 1) ? "2x" : "1x");
	printf("    LPF1       =%s\n", ((reg >> 14) & 1) ? "EXT" : "INT");
	printf("    VTC_EN1    =%s\n", ((reg >> 15) & 1) ? "ON" : "OFF");
	printf("    KV_EN1     =%s\n", ((reg >> 16) & 1) ? "ON" : "OFF");
	printf("    PLL_EN1    =%s\n", ((reg >> 17) & 1) ? "ON" : "OFF");
}

static void Rf2958Print_IFPLL2(uint32_t reg) {
	printf("IFPLL2=%05X\n", reg);
}

static void Rf2958Print_IFPLL3(uint32_t reg) {
	printf("IFPLL3=%05X\n", reg);
	printf("    KV_DEF1=%d\n", ((reg >>  0) & 0x00F));
	printf("    CT_DEF1=%d\n", ((reg >>  4) & 0x00F));
	printf("    DN1    =%d\n", ((reg >>  8) & 0x1FF));
}

static void Rf2958Print_RFPLL1(uint32_t reg) {
	printf("RFPLL1=%05X\n", reg);
	printf("    DAC       =%d\n", ((reg >>  0) & 0x03));
	printf("    P         =%s\n", ((reg >>  9) & 0x01) ? "Prescale 8/10" : "Prescale 8/9");
	printf("    LD_EN     =%s\n", ((reg >> 10) & 0x01) ? "ON" : "OFF");
	printf("    AUTOCAL_EN=%s\n", ((reg >> 11) & 0x01) ? "ON" : "OFF");
	printf("    PDP       =%s\n", ((reg >> 12) & 0x01) ? "-" : "+");
	printf("    CPL       =%s\n", ((reg >> 13) & 0x01) ? "2x" : "1x");
	printf("    LPF       =%s\n", ((reg >> 14) & 0x01) ? "EXT" : "INT");
	printf("    VTC_EN    =%s\n", ((reg >> 15) & 0x01) ? "ON" : "OFF");
	printf("    KV_EN     =%s\n", ((reg >> 16) & 0x01) ? "ON" : "OFF");
	printf("    PLL_EN    =%s\n", ((reg >> 17) & 0x01) ? "ON" : "OFF");
}

static void Rf2958Print_RFPLL2(uint32_t reg) {
	printf("RFPLL2=%05X\n", reg);
	//NUMER+DENOM
}

static void Rf2958Print_RFPLL3(uint32_t reg) {
	printf("RFPLL3=%05X\n", reg);
	//NUMER
}

static void Rf2958Print_RFPLL4(uint32_t reg) {
	printf("RFPLL4=%05X\n", reg);
	printf("    KV_DEF=%d\n", ((reg >>  0) & 0x00F));
	printf("    CT_DEF=%d\n", ((reg >>  4) & 0x00F));
	printf("    DN    =%d\n", ((reg >>  8) & 0x1FF));
}

static void Rf2958Print_CAL1(uint32_t reg) {
	printf("CAL1=%05X\n", reg);
	printf("    LD_WINDOW =%d\n", ((reg >>  0) & 0x03));
	printf("    M_CT_VALUE=%d\n", ((reg >>  3) & 0x1F));
	printf("    TLOCK     =%d\n", ((reg >>  8) & 0x1F));
	printf("    TVCO      =%d\n", ((reg >> 13) & 0x1F));
}

static void Rf2958Print_TXRX1(uint32_t reg) {
	const char *const pcontrol[] = { "EXT", "EXT", "TXVGC", "INT" };
	printf("TXRX1=%05X\n", reg);
	printf("    TXBYPASS  =%s\n", ((reg >>  0) & 0x01) ? "ON" : "OFF");
	printf("    INTBIASEN =%s\n", ((reg >>  1) & 0x01) ? "ON" : "OFF");
	printf("    TXENMODE  =%s\n", ((reg >>  2) & 0x01) ? "BBEN" : "TXEN");
	printf("    TXDIFFMODE=%s\n", ((reg >>  3) & 0x01) ? "Single-ended" : "Differential");
	printf("    TXLPFBW   =%d\n", ((reg >>  4) & 0x07));
	printf("    RXLPFBW   =%d\n", ((reg >>  7) & 0x07));
	printf("    TXVGC     =%d\n", ((reg >> 10) & 0x1F));
	printf("    PCONTROL  =%s\n", pcontrol[(reg >> 15) & 0x03]);
	printf("    RXDCFBBYPS=%s\n", ((reg >> 17) & 0x01) ? "OFF" : "ON");
}

static void Rf2958Print_PCNT1(uint32_t reg) {
	printf("PCNT1=%05X\n", reg);
	printf("    TX_DELAY =%.1fus\n",  ((reg >>  0) & 0x07) * 0.5);
	printf("    PC_OFFSET=%.1fus\n",  (((reg >>  3) & 0x1F) - (((reg >> 8) & 1) << 5)) * 0.5);
	printf("    P_DESIRED=%.1fdBm\n", ((reg >>  9) & 0x3F) * 0.5);
	printf("    MID_BIAS =%.1fV\n",   ((reg >> 15) & 0x03) * 0.125 + 1.6);
}

static void Rf2958Print_PCNT2(uint32_t reg) {
	printf("PCNT2=%05X\n", reg);
	printf("    MIN_POWER=%.1fdBm\n", ((reg >>  0) & 0x3F) * 0.5);
	printf("    MID_POWER=%.1fdBm\n", ((reg >>  6) & 0x3F) * 0.5);
	printf("    MAX_POWER=%.1fdBm\n", ((reg >> 12) & 0x3F) * 0.5);
}

static void Rf2958Print_VCOT1(uint32_t reg) {
	printf("VCOT1=%05X\n", reg);
	printf("    AUX1=%s\n", ((reg >> 16) & 1) ? "ON" : "OFF");
	printf("    AUX =%s\n", ((reg >> 17) & 1) ? "ON" : "OFF");
}

static void Rf2958Print_TEST(uint32_t reg) {
	printf("TEST=%05X\n", reg);
}

static void Rf2958Print_RST(uint32_t reg) {
	printf("RST=%05X\n", reg);
}

static void Rf2958PrintRegister(int regno, uint32_t reg) {
	switch (regno) {
		case 0x00: Rf2958Print_CFG1(reg);   break;
		case 0x01: Rf2958Print_IFPLL1(reg); break;
		case 0x02: Rf2958Print_IFPLL2(reg); break;
		case 0x03: Rf2958Print_IFPLL3(reg); break;
		case 0x04: Rf2958Print_RFPLL1(reg); break;
		case 0x05: Rf2958Print_RFPLL2(reg); break;
		case 0x06: Rf2958Print_RFPLL3(reg); break;
		case 0x07: Rf2958Print_RFPLL4(reg); break;
		case 0x08: Rf2958Print_CAL1(reg);   break;
		case 0x09: Rf2958Print_TXRX1(reg);  break;
		case 0x0A: Rf2958Print_PCNT1(reg);  break;
		case 0x0B: Rf2958Print_PCNT2(reg);  break;
		case 0x0C: Rf2958Print_VCOT1(reg);  break;
		case 0x1B: Rf2958Print_TEST(reg);   break;
		case 0x1F: Rf2958Print_RST(reg);    break;
		default: printf("Register %02X=%05X\n", regno, reg);
	}
}


static void RfMm3156PrintRegister(int regno, uint8_t reg) {
	printf("Register %02X=%02X\n", regno, reg);
}





void CmdHelpWl(void) {
	puts("");
	puts("Usage: wl <command> [arguments...]");
	puts("");
	puts("This command operates on the wireless configuration.");
	puts("");
	puts("Commands:");
	puts("  info    Display the wireless initialization info.");
	puts("  mrf     Read or modify RF registers.");
	puts("  mbr     Read or modify BB registers.");
}



// ----- MBR command

static void CmdHelpWlMbr(FlashRfBbInfo *wl) {
	puts("");
	puts("Usage: wl mbr <regname> [value]");
	puts("");
	puts("View or modify an initial BBP register setting. The regname parameter can be");
	puts("the register address or name from the following:");
	(void) wl;
	
	int scrpos = 2;
	printf("  ");
	for (unsigned int i = 0; i < sizeof(sBbpRegisterNames) / sizeof(sBbpRegisterNames[0]) - 1; i++) {
		const char *name = sBbpRegisterNames[i];
		if (strcmp(name, "?") == 0) continue;
		
		if ((scrpos + strlen(name) + 2) >= 80) {
			printf("\n  ");
			scrpos = 2;
		}
		
		scrpos += printf("%s", name);
		scrpos += printf(" ");
	}
	puts("");
}

static void CmdWlMbr(FlashHeader *hdr, FlashRfBbInfo *wl, int argc, const char **argv) {
	(void) hdr;
	if (argc < 2) {
		CmdHelpWlMbr(wl);
		return;
	}
	
	//get register name.
	const char *regarg = argv[1];
	const char *valarg = NULL;
	if (argc >= 3) valarg = argv[2];
	
	int regno = -1;
	for (unsigned int i = 0; i < sizeof(sBbpRegisterNames) / sizeof(sBbpRegisterNames[0]); i++) {
		if (stricmp(regarg, sBbpRegisterNames[i]) == 0) {
			regno = i;
			break;
		}
	}
	if (regno == -1) {
		regno = ParseArgNumber(regarg);
	}
	
	if (regno < 0 || regno >= 0x69) {
		printf("Invalid BBP register address %02X.\n", regno);
		return;
	}
	
	//get/put
	const char *name = sBbpRegisterNames[regno];
	if (valarg == NULL) {
		//get
		if (strcmp(name, "?") == 0) printf("BBP[%02X]", regno);
		else printf("%s", name);
		printf(" = %02X\n", wl->bbInitRegs[regno]);
	} else {
		//put
		wl->bbInitRegs[regno] = ParseArgNumber(valarg);
		WlCalcSum(wl);
	}
}



// ----- INFO command

static void PrintRfInitRf2958(FlashHeader *hdr, FlashRfBbInfo *wl) {
	//RF init for RF2958
	FlashRf2958Info *regs = (FlashRf2958Info *) (((unsigned char *) hdr) + 0xCE);
	if (wl->rfRegisterBits != 24) {
		//RF command initialization should be 24 bits wide.
		printf("  Incorrect initialization bit size for RF2958(%d).\n", wl->rfRegisterBits);
		return;
	}
	
	for (unsigned int i = 0; i < wl->rfInitRegisterCount; i++) {
		unsigned char b0 = regs->initRegs[i][0];
		unsigned char b1 = regs->initRegs[i][1];
		unsigned char b2 = regs->initRegs[i][2];
		uint32_t cmd = (b0 << 0) | (b1 << 8) | (b2 << 16);

		uint32_t val = (cmd & 0x03FFFF) >>  0; // 18-bit
		int regno =    (cmd & 0x7C0000) >> 18; // 5-bit
		int wmode =    (cmd & 0x800000) >> 23; // 1-bit
		
		if (!wmode) {
			printf("  %X: %-6s = %05X\n", i, sRf2958RegisterNames[regno], val);
		} else {
			printf("  %X: %-6s R\n", i, sRf2958RegisterNames[regno]);
		}
	}
}

static void PrintRfInitMm3156(FlashHeader *hdr, FlashRfBbInfo *wl) {
	FlashRfMm3156Info *regs = (FlashRfMm3156Info *) (((unsigned char *) hdr) + 0xCE);
	(void) wl;
	
	for (unsigned int i = 0; i < 0x29; i++) {
		const char *name = sMm3156RegisterNames[i];
		
		if (strcmp(name, "?") == 0) printf("  reg%02X ", i);
		else printf("  %-6s", name);
		printf(" = %02X ", regs->initRegs[i]);
		
		if ((i % 4) == 3) puts("");
	}
	puts("");
}

static void PrintChannelCtlRf2958(FlashHeader *hdr, FlashRfBbInfo *wl) {
	FlashRf2958Info *regs = (FlashRf2958Info *) (((unsigned char *) hdr) + 0xCE);
	(void) wl;
	
	for (unsigned int i = 1; i <= 14; i++) {
		printf("  Ch. %2d : ", i);

		//RFU regs
		uint32_t freqv = 0;
		for (unsigned int j = 0; j < 2; j++) {
			unsigned char b0 = regs->channelRegs[i - 1][j][0];
			unsigned char b1 = regs->channelRegs[i - 1][j][1];
			unsigned char b2 = regs->channelRegs[i - 1][j][2];
			uint32_t cmd = (b0 << 0) | (b1 << 8) | (b2 << 16);

			uint32_t val = (cmd & 0x03FFFF) >>  0; // 18-bit
			int regno =    (cmd & 0x7C0000) >> 18; // 5-bit
			int wmode =    (cmd & 0x800000) >> 23; // 1-bit
			(void) wmode;
			printf("RFU[%s]=%05X ", sRf2958RegisterNames[regno], val);

			if (regno == 5) freqv |= val << 18;
			if (regno == 6) freqv |= val <<  0;
		}

		double freq = freqv * (22000000.0 / 16777216.0) + 374000000;
		printf("(F=%dMHz) ", (int) (freq / 1000000.0 + 0.5));

		//BBP regs
		for (unsigned int j = 0; j < 1; j++) {
			printf("BBP[ACG]=%02X ", regs->bbpChannelAgc[i - 1]);
		}

		//TXPWR
		printf("TXPWR=%02X\n", regs->rfuChannelTxPower[i - 1]);
	}
}

static void PrintChannelCtlMm3156(FlashHeader *hdr, FlashRfBbInfo *wl) {
	FlashRfMm3156Info *regs = (FlashRfMm3156Info *) (((unsigned char *) hdr) + 0xCE);
	(void) wl;
	
	//
	for (unsigned int i = 1; i <= 14; i++) {
		printf("  Ch. %2d : ", i);
		
		//RFU registers
		for (unsigned int j = 0; j < 2; j++) {
			unsigned int regno = regs->rfuChannelRegs[j][0];
			unsigned int regval = regs->rfuChannelRegs[j][i];
			
			printf("RFU[%s]=%02X ", sMm3156RegisterNames[regno], regval);
		}
		
		//BBP registers
		for (unsigned int j = 0; j < regs->bbpCount; j++) {
			unsigned int regno = regs->bbpChannelRegs[j][0];
			unsigned int regval = regs->bbpChannelRegs[j][i];
			
			printf("BBP[%s]=%02X ", sBbpRegisterNames[regno], regval);
		}
		
		printf("\n");
	}
}

static void CmdWlInfo(FlashHeader *hdr, FlashRfBbInfo *wl) {
	// MAC initialization
	puts("");
	puts("MAC Initialization:");
	for (int i = 0; i < 0x10/2; i++) {
		printf("  %-19s : %04X          ", sMacInitRegisterNames[i + 0], wl->macInitRegs[i + 0]);
		printf("  %-19s : %04X\n",         sMacInitRegisterNames[i + 8], wl->macInitRegs[i + 8]);
	}

	// BBP registers
	puts("");
	puts("BBP Initialization:");
	printf("     | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \n");
	printf("  ---+-------------------------------------------------\n");
	for (int i = 0; i < 7; i++) {
		int nRow = 0x10;
		if (i == 6) nRow = 0x09;
		
		printf("  %X0 | ", i);
		for (int j = 0; j < nRow; j++) printf("%02X ", wl->bbInitRegs[i * 0x10 + j]);
		puts("");
	}

	// RFU registers
	puts("");
	printf("RFU Initialization (%s):\n", GetRfType(wl->rfType));
	switch (wl->rfType) {
		case RF_TYPE_RF2958:
			PrintRfInitRf2958(hdr, wl);
			break;
		case RF_TYPE_MM3156:
			PrintRfInitMm3156(hdr, wl);
			break;
	}

	// Channel control
	puts("");
	puts("Channel Control:");
	switch (wl->rfType) {
		case RF_TYPE_MAX2822:
		case RF_TYPE_RF2958:
			PrintChannelCtlRf2958(hdr, wl);
			break;
		case RF_TYPE_MM3156:
			PrintChannelCtlMm3156(hdr, wl);
			break;
	}
}



// ----- MRF command

static void CmdHelpWlMrf(FlashRfBbInfo *wl) {
	puts("");
	puts("Usage: wl mrf <regname> [value]");
	puts("");
	puts("View or modify an initial RFU register setting. The regname parameter can be");
	puts("the register address or name from the following:");

	const char *const *regnames = NULL;
	switch (wl->rfType) {
		case RF_TYPE_RF2958:
			regnames = sRf2958RegisterNames;
			break;
		case RF_TYPE_MM3156:
			regnames = sMm3156RegisterNames;
			break;
	}
	if (regnames == NULL) return;

	int scrpos = 2;
	printf("  ");

	while (*regnames != NULL) {
		if (strcmp(*regnames, "?") != 0) {
			if ((scrpos + strlen(*regnames) + 2) >= 80) {
				printf("\n  ");
				scrpos = 2;
			}

			scrpos += printf("%s", *regnames);
			if (*(regnames + 1) != NULL) scrpos += printf(", ");
		}
		regnames++;
	}
	printf("\n");
}

static void CmdWlMrf(FlashHeader *hdr, FlashRfBbInfo *wl, int argc, const char **argv) {
	if (argc < 2) {
		CmdHelpWlMrf(wl);
		return;
	}

	//test RF: no RF
	if (wl->rfType == RF_TYPE_TEST) {
		puts("RF=TEST: no registers to configure.");
		return;
	}

	const char *const *regnames = NULL;
	switch (wl->rfType) {
		case RF_TYPE_RF2958 : regnames = sRf2958RegisterNames; break;
		case RF_TYPE_MAX2822: regnames = sRf2958RegisterNames; break;
		case RF_TYPE_MM3156 : regnames = sMm3156RegisterNames; break;
	}
	
	//are we setting a register for a channel?
	int channel = -1;
	if (argc >= 4) {
		if (stricmp(argv[1], "channel") == 0 || stricmp(argv[1], "ch") == 0) {
			channel = ParseArgNumber(argv[2]);
			if (channel < 1 || channel > 14) {
				printf("Incorrect channel %d.\n", channel);
				return;
			}
		}
	}
	
	const char *regarg = argv[1];
	const char *valarg = NULL;
	if (argc >= 3) valarg = argv[2];
	if (channel != -1) {
		regarg = argv[3];
		valarg = NULL;
		if (argc >= 5) valarg = argv[4];
	}

	int regno = -1;
	if (regnames != NULL) {
		int i = 0, find = 0;
		const char *const *cpy = regnames;
		while (*cpy != NULL) {
			if (stricmp(*cpy, regarg) == 0) {
				find = 1;
				break;
			}
			cpy++;
			i++;
		}
		if (find) regno = i;
	}

	if (regno == -1) {
		regno = ParseArgNumber(regarg);
		if (regno == 0 && regarg[0] != '0') {
			printf("Unknown register: %s\n", regarg);
			return;
		}
	}
	
	puts("");
	int found = 0;
	switch (wl->rfType) {
		case RF_TYPE_RF2958:
		case RF_TYPE_MAX2822:
		{
			FlashRf2958Info *regs = (FlashRf2958Info *) (((unsigned char *) hdr) + 0xCE);
			
			unsigned int nRegCheck = wl->rfInitRegisterCount;
			uint8_t (*initRegs)[3] = regs->initRegs;
			if (channel != -1) {
				nRegCheck = 2;
				initRegs = regs->channelRegs[channel - 1];
			}
			
			for (unsigned int i = 0; i < nRegCheck; i++) {
				unsigned char b0 = initRegs[i][0];
				unsigned char b1 = initRegs[i][1];
				unsigned char b2 = initRegs[i][2];
				uint32_t cmd = (b0 << 0) | (b1 << 8) | (b2 << 16);
		
				uint32_t fval = (cmd & 0x03FFFF) >>  0; // 18-bit
				int fregno =    (cmd & 0x7C0000) >> 18; // 5-bit
				int fwmode =    (cmd & 0x800000) >> 23; // 1-bit
				
				if (regno == fregno) {
					if (valarg == NULL) {
						//print
						Rf2958PrintRegister(regno, fval);
					} else {
						//write
						int newval = ParseArgNumber(valarg);
						cmd = (fwmode << 23) | (fregno << 18) | (newval & 0x3FFFF);
						initRegs[i][0] = (cmd >>  0) & 0xFF;
						initRegs[i][1] = (cmd >>  8) & 0xFF;
						initRegs[i][2] = (cmd >> 16) & 0xFF;
						WlCalcSum(wl);
					}
					
					//indicate found
					found = 1;
					break;
				}
			}
			break;
		}
		case RF_TYPE_MM3156:
		{
			FlashRfMm3156Info *regs = (FlashRfMm3156Info *) (((unsigned char *) hdr) + 0xCE);
			
			//uint8_t *initRegs = regs->initRegs;
			uint8_t *pReg = NULL;
			if (channel != -1) {
				//search for channel seting register.
				int regIndex = -1;
				for (int i = 0; i < 2; i++) {
					int fregno = regs->rfuChannelRegs[i][0];
					if (fregno == regno) {
						regIndex = i;
						break;
					}
				}
				if (regIndex == -1) break; // not found
				
				pReg = &(regs->rfuChannelRegs[regIndex] + 1)[channel - 1];
			} else {
				//look up in the init array
				if (regno >= 0 && regno < 27) {
					pReg = &regs->initRegs[regno];
				}
			}
			if (pReg == NULL) break;
			
			if (valarg == NULL) {
				//print
				RfMm3156PrintRegister(regno, *pReg);
			} else {
				//write
				*pReg = ParseArgNumber(valarg);
				WlCalcSum(wl);
			}
			
			//indicate found
			found = 1;
			break;
		}
	}

	if (!found) {	
		printf("Register %s (%X) not found in %s.\n", regnames[regno], regno,
			(channel == -1) ? "wireless initialization" : "channel configuration");
	}
}

void CmdProcWl(int argc, const char **argv) {
	if (!RequireFirmwareImage()) return;

	if (argc < 2) {
		CmdHelpWl();
		return;
	}

	//Commands:
	// wl info                           Print wireless config info
	// wl mrf [channel <n>] <reg> [val]  Read/write an RFU register
	// wl mbr <reg> [val]                Read/write a BBP register
	// wl setmac <mode>                  Generate a MAC address
	// wl verify                         Verify wireless config
	// wl fix                            Fix wireless config
	
	unsigned int size;
	unsigned char *buffer = GetFirmwareImage(&size);
	
	//flash header
	FlashHeader *hdr = (FlashHeader *) buffer;
	FlashRfBbInfo *wl = (FlashRfBbInfo *) (buffer + 0x2A);

	const char *cmd = argv[1];
	if (stricmp(cmd, "info") == 0) {
		CmdWlInfo(hdr, wl);
	} else if (stricmp(cmd, "mrf") == 0 || stricmp(cmd, "drf") == 0) {
		CmdWlMrf(hdr, wl, argc - 1, argv + 1);
	} else if (stricmp(cmd, "mbr") == 0 || stricmp(cmd, "dbr") == 0) {
		CmdWlMbr(hdr, wl, argc - 1, argv + 1);
	} else {
		printf("wl: Unrecognized command '%s'.\n", cmd);
	}
	
}
