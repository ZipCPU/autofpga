////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	plist.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A PLIST is a list (C++ vector) of bus slaves, herein called
//		peripherals.  This file defines the methods associated with
//	such a list.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017-2021, Gisselquist Technology, LLC
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
#ifndef	PLIST_H
#define	PLIST_H

#include <vector>

#include "parser.h"

class	BUSINFO;

//
// The PLIST, made of PERIPH(erals)
//
// A structure to contain information about lists of peripherals
//
class	PERIPH {
public:
	unsigned long	p_base, p_regbase;	// In octets
	unsigned long	p_naddr;	// In words, given in file
	//
	// p_awid = number of address lines for this slave
	//	In word addressing, this is log_2(p_naddr)
	//	Otherwise, its defined as log_2(p_naddr * bus->data_width()/8)
	//   This is on the *slave* bus.  For subbus's, it may not be the same
	//	as the width on the *master* bus.
	unsigned	p_awid;		//
	unsigned long	p_mask;		// Words.  Bit is true if relevant for address selection
	//
	unsigned	p_sbaw;
	STRINGP		p_name;
	MAPDHASH	*p_phash;
	BUSINFO		*p_slave_bus;	// We are a slave of this bus
	BUSINFO		*p_master_bus;	// We might master this bus beneath us

	STRINGP	name(void) { return p_name; };
	virtual	bool	issingle(void);
	virtual	bool	isdouble(void);
	virtual	bool	isbus(void);
	virtual	bool	ismemory(void);
	virtual	bool	read_only(void);
	virtual	bool	write_only(void);
	virtual unsigned get_slave_address_width(void);
		unsigned awid(void) { return get_slave_address_width(); }
		unsigned naddr(void);
	virtual	void	integrity_check(void);
	STRINGP	bus_prefix(void);
};

typedef	PERIPH *PERIPHP;

class	PLIST : public std::vector<PERIPHP> {
	unsigned	m_address_width;
public:
	PLIST(void) {}
	STRINGP		m_stype;

	void	set_stype(STRING &stype);
	//
	// Add a peripheral to a given list of peripherals
	int	add(MAPDHASH *phash);
	int	add(PERIPHP p);
	void	assign_addresses(unsigned dwidth, unsigned nullsz = 0,
				unsigned bus_min_address_width = 0);
	bool	get_base_address(MAPDHASH *phash, unsigned &base);
	unsigned	min_addr_size_bytes(unsigned, unsigned,
				unsigned nullsz=0);
	unsigned	min_addr_size_octets(unsigned, unsigned,
				unsigned nullsz=0,
				unsigned daddr=2);
	unsigned get_address_width(void) {
		return m_address_width;
	}
	void	integrity_check(void);
	/*
	void	remove(std::vector<PERIPHP>::iterator ptr) {
		std::vector<PERIPHP>::remove(ptr);
	}
	*/
};

// A pointer to a set of peripherals
typedef	PLIST *PLISTP;

//
// Count the number of peripherals that are children of this top level hash,
// and count them by type as well.
//
extern	int count_peripherals(MAPDHASH &info);

//
// compare_naddr
//
// This is part of the peripheral sorting mechanism, whereby peripherals are
// sorted by the numbers of addresses they use.  Peripherals using fewer
// addresses are placed first, with peripherals using more addresses placed
// later.
//
extern	bool	compare_naddr(PERIPHP a, PERIPHP b);

//
// compare_address
//
// This is another means of sorting peripherals, this time by their address
// within their bus.
//
extern	bool	compare_address(PERIPHP a, PERIPHP b);

//
// compare_regaddr
//
// When examining registers that are accessed across busses, it becomes
// important to be able to sort peripherals from the point of view of a master
// that might be able to reach them.  This is the purpose of the REGBASE
// address.  This compare sorts with respect to that address
//
extern	bool	compare_regaddr(PERIPHP a, PERIPHP b);

/*
 * build_plist
 *
 * Collect our peripherals into one of four lists.  This allows us to have
 * ordered access through the peripherals later.
 */
extern	void	build_plist(MAPDHASH &info);

#endif	// PLIST_H
