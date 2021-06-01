////////////////////////////////////////////////////////////////////////////////
//
// Filename:	../demo-out/toplevel.v
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// DO NOT EDIT THIS FILE!
// Computer Generated: This file is computer generated by AUTOFPGA. DO NOT EDIT.
// DO NOT EDIT THIS FILE!
//
// CmdLine:	./autofpga ./autofpga -d -o ../demo-out -I ../auto-data bkram.txt buserr.txt clkcounter.txt clock.txt enet.txt flash.txt global.txt gpio.txt gps.txt hdmi.txt icape.txt legalgen.txt mdio.txt pic.txt pwrcount.txt rtcdate.txt rtcgps.txt sdram.txt sdspi.txt spio.txt version.txt wbmouse.txt wboledbw.txt wbpmic.txt wbscopc.txt wbscope.txt wbubus.txt xpander.txt zipmaster.txt
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2021, Gisselquist Technology, LLC
// {{{
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
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
// }}}
// License:	GPL, v3, as defined and found on www.gnu.org,
// {{{
//		http://www.gnu.org/licenses/gpl.html
//
////////////////////////////////////////////////////////////////////////////////
//
// }}}
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
// The only exception is that any clocks with CLOCK.TOP tags will
// also appear in this list
//
module	toplevel(i_clk,
		// HDMI output clock
		o_hdmi_out_clk_n, o_hdmi_out_clk_p,
		// HDMI output pixels
		o_hdmi_out_p, o_hdmi_out_n,
		// GPIO ports
		io_hdmi_in_cec,
		o_hdmi_in_hpa,	// Hotplug assert
		o_hdmi_in_txen,
		io_hdmi_out_cec,
		i_hdmi_out_hpd_n, // Hotplug detect
		o_sd_reset,
		i_gps_3df,
		// Top level Quad-SPI I/O ports
		o_qspi_cs_n, io_qspi_dat,
		// The GPS-UART
		i_gpsu_rx, o_gpsu_tx,
		// Ethernet control (packets) lines
		o_net_reset_n,
		// eth_int_b	// Interrupt, leave floating
		// eth_pme_b	// Power management event, leave floating
		i_net_rx_clk, i_net_rx_ctl, i_net_rxd,
		o_net_tx_clk, o_net_tx_ctl, o_net_txd,
		// SD Card
		o_sd_sck, io_sd_cmd, io_sd, i_sd_cd_n,
		// HDMI input clock, and then data
		i_hdmi_in_clk_n, i_hdmi_in_clk_p,
		i_hdmi_in_p, i_hdmi_in_n,
		// The GPS 1PPS signal port
		i_gps_pps,
		// Toplevel ethernet MDIO ports
		o_eth_mdclk, io_eth_mdio,
		// HDMI output EDID I2C ports
		io_hdmi_out_scl, io_hdmi_out_sda,
		// SDRAM I/O port wires
		ddr3_reset_n, ddr3_cke, ddr3_ck_p, ddr3_ck_n,
		ddr3_ras_n, ddr3_cas_n, ddr3_we_n,
		ddr3_dqs_p, ddr3_dqs_n,
		ddr3_addr, ddr3_ba,
		ddr3_dq, ddr3_dm, ddr3_odt,
		// SPIO interface
		i_sw, i_btnc, i_btnd, i_btnl, i_btnr, i_btnu, o_led,
		// The PS/2 Mouse
		io_ps2_clk, io_ps2_data,
		// OLED control interface (roughly SPI)
		o_oled_sck, o_oled_mosi, o_oled_dcn,
		o_oled_reset_n, o_oled_panel_en, o_oled_logic_en,
		// The PMic3 microphone wires
		o_mic_csn, o_mic_sck, i_mic_din,
		// UART/host to wishbone interface
		i_host_uart_rx, o_host_uart_tx,
		// HDMI input EDID I2C ports
		io_hdmi_in_scl, io_hdmi_in_sda);
	//
	// Declaring our input and output ports.  We listed these above,
	// now we are declaring them here.
	//
	// These declarations just copy data from the @TOP.IODECLS key,
	// or from the @MAIN.IODECL key if @TOP.IODECL is absent.  For
	// those peripherals that don't do anything at the top level,
	// the @MAIN.IODECL key should be sufficient, so the @TOP.IODECL
	// key may be left undefined.
	//
	// We start with any @CLOCK.TOP keys
	//
	input	wire		i_clk;
	// HDMI output clock
	output	wire	o_hdmi_out_clk_n, o_hdmi_out_clk_p;
	// HDMI output pixels
	output	[2:0]	o_hdmi_out_p, o_hdmi_out_n;
	// GPIO wires
	inout	wire	io_hdmi_in_cec;
	output	wire	o_hdmi_in_hpa;
	output	wire	o_hdmi_in_txen;
	inout	wire	io_hdmi_out_cec;
	input	wire	i_hdmi_out_hpd_n;
	output	wire	o_sd_reset;
	input	wire	i_gps_3df;
	// Quad SPI flash
	output	wire		o_qspi_cs_n;
	inout	wire	[3:0]	io_qspi_dat;
	input	wire		i_gpsu_rx;
	output	wire		o_gpsu_tx;
	// Ethernet (RGMII) port wires
	output	wire		o_net_reset_n;
	input	wire		i_net_rx_clk, i_net_rx_ctl;
	input	wire	[3:0]	i_net_rxd;
	output	wire		o_net_tx_clk, o_net_tx_ctl;
	output	wire	[3:0]	o_net_txd;
	// SD Card
	output	wire		o_sd_sck;
	inout	wire		io_sd_cmd;
	inout	wire	[3:0]	io_sd;
	input	wire		i_sd_cd_n;
	// HDMI input clock
	input	wire	i_hdmi_in_clk_n, i_hdmi_in_clk_p;
	input	[2:0]	i_hdmi_in_p, i_hdmi_in_n;
	//The GPS Clock
	input	wire		i_gps_pps;
	// Ethernet control (MDIO)
	output	wire		o_eth_mdclk;
	inout	wire		io_eth_mdio;
	// HDMI output EDID I2C ports
	inout	wire	io_hdmi_out_scl, io_hdmi_out_sda;
	// I/O declarations for the DDR3 SDRAM
	output	wire		ddr3_reset_n;
	output	wire	[0:0]	ddr3_cke;
	output	wire	[0:0]	ddr3_ck_p, ddr3_ck_n;
	// output	wire	[0:0]	ddr3_cs_n; // This design has no CS pin
	output	wire		ddr3_ras_n, ddr3_cas_n, ddr3_we_n;
	output	wire	[2:0]	ddr3_ba;
	output	wire	[14:0]	ddr3_addr;
	output	wire	[0:0]	ddr3_odt;
	output	wire	[1:0]	ddr3_dm;
	inout	wire	[1:0]	ddr3_dqs_p, ddr3_dqs_n;
	inout	wire	[15:0]	ddr3_dq;

	// SPIO interface
	input	wire	[8-1:0]	i_sw;
	input	wire		i_btnc, i_btnd, i_btnl, i_btnr, i_btnu;
	output	wire	[8-1:0]	o_led;
	inout	wire	io_ps2_clk, io_ps2_data;
	// OLEDBW interface
	output	wire		o_oled_sck, o_oled_mosi,
				o_oled_dcn, o_oled_reset_n, o_oled_panel_en,
				o_oled_logic_en;
	output	wire		o_mic_csn, o_mic_sck;
	input	wire		i_mic_din;
	input	wire		i_host_uart_rx;
	output	wire		o_host_uart_tx;
	// HDMI input EDID I2C ports
	inout	wire	io_hdmi_in_scl, io_hdmi_in_sda;


	//
	// Declaring component data, internal wires and registers
	//
	// These declarations just copy data from the @TOP.DEFNS key
	// within the component data files.
	//
	wire		w_hdmi_out_hsclk, w_hdmi_out_logic_clk;
	wire	[9:0]	w_hdmi_out_r, w_hdmi_out_g, w_hdmi_out_b;
	// GPIO declarations.  The two wire busses are just virtual lists of
	// input (or output) ports.
	wire	[15:0]	i_gpio, o_gpio;
	wire		w_hdmi_out_en;
	wire		w_hdmi_bypass_sda;
	wire		w_hdmi_bypass_scl;
	wire		s_clk, s_reset;
	wire		w_qspi_sck, w_qspi_cs_n;
	wire	[1:0]	qspi_bmod;
	wire	[3:0]	qspi_dat;
	// Ethernet (RGMII) port wires
	wire	[7:0]		w_net_rxd,  w_net_txd;
	wire			w_net_rxdv, w_net_rxerr,
				w_net_txctl;
	wire	[1:0]		w_net_tx_clk;
	wire		w_sd_cmd;
	wire	[3:0]	w_sd_data;

	wire		i_sd_cmd;
	wire	[3:0]	i_sd;
	wire		w_hdmi_in_logic_clk, w_hdmi_in_hsclk,
			w_hdmi_in_clk_no_buf, w_hdmi_in_clk_no_delay,
			w_hdmi_in_clk_raw;
	wire	[9:0]	w_hdmi_in_red, w_hdmi_in_green, w_hdmi_in_blue;
	// HDMI input (sink) delay definition(s)
	wire	[4:0]	w_hdmi_in_delay, w_hdmi_in_actual_delay_r,
			w_hdmi_in_actual_delay_g, w_hdmi_in_actual_delay_b;
	//
	wire		w_hdmi_in_pll_locked;
	// Ethernet control (MDIO)
	wire		w_mdio, w_mdwe;
	// HDMI command I2C wires, to support the EDID protocol
	// These are used to determine if the bus wires are to be set to zero
	// or not
	wire		w_hdmi_out_scl, w_hdmi_out_sda;
	// Wires necessary to run the SDRAM
	//
	wire	sdram_cyc, sdram_stb, sdram_we,
		sdram_stall, sdram_ack, sdram_err;
	wire	[(29-4-1):0]	sdram_addr;
	wire	[(128-1):0]	sdram_wdata,
					sdram_rdata;
	wire	[(128/8-1):0]	sdram_sel;
	wire	[31:0]			sdram_dbg;

	//
	// Wires coming back from the SDRAM
	wire	s_clk, s_reset;
	wire	[8-1:0]	w_led;
	wire	[1:0]	w_ps2;
	// HDMI command I2C wires, to support the EDID protocol
	// These are used to determine if the bus wires are to be set to zero
	// or not
	wire		w_hdmi_in_scl, w_hdmi_in_sda;


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

	main	thedesign(s_clk, s_reset,
 		// Reset wire for the ZipCPU
 		s_reset,
		// HDMI output ports
		w_hdmi_out_logic_clk,
		// HDMI output pixels, set within the main module
		w_hdmi_out_r, w_hdmi_out_g, w_hdmi_out_b,
		// GPIO wires
		i_gpio, o_gpio,
		// Quad SPI flash
		w_qspi_cs_n, w_qspi_sck, qspi_dat, io_qspi_dat, qspi_bmod,
		// The GPS-UART
		i_gpsu_rx, o_gpsu_tx,
		// Ethernet (RGMII) connections
		o_net_reset_n,
		i_net_rx_clk, w_net_rxdv,  w_net_rxdv ^ w_net_rxerr, w_net_rxd,
		w_net_tx_clk, w_net_txctl, w_net_txd,
		// SD Card
		o_sd_sck, w_sd_cmd, w_sd_data, i_sd_cmd, i_sd, !i_sd_cd_n,
		// HDMI input clock
		w_hdmi_in_logic_clk,
		w_hdmi_in_red, w_hdmi_in_green, w_hdmi_in_blue,
		w_hdmi_in_hsclk,
		// HDMI input (sink) delay
		w_hdmi_in_actual_delay_r, w_hdmi_in_actual_delay_g,
		w_hdmi_in_actual_delay_b, w_hdmi_in_delay,
		// The GPS 1PPS signal port
		i_gps_pps,
		o_eth_mdclk, w_mdio, w_mdwe, io_eth_mdio,
		// EDID port for the HDMI source
		io_hdmi_out_scl, io_hdmi_out_sda, w_hdmi_out_scl, w_hdmi_out_sda,
		// The SDRAM interface to an toplevel AXI4 module
		//
		sdram_cyc, sdram_stb, sdram_we,
			sdram_addr, sdram_wdata, sdram_sel,
			sdram_stall, sdram_ack, sdram_rdata,
			sdram_err
			, sdram_dbg,
		i_sw, i_btnc, i_btnd, i_btnl, i_btnr, i_btnu, w_led,
		// The PS/2 Mouse
		{ io_ps2_clk, io_ps2_data }, w_ps2,
		// OLED control interface (roughly SPI)
		o_oled_sck, o_oled_mosi, o_oled_dcn,
		o_oled_reset_n, o_oled_panel_en, o_oled_logic_en,
		// The PMic3 microphone wires
		o_mic_csn, o_mic_sck, i_mic_din,
		// UART/host to wishbone interface
		i_host_uart_rx, o_host_uart_tx,
		// HDMI input EDID I2C ports
		io_hdmi_in_scl, io_hdmi_in_sda, w_hdmi_in_scl, w_hdmi_in_sda);


	//
	// Our final section to the toplevel is used to provide all of
	// that special logic that couldnt fit in main.  This logic is
	// given by the @TOP.INSERT tag in our data files.
	//


	assign	w_hdmi_out_hsclk     = w_hdmi_in_hsclk;
	assign	w_hdmi_out_logic_clk = w_hdmi_in_logic_clk;

	xhdmiout ohdmick(w_hdmi_out_logic_clk, w_hdmi_out_hsclk, w_hdmi_out_en,
			10'h3e0,
			{ o_hdmi_out_clk_p, o_hdmi_out_clk_n });
	xhdmiout ohdmir(w_hdmi_out_logic_clk, w_hdmi_out_hsclk, w_hdmi_out_en,
			w_hdmi_out_r,
			{ o_hdmi_out_p[2], o_hdmi_out_n[2] });
	xhdmiout ohdmig(w_hdmi_out_logic_clk, w_hdmi_out_hsclk, w_hdmi_out_en,
			w_hdmi_out_g,
			{ o_hdmi_out_p[1], o_hdmi_out_n[1] });
	xhdmiout ohdmib(w_hdmi_out_logic_clk, w_hdmi_out_hsclk, w_hdmi_out_en,
			w_hdmi_out_b,
			{ o_hdmi_out_p[0], o_hdmi_out_n[0] });

	assign	i_gpio = { 10'h0,
			w_hdmi_in_pll_locked,
			sysclk_locked, i_gps_3df,
			!i_hdmi_out_hpd_n, !i_sd_cd_n,
			io_hdmi_out_cec, io_hdmi_in_cec };
	assign	io_hdmi_in_cec  = o_gpio[0] ? 1'bz : 1'b0;
	assign	io_hdmi_out_cec = o_gpio[1] ? 1'bz : 1'b0;
	assign	w_hdmi_bypass_scl=o_gpio[2];
	assign	w_hdmi_bypass_sda=o_gpio[3];
	assign	o_hdmi_in_txen  = o_gpio[4];
	assign	o_hdmi_in_hpa   = o_gpio[5];	// Hotplug assert
	assign	o_sd_reset      = o_gpio[6];
	assign	w_hdmi_out_en   = o_gpio[7];

	assign	s_clk = i_clk;
	// assign	s_reset = 1'b0; // This design requires local, not global resets

	//
	//
	// Wires for setting up the QSPI flash wishbone peripheral
	//
	//
	// QSPI)BMOD, Quad SPI bus mode, Bus modes are:
	//	0?	Normal serial mode, one bit in one bit out
	//	10	Quad SPI mode, going out
	//	11	Quad SPI mode coming from the device (read mode)
	assign io_qspi_dat = (~qspi_bmod[1])?({2'b11,1'bz,qspi_dat[0]})
				:((qspi_bmod[0])?(4'bzzzz):(qspi_dat[3:0]));
	assign	o_qspi_cs_n = w_qspi_cs_n;

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
	.USRCCLKO(w_qspi_sck),
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



	xiddr	rx0(i_net_rx_clk, i_net_rxd[0], { w_net_rxd[4], w_net_rxd[0] });
	xiddr	rx1(i_net_rx_clk, i_net_rxd[1], { w_net_rxd[5], w_net_rxd[1] });
	xiddr	rx2(i_net_rx_clk, i_net_rxd[2], { w_net_rxd[6], w_net_rxd[2] });
	xiddr	rx3(i_net_rx_clk, i_net_rxd[3], { w_net_rxd[7], w_net_rxd[3] });
	xiddr	rxc(i_net_rx_clk, i_net_rx_ctl, { w_net_rxdv,   w_net_rxerr });

	xoddr	tx0(s_clk_125mhz, { w_net_txd[0], w_net_txd[4] }, o_net_txd[0]);
	xoddr	tx1(s_clk_125mhz, { w_net_txd[1], w_net_txd[5] }, o_net_txd[1]);
	xoddr	tx2(s_clk_125mhz, { w_net_txd[2], w_net_txd[6] }, o_net_txd[2]);
	xoddr	tx3(s_clk_125mhz, { w_net_txd[3], w_net_txd[7] }, o_net_txd[3]);
	xoddr	txc(s_clk_125mhz, { w_net_txctl,  w_net_txctl  }, o_net_tx_ctl);
	xoddr	txck(s_clk_125d,{w_net_tx_clk[1],w_net_tx_clk[0]},o_net_tx_clk);



	//
	//
	// Wires for setting up the SD Card Controller
	//
	//
	// IOBUF sd_cmd_buf(.T(w_sd_cmd),.O(i_sd_cmd), .I(1'b0), .IO(io_sd_cmd));
	IOBUF sd_dat0_bf(.T(1'b1),.O(i_sd[0]),.I(1'b1),.IO(io_sd[0]));
	IOBUF sd_dat1_bf(.T(1'b1),.O(i_sd[1]),.I(1'b1),.IO(io_sd[1]));
	IOBUF sd_dat2_bf(.T(1'b1),.O(i_sd[2]),.I(1'b1),.IO(io_sd[2]));
	// IOBUF sd_dat3_bf(.T(w_sd_data[3]),.O(i_sd[3]),.I(1'b0),.IO(io_sd[3]));

	IOBUF sd_cmd_buf(.T(1'b0),.O(i_sd_cmd), .I(w_sd_cmd), .IO(io_sd_cmd));
	IOBUF sd_dat3_bf(.T(1'b0),.O(i_sd[3]),.I(w_sd_data[3]),.IO(io_sd[3]));
	



	// First, let's get our clock up and running
	// 1. Convert from differential to single
	IBUFDS	hdmi_in_clk_ibuf(
			.I(i_hdmi_in_clk_p), .IB(i_hdmi_in_clk_n),
			.O(w_hdmi_in_clk_raw));

	BUFG	rawckbuf(.I(w_hdmi_in_clk_raw), .O(w_hdmi_in_clk_no_buf));



	// 3. Use that signal to generate a clock
	xhdmiiclk xhclkin(s_clk, w_hdmi_in_clk_no_buf, o_hdmi_in_txen,
			w_hdmi_in_hsclk, w_hdmi_in_logic_clk,
			w_hdmi_in_pll_locked);

	xhdmiin xhin_r(w_hdmi_in_logic_clk, w_hdmi_in_hsclk,
		o_hdmi_in_txen,
		w_hdmi_in_delay, w_hdmi_in_actual_delay_r,
		{ i_hdmi_in_p[2], i_hdmi_in_n[2] }, w_hdmi_in_red);
	xhdmiin	xhin_g(w_hdmi_in_logic_clk, w_hdmi_in_hsclk,
		o_hdmi_in_txen,
		w_hdmi_in_delay, w_hdmi_in_actual_delay_g,
		{ i_hdmi_in_p[1], i_hdmi_in_n[1] }, w_hdmi_in_green);
	xhdmiin xhin_b(w_hdmi_in_logic_clk, w_hdmi_in_hsclk,
		o_hdmi_in_txen,
		w_hdmi_in_delay, w_hdmi_in_actual_delay_b,
		{ i_hdmi_in_p[0], i_hdmi_in_n[0] }, w_hdmi_in_blue);

	// Xilinx requires an IDELAYCTRL to be added any time the IDELAY
	// element is included in the design.  Strangely, this doesn't need
	// to be conencted to the IDELAY in any fashion, it just needs to be
	// provided with a clock.
	//
	// I should associate this delay with a delay group --- just haven't
	// done that yet.
	wire	delay_reset;
	assign	delay_reset = 1'b0;
	/*
	always @(posedge s_clk)
		delay_reset <= !sys_clk_locked;
	*/
	IDELAYCTRL dlyctrl(.REFCLK(s_clk_200mhz), .RDY(), .RST(delay_reset));



	assign	io_eth_mdio = (w_mdwe)?w_mdio : 1'bz;

	// The EDID I2C port for the HDMI source port
	//
	// We need to make certain we only force the pin to a zero (drain)
	// when trying to do so.  Otherwise we let it float (back) high.
	assign	io_hdmi_out_scl = ((w_hdmi_bypass_scl)&&(w_hdmi_out_scl)) ? 1'bz : 1'b0;
	assign	io_hdmi_out_sda = ((w_hdmi_bypass_sda)&&(w_hdmi_out_sda)) ? 1'bz : 1'b0;

	wire	[31:0]	sdram_debug;

	migsdram #(.AXIDWIDTH(1), .WBDATAWIDTH(128),
			.DDRWIDTH(16),
			.RAMABITS(29)) sdrami(
		.i_clk(i_clk_buffered),
		.i_clk_200mhz(s_clk_200mhz),
		.o_sys_clk(s_clk),
		// .i_rst(!i_cpu_resetn),
		.i_rst(upper_plls_stable[3:2] != 2'b11),
		.o_sys_reset(s_reset),
		//
		.i_wb_cyc(sdram_cyc), .i_wb_stb(sdram_stb),
		.i_wb_we(sdram_we), .i_wb_addr(sdram_addr),
			.i_wb_data(sdram_wdata), .i_wb_sel(sdram_sel),
		.o_wb_stall(sdram_stall),    .o_wb_ack(sdram_ack),
			.o_wb_data(sdram_rdata), .o_wb_err(sdram_err),
		//
		.o_ddr_ck_p(ddr3_ck_p), .o_ddr_ck_n(ddr3_ck_n),
		.o_ddr_reset_n(ddr3_reset_n), .o_ddr_cke(ddr3_cke),
		// .o_ddr_cs_n(ddr3_cs_n),	// No CS on this chip
		.o_ddr_ras_n(ddr3_ras_n),
		.o_ddr_cas_n(ddr3_cas_n), .o_ddr_we_n(ddr3_we_n),
		.o_ddr_ba(ddr3_ba), .o_ddr_addr(ddr3_addr),
		.o_ddr_odt(ddr3_odt), .o_ddr_dm(ddr3_dm),
		.io_ddr_dqs_p(ddr3_dqs_p), .io_ddr_dqs_n(ddr3_dqs_n),
		.io_ddr_data(ddr3_dq)
		,  .o_ram_dbg(sdram_dbg)
		);
 	

	assign	o_led = { w_led[8-1:2], (w_led[1] || !clocks_locked),
			w_led[0] | s_reset };

	// WB-Mouse
	//
	// Adjustments necessary to turn the PS/2 logic to pull-up logic,
	// with a high impedence state if not used.
	assign	io_ps2_clk  = (w_ps2[1])? 1'bz:1'b0;
	assign	io_ps2_data = (w_ps2[0])? 1'bz:1'b0;

	// The EDID I2C port for the HDMI source port
	//
	// We need to make certain we only force the pin to a zero (drain)
	// when trying to do so.  Otherwise we let it float (back) high.
	assign	io_hdmi_in_scl = (w_hdmi_in_scl) ? 1'bz : 1'b0;
	assign	io_hdmi_in_sda = (w_hdmi_in_sda) ? 1'bz : 1'b0;



endmodule // end of toplevel.v module definition
