////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldtestb.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To build a test framework that can be used with Verilator for
//		handling multiple clocks.
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
#ifndef	BLDTESTB_H
#define	BLDTESTB_H

#include "parser.h"
#include "mapdhash.h"


class	CLKITEM {
	unsigned long	m_interval_ps;
public:
	STRINGP	m_name, m_wire;

	CLKITEM(void) {
		m_name = NULL;
		m_wire = NULL;
		m_interval_ps = 2ul;
	}

	unsigned long	interval_ps(void) {
		return m_interval_ps;
	}

	void set(STRINGP name, STRINGP wire, unsigned long ps) {
		m_name = name;
		m_wire = wire;
		m_interval_ps = ps;
	}
};
typedef	std::vector<CLKITEM>	CLKLIST;


// A list of all of the clocks found within this design
extern	CLKLIST	cklist;

// Search through the master hash looking for all of the clocks within it.
extern	void	find_clocks(MAPDHASH &master);

// Build the test bench sim driver helper, that handles clocks and clock ticks.
extern	void	build_testb_h(MAPDHASH &master, FILE *fp, STRING &fname);

#endif	// BLDTESTB_H

