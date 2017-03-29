#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "netb";
static	int	naddr = (1<<12);
static	const	char	*access  = "";
static	const	char	*ext_ports  = "";
static	const	char	*ext_decls  = "";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
"`ifndef\tETHERNET_ACCESS\n"
"\t// Ethernet packet memory declaration\n"
"\t//\n"
"\t// The only time this needs to be defined is when the ethernet module\n"
"\t// itself isnt defined.  Otherwise, the access is accomplished by the\n"
"\t// ethernet module\n"
"\n"
	"\tmemdev #(13)\n"
		"\t\tenet_buffers(i_clk,\n"
		"\t\t\t(wb_cyc), (wb_stb)&&(netb_sel),(wb_we)&&(wb_addr[11]),\n"
		"\t\t\t\twb_addr[11:0], wb_data, wb_sel\n"
		"\t\t\t\tnetb_ack, netb_stall, netb_data);\n"
"\n"
"`else\n\n"
"\tassign\tnetb_ack   = 1\'b0;\n"
"\tassign\tnetb_stall = 1\'b0;\n"
"\n"
"`endif\n\n";

static	const	char	*alt_insert = "";
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "R_NET_RXBUF",  "RAM" },
	{ (1<<11), "R_NET_TXBUF",  "RAM" },
	{-1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct = "";
static	const	char	*ioname = "";


extern	AUTOPDATA	netb_data;

AUTOPDATA	netb_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};


static	const char	*pprefix = "netp";
static	int		pnaddr = 8;
static	const	char	*paccess  = "ETHERNET_ACCESS";
static	const	char	*pext_ports  = "\t// Ethernet control (packets) lines\n"
	"\to_net_reset_n, i_net_rx_clk, i_net_col, i_net_crs, i_net_dv,\n"
	"\t\ti_net_rxd, i_net_rxerr,\n"
	"\ti_net_tx_clk, o_net_tx_en, o_net_txd\n";
static	const	char	*pext_decls  = 
	"\t// Ethernet control\n"
	"\toutput\twire\t\to_net_reset_n;\n"
	"\tinput\t\t\ti_net_rx_clk, i_net_col, i_net_crs, i_net_dv;\n"
	"\tinput\t\t[3:0]\ti_net_rxd;\n"
	"\tinput\t\t\ti_net_rxerr;\n"
	"\tinput\t\t\ti_net_tx_clk;\n"
	"\toutput\twire	\to_net_tx_en;\n"
	"\toutput\twire	[3:0]\to_net_txd;\n";

static	const	char	*pmain_defns = "\twire\tenet_rx_int, enet_tx_int;\n";
static	const	char	*pdbg_defns  = "";
static	const	char	*pmain_insert = ""
	"\tenetpackets	#(12)\n"
	"\t\tnetctrl(i_clk, i_rst, wb_cyc,(wb_stb)&&((netp_sel)||(netb_sel)),\n"
		"\t\t\twb_we, { (netb_sel), wb_addr[10:0] }, wb_data, wb_sel,\n"
			"\t\t\t\tnetp_ack, netp_stall, netp_data,\n"
		"\t\t\to_net_reset_n,\n"
		"\t\t\ti_net_rx_clk, i_net_col, i_net_crs, i_net_dv, i_net_rxd,\n"
			"\t\t\t\ti_net_rxerr,\n"
		"\t\t\ti_net_tx_clk, o_net_tx_en, o_net_txd,\n"
		"\t\t\tenet_rx_int, enet_tx_int);\n\n";

static	const	char	*palt_insert = "";
static	const	char	*ptoplvl = "";
static	const	char	*pdbg_insert = "";
static	REGINFO	pipregs[] = {
	{ 0, "R_NET_RXCMD",  "RXCMD" },	// NETRX
	{ 1, "R_NET_TXCMD",  "TXCMD" },	// NETTX
	{ 2, "R_NET_MACHI",  "MACHI" },
	{ 3, "R_NET_MACLO",  "MACLO" },
	{ 4, "R_NET_RXMISS", "NETMISS" },
	{ 5, "R_NET_RXERR",  "NETERR" },
	{ 6, "R_NET_RXCRC",  "NETXCRC" },
	{ 7, "R_NET_TXCOL",  "NETCOL" } };
static REGINFO *ppregs = &pipregs[0];

static	const	char	*pcstruct =
"// Network packet interface\n"
"#define\tENET_TXGO	\t0x004000\n"
"#define\tENET_TXBUSY	\t0x004000\n"
"#define\tENET_NOHWCRC	\t0x008000\n"
"#define\tENET_NOHWMAC	\t0x010000\n"
"#define\tENET_RESET	\t0x020000\n"
"#define\tENET_NOHWIPCHK\t	0x040000\n"
"#define\tENET_TXCMD(LEN)\t	((LEN)|ENET_TXGO)\n"
"#define\tENET_TXCLR	\t0x038000\n"
"#define\tENET_TXCANCEL	\t0x000000\n"
"#define\tENET_RXAVAIL	\t0x004000\n"
"#define\tENET_RXBUSY	\t0x008000\n"
"#define\tENET_RXMISS	\t0x010000\n"
"#define\tENET_RXERR	\t0x020000\n"
"#define\tENET_RXCRC	\t0x040000	// Set on a CRC error\n"
"#define\tENET_RXLEN	\trxcmd & 0x0ffff\n"
"#define\tENET_RXCLR	\t0x004000\n"
"#define\tENET_RXBROADCAST\t0x080000\n"
"#define\tENET_RXCLRERR	\t0x078000\n"
"#define\tENET_TXBUFLN(NET)\t(1<<(NET.txcmd>>24))\n"
"#define\tENET_RXBUFLN(NET)\t(1<<(NET.rxcmd>>24))\n"
"typedef\tstruct ENETPACKET_S {\n"
	"\tunsigned\tn_rxcmd, n_txcmd;\n"
	"\tunsigned long\tn_mac;\n"
	"\tunsigned\tn_rxmiss, n_rxerr, n_rxcrc, n_txcol;\n"
"} ENETPACKET;\n";

static	const	char	*pioname = "io_enet";


extern	AUTOPDATA	netp_data;

AUTOPDATA	netp_data = {
	pprefix, pnaddr, paccess, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, ppregs, cstruct, ioname
	};

