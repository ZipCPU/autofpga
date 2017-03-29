////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bkram.c
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Older description of a block RAM.  Doesn't work with the (newer)
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

