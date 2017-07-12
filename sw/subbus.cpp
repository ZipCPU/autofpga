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
fprintf(gbl_dump, "SUBBUS::GET-SLAVE-ADDRESS-WIDTH: master (%s), width (%d)\n",
		((!p_master_bus)||(!p_master_bus->m_name))?"(Null)":p_master_bus->m_name->c_str(),
		p_awid);
	if (p_master_bus->data_width() != p_slave_bus->data_width()) {
		p_awid += nextlg(p_master_bus->data_width()/8);
		p_awid -= nextlg(p_slave_bus->data_width()/8);
	}
	p_naddr = (1u<<p_awid);

fprintf(gbl_dump, "SUBBUS::GET-SLAVE-ADDRESS-WIDTH: << %d >> %d\n",
		nextlg(p_master_bus->data_width()/8),
		nextlg(p_slave_bus->data_width()/8));
fprintf(gbl_dump, "SUBBUS::GET-SLAVE-ADDRESS-WIDTH: -----> %d\n", p_awid);

	return p_awid;
}

unsigned SUBBUS::get_base_address(MAPDHASH *phash) {
	unsigned base;

fprintf(gbl_dump, "SUBBUS::GET-SLAVE-ADDRESS: ");

	assert(p_master_bus);
	assert(p_master_bus != p_slave_bus);

	if (p_master_bus->get_base_address(phash, base)) {
		base += p_base;
fprintf(gbl_dump, "%08x (true)\n", base);
		return true;
	}
fprintf(gbl_dump, "%08x (false)\n", base);
	return false;
}

void	SUBBUS::assign_addresses(void) {
	assert(p_master_bus);
	assert(p_master_bus != p_slave_bus);
	fprintf(gbl_dump, "SUBBUS::Assign-addresses\n");
	p_master_bus->assign_addresses();
	fprintf(gbl_dump, "SUBBUS::Assign-addresses ---- done\n");
}

bool	SUBBUS::need_translator(void) {
	assert(p_slave_bus);
	assert(p_master_bus);
	return p_slave_bus->need_translator(p_master_bus);
}
