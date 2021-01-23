////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	gather.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To gather all peripherals together, using a given bus to do so
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
#ifndef	GATHER_H
#define	GATHER_H

#include <vector>

#include "plist.h"
#include "businfo.h"

typedef	std::vector<PERIPHP>	APLIST;

extern 	void	gather_peripherals(APLIST *alist, BUSINFO *bus, PLIST *plist, unsigned base=0);

extern 	APLIST	*gather_peripherals(BUSINFO *bus);

extern	APLIST	*full_gather(void);

#endif
