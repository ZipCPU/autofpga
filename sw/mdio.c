#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "mdio";
static	int		naddr = 32;
static	const	char	*access  = "NETCTRL_ACCESS";
static	const	char	*ext_ports  = "\t\t// The ethernet MDIO wires\n\t\to_mdclk, o_mdio, o_mdwe, i_mdio";
static	const	char	*ext_decls  = "\t// Ethernet control (MDIO)\n\toutput\twire\t\to_mdclk, o_mdio, o_mdwe;\n\tinput\t\t\ti_mdio;";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twire[31:0]\tmdio_debug;\n"
	"\tenetctrl #(2)\n"
		"\t\tmdio(i_clk, i_rst, wb_cyc, (wb_stb)&&(mdio_sel), wb_we,\n"
		"\t\t\t\twb_addr[4:0], wb_data[15:0],\n"
		"\t\t\tmdio_ack, mdio_stall, mdio_data,\n"
		"\t\t\to_mdclk, o_mdio, i_mdio, o_mdwe);\n";
static	const	char	*alt_insert = ""
	"\tassign	o_mdclk = 1\'b1;\n"
	"\tassign	o_mdio  = 1\'b1;\n"
	"\tassign	o_mdwe  = 1\'b0;\n";
static	const	char	*toplvl = "\tassign\nio_eth_mdio = (w_mdwe)?w_mdio : 1'bz;\n";
static	const	char	*dbg_insert = ""; // "mdio_debug";
static	REGINFO	ipregs[] = {
	// Ethernet configuration (MDIO) port
	{ 0, "R_MDIO_BMCR",  	"BMCR"    },
	{ 1, "R_MDIO_BMSR", 	"BMSR"    },
	{ 2, "R_MDIO_PHYIDR1",	"PHYIDR1" },
	{ 3, "R_MDIO_PHYIDR2",	"PHYIDR2" },
	{ 4, "R_MDIO_ANAR",	"ANAR"    },
	{ 5, "R_MDIO_ANLPAR",	"ANLPAR"  },
	// 5 is also the ANLPARNP register
	{ 6, "R_MDIO_ANER",	"ANER"    },
	{ 7, "R_MDIO_ANNPTR",	"ANNPTR"  },
	// Registers 8-15 are reserved, and so not defined here
	{ 16, "R_MDIO_PHYSTS",	"PHYSYTS" },
	// 17-19 are reserved
	{ 20, "R_MDIO_FCSCR",	"FCSCR"   },
	{ 21, "R_MDIO_RECR",	"RECR"    },
	{ 22, "R_MDIO_PCSR",	"PCSR"    },
	{ 23, "R_MDIO_RBR",	"RBR"     },
	{ 24, "R_MDIO_LEDCR",	"LEDCR"   },
	{ 25, "R_MDIO_PHYCR",	"PHYCR"   },
	{ 26, "R_MDIO_BTSCR",	"BTSCR"   },
	{ 27, "R_MDIO_CDCTRL",	"CDCTRL"  },
	// 28 is reserved
	{ 29, "R_MDIO_EDCR",	"EDCR"    },
	{ -1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"//\n// The Ethernet MDIO interface\n//\n"
"#define MDIO_BMCR\t0x00\n"
"#define MDIO_BMSR\t0x01\n"
"#define MDIO_PHYIDR1\t0x02\n"
"#define MDIO_PHYIDR2\t0x03\n"
"#define MDIO_ANAR\t0x04\n"
"#define MDIO_ANLPAR\t0x05\n"
"#define MDIO_ANLPARNP\t0x05\t// Duplicate register address\n"
"#define MDIO_ANER\t0x06\n"
"#define MDIO_ANNPTR\t0x07\n"
"#define MDIO_PHYSTS\t0x10\n"
"#define MDIO_FCSCR\t0x14\n"
"#define MDIO_RECR\t0x15\n"
"#define MDIO_PCSR\t0x16\n"
"#define MDIO_RBR\t0x17\n"
"#define MDIO_LEDCR\t0x18\n"
"#define MDIO_PHYCR\t0x19\n"
"#define MDIO_BTSCR\t0x1a\n"
"#define MDIO_CDCTRL\t0x1b\n"
"#define MDIO_EDCR\t0x1d\n"
"\n"
"typedef struct ENETMDIO_S {\n"
	"\tunsigned\te_v[32];\n"
"} ENETMDIO;\n\n";
static	const	char	*ioname = "io_netmd";


extern	AUTOPDATA	mdio_data;

AUTOPDATA	mdio_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
