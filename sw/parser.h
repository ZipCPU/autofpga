////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	parser.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To define the processes used to parse a raw input file.  This
//		does not include the code necessary to evaluate any strings
//	within that input file, only the code to turn the input file into
//	KEY-VALUE pairs. used by a parsed input file.  
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
#ifndef	PARSER_H
#define	PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mapdhash.h"

extern	STRING	*rawline(FILE *fp);
extern	STRING	*getline(FILE *fp);
extern	MAPDHASH	*parsefile(FILE *fp, const STRING &search="");
extern	MAPDHASH	*parsefile(const char *fname, const STRING &search="");
extern	MAPDHASH	*parsefile(const STRING &fname, const STRING &search="");

#endif
