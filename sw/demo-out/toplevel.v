


//
// TO BE PLACED INTO toplevel.v
//
`default_nettype	none


//
// Here we declare our toplevel.v (toplevel) design module.
// All design logic must take place beneath this top level.
//
// The port declarations just copy data from the @TOP.PORTLIST
// key, or equivalently from the @MAIN.PORTLIST key if
// @TOP.PORTLIST is absent.  For those peripherals that don't need
// any top level logic, the @MAIN.PORTLIST should be sufficent,
// so the @TOP.PORTLIST key may be left undefined.
//
module	toplevel(i_clk,
		// The GPS 1PPS signal port
		i_gps_pps,
		// Ethernet control (packets) lines
		o_net_reset_n, i_net_rx_clk, i_net_col, i_net_crs, i_net_dv,
			i_net_rxd, i_net_rxerr,
		i_net_tx_clk, o_net_tx_en, o_net_txd,
		// The UART console
		i_aux_rx, o_aux_tx, i_aux_cts_n, o_aux_rts_n,
		// Toplevel ethernet MDIO ports
		o_eth_mdclk, io_eth_mdio,
		// SD Card
		o_sd_sck, io_sd_cmd, io_sd, i_sd_cs, i_sd_wp,
		// Top level Quad-SPI I/O ports
		o_qspi_cs_n, io_qspi_dat,
		// The PS/2 Mouse
		io_ps_clk, io_ps_data,
		// The GPS-UART
		i_gps_rx, o_gps_tx,
	// OLED control interface (roughly SPI)
	o_oled_sck, o_oled_cs_n, o_oled_mosi, o_oled_dcn,
	o_oled_reset_n, o_oled_vccen, o_oled_pmoden);
	//
	// Declaring our input and output ports.  We listed these above,
	// now we are declaring them here.
	//
	// These declarations just copy data from the @TOP.IODECLS key,
	// or from the @MAIN.IODECLS key if @TOP.IODECLS is absent.  For
	// those peripherals that don't do anything at the top level,
	// the @MAIN.IODECLS key should be sufficient, so the @TOP.IODECLS
	// key may be left undefined.
	//
	input			i_clk;
	//The GPS Clock
	input			i_gps_pps;
	// Ethernet control (MDIO)
	output	wire		o_mdclk, o_mdio, o_mdwe;
	input			i_mdio;
	// SD-Card declarations
	output	wire		o_sd_sck, o_sd_cmd;
	output	wire	[3:0]	o_sd_data;
	input			i_sd_cmd;
	input		[3:0]	i_sd_data;
	input			i_sd_detect;
	input			i_gps_rx;
	output	wire		o_gps_tx;
	// OLEDRGB interface
	output	wire		o_oled_sck, o_oled_cs_n, o_oled_mosi,
				o_oled_dcn, o_oled_reset_n, o_oled_vccen,
				o_oled_pmoden;


	//
	// Declaring component data, internal wires and registers
	//
	// These declarations just copy data from the @TOP.DEFNS key
	// within the component data files.
	//
	wire	w_mdio, w_mdwe;
	wire		w_sd_cmd;
	wire	[3:0]	w_sd_data;
	wire		w_qspi_sck, w_qspi_cs_n;
	wire	[1:0]	qspi_bmod;
	wire	[3:0]	qspi_dat;
	wire	[3:0]	i_qspi_dat;
	wire	[1:0]	w_ps2;


	//
	// Time to call the main module within main.v.  Remember, the purpose
	// of the main.v module is to contain all of our portable logic.
	// Things that are Xilinx (or even Altera) specific, or for that
	// matter anything that requires something other than on-off logic,
	// such as the high impedence states required by many wires, is
	// kept in this (toplevel.v) module.  Everything else goes in
	// main.v.
	//
	// We automatically place s_clk, and s_reset here.  You may need
	// to define those above.  (You did, didn't you?)  Other
	// component descriptions come from the keys @TOP.MAIN (if it
	// exists), or @MAIN.PORTLIST if it does not.
	//

	main(s_clk, s_reset,
		// The GPS 1PPS signal port
		i_gps_pps,
		// Ethernet control (packets) lines
		o_net_reset_n, i_net_rx_clk, i_net_col, i_net_crs, i_net_dv,
			i_net_rxd, i_net_rxerr,
		i_net_tx_clk, o_net_tx_en, o_net_txd,
		// The UART console
		i_aux_rx, o_aux_tx, i_aux_cts_n, o_aux_rts_n,
		o_eth_mdclk, w_mdio, w_mdwe, io_eth_mdio,
		// SD Card
		o_sd_sck, w_sd_cmd, w_sd_data, io_sd_cmd, io_sd, i_sd_cs,
		// Quad SPI flash
		w_qspi_cs_n, w_qspi_sck, qspi_dat, i_qspi_dat, qspi_bmod,
		// The PS/2 Mouse
		io_ps2, w_ps2,
		// The GPS-UART
		i_gps_rx, o_gps_tx,
	// OLED control interface (roughly SPI)
	o_oled_sck, o_oled_cs_n, o_oled_mosi, o_oled_dcn,
	o_oled_reset_n, o_oled_vccen, o_oled_pmoden);


	//
	// Our final section to the toplevel is used to provide all of
	// that special logic that couldnt fit in main.  This logic is
	// given by the @TOP.INSERT tag in our data files.
	//


	assign	io_eth_mdio = (w_mdwe)?w_mdio : 1'bz;

	//
	//
	// Wires for setting up the SD Card Controller
	//
	//
	assign io_sd_cmd = w_sd_cmd ? 1'bz:1'b0;
	assign io_sd[0] = w_sd_data[0]? 1'bz:1'b0;
	assign io_sd[1] = w_sd_data[1]? 1'bz:1'b0;
	assign io_sd[2] = w_sd_data[2]? 1'bz:1'b0;
	assign io_sd[3] = w_sd_data[3]? 1'bz:1'b0;


	// QSPI)BMOD, Quad SPI bus mode, Bus modes are:
	//	0?	Normal serial mode, one bit in one bit out
	//	10	Quad SPI mode, going out
	//	11	Quad SPI mode coming from the device (read mode)
	assign io_qspi_dat = (~qspi_bmod[1])?({2'b11,1'bz,qspi_dat[0]})
				:((qspi_bmod[0])?(4'bzzzz):(qspi_dat[3:0]));

	// The following primitive is necessary in many designs order to gain
	// access to the o_qspi_sck pin.  It's not necessary on the Arty,
	// simply because they provide two pins that can drive the QSPI
	// clock pin.
	wire	[3:0]	su_nc;	// Startup primitive, no connect
	STARTUPE2 #(
		// Leave PROG_USR false to avoid activating the program
		// event security feature.  Notes state that such a feature
		// requires encrypted bitstreams.
		.PROG_USR("FALSE"),
		// Sets the configuration clock frequency (in ns) for
		// simulation.
		.SIM_CCLK_FREQ(0.0)
	) STARTUPE2_inst (
	// CFGCLK, 1'b output: Configuration main clock output -- no connect
	.CFGCLK(su_nc[0]),
	// CFGMCLK, 1'b output: Configuration internal oscillator clock output
	.CFGMCLK(su_nc[1]),
	// EOS, 1'b output: Active high output indicating the End Of Startup.
	.EOS(su_nc[2]),
	// PREQ, 1'b output: PROGRAM request to fabric output
	//	Only enabled if PROG_USR is set.  This lets the fabric know
	//	that a request has been made (either JTAG or pin pulled low)
	//	to program the device
	.PREQ(su_nc[3]),
	// CLK, 1'b input: User start-up clock input
	.CLK(1'b0),
	// GSR, 1'b input: Global Set/Reset input
	.GSR(1'b0),
	// GTS, 1'b input: Global 3-state input
	.GTS(1'b0),
	// KEYCLEARB, 1'b input: Clear AES Decrypter Key input from BBRAM
	.KEYCLEARB(1'b0),
	// PACK, 1-bit input: PROGRAM acknowledge input
	//	This pin is only enabled if PROG_USR is set.  This allows the
	//	FPGA to acknowledge a request for reprogram to allow the FPGA
	//	to get itself into a reprogrammable state first.
	.PACK(1'b0),
	// USRCLKO, 1-bit input: User CCLK input -- This is why I am using this
	// module at all.
	.USRCCLKO(qspi_sck),
	// USRCCLKTS, 1'b input: User CCLK 3-state enable input
	//	An active high here places the clock into a high impedence
	//	state.  Since we wish to use the clock as an active output
	//	always, we drive this pin low.
	.USRCCLKTS(1'b0),
	// USRDONEO, 1'b input: User DONE pin output control
	//	Set this to "high" to make sure that the DONE LED pin is
	//	high.
	.USRDONEO(1'b1),
	// USRDONETS, 1'b input: User DONE 3-state enable output
	//	This enables the FPGA DONE pin to be active.  Setting this
	//	active high sets the DONE pin to high impedence, setting it
	//	low allows the output of this pin to be as stated above.
	.USRDONETS(1'b1)
	);


	// WB-Mouse
	//
	// Adjustments necessary to turn the PS/2 logic to pull-up logic,
	// with a high impedence state if not used.
	assign	io_ps_clk  = (w_ps2[1])? 1'bz:1'b0;
	assign	io_ps_data = (w_ps2[0])? 1'bz:1'b0;



endmodule; // end of toplevel.v module definition
