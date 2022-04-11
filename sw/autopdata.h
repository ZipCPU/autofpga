////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	autopdata.h
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
// Copyright (C) 2017-2022, Gisselquist Technology, LLC
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
#ifndef	AUTOPDATA_H
#define	AUTOPDATA_H

typedef	struct	REGINFO_S {
	int		offset;
	const char	*defname;
	const char	*namelist;
} REGINFO;

typedef	struct	AUTOPDATA_S {
	const char	*prefix;
	int	naddr;
	const char	*access;
	const char	*ext_ports, *ext_decls;
	const char	*main_defns;
	const char	*dbg_defns;
	const char	*main_insert;
	const char	*alt_insert;
	const char	*dbg_insert;
	REGINFO		*pregs;
	const char	*cstruct, *ioname;
} AUTOPDATA;

#endif
