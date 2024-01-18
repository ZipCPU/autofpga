////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	predicates.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:
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
////////////////////////////////////////////////////////////////////////////////
//
// }}}
#ifndef	PREDICATES_H
#define	PREDICATES_H

#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#include "parser.h"

//
// Is the given location within our hash a master?  Look it up.
//
// To be a bus master, it must have a @MASTER.TYPE field.
//
bool	isbusmaster(MAPDHASH &phash);
//
// Does the given location describe access to a bus lying beneath it?
//
// To be true, isbusmaster() must be true, and the MASTER.BUS.TYPE field
// must be one of: SUBBUS, XCLOCK, ARBITER, etc.
//
bool	issubbus(MAPDHASH &phash);
bool	isarbiter(MAPDHASH &phash);
//
// Same thing, but when given a location within the tree, rather than a hash
// value.
bool	isbusmaster(MAPT &pmap);

//
// Is the given hash a definition of a peripheral
//
// To be a peripheral, it must have a @SLAVE.TYPE field.
//
bool	isperipheral(MAPDHASH &phash);
bool	isperipheral(MAPT &pmap);

//
// Does the given hash define a programmable interrupt controller?
//
// If so, it must have a @PIC.MAX field identifying the maximum number of
// interrupts that can be assigned to it.
bool	ispic(MAPDHASH &phash);
bool	ispic(MAPT &pmap);

// Does this reference a memory peripheral?
bool	ismemory(MAPDHASH &phash);
bool	ismemory(MAPT &pmap);

// Does component reference a bus?
bool	refbus(MAPDHASH &phash);
bool	refbus(MAPT &pmap);

// Does component reference a clock?
bool	refclock(MAPDHASH &phash);
bool	refclock(MAPT &pmap);

// Does the toplevel map contain a CPU?
bool	has_cpu(MAPDHASH &phash);

// Does the option set describe a read-only peripheral
bool	read_only_option(STRINGP op);
// bool	read_only_option(MAPDHASH &phash);
// Does the option set describe a write-only peripheral
bool	write_only_option(STRINGP op);
// bool	write_only_option(MAPDHASH &phash);

#endif	// PREDICATES_H
