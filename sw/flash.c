#include <stdio.h>
#include "autopdata.h"

static	const char	*prefix = "flash";
static	int	naddr = (1<<24);
static	const	char	*access  = "FLASH_ACCESS";
static	const	char	*ext_ports  = "\t\t// The QSPI Flash\n"
		"\t\to_qspi_cs_n, o_qspi_sck, o_qspi_dat, i_qspi_dat, o_qspi_mod";
static	const	char	*ext_decls  = "\t// The QSPI flash\n"
	"\toutput\twire\t\to_qspi_cs_n, o_qspi_sck;\n"
	"\toutput\twire\t[3:0]\to_qspi_dat;\n"
	"\tinput\twire\t[3:0]\ti_qspi_dat;\n"
	"\toutput\twire\t[1:0]\to_qspi_mod;";
static	const	char	*main_defns = "";
static	const	char	*dbg_defns  = "";
static	const	char	*main_insert = ""
	"\twbqspiflash #(24)\n"
		"\t\t) flashmem(i_clk,\n"
		"\t\t\t(wb_cyc), (wb_stb)&&(flash_sel), (wb_stb)&&(flctl_sel),wb_we,\n"
		"\t\t\t\twb_addr[23:0], wb_data,\n"
		"\t\t\t\tflash_ack, flash_stall, flash_data,\n"
		"\t\t\t\to_qspi_sck, o_qspi_cs_n, o_qspi_dat, i_qspi_dat,\n"
		"\t\t\t\tflash_interrupt)\n";
static	const	char	*alt_insert = ""
	"\tassign	o_qspi_sck  = 1\'b1;\n"
	"\tassign	o_qspi_cs_n = 1\'b1;\n"
	"\tassign	o_qspi_mod  = 2\'b01;\n"
	"\tassign	o_qspi_dat  = 4\'b1111;\n"
	;
static	const	char	*toplvl = "";
static	const	char	*dbg_insert = "";
static	REGINFO	ipregs[] = {
	{ 0, "FLASHMEM",  "FLASH" },
	{-1, "", "" } };
static REGINFO *pregs = &ipregs[0];

static	const	char	*cstruct = "";
static	const	char	*ioname = "";


extern	AUTOPDATA	flash_data;

AUTOPDATA	flash_data = {
	prefix, naddr, access, ext_ports, ext_decls, main_defns, dbg_defns,
		main_insert, alt_insert, dbg_insert, pregs, cstruct, ioname
	};

static	const char	*ctlprefix = "flctl";
static	int		ctlnaddr = 4;
static	const	char	*ctlaccess  = "";
static	const	char	*ctlext_ports  = "";
static	const	char	*ctlext_decls  = "";
static	const	char	*ctlmain_defns = "";
static	const	char	*ctldbg_defns  = "";
static	const	char	*ctlmain_insert = "";
static	const	char	*ctlalt_insert = "";
static	const	char	*ctltoplvl = "";
static	const	char	*dbg_ctlinsert = "";
static	REGINFO	ctlipregs[] = {
	{ 0, "R_QSPI_EREG",  "QSPIE" },
	{ 1, "R_QSPI_CREG",  "QSPIC" },
	{ 2, "R_QSPI_SREG",  "QSPIS" },
	{ 3, "R_QSPI_IDREG", "QSPII" },
	{-1, "", "" } };
static REGINFO *ctlpregs = &ctlipregs[0];

static	const	char	*ctlcstruct = "typedef struct FLASHCTL_S {\n"
"\tunsigned\tf_ereg, f_config, f_status, f_id;\n"
"} FLASHCTL;\n";
static	const	char	*ctlioname = "io_flctl";


extern	AUTOPDATA	flctl_data;

AUTOPDATA	flctl_data = {
	ctlprefix, ctlnaddr, ctlaccess, ctlext_ports, ctlext_decls, ctlmain_defns, ctldbg_defns,
		ctlmain_insert, ctlalt_insert, dbg_ctlinsert, ctlpregs, ctlcstruct, ctlioname
	};

