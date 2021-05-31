////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	subbus.cpp
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A subbus component is a special type of bus slave that is also
//		a bus master on another bus at the same time.  The sub bus
//	that the peripheral is a master of then determines the address range of
//	the peripheral on the bus that it is a slave of.
//
//	Classic examples of subbus's would be bridges.  DMA components are not
//	subbus's, but rather masters and slaves in their own right.
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

#include "plist.h"
#include "businfo.h"
#include "subbus.h"
#include "bitlib.h"
#include "globals.h"
#include "msgs.h"

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
	unsigned	awid;

	assert(p_master_bus);
	if (p_master_bus == p_slave_bus)
		gbl_msg.fatal("ERR: Component %s cannot be both slave to bus %s and master of it as a subbus\n",
			(p_name->c_str()) ? p_name->c_str() : "(Unknown?!?!)",
			(p_master_bus->name()->c_str()) ?
				 p_master_bus->name()->c_str() : "(Un-named)",
			(p_slave_bus->name()->c_str())
				? p_slave_bus->name()->c_str() : "(Un-named)");
	assert(p_master_bus != p_slave_bus);

	// Calculate the address width for bytes
	awid = p_master_bus->address_width();
	if (p_master_bus->word_addressing())
		awid += nextlg(p_master_bus->data_width()/8);

	p_naddr = (1u<<(awid-nextlg(p_slave_bus->data_width()/8)));

	// Adjust this to be address width for words --- if required
	if (p_slave_bus->word_addressing())
		awid -= nextlg(p_slave_bus->data_width()/8);

	if (p_awid != awid) {
		p_awid = awid;
		setvalue(*p_phash, KYSLAVE_AWID, awid);
	}

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
