////////////////////////////////////////////////////////////////////////////////
//
// Filename:	sw/genbus.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
// {{{
// Purpose:	Defines a generic bus interface which, if implemented, should
//		allow support for any bus type desired.
//
// Translators:
//	To facilitate bus translation, every bus type is defined uniquely by
//	the buses data width, type, and clock.  Data types will (eventually)
//	include: AXI, AXI-lite (or AXIL), WB, and WBC (or WB-classic).
//
//	To avoid unnecessary translation, address width properties will
//	propagate up, ID width will propagate down.  All AXI slaves are required
//	to support an arbitrary number of AXI ID's.
//
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2024, Gisselquist Technology, LLC
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
//
////////////////////////////////////////////////////////////////////////////////
//
// }}}
#ifndef	GENBUS_H
#define	GENBUS_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>

#include "parser.h"
#include "mapdhash.h"
#include "plist.h"
#include "clockinfo.h"
#include "businfo.h"

class	GENBUS {
public:
	BUSINFO	*m_info;	// Generic bus information
	// MLISTP	m_mlist;	// List of bus masters
	// PLISTP	m_plist;	// List of peripheral/slaves

	// GENBUS(BUSINFO *bi);
	// ~GENBUS() {};

	virtual	int	address_width(void) = 0;
	virtual	bool	word_addressing(void) = 0;
	//
	virtual	void	assign_addresses(void) = 0;
	virtual	bool	get_base_address(MAPDHASH *phash, unsigned &base) = 0;
	// virtual	unsigned size(void) { return (m_info && m_info->m_plist) ? m_info->m_plist->size() : 0; };

	// virtual	void	writeout_slave_defn_v(FILE *fp, const char *name, const char *errwire = NULL, const char *btyp="") = 0;
	virtual	void	writeout_bus_slave_defns_v(FILE *fp) = 0;
	virtual	void	writeout_bus_master_defns_v(FILE *fp) = 0;
	virtual	void	writeout_bus_defns_v(FILE *fp) {
		writeout_bus_master_defns_v(fp);
		writeout_bus_slave_defns_v(fp);
	}

	virtual	void	writeout_bus_logic_v(FILE *fp) = 0;

	virtual	void	writeout_no_slave_v(FILE *fp, STRINGP prefix) = 0;
	virtual	void	writeout_no_master_v(FILE *fp) = 0;


	virtual	STRINGP	iansi(BMASTERP m) { return NULL; }
	virtual	STRINGP	oansi(BMASTERP m) { return NULL; }
	virtual	STRINGP	master_ansprefix(BMASTERP m) { return NULL; }
	virtual	STRINGP	master_portlist(BMASTERP m) { return NULL; }
	virtual	STRINGP	master_ansi_portlist(BMASTERP m) { return NULL; }
	virtual	STRINGP	slave_ansprefix(PERIPHP p) { return NULL; }
	virtual	STRINGP	slave_portlist(PERIPHP p) { return NULL; }
	virtual	STRINGP	slave_ansi_portlist(PERIPHP p) { return NULL; }
		STRINGP	name(void);
	unsigned max_name_width(PLIST *pl);
	void	slave_addr(FILE *fp, PLIST *pl, const int addr_lsbs = 0);
	void	slave_mask(FILE *fp, PLIST *pl, const int addr_lsbs = 0);
	virtual	void	integrity_check(void) {};
	bool	bus_option(const STRING &str);
	void	xbar_option(FILE *fp, const STRING&,
			const char *,const char *d=NULL);
};

class	BUSCLASS {
public:
	virtual	STRINGP	name(void) = 0;	// i.e. WB
	virtual	STRINGP	longname(void) = 0;	// i.e. "Wishbone"
	virtual	bool	matchtype(STRINGP) = 0;
	virtual	bool	matchfail(MAPDHASH *bhash) { return false; };
	virtual	bool	matches(BUSINFO *bi);
	virtual	GENBUS	*create(BUSINFO *bi) = 0;
};

#define	NUM_BUS_CLASSES	3
extern	unsigned	num_bus_classes;
extern	BUSCLASS	*busclass_list[NUM_BUS_CLASSES];
#endif	// GENBUS_H
