////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	zipmaster.c
//
// Project:	VideoZip, a ZipCPU SoC supporting video functionality
//
// Purpose:	To describe what needs to be done to make the ZipCPU a part
//		of a main .v file.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015-2017, Gisselquist Technology, LLC
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
const char	*zip_access = "INCLUDE_ZIPCPU";
const char	*zip_decl   = ""
	"\t//\n"
	"\t//\n"
	"\t// Variables/definitions needed by the ZipCPU BUS master\n"
	"\t//\n"
	"\t//\n"
"\n"
	"\tlocalparam\tRESET_ADDRESS = 32\'h01000000;\n"
	"\tlocalparam\tZIP_ADDRESS_WIDTH = 30;\t// Zip-CPU address width\n"
	"\tlocalparam\tZIP_INTS = 9;\t// Number of ZipCPU interrupts\n"
	"\tlocalparam\tZIP_STARTS_HALTED=1'b0;\t// Boolean, should the ZipCPU be halted on startup?\n"
"\n"
	"\twire		zip_cyc, zip_stb, zip_we, zip_cpu_int;\n"
	"\twire	[(ZA-1):0]	w_zip_addr;\n"
	"\twire	[31:0]	zip_addr, zip_data;\n"
	"\twire	[3:0]	zip_sel;\n"
	"\t// and then coming from devices\n"
	"\twire		zip_ack, zip_stall, zip_err;\n"
	"\twire	[(ZIP_INTS-1):0]	w_ints_to_zip_cpu;\n"
	"\t// And for the debug scope (if supported)\n"
	"\twire	[31:0]	zip_debug;\n"
	"\twire\t\tzip_trigger;\n"
	"\tassign\tzip_trigger = zip_debug[0];\n";

const char	*zip_insert = ""
	"\t//\n"
	"\t//\n"
	"\t// The ZipCPU/ZipSystem BUS master\n"
	"\t//\n"
	"\t//\n"
"\n"
	"\tzipsystem #(RESET_ADDRESS,ZIP_ADDRESS_WIDTH,10,ZIP_START_HALTED,ZIP_INTS)\n"
		"\t\tswic(i_clk, 1'b0,\n"
			"\t\t\t// Zippys wishbone interface\n"
			"\t\t\tzip_cyc, zip_stb, zip_we, w_zip_addr, zip_data, zip_sel,\n"
				"\t\t\t\tzip_ack, zip_stall, dwb_idata, zip_err,\n"
			"\t\t\tw_ints_to_zip_cpu, zip_cpu_int,\n"
			"\t\t\t// Debug wishbone interface\n"
			"\t\t\t((wbu_cyc)&&(wbu_zip_sel)),\n"
				"\t\t\t\t((wbu_stb)&&(wbu_zip_sel)),wbu_we, wbu_addr[0],\n"
				"\t\t\t\twbu_data,\n"
				"\t\t\t\tzip_dbg_ack, zip_dbg_stall, zip_dbg_data,\n"
			"\t\t\tzip_debug\n"
			"\t\t\t);\n";

const	char	*zip_alt = "";
