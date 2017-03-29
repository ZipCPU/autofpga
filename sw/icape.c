////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	icape.c
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Older description of the wbicapetwo interface to Xilinx's ICAPE2.
//		Doesn't work with the (newer) file format, but yet demonstrates
//	some of how that newer format might work.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017, Gisselquist Technology, LLC
//
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of  the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
// target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	GPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/gpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "cfg";
static	int		naddr = 32;
static	const	char	*access  = "CFG_ACCESS";
static	const	char	*ext_ports  = "";
static	const	char	*ext_decls  = "";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twire[31:0]\tcfg_debug;\n"
"`ifdef	VERILATOR\n"
"\treg\tr_cfg_ack;\n"
"\talways @(posedge i_clk)\n"
"\t\tr_cfg_ack <= (wb_stb)&&(cfg_sel);\n"
"\tassign\tcfg_stall = 1\'b0;\n"
"\tassign\tcfg_data  = 32\'h00;\n"
"`else\n"
	"\twbicapetwo #(ICAPE_LGDIV)\n"
		"\t\tcfgport(i_clk, wb_cyc, (wb_stb)&&(cfg_sel), wb_we,\n"
		"\t\t\t\twb_addr[4:0], wb_data,\n"
		"\t\t\tcfg_ack, cfg_stall, cfg_data);\n"
"`endif\n";
static	const	char	*alt_insert = "";
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "cfg_debug";
static	REGINFO	ipregs[] = {
	// FPGA CONFIG REGISTERS: 0x4e0-0x4ff
	{  0, "R_CFG_CRC",	"FPGACRC"    },
	{  1, "R_CFG_FAR",	"FPGAFAR"    },
	{  2, "R_CFG_FDRI",	"FPGAFDRI"   },
	{  3, "R_CFG_FDRO",	"FPGAFDRO"   },
	{  4, "R_CFG_CMD",	"FPGACMD"    },
	{  5, "R_CFG_CTL0",	"FPGACTL0"   },
	{  6, "R_CFG_MASK",	"FPGAMASK"   },
	{  7, "R_CFG_STAT",	"FPGASTAT"   },
	{  8, "R_CFG_LOUT",	"FPGALOUT"   },
	{  9, "R_CFG_COR0",	"FPGACOR0"   },
	{ 10, "R_CFG_MFWR",	"FPGAMFWR"   },
	{ 11, "R_CFG_CBC",	"FPGACBC"    },
	{ 12, "R_CFG_IDCODE",	"FPGAIDCODE" },
	{ 13, "R_CFG_AXSS",	"FPGAAXSS"   },
	{ 14, "R_CFG_COR1",	"FPGACOR1"   },
	{ 16, "R_CFG_WBSTAR",	"WBSTAR"     },
	{ 17, "R_CFG_TIMER",	"CFGTIMER"   },
	{ 22, "R_CFG_BOOTSTS",	"BOOTSTS"    },
	{ 24, "R_CFG_CTL1",	"FPGACTL1"   },
	{ 31, "R_CFG_BSPI",	"FPGABSPI"   },
	{ -1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"#define\tCFG_CRC\t0\n"
"#define\tCFG_FAR\t1\n"
"#define\tCFG_FDRI\t2\n"
"#define\tCFG_FDRO\t3\n"
"#define\tCFG_CMD\t4\n"
"#define\tCFG_CTL0\t5\n"
"#define\tCFG_MASK\t6\n"
"#define\tCFG_STAT\t7\n"
"#define\tCFG_LOUT\t8\n"
"#define\tCFG_COR0\t9\n"
"#define\tCFG_MFWR\t10\n"
"#define\tCFG_CBC\t11\n"
"#define\tCFG_IDCODE\t12\n"
"#define\tCFG_AXSS\t13\n"
"#define\tCFG_COR1\t14\n"
"#define\tCFG_WBSTAR\t16\n"
"#define\tCFG_TIMER\t17\n"
"#define\tCFG_BOOTSTS\t22\n"
"#define\tCFG_CTL1\t24\n"
"#define\tCFG_BSPI\t31\n"
"\n";
static	const	char	*ioname = "io_icape[32]";


extern	AUTOPDATA	icape_data;

AUTOPDATA	icape_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
