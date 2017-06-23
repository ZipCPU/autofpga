////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	businfo.h
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
#ifndef	BUSINFO_H
#define	BUSINFO_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>

#include "parser.h"
#include "mapdhash.h"
#include "plist.h"

class	BUSINFO {
	bool	m_addresses_assigned;
public:
	PLISTP    m_plist;
	STRINGP   m_name, m_prefix, m_type;
	MAPDHASH *m_clock; // , *m_bhash;
	int	m_data_width, m_address_width, m_nullsz;
	bool	m_has_slist, m_has_dlist;

	BUSINFO(void) { // MAPDHASH *hash) {
		// m_bhash  = hash;
		m_plist  = NULL;
		m_name   = NULL;
		m_prefix = NULL;
		m_type   = NULL;
		m_clock  = NULL;
		m_nullsz = 0;
		m_data_width = 0;
		m_addresses_assigned = false;
		m_address_width = 0;
	}

	bool	get_base_address(MAPDHASH *phash, unsigned &base);
	void	assign_addresses(void);
	int	address_width(void);
	unsigned	min_addr_size(unsigned np, unsigned mina);
	PERIPH *add(PERIPHP p) {
		int	pi;

		if (!m_plist)
			m_plist = new PLIST();
		pi = m_plist->add(p);
		p = (*m_plist)[pi];
		p->p_slave_bus = this;

		return p;
	}
	PERIPH *add(MAPDHASH *phash) {
		PERIPH	*p;
		int	pi;

		if (!m_plist)
			m_plist = new PLIST();
		pi = m_plist->add(phash);
		p = (*m_plist)[pi];
		p->p_slave_bus = this;

		return p;
	}
	bool	need_translator(BUSINFO *b);
};

class	BUSLIST : public std::vector<BUSINFO *>	{
public:
	BUSINFO *find_bus(STRINGP name);
	unsigned	get_base_address(MAPDHASH *phash);
	void	addperipheral(MAPDHASH *phash);
	void	addperipheral(MAPT &map);

	void	adddefault(STRINGP defname);

	void	addbus(MAPDHASH *phash);
	void	addbus(MAPT &map);
};

extern	void	build_bus_list(MAPDHASH &master);
extern	void	mkselect2(FILE *, MAPDHASH &master);
extern	BUSINFO *find_bus(STRINGP name);
extern	bool	need_translator(BUSINFO *a, BUSINFO *b);

#endif	// BUSINFO_H
