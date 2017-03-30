////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	sdspi.c
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Older description of how an SD card might be connected to a bus.
//		This description doesn't (yet) work with the (newer) file format,
//	but yet demonstrates some of how that newer format might work.
//
//	An interesting consequence of this description is that upgrading to a
//	proper SDIO device would involve no more than swapping this file for an
//	sdio.c peripheral description file.
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

static	const char	*prefix = "sdcard";
static	int		naddr = 4;
static	const	char	*access  = "SDSPI_ACCESS";
static	const	char	*ext_ports  = "\t\t// The SD-Card wires\n\t\to_sd_sck, o_sd_cmd, o_sd_data, i_sd_cmd, i_sd_data, i_sd_detect";
static	const	char	*ext_decls  = "\t// SD-Card declarations\n"
	"\toutput\twire\t\to_sd_sck, o_sd_cmd;\n"
	"\toutput\twire\t[3:0]\to_sd_data;\n"
	"\tinput\t\t\ti_sd_cmd;\n"
	"\tinput\t\t[3:0]\ti_sd_data;\n"
	"\tinput\t\t\ti_sd_detect;";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twire[31:0]\tsd_debug;\n"
	"\t// SPI mapping\n"
	"\twire\tw_sd_cs_n, w_sd_mosi, w_s_miso;\n"
	"\n"
	"\tsdspi\tsdctrl(i_clk,\n"
		"\t\t\twb_cyc, (wb_stb)&&(sdcard_sel), wb_we,\n"
			"\t\t\t\twb_addr[1:0], wb_data,\n"
			"\t\t\t\tsdcard_ack, sdcard_stall, sdcard_data,\n"
		"\t\t\tw_sd_cs_n, o_sd_sck, w_sd_mosi, w_sd_miso,\n"
		"\t\t\tsdcard_int, 1\'b1, sd_dbg);\n"
	"\n"
	"\tassign\tw_sd_miso = i_sd_data[0];\n"
	"\tassign\to_sd_data = { w_sd_cs_n, 3\'b111 };\n"
	"\tassign\to_sd_cmd  = w_sd_mosi;\n\n";
static	const	char	*alt_insert = ""
	"\tassign\to_sd_sck   = 1\'b1;\n"
	"\tassign\to_sd_cmd   = 1\'b1;\n"
	"\tassign\to_sd_data  = 4\'hf;\n"
	"\tassign\tsdcard_int = 1'b0\n";
static	const	char	*toplvl = ""
	"\t//\n"
	"\t//\n"
	"\t// Wires for setting up the SD Card Controller\n"
	"\t//\n"
	"\t//\n"
	"\tassign io_sd_cmd = w_sd_cmd ? 1'bz:1'b0;\n"
	"\tassign io_sd[0] = w_sd_data[0]? 1'bz:1'b0;\n"
	"\tassign io_sd[1] = w_sd_data[1]? 1'bz:1'b0;\n"
	"\tassign io_sd[2] = w_sd_data[2]? 1'bz:1'b0;\n"
	"\tassign io_sd[3] = w_sd_data[3]? 1'bz:1'b0;\n\n";

static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	// Ethernet configuration (MDIO) port
	{ 0, "R_SDCARD_CTRL",  	"SDCARD"    },
	{ 1, "R_SDCARD_DATA", 	"SDDATA"    },
	{ 2, "R_SDCARD_FIFOA",	"SDFIFOA" }, // SDFIF0, SDFIFA
	{ 3, "R_SDCARD_FIFOB",	"SDFIFOB" }, // SDFIF1, SDFIFB
	{ -1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"#define\tSD_SETAUX\t0x0ff\n"
"#define\tSD_READAUX\t0x0bf\n"
"#define\tSD_CMD\t	0x040\n"
"#define\tSD_FIFO_OP\t0x0800	// Read only\n"
"#define\tSD_WRITEOP\t0x0c00	// Write to the FIFO\n"
"#define\tSD_ALTFIFO\t0x1000\n"
"#define\tSD_BUSY\t	0x4000\n"
"#define\tSD_ERROR\t0x8000\n"
"#define\tSD_CLEARERR\t0x8000\n"
"#define\tSD_READ_SECTOR\t((SD_CMD|SD_CLEARERR|SD_FIFO_OP)+17)\n"
"#define\tSD_WRITE_SECTOR\t((SD_CMD|SD_CLEARERR|SD_WRITEOP)+24)\n"
"\n"
"typedef\tstruct SDCARD_S {\n"
	"\tunsigned\tsd_ctrl, sd_data, sd_fifo[2];\n"
"} SDCARD;\n\n";

static	const	char	*ioname = "io_sd";


extern	AUTOPDATA	sdspi_data;

AUTOPDATA	sdspi_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
