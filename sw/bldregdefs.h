////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldregdefs.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To build the regdefs.h and regdefs.cpp files.
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
#ifndef	BLDREGDEFS_H
#define	BLDREGDEFS_H

#include <stdio.h>

#include "mapdhash.h"
#include "plist.h"

extern	int	get_longest_defname(PLIST &plist);

//
// write_regdefs
//
// This writes out the definitions of all the registers found within the plist
// to the C++ header file given by fp.  It takes as a parameter the longest
// name definition, which we use to try to line things up in a prettier fashion
// than we could without it.
//
extern	void write_regdefs(FILE *fp, PLIST &plist, unsigned longest_defname);

//
// build_regdefs_h
//
//
// This builds a regdefs.h file, a file that can be used on a host in order
// to access our design.
//
extern	void	build_regdefs_h(  MAPDHASH &master, FILE *fp, STRING &fname);

//
// get_longest_uname
//
// This is very similar to the get longest defname (get length of the longest
// variable definition name) above, save that this is applied to the user
// names within regdefs.cpp
//
extern	unsigned	get_longest_uname(PLIST &plist);

//
// write_regnames
//
//
extern	void write_regnames(FILE *fp, PLIST &plist,
		unsigned longest_defname, unsigned longest_uname);


//
// build_regdefs_cpp
//
extern	void	build_regdefs_cpp(MAPDHASH &master, FILE *fp, STRING &fname);

#endif	// BLDREGDEFS_H
