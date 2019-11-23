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
#include "bitlib.h"
#include "msgs.h"
#include "bus/wb.h"
#include "bus/axil.h"
#include "bus/axi.h"
#include "keys.h"
#include "mapdhash.h"

WBBUSCLASS	wbclass;
AXILBUSCLASS	axilclass;
AXIBUSCLASS	axiclass;

BUSCLASS	*busclass_list[NUM_BUS_CLASSES] = {
		&wbclass, &axilclass, &axiclass };
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

void	GENBUS::xbar_option(FILE *fp, const STRING &key, const char *pat, const char *def) {
	const char *loc = strchr(pat, '%');
	STRINGP		str;
	int		val, prech;

	assert(loc != NULL);
	prech = loc-pat;

	if (!bus_option(key)) {
		if (NULL != def)
			fprintf(fp, "%.*s%s%s", prech, pat, def, loc+1);
		return;
	}

	if (getvalue(*m_info->m_hash, key, val)) {
		fprintf(fp, "%.*s%d%s", prech, pat, val, loc+1);
		return;
	}

	if (NULL != (str = getstring(*m_info->m_hash, key))) {
		fprintf(fp, "%.*s%s%s", prech, pat, str->c_str(), loc+1);
		return;
	}

	if (NULL != def) {
		fprintf(fp, "%.*s%s%s", prech, pat, def, loc+1);
		return;
	}
}

STRINGP	GENBUS::name(void) {
	if (m_info)
		return m_info->name();
	return NULL;
}

void	GENBUS::slave_addr(FILE *fp, PLIST *pl, const int addr_lsbs) {
	// int		lgdw = nextlg(m_info->data_width())-3;
	unsigned	slave_name_width = 0;

	for(unsigned k=pl->size()-1; k>0; k=k-1) {
		PERIPHP	p = (*pl)[k];
		unsigned	sz;

		sz = p->name()->size();
		if (slave_name_width < sz)
			slave_name_width = sz;
	}

	fprintf(fp,
		"\t\t.SLAVE_ADDR({\n");
	for(unsigned k=pl->size()-1; k>0; k=k-1) {
		PERIPHP	p = (*pl)[k];

		fprintf(fp, "\t\t\t{ %d\'h%0*lx },\n",
			address_width()-addr_lsbs,
			(address_width()-addr_lsbs+3)/4,
			((*pl)[k]->p_base)>>addr_lsbs);
		fprintf(fp, " // %*s: 0x%0*lx\n", slave_name_width,
			p->name()->c_str(),
			(address_width()+3)/4, p->p_base);

	} fprintf(fp, "\t\t\t{ %d\'h%0*lx }  // %*s: 0x%0*lx\n",
			address_width()-addr_lsbs,
			(address_width()-addr_lsbs+3)/4,
			((*pl)[0]->p_base)>>addr_lsbs,
			slave_name_width,
			(*pl)[0]->name()->c_str(),
			(address_width()+3)/4,
			(*pl)[0]->p_base);
	fprintf(fp, "\t\t}");
}

void	GENBUS::slave_mask(FILE *fp, PLIST *pl, const int addr_lsbs) {
	int		lgdw = nextlg(m_info->data_width())-3;
	unsigned	slave_name_width = 0;

	for(unsigned k=pl->size()-1; k>0; k=k-1) {
		PERIPHP	p = (*pl)[k];
		unsigned	sz;

		sz = p->name()->size();
		if (slave_name_width < sz)
			slave_name_width = sz;
	}

	fprintf(fp, "\t\t.SLAVE_MASK({\n");
	for(unsigned k=pl->size()-1; k>0; k=k-1) {
		PERIPHP	p = (*pl)[k];

		fprintf(fp, "\t\t\t{ %d\'h%0*lx }, // %*s\n",
			address_width()-addr_lsbs,
			(address_width()-addr_lsbs+3)/4,
			p->p_mask << (addr_lsbs-lgdw),
			slave_name_width, p->name()->c_str());
	} fprintf(fp, "\t\t\t{ %d\'h%0*lx }  // %*s\n",
			address_width()-addr_lsbs,
			(address_width()-addr_lsbs+3)/4,
			((*pl)[0]->p_mask << (lgdw - addr_lsbs)),
			slave_name_width, (*pl)[0]->name()->c_str());
	fprintf(fp, "\t\t})");
}

bool	BUSCLASS::matches(BUSINFO *bi) {
	MAPDHASH *bhash = bi->m_hash;
	STRINGP	btype;

	// Check any slaves of this bus to see if we can
	// handle them
	btype = getstring(bhash, KY_TYPE);
	return matchtype(btype);
}
