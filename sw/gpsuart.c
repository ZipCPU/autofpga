#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "gpsu";
static	int	naddr = 4;
static	const	char	*access  = "GPSUART_ACCESS";
static	const	char	*ext_ports  = "\t\t// The GPS-UART\n\t\ti_gps_rx, o_gps_tx";
static	const	char	*ext_decls  = "\tinput\t\t\ti_gps_rx;\n\toutput\twire\t\to_gps_tx;";
static	const	char	*main_defns = ""
	"\twire\tgpsurx_int, gpsutx_int, gpsurxf_int, gpsutxf_int;\n";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twbuart #(.DEFAULT_STEP(GPSCLOCK_DEFAULT_STEP))\n"
		"\t\tgpsuart(i_clk, 1\'b0,\n"
		"\t\t\t(wb_stb)&&(gpsu_sel), wb_we, wb_addr[1:0], wb_data,\n"
		"\t\t\t\tgpsu_ack, gpsu_stall, gpsu_data,\n"
		"\t\t\t\ti_aux_rx, o_aux_tx, i_aux_cts_n, o_aux_rts_n,\n"
		"\t\t\t\tgpsurx_int, gpsutx_int, gpsurxf_int, gpsutxf_int);\n";
static	const	char	*alt_insert = ""
	"\tassign	o_gps_tx = 1\'b1;\n"
	"\tassign	o_gpsurx_int = 1\'b0;\n"
	"\tassign	o_gpsutx_int = 1\'b0;\n"
	"\tassign	o_gpsurxf_int = 1\'b0;\n"
	"\tassign	o_gpsutxf_int = 1\'b0;\n"
	;
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "R_UART_SETUP",  "GPSSETUP" },
	{ 1, "R_UART_FIFO",   "GPSFIFO"  },
	{ 2, "R_UART_UARTRX", "GPSRX" },
	{ 3, "R_UART_UARTTX", "GPSTX" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct =
"#ifndef\tWBUART_H\n"
"#define\tWBUART_H\n"
"\n"
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
"} WBUART;\n"
"#endif\n\n";
static	const	char	*ioname = "io_gpsu";


extern	AUTOPDATA	gpsu_data;

AUTOPDATA	gpsu_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};
