////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldsim.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To build the main_tb.cpp file.
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
#ifndef	BLDSIM_H
#define	BLDSIM_H

#include <stdio.h>
#include <string>

#include "parser.h"

extern	int	gbl_err;
extern	FILE	*gbl_dump;

extern	void	writeout(FILE *fp, MAPDHASH &master, const STRING &ky);
extern	bool	tb_same_clock(MAPDHASH &info, STRINGP ckname);
extern	bool	tb_tick(MAPDHASH &info, STRINGP ckname, FILE *fp);
extern	void	tb_dbg_condition(MAPDHASH &info, STRINGP ckname, FILE *fp);
extern	void	tb_debug(MAPDHASH &info, STRINGP ckname, FILE *fp);
extern	void	build_main_tb_cpp(MAPDHASH &master, FILE *fp, STRING &fname) ;

#endif
