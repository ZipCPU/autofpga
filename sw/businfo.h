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
// Copyright (C) 2017-2020, Gisselquist Technology, LLC
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

class	BUSINFO;
class	GENBUS;

#include "parser.h"
#include "mapdhash.h"
#include "plist.h"
#include "mlist.h"
#include "clockinfo.h"
#include "keys.h"

#include "genbus.h"

class	BUSINFO {
	GENBUS	*m_genbus;	// Bus logic generator
	int	m_nullsz;
	STRINGP	m_name;
public:

	// List of peripherals on the bus
	PLISTP    m_plist;
	// List of bus masters
	MLISTP		m_mlist;
	//
	STRINGP   m_type;
	CLOCKINFO *m_clock; // , *m_bhash;
	MAPDHASH	*m_hash;
	int	m_data_width, m_address_width;
	bool	m_addresses_assigned;

	BUSINFO(void) { // MAPDHASH *hash)
		// m_bhash  = hash;
		m_plist  = NULL;
		m_mlist  = NULL;
		m_name   = NULL;
		m_type   = NULL;
		m_clock  = NULL;
		m_hash   = NULL;
		m_genbus = NULL;
		m_nullsz = 0;
		m_data_width = 0;
		m_addresses_assigned = false;
		m_address_width = 0;
	}

	BUSINFO(STRINGP name) {
		m_plist  = NULL;
		m_mlist  = NULL;
		m_name   = name;
		m_type   = NULL;
		m_clock  = NULL;
		m_hash   = new MAPDHASH();
		m_genbus = NULL;
		m_nullsz = 0;
		m_data_width = 0;
		m_addresses_assigned = false;
		m_address_width = 0;

		setstring(m_hash, KY_NAME, name);
	}

	bool	need_translator(BUSINFO *b);

	bool	get_base_address(MAPDHASH *phash, unsigned &base);
	bool	word_addressing(void);
	void	assign_addresses(void);
	int	address_width(void);
	int	data_width(void);
	void	add(void);
	PERIPH *add(PERIPHP p);
	PERIPH *add(MAPDHASH *phash);
	void	addmaster(MAPDHASH *hash) {
		if (!m_mlist)
			m_mlist = new MLIST();
		BMASTERP bp = new BMASTER(hash);
		m_mlist->push_back(bp);
	}

	void	writeout_bus_slave_defns_v(FILE *fp);
	void	writeout_bus_master_defns_v(FILE *fp);
	void	writeout_bus_defns_v(FILE *fp);
	void	writeout_bus_logic_v(FILE *fp);

	void	writeout_no_slave_v(FILE *fp, STRINGP prefix);
	void	writeout_no_master_v(FILE *fp);

	PERIPHP operator[](unsigned i);
	unsigned size(void);
	void	init(MAPDHASH *phash);
	void	init(STRINGP bname);
	void	merge(STRINGP component, MAPDHASH *hash);
	void	integrity_check(void);
	bool	ismember_of(MAPDHASH *phash);
	STRINGP	name(void);
	STRINGP	prefix(STRINGP p = NULL);
	STRINGP	btype(void);
	STRINGP	reset_wire(void);
	STRINGP	slave_iansi(PERIPHP);
	STRINGP	slave_oansi(PERIPHP);
	STRINGP	slave_ansprefix(PERIPHP);
	STRINGP	slave_portlist(PERIPHP);
	STRINGP	slave_ansi_portlist(PERIPHP);
	STRINGP	master_portlist(BMASTERP);
	STRINGP	master_ansi_portlist(BMASTERP);
	GENBUS	*generator(void);	// Bus logic generator
};

class	BUSLIST : public std::vector<BUSINFO *>	{
public:
	BUSINFO *find_bus(STRINGP name);
	BUSINFO *find_bus(MAPDHASH *hash);
	unsigned	get_base_address(MAPDHASH *phash);
	void	addperipheral(MAPDHASH *phash);
	void	addperipheral(MAPT &map);
	void	addmaster(MAPDHASH *phash);
	void	addmaster(MAPT &map);

	void	adddefault(MAPDHASH &master, STRINGP defname);

	BUSINFO *newbus_aux(STRINGP cname, MAPDHASH *bp);
	BUSINFO *newbus_aux(STRINGP cname, STRINGP bn);
	void	addbus(STRINGP cname, STRINGP busname);
	void	addbus(STRINGP cname, MAPDHASH *phash);
	void	addbus(STRINGP cname, MAPT &map);
	// void	countsio(MAPDHASH *phash);
	// void	countsio(MAPT &map);
	BUSINFO *find_bus_of_peripheral(MAPDHASH *phash);
	void	assign_bus_types(void);
	void	checkforbusdefns(STRINGP, MAPDHASH *, const STRING &);
};

extern	void	build_bus_list(MAPDHASH &master);
extern	BUSINFO *find_bus_of_peripheral(MAPDHASH *phash);
extern	BUSINFO *find_bus(MAPDHASH *hash);
extern	BUSINFO *find_bus(STRINGP name);
extern	bool	need_translator(BUSINFO *a, BUSINFO *b);
extern	void	writeout_bus_defns_v(FILE *fp);
extern	void	writeout_bus_select_v(FILE *fp);
extern	void	writeout_bus_logic_v(FILE *fp);

#endif	// BUSINFO_H
