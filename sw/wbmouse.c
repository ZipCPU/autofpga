#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "mous";
static	int	naddr = 4;
static	const	char	*access  = "MOUSE_ACCESS";
static	const	char	*ext_ports  = "\t\t// The PS/2 Mouse\n\t\ti_ps2, o_ps2";
static	const	char	*ext_decls  = "\t// The PS/2 Mouse\n"
	"\tinput\t\t[1:0]\ti_ps2;\n"
	"\toutput\twire\t[1:0]\to_ps2;";
static	const	char	*main_defns = "\twire\t[31:0]\tscrn_mouse;";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twbmouse\n"
		"\t\tthemouse(i_clk,\n"
		"\t\t\t(wb_cyc), (wb_stb)&&(mous_sel), wb_we, wb_addr[1:0], wb_data,\n"
		"\t\t\t\tmous_ack, mous_stall, mous_data,\n"
		"\t\t\t\ti_ps2, o_ps2,\n"
		"\t\t\t\tscrn_mouse, mous_interrupt);\n";
static	const	char	*alt_insert = ""
	"\tassign	scrn_mouse     = 32'h00;\n"
	"\tassign	mous_interrupt = 1\'b0;\n"
	"\tassign	o_ps2          = 2\'b11;\n";
static	const	char	*toplvl =
	"\tassign\tio_ps_clk  = (o_ps2[1])? 1\'bz:1\'b0\n"
	"\tassign\tio_ps_data = (o_ps2[0])? 1\'bz:1\'b0\n";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "R_MOUSE_STAT",  "MSTAT" },
	{ 1, "R_MOUSE_RAW",   "MRAW"  },
	{ 2, "R_SCRN_MOUSE",  "MOUSE" },
	{ 3, "R_SCRN_SIZE",   "MSIZ"  } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"\n"
"typedef struct  WBMOUSE_S {\n"
	"\tunsigned\tm_bus, m_raw, m_screen, m_size;\n"
"} WBUART;\n\n";
static	const	char	*ioname = "io_mouse";


extern	AUTOPDATA	mouse_data;

AUTOPDATA	mouse_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
