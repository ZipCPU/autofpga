#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "mem";
static	int	naddr = (1<<19);
static	const	char	*access  = "BLKRAM_ACCESS";
static	const	char	*ext_ports  = "";
static	const	char	*ext_decls  = "";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\tmemdev #(.LGMEMSZ(20), .EXTRACLOCK(0))\n"
		"\t\tblkram(i_clk,\n"
		"\t\t\t(wb_cyc), (wb_stb)&&(mem_sel), wb_we,\n"
		"\t\t\t\twb_addr[17:0], wb_data, wb_sel\n"
		"\t\t\t\tmem_ack, mem_stall, mem_data);\n";
static	const	char	*alt_insert = "";
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "BKRAM",  "RAM" },
	{-1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct = "";
static	const	char	*ioname = "";


extern	AUTOPDATA	mem_data;

AUTOPDATA	mem_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};

