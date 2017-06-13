////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	plist.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A PLIST is a list of peripherals.  This file defines the methods
//		associated with such a list.
//
//
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
#ifndef	PLIST_H
#define	PLIST_H

#include <vector>

#include "parser.h"

//
// The PLIST, made of PERIPH(erals)
//
// A structure to contain information about lists of peripherals
//
typedef	struct PERIPH_S {
	unsigned	p_base;		// In octets
	unsigned	p_naddr;	// In words, given in file
	unsigned	p_awid;		// Log_2 (octets)
	unsigned	p_mask;		// Words.  Bit is true if relevant for address selection
	//
	unsigned	p_skipaddr;
	unsigned	p_skipmask;
	unsigned	p_sadrmask;
	unsigned	p_skipnbits;
	//
	unsigned	p_sbaw;
	STRINGP		p_name;
	MAPDHASH	*p_phash;
} PERIPH, *PERIPHP;
typedef	std::vector<PERIPHP>	PLIST;

//
// Define some global variables, such as a couple different lists of various
// peripheral sets
extern	PLIST	plist, slist, dlist, mlist;

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

extern	bool	compare_address(PERIPHP a, PERIPHP b);

//
// Add a peripheral to a given list of peripherals
extern	int	addto_plist(PLIST &plist, MAPDHASH *phash);
extern	int	addto_plist(PLIST &plist, PERIPHP p);

/*
 * build_skiplist
 *
 * Prior to any address decoding, we can build a skip list.  This collects
 * all of the relevant bits necessary for peripheral address decoding among
 * all of the peripherals within the list.  The result is that address decoding
 * can take place with fewer bits that need to be checked.  The other result,
 * though, is that a peripheral may have many locations in the memory space.
 */
extern	void	buildskip_plist(PLIST &plist, unsigned nulladdr);

/*
 * build_plist
 *
 * Collect our peripherals into one of four lists.  This allows us to have
 * ordered access through the peripherals later.
 */
extern	void	build_plist(MAPDHASH &info);

#endif	// PLIST_H
