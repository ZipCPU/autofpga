////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	ifdefs.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Components we have can contain an ACCESS keyword that can then
//		be used by logic to determine if the component exists or not.
//
//	These ACCESS lines may also depend upon the ACCESS lines of other
//	components.
//
//	main.v		Gets all of our work.  The ACCESS lines are used at
//			the top of this component to turn things on (or off)
//
//	toplevel.v	Doesn't get touched.  If you include your component
//			in the autofpga list, it will define external ports
//			for it, independent of the ACCESS lines.
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
#ifndef	IFDEFS_H
#define	IFDEFS_H
#include <stdio.h>
#include "mapdhash.h"

extern	void	build_access_ifdefs_v(MAPDHASH &master, FILE *fp);
#endif
