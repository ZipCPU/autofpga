////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	clockinfo.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:
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
#ifndef	CLOCKINFO_H
#define	CLOCKINFO_H

#include "parser.h"
#include "mapdhash.h"


class	CLOCKINFO {
public:
	MAPDHASH	*m_hash;
	unsigned long	m_interval_ps;
	STRINGP	m_name, m_wire;
	static const unsigned long	UNKNOWN_PS,
			PICOSECONDS_PER_SECOND;

	CLOCKINFO(void);

	unsigned long	interval_ps(void) {
		return m_interval_ps;
	}

	unsigned long	setfrequency(const char *sfreq_hz) {
		return setfrequency(strtoul(sfreq_hz, NULL, 0));
	}

	unsigned long	setfrequency(unsigned long frequency_hz);
	void	setname(STRINGP name);
	void	setwire(STRINGP wire);

	void set(STRINGP name, STRINGP wire, unsigned long frequency_hz) {
		setname(name);
		setwire(wire);
		setfrequency(frequency_hz);
	}

	void set(STRINGP name, STRINGP wire, char *sfreq) {
		setname(name);
		setwire(wire);
		setfrequency(sfreq);
	}
};
typedef	std::vector<CLOCKINFO>	CLKLIST;


// A list of all of the clocks found within this design
extern	CLKLIST	cklist;

// Search through the master hash looking for all of the clocks within it.
extern	CLOCKINFO	*getclockinfo(STRING &clock_name);
extern	CLOCKINFO	*getclockinfo(STRINGP clock_name);

// Search through the master hash looking for all of the clocks within it.
extern	void	find_clocks(MAPDHASH &master);

#endif	// CLOCKINFO_H

