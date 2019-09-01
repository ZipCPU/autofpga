////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	subbus.cpp
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
// Copyright (C) 2017-2019, Gisselquist Technology, LLC
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

#include "plist.h"
#include "businfo.h"
#include "subbus.h"
#include "bitlib.h"
#include "globals.h"

SUBBUS::SUBBUS(MAPDHASH *info, STRINGP subname, BUSINFO *subbus) {
	p_base = 0;
	p_naddr= 0;
	p_awid = 0;
	p_mask = 0;

	p_sbaw  = 0;
	p_name  = subname;
	p_phash = info;
	p_master_bus = subbus;
}

bool	SUBBUS::isbus(void) { return true; }
unsigned	SUBBUS::get_slave_address_width(void) {
	assert(p_master_bus);
	assert(p_master_bus != p_slave_bus);

	p_awid = p_master_bus->address_width();
	if (p_master_bus->data_width() != p_slave_bus->data_width()) {
		p_awid += nextlg(p_master_bus->data_width()/8);
		p_awid -= nextlg(p_slave_bus->data_width()/8);
	}
	p_naddr = (1u<<p_awid);

	return p_awid;
}

/*
bool SUBBUS::get_base_address(MAPDHASH *phash, unsigned &base) {
	assert(p_master_bus);
	assert(p_master_bus != p_slave_bus);

	if (p_slave_bus->get_base_address(phash, base)) {
		// If the bus we are mastering has an offset, add that to our
		// base address
		base += p_base;
		return true;
	}

	return false;
}
*/

void	SUBBUS::assign_addresses(void) {
	assert(p_master_bus);
	assert(p_master_bus != p_slave_bus);

	// Assign addresses to the bus that we master from here
	p_master_bus->assign_addresses();
}

bool	SUBBUS::need_translator(void) {
	assert(p_slave_bus);
	assert(p_master_bus);
	return p_slave_bus->need_translator(p_master_bus);
}
