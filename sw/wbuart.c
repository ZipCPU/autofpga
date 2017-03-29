////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	wbuart.c
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Older description of a UART.  Doesn't work with the (newer)
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

static	const char	*prefix = "uart";
static	int	naddr = 4;
static	const	char	*access  = "CONSOLE_ACCESS";
static	const	char	*ext_ports  = "\t\t// The UART console\n\t\ti_aux_rx, o_aux_tx, i_aux_cts_n, o_aux_rts_n";
static	const	char	*ext_decls  = "\tinput\t\t\ti_aux_rx, i_aux_cts_n;\n\toutput\twire\t\to_aux_tx, o_aux_rts_n;";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twbuart #(.DEFAULT_STEP(GPSCLOCK_DEFAULT_STEP))\n"
		"\t\t) console(i_clk, 1\'b0,\n"
		"\t\t\t(wb_stb)&&(uart_sel), wb_we, wb_addr[1:0], wb_data,\n"
		"\t\t\t\tuart_ack, uart_stall, uart_data,\n"
		"\t\t\t\ti_aux_rx, o_aux_tx, i_aux_cts_n, o_aux_rts_n,\n"
		"\t\t\t\tuartrx_int, uarttx_int, uartrxf_int, uarttxf_int);\n";
static	const	char	*alt_insert = ""
	"\tassign	o_uart_tx    = 1\'b1;\n"
	"\tassign	o_uart_rts_n = 1\'b0;\n"
	;
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "R_UART_SETUP",  "USETUP" },
	{ 1, "R_UART_FIFO",   "UFIFO"  },
	{ 2, "R_UART_UARTRX", "RX" },
	{ 3, "R_UART_UARTTX", "TX" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"#define UART_PARITY_NONE        0\n"
"#define UART_HWFLOW_OFF         0x40000000\n"
"#define UART_PARITY_ODD         0x04000000\n"
"#define UART_PARITY_EVEN        0x05000000\n"
"#define UART_PARITY_SPACE       0x06000000\n"
"#define UART_PARITY_MARK        0x07000000\n"
"#define UART_STOP_ONEBIT        0\n"
"#define UART_STOP_TWOBITS       0x08000000\n"
"#define UART_DATA_8BITS         0\n"
"#define UART_DATA_7BITS         0x10000000\n"
"#define UART_DATA_6BITS         0x20000000\n"
"#define UART_DATA_5BITS         0x30000000\n"
"#define UART_RX_BREAK           0x0800\n"
"#define UART_RX_FRAMEERR        0x0400\n"
"#define UART_RX_PARITYERR       0x0200\n"
"#define UART_RX_NOTREADY        0x0100\n"
"#define UART_RX_ERR             (-256)\n"
"#define UART_TX_BUSY            0x0100\n"
"#define UART_TX_BREAK           0x0200\n"
"\n"
"typedef struct  WBUART_S {\n"
	"\tunsigned\t_setup;\n"
	"\tunsigned\t_fifo;\n"
	"\tunsigned\t_rx, u_tx;\n"
"} WBUART;\n\n";
static	const	char	*ioname = "io_uart";


extern	AUTOPDATA	console_data;

AUTOPDATA	console_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
