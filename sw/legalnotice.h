////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	legalnotice.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	
//
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2021, Gisselquist Technology, LLC
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
#ifndef	LEGALNOTICE_H
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "mapdhash.h"
#include "parser.h"

// Insert a given legal notice into the FILE stream
//
// 1. We'll adjust the projuct line for the current project
// 2. We'll also add a notice that the file so created was computer generated,
//	and thus should not be edited
// 3. This notice will include our command line as well
//
extern	void	legal_notice(MAPDHASH &info, FILE *fp, STRING &fname,
			const char *cline = NULL,
			const char *comment = "//");
#endif
