////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	genbus.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Defines a generic bus class, from which other buses may derive.
//		More importantly, describes a way that different potential
//	bus classes/implementations may be searched for an appropriate one.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2019, Gisselquist Technology, LLC
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
#include "genbus.h"
#include "msgs.h"
#include "bus/wb.h"
#include "bus/axil.h"
#include "keys.h"
#include "mapdhash.h"

WBBUSCLASS	wbclass;
AXILBUSCLASS	axilclass;

BUSCLASS	*busclass_list[NUM_BUS_CLASSES] = { &wbclass, &axilclass };
unsigned	num_bus_classes = NUM_BUS_CLASSES;

/*
void	GENBUS::integrity_check(void) {
	if (!m_info) {
		gbl_msg.error("Bus logic generator with no bus info structure\n");
		return;
	}

	if (!m_info->m_plist)
		gbl_msg.error("Bus logic generator with no peripherals attached\n");

	if (!m_info->m_name)
		gbl_msg.error("Bus has no name!\n");

	if (!m_info->m_clock)
		gbl_msg.error("Bus has no assigned clock!\n");
}
*/

bool	GENBUS::bus_option(const STRING &ky) {
	MAPDHASH	*option;

	option = getmap(m_info->m_hash, ky);
	return	(option != NULL);
}

STRINGP	GENBUS::name(void) {
	if (m_info)
		return m_info->name();
	return NULL;
}

bool	BUSCLASS::matches(BUSINFO *bi) {
	MAPDHASH *bhash = bi->m_hash;
	STRINGP	btype;

	// Check any slaves of this bus to see if we can
	// handle them
	btype = getstring(bhash, KY_TYPE);
	return matchtype(btype);
}
