////////////////////////////////////////////////////////////////////////////////
//
// Filename:	../demo-out/main.v
//
// Project:	VideoZip, a ZipCPU SoC supporting video functionality
//
// DO NOT EDIT THIS FILE!
// Computer Generated: This file is computer generated by AUTOFPGA. DO NOT EDIT.
// DO NOT EDIT THIS FILE!
//
// CmdLine:	./autofpga ./autofpga -o ../demo-out -I ../auto-data global.txt clock.txt bkram.txt flash.txt zipmaster.txt wbubus.txt dlyarbiter.txt gps.txt icape.txt mdio.txt spio.txt wboledbw.txt rtcdate.txt hdmi.txt clkcounter.txt gpio.txt pwrcount.txt wbpmic.txt version.txt buserr.txt pic.txt rtcgps.txt wbmouse.txt sdspi.txt
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
`default_nettype	none
//
//
// Here is a list of defines which may be used, post auto-design
// (not post-build), to turn particular peripherals (and bus masters)
// on and off.  In particular, to turn off support for a particular
// design component, just comment out its respective define below
//
// These lines are taken from the respective @ACCESS tags for each of our
// components.  If a component doesn't have an @ACCESS tag, it will not
// be listed here.
//
// First, the independent access fields for any bus masters
`define	SDSPI_ACCESS
`define	RTC_ACCESS
`define	MICROPHONE_ACCESS
`define	GPIO_ACCESS
`define	HDMI_OUT_EDID_ACCESS
`define	GPS_CLOCK
`define	HDMIIN_ACCESS
`define	FLASH_ACCESS
`define	FLASH_ACCESS
`define	BKRAM_ACCESS
`define	MOUSE_ACCESS
`define	HDMI_IN_EDID_ACCESS
`define	WBUBUS_MASTER
`define	INCLUDE_ZIPCPU
`define	OLEDBW_ACCESS
`define	CFG_ACCESS
`define	BUSPIC_ACCESS
`define	GPSUART_ACCESS
`define	NETCTRL_ACCESS
`define	SPIO_ACCESS
// And then for the independent peripherals
//
//
// Then, the list of those things that have dependencies
//
//
`ifdef	RTC_ACCESS
`define	RTCDATE_ACCESS
`endif
`ifdef	SDSPI_ACCESS
`define	SDSPI_SCOPE
`endif
//
// End of dependency list
//
//
//
//
// Finally, we define our main module itself.  We start with the list of
// I/O ports, or wires, passed into (or out of) the main function.
//
// These fields are copied verbatim from the respective I/O port lists,
// from the fields given by @MAIN.PORTLIST
//
module	main(i_clk, i_reset,
		// The SD-Card wires
		o_sd_sck, o_sd_cmd, o_sd_data, i_sd_cmd, i_sd_data, i_sd_detect,
		// The PMic3 microphone wires
		o_mic_csn, o_mic_sck, i_mic_din,
		// GPIO ports
		i_gpio, o_gpio,
		// HDMI out (source) EDID I2C ports
		i_hdmi_out_scl, i_hdmi_out_sda, o_hdmi_out_scl, o_hdmi_out_sda,
		// The GPS 1PPS signal port
		i_gps_pps,
		// HDMI input ports
		i_hdmi_in_clk,
		i_hdmi_in_r, i_hdmi_in_g, i_hdmi_in_b,
		i_hdmi_in_hsclk, i_clk_200mhz,
		// HDMI input delay control
		i_hdmi_in_actual_delay_r, i_hdmi_in_actual_delay_g,
		i_hdmi_in_actual_delay_b, o_hdmi_in_delay,
		// The QSPI Flash
		o_qspi_cs_n, o_qspi_sck, o_qspi_dat, i_qspi_dat, o_qspi_mod,
		// The PS/2 Mouse
		i_ps2, o_ps2,
		// HDMI input EDID I2C ports
		i_hdmi_in_scl, i_hdmi_in_sda, o_hdmi_in_scl, o_hdmi_in_sda,
		// Command and Control port
		i_host_rx_stb, i_host_rx_data,
		o_host_tx_stb, o_host_tx_data, i_host_tx_busy,
		i_cpu_reset,
		// OLED control interface (roughly SPI)
		o_oled_sck, o_oled_mosi, o_oled_dcn,
		o_oled_reset_n, o_oled_panel_en, o_oled_logic_en,
		// HDMI output ports
		i_hdmi_out_clk,
		// HDMI output pixels
		o_hdmi_out_r, o_hdmi_out_g, o_hdmi_out_b,
		// The GPS-UART
		i_gpsu_rx, o_gpsu_tx,
		// The ethernet MDIO wires
		o_mdclk, o_mdio, o_mdwe, i_mdio,
		// SPIO interface
		i_sw, i_btnc, i_btnd, i_btnl, i_btnr, i_btnu, o_led);
//
// Any parameter definitions
//
// These are drawn from anything with a MAIN.PARAM definition.
// As they aren't connected to the toplevel at all, it would
// be best to use localparam over parameter, but here we don't
// check
	localparam [31:0] GPSCLOCK_DEFAULT_STEP = 32'haabcc771;
	//
	//
	// Variables/definitions needed by the ZipCPU BUS master
	//
	//
	// A 32-bit address indicating where teh ZipCPU should start running
	// from
	localparam	RESET_ADDRESS = @$RESET_ADDRESS;
	//
	// The number of valid bits on the bus
	localparam	ZIP_ADDRESS_WIDTH = 30;	// Zip-CPU address width
	//
	// Number of ZipCPU interrupts
	localparam	ZIP_INTS = 16;
	//
	// ZIP_START_HALTED
	//
	// A boolean, indicating whether or not the ZipCPU be halted on startup?
	localparam	ZIP_START_HALTED=1'b1;
	localparam	ICAPE_LGDIV=3;
//
// The next step is to declare all of the various ports that were just
// listed above.  
//
// The following declarations are taken from the values of the various
// @MAIN.IODECL keys.
//
	input	wire		i_clk, i_reset;
	// SD-Card declarations
	output	wire		o_sd_sck, o_sd_cmd;
	output	wire	[3:0]	o_sd_data;
	input	wire		i_sd_cmd;
	input	wire	[3:0]	i_sd_data;
	input	wire		i_sd_detect;
	output	wire		o_mic_csn, o_mic_sck;
	input	wire		i_mic_din;
	// HDMI input EDID I2C ports
	input	wire		i_hdmi_out_scl, i_hdmi_out_sda;
	output	wire		o_hdmi_out_scl, o_hdmi_out_sda;
	//The GPS Clock
	input	wire		i_gps_pps;
	// HDMI input ports
	input	wire		i_hdmi_in_clk;
	input	wire	[9:0]	i_hdmi_in_r, i_hdmi_in_g, i_hdmi_in_b;
	input	wire		i_hdmi_in_hsclk, i_clk_200mhz;
	// Sub-pixel delay control
	input	wire	[4:0]	i_hdmi_in_actual_delay_r;
	input	wire	[4:0]	i_hdmi_in_actual_delay_g;
	input	wire	[4:0]	i_hdmi_in_actual_delay_b;
	output	wire	[4:0]	o_hdmi_in_delay;
	// The QSPI flash
	output	wire		o_qspi_cs_n, o_qspi_sck;
	output	wire	[3:0]	o_qspi_dat;
	input	wire	[3:0]	i_qspi_dat;
	output	wire	[1:0]	o_qspi_mod;
	// The PS/2 Mouse
	input		[1:0]	i_ps2;
	output	wire	[1:0]	o_ps2;
	// HDMI input EDID I2C ports
	input	wire		i_hdmi_in_scl, i_hdmi_in_sda;
	output	wire		o_hdmi_in_scl, o_hdmi_in_sda;
	input	wire		i_host_rx_stb;
	input	wire	[7:0]	i_host_rx_data;
	output	wire		o_host_tx_stb;
	output	wire	[7:0]	o_host_tx_data;
	input	wire		i_host_tx_busy;
	input	wire		i_cpu_reset;
	// OLEDBW interface
	output	wire		o_oled_sck, o_oled_mosi,
				o_oled_dcn, o_oled_reset_n, o_oled_panel_en,
				o_oled_logic_en;
	// HDMI output clock
	input	wire	i_hdmi_out_clk;
	// HDMI output pixels
	output	wire	[9:0]	o_hdmi_out_r, o_hdmi_out_g, o_hdmi_out_b;
	input	wire		i_gpsu_rx;
	output	wire		o_gpsu_tx;
	// Ethernet control (MDIO)
	output	wire		o_mdclk, o_mdio, o_mdwe;
	input	wire		i_mdio;
	// SPIO interface
	input	wire	[7:0]	i_sw;
	input	wire		i_btnc, i_btnd, i_btnl, i_btnr, i_btnu;
	output	wire	[7:0]	o_led;


	//
	// Declaring wishbone master bus data
	//
	wire		wb_cyc, wb_stb, wb_we, wb_stall, wb_err;
	reg	wb_ack;	// ACKs delayed by extra clock
	wire	[(30-1):0]	wb_addr;
	wire	[31:0]	wb_data;
	reg	[31:0]	wb_idata;
	wire	[3:0]	wb_sel;




	//
	// Declaring interrupt lines
	//
	// These declarations come from the various components values
	// given under the @INT.<interrupt name>.WIRE key.
	//
	wire	sdcard_int;	// sdcard.INT.SDCARD.WIRE
	wire	rtc_int;	// rtc.INT.RTC.WIRE
	wire	pmic_int;	// pmic.INT.MIC.WIRE
	wire	gpio_int;	// gpio.INT.GPIO.WIRE
	wire	scop_hdmiin_int;	// scope_hdmiin.INT.HINSCOPE.WIRE
	wire	edid_out_int;	// edout.INT.EDID.WIRE
	wire	ck_pps;	// gck.INT.PPS.WIRE
	wire	hdmiin_int;	// hdmiin.INT.VSYNC.WIRE
	wire	flash_interrupt;	// flash.INT.FLASH.WIRE
	wire	mous_interrupt;	// mous.INT.MOUSE.WIRE
	wire	scope_sdcard_int;	// scope_sdcard.INT.SDSCOPE.WIRE
	wire	zip_cpu_int;	// zip.INT.ZIP.WIRE
	wire	oled_int;	// oled.INT.OLED.WIRE
	wire	w_bus_int;	// buspic.INT.BUS.WIRE
	wire	gpsutx_int;	// gpsu.INT.GPSTX.WIRE
	wire	gpsutxf_int;	// gpsu.INT.GPSTXF.WIRE
	wire	gpsurx_int;	// gpsu.INT.GPSRX.WIRE
	wire	gpsurxf_int;	// gpsu.INT.GPSRXF.WIRE
	wire	spio_int;	// spio.INT.SPIO.WIRE
	wire	scop_edid_int;	// scop_edid.INT.SCOPE.WIRE


	//
	// Declaring Bus-Master data, internal wires and registers
	//
	// These declarations come from the various components values
	// given under the @MAIN.DEFNS key, for those components with
	// an MTYPE flag.
	//


	//
	// Declaring Peripheral data, internal wires and registers
	//
	// These declarations come from the various components values
	// given under the @MAIN.DEFNS key, for those components with a
	// PTYPE key but no MTYPE key.
	//


	//
	// Declaring other data, internal wires and registers
	//
	// These declarations come from the various components values
	// given under the @MAIN.DEFNS key, but which have neither PTYPE
	// nor MTYPE keys.
	//
	wire[31:0]	sdspi_debug;
	// Definitions in support of the GPS driven RTC
	wire	rtc_ppd, rtc_pps;
	reg	r_rtc_ack;
`include "builddate.v"
	localparam	NGPI = 16, NGPO=16;
	// GPIO ports
	input		[(NGPI-1):0]	i_gpio;
	output	wire	[(NGPO-1):0]	o_gpio;
	reg	r_sysclk_ack;
	wire	[31:0]	hdmi_in_data;
	reg	r_clkhdmiin_ack;
	wire	[31:0]	edido_dbg;
	reg	[31:0]	r_pwrcount_data;
	wire	gps_pps, gps_led, gps_locked, gps_tracking;
	wire	[63:0]	gps_now, gps_err, gps_step;
	wire	[1:0]	gps_dbg_tick;
	wire	[31:0]	hin_dbg_scope;
	wire	[29:0]	hin_pixels;
	wire	[9:0]	hdmi_in_r;
	wire	[9:0]	hdmi_in_g;
	wire	[9:0]	hdmi_in_b;
	// scrn_mouse is a 32-bit field containing 16-bits of x-position and
	// 16-bits of y position, limited to the size of the screen.
	wire	[31:0]	scrn_mouse;
	wire	[31:0]	edid_dbg;
	wire	scope_sdcard_trigger,
		scope_sdcard_ce;
	// Definitions for the WB-UART converter.  We really only need one
	// (more) non-bus wire--one to use to select if we are interacting
	// with the ZipCPU or not.
	wire		wbu_zip_sel;
	wire	[0:0]	wbubus_dbg;
`ifndef	INCLUDE_ZIPCPU
	wire		zip_dbg_ack, zip_dbg_stall;
	wire	[31:0]	zip_dbg_data;
`endif
	// ZipSystem/ZipCPU connection definitions
	// All we define here is a set of scope wires
	wire	[31:0]	zip_debug;
	wire		zip_trigger;
	wire		zip_dbg_ack, zip_dbg_stall;
	wire	[31:0]	zip_dbg_data;
	wire	[15:0] zip_int_vector;
	// Bus arbiter's lines
	wire		dwb_cyc, dwb_stb, dwb_we, dwb_ack, dwb_stall, dwb_err;
	wire	[(30-1):0]	dwb_addr;
	wire	[31:0]	dwb_odata, dwb_idata;
	wire	[3:0]	dwb_sel;
	wire	w_gpsu_cts_n, w_gpsu_rts_n;
	assign	w_gpsu_cts_n=1'b1;
	wire	tb_pps;
	reg	r_clkhdmiout_ack;
	wire	[4:0]	w_btn;
	wire		edid_scope_trigger;
	wire	[30:0]	edid_scope_data;


	//
	// Declaring interrupt vector wires
	//
	// These declarations come from the various components having
	// PIC and PIC.MAX keys.
	//
	wire	[14:0]	sys_int_vector;
	wire	[14:0]	alt_int_vector;
	wire	[14:0]	bus_int_vector;

	// Declare those signals necessary to build the bus, and detect
	// bus errors upon it.
	//
	wire	none_sel;
	reg	many_sel, many_ack;
	reg	[31:0]	r_bus_err;

	//
	// Wishbone master wire declarations
	//
	// These are given for every configuration file with an @MTYPE
	// tag, and the names are prefixed by whatever is in the @PREFIX tag.
	//


	//
	// Wishbone slave wire declarations
	//
	// These are given for every configuration file with a @PTYPE
	// tag, and the names are given by the @PREFIX tag.
	//


	// Wishbone peripheral address decoding
	// This particular address decoder decodes addresses for all
	// peripherals (components with a @PTYPE tag), based upon their
	// NADDR (number of addresses required) tag
	//

	assign	none_sel = (wb_stb)&&({ } == 0);
	//
	// many_sel
	//
	// This should *never* be true .... unless the address decoding logic
	// is somehow broken.  Given that a computer is generating the
	// addresses, that should never happen.  However, since it has
	// happened to me before (without the computer), I test/check for it
	// here.
	//
	// Devices are placed here as a natural result of the address
	// decoding logic.  Thus, any device with a sel_ line will be
	// tested here.
	//
`ifdef	VERILATOR

	always @(*)
		case({})
			0'h0: many_sel = 1'b0;
			default: many_sel = (wb_stb);
		endcase

`else	// VERILATOR

	always @(*)
		case({})
			0'h0: many_sel <= 1'b0;
			default: many_sel <= (wb_stb);
		endcase

`endif	// VERILATOR

	//
	// many_ack
	//
	// It is also a violation of the bus protocol to produce multiply
	// acks at once and on the same clock.  In that case, the bus
	// can't decide which result to return.  Worse, if someone is waiting
	// for a return value, that value will never come since another ack
	// masked it.
	//
	// The other error that isn't tested for here, no would I necessarily
	// know how to test for it, is when peripherals return values out of
	// order.  Instead, I propose keeping that from happening by
	// guaranteeing, in software, that two peripherals are not accessed
	// immediately one after the other.
	//
	always @(posedge i_clk)
		case({})
			0'h0: many_ack <= 1'b0;
		default: many_ack <= (wb_cyc);
		endcase
	//
	// wb_ack
	//
	// The returning wishbone ack is equal to the OR of every component that
	// might possibly produce an acknowledgement, gated by the CYC line.  To
	// add new components, OR their acknowledgements in here.
	//
	// To return an ack here, a component must have a @PTYPE.  Acks from
	// any @PTYPE SINGLE and DOUBLE components have been collected
	// together into sio_ack and dio_ack respectively, which will appear.
	// ahead of any other device acks.
	//
	always @(posedge i_clk)
		wb_ack <= (wb_cyc)&&(|{});


	assign	wb_stall = 1'b0;


	//
	// wb_err
	//
	// This is the bus error signal.  It should never be true, but practice
	// teaches us otherwise.  Here, we allow for three basic errors:
	//
	// 1. STB is true, but no devices are selected
	//
	//	This is the null pointer reference bug.  If you try to access
	//	something on the bus, at an address with no mapping, the bus
	//	should produce an error--such as if you try to access something
	//	at zero.
	//
	// 2. STB is true, and more than one device is selected
	//
	//	(This can be turned off, if you design this file well.  For
	//	this line to be true means you have a design flaw.)
	//
	// 3. If more than one ACK is every true at any given time.
	//
	//	This is a bug of bus usage, combined with a subtle flaw in the
	//	WB pipeline definition.  You can issue bus requests, one per
	//	clock, and if you cross device boundaries with your requests,
	//	you may have things come back out of order (not detected here)
	//	or colliding on return (detected here).  The solution to this
	//	problem is to make certain that any burst request does not cross
	//	device boundaries.  This is a requirement of whoever (or
	//	whatever) drives the bus.
	//
	assign	wb_err = ((wb_stb)&&(none_sel || many_sel))
				|| ((wb_cyc)&&(many_ack));

	always @(posedge i_clk)
		if (wb_err)
			r_bus_err <= { wb_addr, 2'b00 };

	//Now we turn to defining all of the parts and pieces of what
	// each of the various peripherals does, and what logic it needs.
	//
	// This information comes from the @MAIN.INSERT and @MAIN.ALT tags.
	// If an @ACCESS tag is available, an ifdef is created to handle
	// having the access and not.  If the @ACCESS tag is `defined above
	// then the @MAIN.INSERT code is executed.  If not, the @MAIN.ALT
	// code is exeucted, together with any other cleanup settings that
	// might need to take place--such as returning zeros to the bus,
	// or making sure all of the various interrupt wires are set to
	// zero if the component is not included.
	//
	//
	// Declare the interrupt busses
	//
	assign	sys_int_vector = {
		1'b0,
		1'b0,
		oled_int,
		mous_interrupt,
		ck_pps,
		edid_out_int,
		pmic_int,
		sdcard_int,
		w_bus_int,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0
	};
	assign	alt_int_vector = {
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		gpsurx_int,
		gpio_int,
		rtc_int,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0
	};
	assign	bus_int_vector = {
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		1'b0,
		scop_edid_int,
		spio_int,
		mous_interrupt,
		flash_interrupt,
		scop_hdmiin_int,
		sdcard_int
	};
`ifdef	SDSPI_ACCESS
	// SPI mapping
	wire	w_sd_cs_n, w_sd_mosi, w_sd_miso;

	sdspi	sdcardi(i_clk,
		wb_cyc,
			(wb_stb)&&(sdcard_sel),
			wb_we,
			wb_addr[1:0],
			wb_data,
			sdcard_ack, sdcard_stall, sdcard_data,
		w_sd_cs_n, o_sd_sck, w_sd_mosi, w_sd_miso,
		sdcard_int, 1'b1, sdspi_debug);

	assign	w_sd_miso = i_sd_data[0];
	assign	o_sd_data = { w_sd_cs_n, 3'b111 };
	assign	o_sd_cmd  = w_sd_mosi;
`else	// SDSPI_ACCESS
	assign	o_sd_sck   = 1'b1;
	assign	o_sd_cmd   = 1'b1;
	assign	o_sd_data  = 4'hf;
	assign	sdcard_int = 1'b0;	// sdcard.INT.SDCARD.WIRE
`endif	// SDSPI_ACCESS

`ifdef	RTC_ACCESS
	rtcgps	#(32'h002af31d) thertc(i_clk,
		wb_cyc, (wb_stb)&&(rtc_sel), wb_we, wb_addr[1:0], wb_data,
		rtc_data, rtc_int, rtc_ppd,
		gps_tracking, ck_pps, gps_step[47:16], rtc_pps);
	initial	r_rtc_ack = 1'b0;
	always @(posedge i_clk)
		r_rtc_ack <= (wb_stb)&&(rtc_sel);
	assign	rtc_ack = r_rtc_ack;
`else	// RTC_ACCESS
	assign	rtc_pps = 1'b0;
	assign	rtc_int = 1'b0;	// rtc.INT.RTC.WIRE
`endif	// RTC_ACCESS

	assign	version_data = `DATESTAMP;
	assign	version_ack = 1'b0;
	assign	version_stall = 1'b0;
`ifdef	MICROPHONE_ACCESS
	wbmic #(.DEFAULT_RELOAD(2083))
 		microphone(i_clk, 1'b0,
 			wb_cyc, (wb_stb)&&(pmic_sel), wb_we,
				wb_addr[0], wb_data,
 			pmic_ack, pmic_stall, pmic_data,
			o_mic_csn, o_mic_sck, i_mic_din, pmic_int);
`else	// MICROPHONE_ACCESS
	assign	o_mic_csn    = 1'b1;
	assign	o_mic_sck    = 1'b1;
	assign	pmic_int = 1'b0;	// pmic.INT.MIC.WIRE
`endif	// MICROPHONE_ACCESS

`ifdef	GPIO_ACCESS
	//
	// GPIO
	//
	// Not used (yet), but this interface should allow us to control up to
	// 16 GPIO inputs, and another 16 GPIO outputs.  The interrupt trips
	// when any of the inputs changes.  (Sorry, which input isn't (yet)
	// selectable.)
	//
	localparam	INITIAL_GPIO = 16'h0f;
	wbgpio	#(NGPI, NGPO, INITIAL_GPIO)
		gpioi(i_clk, 1'b1, (wb_stb)&&(gpio_sel), 1'b1,
			wb_data, gpio_data, i_gpio, o_gpio, gpio_int);
`else	// GPIO_ACCESS
	assign	gpio_int = 1'b0;	// gpio.INT.GPIO.WIRE
`endif	// GPIO_ACCESS

	clkcounter clksysclkctr(i_clk, ck_pps, i_clk, sysclk_data);
	always @(posedge i_clk)
		r_sysclk_ack <= (wb_stb)&&(sysclk_sel);
	assign	sysclk_ack   = r_sysclk_ack;
	assign	sysclk_stall = 1'b0;

	wbscope #(.LGMEM(5'd14), .SYNCHRONOUS(0)
		) copyhdmiin(i_hdmi_in_clk, 1'b1, hin_dbg_scope[31], hin_dbg_scope,
		i_clk, wb_cyc, (wb_stb)&&(scope_hdmiin_sel), wb_we, wb_addr[0],
				wb_data,
			scope_hdmiin_ack, scope_hdmiin_stall,
			scope_hdmiin_data,
		scop_hdmiin_int);
	clkcounter clkclkhdmiinctr(i_clk, ck_pps, i_clk, clkhdmiin_data);
	always @(posedge i_clk)
		r_clkhdmiin_ack <= (wb_stb)&&(clkhdmiin_sel);
	assign	clkhdmiin_ack   = r_clkhdmiin_ack;
	assign	clkhdmiin_stall = 1'b0;
`ifdef	HDMI_OUT_EDID_ACCESS
	wbi2cmaster	#(.READ_ONLY(1'b1),.MEM_ADDR_BITS(8)) the_edout(i_clk, 1'b0,
		wb_cyc, (wb_stb)&&(edout_sel), wb_we, wb_addr[6:0], wb_data,
			wb_sel, edout_ack, edout_stall, edout_data,
		i_hdmi_out_scl, i_hdmi_out_sda, o_hdmi_out_scl, o_hdmi_out_sda,
		edid_out_int,
		edido_dbg);
`else	// HDMI_OUT_EDID_ACCESS
	assign	o_hdmi_out_scl = 1'b1;
	assign	o_hdmi_out_sda = 1'b1;
	assign	edid_out_int = 1'b0;	// edout.INT.EDID.WIRE
`endif	// HDMI_OUT_EDID_ACCESS

	assign	o_hdmi_out_r = hdmi_in_r;
	assign	o_hdmi_out_g = hdmi_in_g;
	assign	o_hdmi_out_b = hdmi_in_b;

	initial	r_pwrcount_data = 32'h0;
	always @(posedge i_clk)
	if (r_pwrcount_data[31])
		r_pwrcount_data[30:0] <= r_pwrcount_data[30:0] + 1'b1;
	else
		r_pwrcount_data[31:0] <= r_pwrcount_data[31:0] + 1'b1;
	assign	pwrcount_data = r_pwrcount_data;
`ifdef	GPS_CLOCK
	wire	[1:0]	ck_dbg;

	gpsclock #(.DEFAULT_STEP(GPSCLOCK_DEFAULT_STEP))
		ppsck(i_clk, 1'b0, gps_pps, ck_pps, gps_led,
			(wb_stb)&&(gck_sel), wb_we, wb_addr[1:0], wb_data,
				gck_ack, gck_stall, gck_data,
			gps_tracking, gps_now, gps_step, gps_err, gps_locked,
			ck_dbg);
`else	// GPS_CLOCK
	wire	[31:0]	pre_step;
	assign	pre_step = { 16'h00, (({GPSCLOCK_DEFAULT_STEP[27:0],20'h00})
				>>GPSCLOCK_DEFAULT_STEP[31:28]) };
	always @(posedge i_clk)
		{ ck_pps, gps_step[31:0] } <= gps_step + pre_step;
	assign	gck_stall  = 1'b0;
	assign	gps_now    = 64'h0;
	assign	gps_err    = 64'h0;
	assign	gps_step   = 64'h0;
	assign	gps_led    = 1'b0;
	assign	gps_locked = 1'b0;

	assign	ck_pps = 1'b0;	// gck.INT.PPS.WIRE
`endif	// GPS_CLOCK

`ifdef	HDMIIN_ACCESS
	// HDMI input processor
	hdmiin	thehdmmiin(i_clk, i_hdmi_in_clk, ck_pps,
			//
			i_hdmi_in_actual_delay_r,
			i_hdmi_in_actual_delay_g,
			i_hdmi_in_actual_delay_b,
			o_hdmi_in_delay,
			//
			i_hdmi_in_r, i_hdmi_in_g, i_hdmi_in_b,
			wb_cyc, (wb_stb)&&(hdmiin_sel), wb_we, wb_addr[3:0],
				wb_data, wb_sel,
			hdmiin_ack, hdmiin_stall, hdmiin_data,
			hdmiin_int,
			hin_pixels, hin_dbg_scope);

	assign	hdmi_in_r = hin_pixels[29:20];
	assign	hdmi_in_g = hin_pixels[19:10];
	assign	hdmi_in_b = hin_pixels[ 9: 0];
`else	// HDMIIN_ACCESS
	assign	hdmiin_int = 1'b0;	// hdmiin.INT.VSYNC.WIRE
`endif	// HDMIIN_ACCESS

`ifdef	FLASH_ACCESS
	// The Flash control interface result comes back together with the
	// flash interface itself.  Hence, we always return zero here.
	assign	flctl_ack   = 1'b0;
	assign	flctl_stall = 1'b0;
	assign	flctl_data  = 0;
`else	// FLASH_ACCESS
`endif	// FLASH_ACCESS

`ifdef	FLASH_ACCESS
	wbqspiflash #(24)
		flashmem(i_clk,
			(wb_cyc), (wb_stb)&&(flash_sel), (wb_stb)&&(flctl_sel),wb_we,
			wb_addr[(24-3):0], wb_data,
			flash_ack, flash_stall, flash_data,
			o_qspi_sck, o_qspi_cs_n, o_qspi_mod, o_qspi_dat, i_qspi_dat,
			flash_interrupt);
`else	// FLASH_ACCESS
	assign	o_qspi_sck  = 1'b1;
	assign	o_qspi_cs_n = 1'b1;
	assign	o_qspi_mod  = 2'b01;
	assign	o_qspi_dat  = 4'b1111;
	assign	flash_interrupt = 1'b0;	// flash.INT.FLASH.WIRE
`endif	// FLASH_ACCESS

`ifdef	BKRAM_ACCESS
	memdev #(.LGMEMSZ(20), .EXTRACLOCK(1))
		bkrami(i_clk,
			(wb_cyc), (wb_stb)&&(bkram_sel), wb_we,
				wb_addr[(20-3):0], wb_data, wb_sel,
				bkram_ack, bkram_stall, bkram_data);
`else	// BKRAM_ACCESS
`endif	// BKRAM_ACCESS

`ifdef	MOUSE_ACCESS
	wbmouse themouse(i_clk,
		(wb_cyc), (wb_stb)&&(mous_sel), wb_we, wb_addr[1:0], wb_data,
			mous_ack, mous_stall, mous_data,
		i_ps2, o_ps2,
		scrn_mouse, mous_interrupt);
`else	// MOUSE_ACCESS
	// If there is no mouse, declare mouse types of things to be .. absent
	assign	scrn_mouse     = 32'h00;
	assign	o_ps2          = 2'b11;
	assign	mous_interrupt = 1'b0;	// mous.INT.MOUSE.WIRE
`endif	// MOUSE_ACCESS

`ifdef	HDMI_IN_EDID_ACCESS
	wbi2cslave	#( .INITIAL_MEM("edid.hex"),
		.I2C_READ_ONLY(1'b1),
		.MEM_ADDR_BITS(8))
	    the_input_edid(i_clk, 1'b0,
		wb_cyc, (wb_stb)&&(edin_sel), wb_we, wb_addr[8-3:0], wb_data,
			wb_sel, edin_ack, edin_stall, edin_data,
		i_hdmi_in_scl, i_hdmi_in_sda, o_hdmi_in_scl, o_hdmi_in_sda,
		edid_dbg);
`else	// HDMI_IN_EDID_ACCESS
	assign	o_hdmi_in_scl = 1'b1;
	assign	o_hdmi_in_sda = 1'b1;
`endif	// HDMI_IN_EDID_ACCESS

`ifdef	SDSPI_SCOPE
	assign	scope_sdcard_trigger = (wb_stb)
				&&(sdcard_sel)&&(wb_we);
	assign	scope_sdcard_ce = 1'b1;
	wbscope #(5'h9) sdspiscope(i_clk, scope_sdcard_ce,
			scope_sdcard_trigger,
			sdspi_debug,
			i_clk, wb_cyc,
			(wb_stb)&&(scope_sdcard_sel),
			wb_we,
			wb_addr[0],
			wb_data,
			scope_sdcard_ack,
			scope_sdcard_stall,
			scope_sdcard_data,
			scope_sdcard_int);

`else	// SDSPI_SCOPE
	assign	scope_sdcard_int = 1'b0;	// scope_sdcard.INT.SDSCOPE.WIRE
`endif	// SDSPI_SCOPE

`ifdef	WBUBUS_MASTER
`ifdef	INCLUDE_ZIPCPU
	assign	wbu_zip_sel   = wbu_addr[29];
`else
	assign	wbu_zip_sel   = 1'b0;
	assign	zip_dbg_ack   = 1'b0;
	assign	zip_dbg_stall = 1'b0;
	assign	zip_dbg_data  = 0;
`endif
`ifndef	BUSPIC_ACCESS
	wire	w_bus_int;
	assign	w_bus_int = 1'b0;
`endif
	wire	[31:0]	wbu_tmp_addr;
	wbubus	genbus(i_clk, i_host_rx_stb, i_host_rx_data,
			wbu_cyc, wbu_stb, wbu_we, wbu_tmp_addr, wbu_data,
			(wbu_zip_sel)?zip_dbg_ack:wbu_ack,
			(wbu_zip_sel)?zip_dbg_stall:wbu_stall,
				(wbu_zip_sel)?1'b0:wbu_err,
				(wbu_zip_sel)?zip_dbg_data:wbu_idata,
			w_bus_int,
			o_host_tx_stb, o_host_tx_data, i_host_tx_busy,
			wbubus_dbg[0]);
	assign	wbu_sel = 4'hf;
	assign	wbu_addr = wbu_tmp_addr[(30-1):0];
`else	// WBUBUS_MASTER
`endif	// WBUBUS_MASTER

`ifdef	INCLUDE_ZIPCPU
	//
	//
	// The ZipCPU/ZipSystem BUS master
	//
	//
`ifndef	WBUBUS_MASTER
	wire	wbu_zip_sel;
	assign	wbu_zip_sel = 1'b0;
`endif
	assign	zip_int_vector = { alt_int_vector[14:8], sys_int_vector[14:6] };
	zipsystem #(RESET_ADDRESS,ZIP_ADDRESS_WIDTH,10,ZIP_START_HALTED,ZIP_INTS)
		swic(i_clk, i_cpu_reset,
			// Zippys wishbone interface
			zip_cyc, zip_stb, zip_we, zip_addr, zip_data, zip_sel,
					zip_ack, zip_stall, zip_idata, zip_err,
			zip_int_vector, zip_cpu_int,
			// Debug wishbone interface
			((wbu_cyc)&&(wbu_zip_sel)),
			((wbu_stb)&&(wbu_zip_sel)),wbu_we, wbu_addr[0],
			wbu_data, zip_dbg_ack, zip_dbg_stall, zip_dbg_data,
			zip_debug);
	assign	zip_trigger = zip_debug[0];
`else	// INCLUDE_ZIPCPU
	assign	zip_cpu_int = 1'b0;	// zip.INT.ZIP.WIRE
`endif	// INCLUDE_ZIPCPU

`ifdef	OLEDBW_ACCESS
	wboledbw #(.CBITS(4)) oledctrl(i_clk,
		(wb_cyc), (wb_stb)&&(oled_sel), wb_we,
				wb_addr[1:0], wb_data,
			oled_ack, oled_stall, oled_data,
		o_oled_sck, o_oled_mosi, o_oled_dcn,
		{ o_oled_reset_n, o_oled_panel_en, o_oled_logic_en },
		oled_int);
`else	// OLEDBW_ACCESS
	assign	o_oled_sck     = 1'b1;
	assign	o_oled_mosi    = 1'b1;
	assign	o_oled_dcn     = 1'b1;
	assign	o_oled_reset_n = 1'b0;
	assign	o_oled_panel_en= 1'b0;
	assign	o_oled_logic_en= 1'b0;

	assign	oled_int = 1'b0;	// oled.INT.OLED.WIRE
`endif	// OLEDBW_ACCESS

`ifdef	CFG_ACCESS
	wire[31:0]	cfg_debug;
`ifdef	VERILATOR
	reg	r_cfg_ack;
	always @(posedge i_clk)
		r_cfg_ack <= (wb_stb)&&(cfg_sel);
	assign	cfg_stall = 1'b0;
	assign	cfg_data  = 32'h00;
`else
	wbicapetwo #(ICAPE_LGDIV)
		cfgport(i_clk, wb_cyc, (wb_stb)&&(cfg_sel), wb_we,
			wb_addr[4:0], wb_data,
			cfg_ack, cfg_stall, cfg_data);
`endif
`else	// CFG_ACCESS
`endif	// CFG_ACCESS

`ifdef	RTCDATE_ACCESS
	//
	// The Calendar DATE
	//
	rtcdate	thedate(i_clk, rtc_ppd,
		(wb_stb)&&(date_sel), wb_we, wb_data,
			date_ack, date_stall, date_data);
`else	// RTCDATE_ACCESS
`endif	// RTCDATE_ACCESS

	assign	buserr_data = r_bus_err;
`ifdef	INCLUDE_ZIPCPU
	//
	//
	// And an arbiter to decide who gets access to the bus
	//
	//
	wbpriarbiter #(32,30)	bus_arbiter(i_clk,
		// The Zip CPU bus master --- gets the priority slot
		zip_cyc, zip_stb, zip_we, zip_addr, zip_data, zip_sel,
			zip_ack, zip_stall, zip_err,
		// The UART interface master
		(wbu_cyc)&&(!wbu_zip_sel), (wbu_stb)&&(!wbu_zip_sel), wbu_we,
			wbu_addr[(30-1):0], wbu_data, wbu_sel,
			wbu_ack, wbu_stall, wbu_err,
		// Common bus returns
		dwb_cyc, dwb_stb, dwb_we, dwb_addr, dwb_odata, dwb_sel,
			dwb_ack, dwb_stall, dwb_err);

	// And because the ZipCPU and the Arbiter can create an unacceptable
	// delay, we often fail timing.  So, we add in a delay cycle
`else
	// If no ZipCPU, no delay arbiter is needed
	assign	dwb_cyc  = wbu_cyc;
	assign	dwb_stb  = wbu_stb;
	assign	dwb_we   = wbu_we;
	assign	dwb_addr = wbu_addr;
	assign	dwb_odata = wbu_data;
	assign	dwb_sel  = wbu_sel;
	assign	wbu_ack   = dwb_ack;
	assign	wbu_stall = dwb_stall;
	assign	wbu_err   = dwb_err;
	// assign wbu_idata = dwb_idata;
`endif	// INCLUDE_ZIPCPU

`ifdef	WBUBUS_MASTER
`ifdef	INCLUDE_ZIPCPU
`define	BUS_DELAY_NEEDED
`endif
`endif
`ifdef	BUS_DELAY_NEEDED
	busdelay #(30)	dwb_delay(i_clk,
		dwb_cyc, dwb_stb, dwb_we, dwb_addr, dwb_odata, dwb_sel,
			dwb_ack, dwb_stall, dwb_idata, dwb_err,
		wb_cyc, wb_stb, wb_we, wb_addr, wb_data, wb_sel,
			wb_ack, wb_stall, wb_idata, wb_err);
`else
	// If one of the two, the ZipCPU or the WBUBUS, isn't here, then we
	// don't need the bus delay, and we can go directly from the bus driver
	// to the bus itself
	//
	assign	wb_cyc    = dwb_cyc;
	assign	wb_stb    = dwb_stb;
	assign	wb_we     = dwb_we;
	assign	wb_addr   = dwb_addr;
	assign	wb_data   = dwb_odata;
	assign	wb_sel    = dwb_sel;
	assign	dwb_ack   = wb_ack;
	assign	dwb_stall = wb_stall;
	assign	dwb_err   = wb_err;
	assign	dwb_idata = wb_idata;
`endif
	assign	wbu_idata = dwb_idata;
`i