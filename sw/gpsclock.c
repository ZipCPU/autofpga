////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	gpsclock.c
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Older description of a GPS clock.  Doesn't work with the (newer)
//		file format, but yet demonstrates some of how that newer format
//	might work.
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

static	const char	*prefix = "gck";
static	int	naddr = 4;
static	const	char	*access  = "GPS_CLOCK";
static	const	char	*ext_ports  = "i_gps_pps";
static	const	char	*ext_decls  = "\t//The GPS Clock\n\tinput\t\t\ti_gps_pps;";
static	const	char	*main_defns = "\twire\tgps_pps, ck_pps, gps_led, gps_locked;\n"
		"\twire\t[63:0]	gps_now, gps_err, gps_step;\n";
static	const	char	*dbg_defns  = "\twire\t[1:0]\tgps_dbg_tick;\n";
static	const	char	*main_insert = ""
	"\tgpsclock #(.DEFAULT_STEP(GPSCLOCK_DEFAULT_STEP))\n"
		"\t\tppsck(i_clk, 1\'b0, gps_pps, ck_pps, gps_led,\n"
		"\t\t\t(wb_stb)&&(gck_sel), wb_we, wb_addr[1:0], wb_data,\n"
		"\t\t\t\tgck_ack, gck_stall, gck_data,\n"
		"\t\t\tgps_tracking, gps_now, gps_step, gps_err, gps_locked);\n";
static	const	char	*alt_insert = ""
	"\twire	[31:0]	pre_step;\n"
	"\tassign	pre_step = { 16\'h00, (({GPSCLOCK_DEFAULT_STEP[27:0],20\'h00})>>GPSCLOCK_DEFAULT_STEP[31:28]) };\n"
	"\talways @(posedge i_clk)\n"
		"\t\t{ ck_pps, gps_step[31:0] } <= gps_step + pre_step;\n"
	"\tassign	gck_stall  = 1\'b0;\n"
	"\tassign	gps_now    = 64\'h0;\n"
	"\tassign	gps_err    = 64\'h0;\n"
	"\tassign	gps_step   = 64\'h0;\n"
	"\tassign	gps_led    = 1\'b0;\n"
	"\tassign	gps_locked = 1\'b0;\n"
	;
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "gps_dbg_tick";
static	REGINFO	ipregs[] = {
	{ 0, "R_GPS_ALPHA", "ALPHA" },
	{ 1, "R_GPS_BETA",  "BETA"  },
	{ 2, "R_GPS_GAMMA", "GAMMA" },
	{ 3, "R_GPS_STEP",  "STEP"  } };
static REGINFO *pregs = &ipregs[0];
static	const	char	*cstruct = "typedef\tstruct\tGPSTRACKER_S\t{\n"
	"\tunsigned\tg_alpha, g_beta, g_gamma, g_step;\n"
	"} GPSTRACKER;\n\n";
static	const	char	*ioname = "io_gps";


extern	AUTOPDATA	gpsclock_data;

AUTOPDATA	gpsclock_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};

static	const char	*tbprefix = "gtb";
static	int	tbnaddr = 8;
static	const	char	*tbaccess  = "";
static	const	char	*tbext_ports  = "";
static	const	char	*tbext_decls  = "";
static	const	char	*tbmain_defns = "";
static	const	char	*dbg_tbdefns  = "";
static	const	char	*tbmain_insert = ""
"`ifdef\tGPS_CLOCK\n"
"\n"
	"\tgpsclock_tb\n"
		"\t\tppsck(i_clk, ck_pps, tb_pps,\n"
		"\t\t\t(wb_stb)&&(gtb_sel), wb_we, wb_addr[2:0], wb_data,\n"
		"\t\t\t\tgtb_ack, gtb_stall, gtb_data,\n"
		"\t\t\tgps_err, gps_now, gps_step);\n"
"\n"
"`ifdef	GPSTB\n"
"\tassign\tgps_pps = tb_pps;\n"
"`else\n"
"\tassign\tgps_pps = i_gps_pps;\n"
"`endif\n"
"\n"
"`endif\n"
"\n";

static	const	char	*tbalt_insert = "";
static	const	char	*tbtoplvl = "";
static	const	char	*tbdbg_insert = "";
static	REGINFO	tbregs[] = {
	{ 0, "R_GPSTB_FREQ",    "GPSFREQ" },
	{ 1, "R_GPSTB_JUMP",    "GPSJUMP" },
	{ 2, "R_GPSTB_ERRHI",   "ERRHI"   },
	{ 3, "R_GPSTB_ERRLO",   "ERRLO"   },
	{ 4, "R_GPSTB_COUNTHI", "CNTHI"   },
	{ 5, "R_GPSTB_COUNTLO", "CNTLO"   },
	{ 6, "R_GPSTB_STEPHI",  "STEPHI"  },
	{ 7, "R_GPSTB_STEPLO",  "STEPLO"  } };
static REGINFO *ptbregs = &tbregs[0];
static	const	char	*tbcstruct = "typedef\tstruct\tGPSTB_S\t{\n"
	"\tunsigned\t\ttb_maxcount; tb_jump;\n"
	"\tunsigned\tlong\ttb_err, tb_count, tb_step;\n"
	"} GPSTB;\n\n";
static	const	char	*tbioname = "io_gpstb";

AUTOPDATA	gpstb_data = {
	tbprefix, tbnaddr, tbaccess, tbext_ports, tbext_decls, tbmain_defns, dbg_tbdefns,
		tbmain_insert, tbalt_insert, tbdbg_insert, tbregs, tbcstruct, tbioname
	};

