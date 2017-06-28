////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	subbus.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	
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
#ifndef	SUBBUS_H
#define	SUBBUS_H

#include "plist.h"
#include "businfo.h"

class	SUBBUS : public PERIPH {
public:
	SUBBUS(MAPDHASH *info, STRINGP subname, BUSINFO *subbus);
	virtual	bool	isbus(void);
	virtual	unsigned	get_slave_address_width(void);
	void	add(PERIPHP p) {
		p_master_bus->add(p);
	}
	void	add(MAPDHASH *phash) {
		p_master_bus->add(phash);
	}

	unsigned get_base_address(MAPDHASH *phash);

	void	assign_addresses(void);
	bool	need_translator(void);
};

#endif
