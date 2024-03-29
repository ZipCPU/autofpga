////////////////////////////////////////////////////////////////////////////////
//
// Filename:	../demo-out/regdefs.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// DO NOT EDIT THIS FILE!
// Computer Generated: This file is computer generated by AUTOFPGA. DO NOT EDIT.
// DO NOT EDIT THIS FILE!
//
// CmdLine:	./autofpga -d -o ../demo-out -I ../auto-data bkram.txt buserr.txt clkcounter.txt clock.txt enet.txt flash.txt global.txt gpio.txt gps.txt hdmi.txt icape.txt legalgen.txt mdio.txt pic.txt pwrcount.txt rtcdate.txt rtcgps.txt sdram.txt sdspi.txt spio.txt version.txt wbmouse.txt wboledbw.txt wbpmic.txt wbscopc.txt wbscope.txt wbubus.txt xpander.txt zipmaster.txt
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
#ifndef	REGDEFS_H
#define	REGDEFS_H


//
// The @REGDEFS.H.INCLUDE tag
//
// @REGDEFS.H.INCLUDE for masters
// @REGDEFS.H.INCLUDE for peripherals
// And finally any master REGDEFS.H.INCLUDE tags
// End of definitions from REGDEFS.H.INCLUDE


//
// Register address definitions, from @REGS.#d
//


//
// The @REGDEFS.H.DEFNS tag
//
// @REGDEFS.H.DEFNS for masters
#define	CLKFREQHZ	100000000
#define	R_ZIPCTRL	0x00000008
#define	R_ZIPDATA	0x0000000c
SIM.INIT=
		m_core->i_host_uart_rx = 1;
SIM.CLOCK=clk
SIM.TICK=
		m_core->i_host_uart_rx = (*m_dbgbus)(m_core->o_host_uart_tx);
// @REGDEFS.H.DEFNS for peripherals
#define	FLASHBASE	@$[0x%08x](REGBASE)
#define	FLASHLEN	0x01000000
#define	FLASHLGLEN	24
//
#define	FLASH_RDDELAY	1
#define	FLASH_NDUMMY	6
//
#define	BKRAMBASE	@$[0x%08x](REGBASE)
#define	BKRAMLEN	0x00100000
#define	SDRAMBASE	@$[0x%08x](REGBASE)
#define	SDRAMLEN	0x20000000
// @REGDEFS.H.DEFNS at the top level
// End of definitions from REGDEFS.H.DEFNS
//
// The @REGDEFS.H.INSERT tag
//
// @REGDEFS.H.INSERT for masters
// @REGDEFS.H.INSERT for peripherals

#define	CPU_GO		0x0000
#define	CPU_RESET	0x0040
#define	CPU_INT		0x0080
#define	CPU_STEP	0x0100
#define	CPU_STALL	0x0200
#define	CPU_HALT	0x0400
#define	CPU_CLRCACHE	0x0800
#define	CPU_sR0		0x0000
#define	CPU_sSP		0x000d
#define	CPU_sCC		0x000e
#define	CPU_sPC		0x000f
#define	CPU_uR0		0x0010
#define	CPU_uSP		0x001d
#define	CPU_uCC		0x001e
#define	CPU_uPC		0x001f

#ifdef	BKROM_ACCESS
#define	RESET_ADDRESS	@$[0x%08x](bkrom.REGBASE)
#else
#ifdef	FLASH_ACCESS
#define	RESET_ADDRESS	@$[0x%08x](RESET_ADDRESS)
#else
#define	RESET_ADDRESS	@$[0x%08x](bkram.REGBASE)
#endif	// FLASH_ACCESS
#endif	// BKROM_ACCESS


// Flash control constants
#define	QSPI_FLASH	// This core and hardware support a Quad SPI flash
#define	SZPAGEB		256
#define	PGLENB		256
#define	SZPAGEW		64
#define	PGLENW		64
#define	NPAGES		256
#define	SECTORSZB	(NPAGES * SZPAGEB)	// In bytes, not words!!
#define	SECTORSZW	(NPAGES * SZPAGEW)	// In words
#define	NSECTORS	64
#define	SECTOROF(A)	((A) & (-1<<16))
#define	SUBSECTOROF(A)	((A) & (-1<<12))
#define	PAGEOF(A)	((A) & (-1<<8))

// Network packet interface
#define	ENET_TXGO		0x004000
#define	ENET_TXBUSY		0x004000
#define	ENET_NOHWCRC		0x008000
#define	ENET_NOHWMAC		0x010000
#define	ENET_RESET		0x020000
#define	ENET_NOHWIPCHK		0x040000
#define	ENET_TXCMD(LEN)		((LEN)|ENET_TXGO)
#define	ENET_TXCLR		0x038000
#define	ENET_TXCANCEL		0x000000
#define	ENET_RXAVAIL		0x004000
#define	ENET_RXBUSY		0x008000
#define	ENET_RXMISS		0x010000
#define	ENET_RXERR		0x020000
#define	ENET_RXCRC		0x040000	// Set on a CRC error
#define	ENET_RXLEN		rxcmd & 0x0ffff
#define	ENET_RXCLR		0x004000
#define	ENET_RXBROADCAST	0x080000
#define	ENET_RXCLRERR		0x078000

// @REGDEFS.H.INSERT from the top level
typedef	struct {
	unsigned	m_addr;
	const char	*m_name;
} REGNAME;

extern	const	REGNAME	*bregs;
extern	const	int	NREGS;
// #define	NREGS	(sizeof(bregs)/sizeof(bregs[0]))

extern	unsigned	addrdecode(const char *v);
extern	const	char *addrname(const unsigned v);
// End of definitions from REGDEFS.H.INSERT


#endif	// REGDEFS_H
