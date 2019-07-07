////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	wb.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:
// Tags used:
//
//	BUS.WIDTH
//	BUS.AWID
//
//	SLAVE.TYPE:
//	- SINGLE
//	- DOUBLE
//	- OTHER/MEMORY/BUS
//	SLAVE.OPTIONS=
//	// string of case-insensitive options
//		ROM	(slave has no write ports)
//		WOM	(slave has no read ports)
//		FULL	(slave has no all ports, not just the ports used)
//	SLAVE.SHARE=
//	// slave shares parts of the interface with the other listed slaves
//
//	MASTER.TYPE=
//	(currently ignored)
//
//	INTERCONNECT.TYPE
//	- SINGLE
//	- CROSSBAR
//
//	INTERCONNECT.MASTERS
//	= list of the PREFIXs of all of the masters of this bus
//
//	INTERCONNECT.OPTIONS
//	= string of case-insensitive options
//	OPT_STARVATION_TIMEOUT
//	OPT_LOWPOWER
//	OPT_DBLBUFFER
//
//
//	INTERCONNECT.OPTVAL=
//	= Options that require values
//	- OPT_TIMEOUT
//	- LGMAXBURST
//
// Creates tags:
//
//	BLIST:	A list of bus interconnect parameters, somewhat like:
//			@$(SLAVE.BUS)_cyc,
//			@$(SLAVE.BUS)_stb,
//	(if !ROM&!WOM)	@$(SLAVE.BUS)_we,
//	(if AWID>0)	@$(SLAVE.BUS)_addr[@$(SLAVE.AWID)-1:0],
//	(if !ROM)	@$(SLAVE.BUS)_data,
//	(if !ROM)	@$(SLAVE.BUS)_sel,
//			@$(PREFIX)_stall,
//			@$(PREFIX)_ack,
//	(if !WOM)	@$(PREFIX)_data,
//	(and possibly)	@$(PREFIX)_err
//
//	ASCBLIST.PREFIX: [%io] prefix
//	ASCBLIST.SUFFIX:
//	ASCBLIST.CAPS:
//	ASCBLIST.DATA:
//		.@$(ASCBLIST.PREFIX)cyc@$(ASCBLIST.SUFFIX)(@$SLAVE.BUS)_cyc),
//		.@$(ASCBLIST.PREFIX)stb@$(ASCBLIST.SUFFIX)(@$SLAVE.BUS)_stb),
//		.@$(ASCBLIST.PREFIX)we@$(ASCBLIST.SUFFIX)(@$SLAVE.BUS)_we),
//			.
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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "../parser.h"
#include "../mapdhash.h"
#include "../keys.h"
#include "../kveval.h"
#include "../legalnotice.h"
#include "../bldtestb.h"
#include "../bitlib.h"
#include "../plist.h"
#include "../bldregdefs.h"
#include "../ifdefs.h"
#include "../bldsim.h"
#include "../predicates.h"
#include "../businfo.h"
#include "../globals.h"
#include "../subbus.h"
#include "../msgs.h"
#include "../genbus.h"

#include "wb.h"

#define	PREFIX

extern	WBBUSCLASS	wbclass;

WBBUS::WBBUS(BUSINFO *bi) {
	m_info = bi;
	PLIST	*pl = m_info->m_plist;
	m_plist = pl;
	m_slist = NULL;
	m_dlist = NULL;

	m_is_single = false;
	m_is_double = false;

	BUSINFO	*sbi = NULL, *dbi = NULL;

	countsio();

	if (m_num_single <= 2) {
		if (m_num_double > 0) {
			m_num_double += m_num_single;
			m_num_single = 0;
		} else if (m_num_total > m_num_single) {
			m_num_single = 0;
		}
	} else if (m_num_single == m_num_total) {
		m_num_single = 0;
		m_is_single = true;
	}

	if (m_num_double <= 2)
		m_num_double = 0;
	else if (m_num_double == m_num_total) {
		m_num_double = 0;
		m_is_double  = true;
	} else if (m_num_single + m_num_double == m_num_total) {
		m_is_double = true;
		m_num_double = 0;
	}

	assert(m_num_single < 50);
	assert(m_num_double < 50);
	assert(m_num_total >= m_num_single + m_num_double);

	printf("%d singls and %d doubles identified of %d total\n", m_num_single, m_num_double, m_num_total);
	if (m_is_single)
		printf("This is a single only bus\n");
	if (m_is_double)
		printf("This is a double only bus\n");
	if (!m_is_single && !m_is_double)
		printf("This bus is heterogeneous\n");

	//
	// Master of multiple classes
	//
	if (!m_is_single && m_num_single > 0) {
		sbi = create_sio();
		m_num_total++;
	}

	if (!m_is_double && m_num_double > 0) {
		m_num_total++;
		dbi = create_dio();
	}

	for(unsigned pi = 0; pi< pl->size(); pi++) {
		PERIPHP	p = (*pl)[pi];
		STRINGP	ptyp;

		ptyp = getstring(p->p_phash, KYSLAVE_TYPE);
printf("SLAVE-TYPE: %s\n", (ptyp) ? ptyp->c_str() : "(Null)");
		if (m_slist && ptyp != NULL
				&& ptyp->compare(KYSINGLE) == 0) {
			m_info->m_plist->erase(pl->begin()+pi);
			pi--;
			m_slist->add(p);
			printf("Adjusted slist, plist has %d and slist has %d now\n", (int)pl->size(), (int)m_slist->size());
		} else if (m_dlist && ptyp != NULL
						&& ptyp->compare(KYDOUBLE) == 0) {
			m_info->m_plist->erase(pl->begin()+pi);
			pi--;
			m_dlist->add(p);
			printf("Adjusted dlist, plist has %d and dlist has %d now\n", (int)pl->size(), (int)m_dlist->size());
		} else {
			// Leave this peripheral in m_info->m_plist
		}
	}

	countsio();
	printf("After reallocation, %d, %d, %d\n", m_num_single, m_num_double, m_num_total-m_num_single-m_num_double);

	if (sbi)
		sbi->m_genbus = wbclass.create(sbi);
	if (dbi)
		dbi->m_genbus = wbclass.create(dbi);
}

int	WBBUS::address_width(void) {
	assert(m_info);
	return m_info->m_address_width;
}

bool	WBBUS::get_base_address(MAPDHASH *phash, unsigned &base) {
	if (!m_info || !m_info->m_plist) {
		gbl_msg.error("BUS[%s] has no peripherals!\n",
			(m_info) ? m_info->m_name->c_str() : "(No name)");
		return false;
	} else
		return m_info->m_plist->get_base_address(phash, base);
}

void	WBBUS::assign_addresses(void) {
	int	address_width;
	if (!m_info)
		return;
	gbl_msg.info("Assigning addresses for bus %s\n",
		(m_info) ? m_info->m_name->c_str() : "(No name bus)");
	if (!m_info->m_plist||(m_info->m_plist->size() < 1)) {
		m_info->m_address_width = 0;
	} else if (!m_info->m_addresses_assigned) {
		int	dw = m_info->data_width();

		if (m_slist)
			m_slist->assign_addresses(dw, 0);

		if (m_dlist)
			m_dlist->assign_addresses(dw, 0);

		m_plist->assign_addresses(dw, m_info->m_nullsz);
		address_width = m_plist->get_address_width();
		m_info->m_address_width = address_width;
		if (m_info->m_hash) {
			setvalue(*m_info->m_hash, KY_AWID, m_info->m_address_width);
			REHASH;
		}
	} m_info->m_addresses_assigned = true;
}

BUSINFO *WBBUS::create_sio(void) {
	assert(m_info);

	BUSINFO	*sbi;
	SUBBUS	*subp;
	STRINGP	name;
	MAPDHASH *bushash;

	name = new STRING(STRING("" PREFIX) + (*m_info->m_name) + "_sio");
	bushash = new MAPDHASH();
	setstring(*bushash, KYPREFIX, name);
	setstring(*bushash, KYSLAVE_TYPE, new STRING(KYDOUBLE));
	sbi  = new BUSINFO(name);
	sbi->m_prefix = new STRING("_sio");;
	sbi->m_data_width = m_info->m_data_width;
	sbi->m_clock      = m_info->m_clock;
	subp = new SUBBUS(bushash, name, sbi);
	subp->p_slave_bus = m_info;
	// subp->p_master_bus = set by the slave to be sbi
	m_info->m_plist->add(subp);
	// m_plist->integrity_check();
	sbi->add();
	m_slist = sbi->m_plist;

	return sbi;
}

BUSINFO *WBBUS::create_dio(void) {
	assert(m_info);

	BUSINFO	*dbi;
	SUBBUS	*subp;
	STRINGP	name;
	MAPDHASH	*bushash;

	name = new STRING(STRING("" PREFIX) + (*m_info->m_name) + "_dio");
	bushash = new MAPDHASH();
	setstring(*bushash, KYPREFIX, name);
	setstring(*bushash, KYSLAVE_TYPE, new STRING(KYOTHER));
	dbi  = new BUSINFO(name);
	dbi->m_prefix = new STRING("_dio");;
	dbi->m_data_width = m_info->m_data_width;
	dbi->m_clock      = m_info->m_clock;
	subp = new SUBBUS(bushash, name, dbi);
	subp->p_slave_bus = m_info;
	// subp->p_master_bus = set by the slave to be dbi
	m_info->m_plist->add(subp);
	dbi->add();
	m_dlist = dbi->m_plist;

	return dbi;
}

void	WBBUS::countsio(void) {
	PLIST	*pl = m_info->m_plist;
	STRINGP	strp;

	for(unsigned pi=0; pi< pl->size(); pi++) {
		strp = getstring((*pl)[pi]->p_phash, KYSLAVE_TYPE);
		if (NULL != strp) {
			if (0==strp->compare(KYSINGLE)) {
				m_num_single++;
			} else if (0==strp->compare(KYDOUBLE)) {
				m_num_double++;
			} m_num_total++;
		} else
			m_num_total++;	// Default to OTHER if no type is given
	}
}

void	WBBUS::integrity_check(void) {
	// GENBUS::integrity_check();

	if (m_info && m_info->m_data_width <= 0) {
		gbl_msg.error("ERR: BUS width not defined for %s\n",
			m_info->m_name->c_str());
	}
}

void	WBBUS::writeout_slave_defn_v(FILE *fp, const char* pname,
		const char *errwire, const char *btyp) {
	STRINGP	n = m_info->m_name;

	fprintf(fp, "\t// Wishbone slave definitions for bus %s%s, slave %s\n",
		n->c_str(), btyp, pname);
	fprintf(fp, "\twire\t\t%s_sel, %s_ack, %s_stall",
			pname, pname, pname);
	if ((errwire)&&(errwire[0] != '\0'))
		fprintf(fp, ", %s;\n", errwire);
	else
		fprintf(fp, ";\n");
	fprintf(fp, "\twire\t[%d:0]\t%s_data;\n\n", m_info->data_width()-1, pname);
}

void	WBBUS::writeout_bus_slave_defns_v(FILE *fp) {
	PLIST	*p = m_info->m_plist;
	STRINGP	n = m_info->m_name;

	if (m_slist) {
		for(PLIST::iterator pp=m_slist->begin();
				pp != m_slist->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			writeout_slave_defn_v(fp, (*pp)->p_name->c_str(),
				(errwire)?errwire->c_str(): NULL,
				"(SIO)");
		}
	}

	if (m_dlist) {
		for(PLIST::iterator pp=m_dlist->begin();
				pp != m_dlist->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			writeout_slave_defn_v(fp, (*pp)->p_name->c_str(),
				(errwire)?errwire->c_str(): NULL,
				"(DIO)");
		}
	}

	if (p) {
		for(PLIST::iterator pp=p->begin(); pp != p->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			writeout_slave_defn_v(fp, (*pp)->p_name->c_str(),
				(errwire) ? errwire->c_str() : NULL);
		}
	} else {
		gbl_msg.error("%s has no slaves\n", n->c_str());
	}
}

void	WBBUS::writeout_bus_master_defns_v(FILE *fp) {
	STRINGP	n = m_info->m_name;
	unsigned aw = address_width();
	fprintf(fp, "\t// Wishbone master wire definitions for bus: %s\n",
		n->c_str());
	fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we, %s_stall, %s_err,\n"
			"\t\t\t%s_none_sel;\n"
			"\treg\t\t%s_many_ack;\n"
			"\twire\t[%d:0]\t%s_addr;\n"
			"\twire\t[%d:0]\t%s_data;\n"
			"\treg\t[%d:0]\t%s_idata;\n"
			"\twire\t[%d:0]\t%s_sel;\n"
			"\treg\t\t%s_ack;\n\n",
			n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str(),
			n->c_str(),
			aw-1,
			n->c_str(),
			m_info->data_width()-1, n->c_str(),
			m_info->data_width()-1, n->c_str(),
			(m_info->data_width()/8)-1,
			n->c_str(), n->c_str());
}

void	WBBUS::write_addr_range(FILE *fp, const PERIPHP p, const int dalines) {
	unsigned	w = address_width();
	w = (w+3)/4;
	if (p->p_naddr == 1)
		fprintf(fp, " // 0x%0*lx", w, p->p_base);
	else
		fprintf(fp, " // 0x%0*lx - 0x%0*lx", w, p->p_base,
			w, p->p_base + (p->p_naddr << (dalines))-1);
}
void	WBBUS::writeout_bus_select_v(FILE *fp) {
	STRINGP	n = m_info->m_name;
	STRING	addrbus = STRING((*n)+"_addr");
	unsigned	sbaw = address_width();
	unsigned	dw   = m_info->data_width();
	unsigned	dalines = nextlg(dw/8);
	unsigned	unused_lsbs = 0, mask = 0;

	PLIST	*p = m_info->m_plist;

	if (NULL == p) {
		gbl_msg.error("Bus[%s] has no peripherals\n", n->c_str());
		return;
	}

	fprintf(fp, "\t//\n\t//\n\t//\n\t// Select lines for bus: %s\n\t//\n", n->c_str());
	fprintf(fp, "\t// Address width: %d\n",
		p->get_address_width());
	fprintf(fp, "\t// Data width:    %d\n\t//\n\t//\n\t\n",
		m_info->data_width());

	if (m_slist) {
		sbaw = m_slist->get_address_width();
		mask = 0; unused_lsbs = 0;
		for(unsigned i=0; i<m_slist->size(); i++)
			mask |= (*m_slist)[i]->p_mask;
		for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
			unused_lsbs++;
		for(unsigned i=0; i<m_slist->size(); i++) {
			if ((*m_slist)[i]->p_mask == 0) {
				fprintf(fp, "\tassign\t%12s_sel = 1\'b0; // ERR: (SIO) address mask == 0\n",
					(*m_slist)[i]->p_name->c_str());
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%12s_sel = ((" PREFIX "%s_sio_sel)&&(%s_addr[%2d:%2d] == %2d\'h%0*lx)); ",
					(*m_slist)[i]->p_name->c_str(), n->c_str(),
					n->c_str(),
					sbaw-1, unused_lsbs,
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_slist)[i]->p_base >> (unused_lsbs+dalines));
					write_addr_range(fp, (*m_slist)[i], dalines);
					fprintf(fp, "\n");
			}
		}

	}

	if (m_dlist) {
		sbaw = m_dlist->get_address_width();
		mask = 0; unused_lsbs = 0;
		for(unsigned i=0; i<m_dlist->size(); i++)
			mask |= (*m_dlist)[i]->p_mask;
		for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
			unused_lsbs++;
		for(unsigned i=0; i<m_dlist->size(); i++) {
			if ((*m_dlist)[i]->p_mask == 0) {
				fprintf(fp, "\tassign\t%12s_sel = 1\'b0; // ERR: (DIO) address mask == 0\n",
					(*m_dlist)[i]->p_name->c_str());
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%12s_sel "
					"= ((" PREFIX "%s_dio_sel)&&((%s[%2d:%2d] & %2d\'h%lx) == %2d\'h%0*lx)); ",
				(*m_dlist)[i]->p_name->c_str(),
				n->c_str(),
				addrbus.c_str(),
				sbaw-1,
				unused_lsbs,
				sbaw-unused_lsbs, (*m_dlist)[i]->p_mask >> unused_lsbs,
				sbaw-unused_lsbs,
				(sbaw-unused_lsbs+3)/4,
				(*m_dlist)[i]->p_base>>(dalines+unused_lsbs));
				write_addr_range(fp, (*m_dlist)[i], dalines);
				fprintf(fp, "\n");
			}
		}
	}

	sbaw = address_width();
	mask = 0; unused_lsbs = 0;
	for(unsigned i=0; i< p->size(); i++)
		mask |= (*p)[i]->p_mask;
	for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
		unused_lsbs++;

	if (m_plist->size() == 1) {
		if ((*m_plist)[0]->p_name)
			fprintf(fp, "\tassign\t%12s_sel = (%s_cyc); "
					"// Only one peripheral on this bus\n",
				(*m_plist)[0]->p_name->c_str(),
				n->c_str());
	} else for(unsigned i=0; i< m_plist->size(); i++) {
		if ((*m_plist)[i]->p_name) {
			if ((*m_plist)[i]->p_mask == 0) {
				fprintf(fp, "\tassign\t%12s_sel = 1\'b0; // ERR: address mask == 0\n",
					(*m_plist)[i]->p_name->c_str());
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%12s_sel "
					"= ((%s[%2d:%2d] & %2d\'h%lx) == %2d\'h%0*lx);",
					(*m_plist)[i]->p_name->c_str(),
					addrbus.c_str(),
					sbaw-1,
					unused_lsbs,
					sbaw-unused_lsbs, (*m_plist)[i]->p_mask >> unused_lsbs,
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_plist)[i]->p_base>>(dalines+unused_lsbs));
				write_addr_range(fp, (*m_plist)[i], dalines);
				fprintf(fp, "\n");
			}
		} if ((*m_plist)[i]->p_master_bus) {
			fprintf(fp, "//x2\tWas a master bus as well\n");
		}
	} fprintf(fp, "\t//\n\n");

}

void	WBBUS::writeout_no_slave_v(FILE *fp, STRINGP prefix) {
	STRINGP	n = m_info->m_name;

	fprintf(fp, "\n");
	fprintf(fp, "\t// In the case that there is no %s peripheral responding on the %s bus\n", prefix->c_str(), n->c_str());
	fprintf(fp, "\tassign\t%s_ack   = (%s_stb) && (%s_sel);\n",
			prefix->c_str(), n->c_str(), prefix->c_str());
	fprintf(fp, "\tassign\t%s_stall = 0;\n", prefix->c_str());
	fprintf(fp, "\tassign\t%s_data  = 0;\n", prefix->c_str());
	fprintf(fp, "\n");
}

void	WBBUS::writeout_no_master_v(FILE *fp) {
	if (!m_info || !m_info->m_name)
		gbl_msg.error("(Unnamed bus) has no name!\n");

	// PLIST	*p = m_info->m_plist;
	STRINGP	n = m_info->m_name;

	fprintf(fp, "\n");
	fprintf(fp, "\t// In the case that nothing drives the %s bus ...\n", n->c_str());
	fprintf(fp, "\tassign\t%s_cyc = 1\'b0;\n", n->c_str());
	fprintf(fp, "\tassign\t%s_stb = 1\'b0;\n", n->c_str());
	fprintf(fp, "\tassign\t%s_we  = 1\'b0;\n", n->c_str());
	fprintf(fp, "\tassign\t%s_sel = 0;\n", n->c_str());
	fprintf(fp, "\tassign\t%s_addr= 0;\n", n->c_str());
	fprintf(fp, "\tassign\t%s_data= 0;\n", n->c_str());

	fprintf(fp, "\t// verilator lint_off UNUSED\n");
	fprintf(fp, "\twire\t[%d:0]\tunused_bus_%s;\n",
			3+m_info->data_width(), n->c_str());
	fprintf(fp, "\tassign\tunused_bus_%s = "
		"{ %s_ack, %s_stall, %s_err, %s_data };\n",
		n->c_str(), n->c_str(), n->c_str(),
		n->c_str(), n->c_str());
	fprintf(fp, "\t// verilator lint_on  UNUSED\n");
	fprintf(fp, "\n");
}

void	WBBUS::writeout_bus_logic_v(FILE *fp) {
	// PLIST	*p = m_info->m_plist;
	STRINGP	n = m_info->m_name;
	CLOCKINFO	*c = m_info->m_clock;
	PLIST::iterator	pp;

	if (NULL == m_plist)
		return;

	writeout_bus_select_v(fp);

	if (m_plist->size() == 0) {
		// Since this bus has no slaves, any attempt to access it
		// needs to cause a bus error.
		fprintf(fp,
			"\t\tassign\t%s_err   = %s_stb;\n"
			"\t\tassign\t%s_stall = 1\'b0;\n"
			"\t\talways @(*)\n\t\t\t%s_ack   <= 1\'b0;\n"
			"\t\tassign\t%s_idata = 0;\n"
			"\tassign\t%s_none_sel = 1\'b1;\n",
			n->c_str(), n->c_str(),
			n->c_str(), n->c_str(),
			n->c_str(), n->c_str());
		return;
	} else if (m_plist->size() == 1) {
		STRINGP	strp;

		fprintf(fp,
			"\tassign\t%s_none_sel = 1\'b0;\n"
			"\talways @(*)\n\t\t%s_many_ack = 1\'b0;\n",
			n->c_str(), n->c_str());
		if (NULL != (strp = getstring((*m_plist)[0]->p_phash,
						KYERROR_WIRE))) {
			fprintf(fp, "\tassign\t%s_err = %s;\n",
				n->c_str(), strp->c_str());
		} else
			fprintf(fp, "\tassign\t%s_err = 1\'b0;\n",
				n->c_str());
		fprintf(fp,
			"\tassign\t%s_stall = %s_stall;\n"
			"\talways @(*)\n\t\t%s_ack = %s_ack;\n"
			"\talways @(*)\n\t\t%s_idata = %s_data;\n",
			n->c_str(), (*m_plist)[0]->p_name->c_str(),
			n->c_str(), (*m_plist)[0]->p_name->c_str(),
			n->c_str(), (*m_plist)[0]->p_name->c_str());
		return;
	}

	// none_sel
	fprintf(fp, "\tassign\t%s_none_sel = (%s_stb)&&({\n",
		n->c_str(), n->c_str());
	for(unsigned k=0; k< m_plist->size(); k++) {
		fprintf(fp, "\t\t\t\t%s_sel", (*m_plist)[k]->p_name->c_str());
		if (k != m_plist->size()-1)
			fprintf(fp, ",\n");

	} fprintf(fp, "} == 0);\n\n");

	// many_ack
	if ( m_plist->size() < 2) {
		fprintf(fp, "\talways @(*)\n\t\t%s_many_ack <= 1'b0;\n",
			n->c_str());
	} else {
		fprintf(fp,
"\t//\n"
"\t// WB: many_ack\n"
"\t//\n"
"\t// It is also a violation of the bus protocol to produce multiple\n"
"\t// acks at once and on the same clock.  In that case, the bus\n"
"\t// can't decide which result to return.  Worse, if someone is waiting\n"
"\t// for a return value, that value will never come since another ack\n"
"\t// masked it.\n"
"\t//\n"
"\t// The other error that isn't tested for here, no would I necessarily\n"
"\t// know how to test for it, is when peripherals return values out of\n"
"\t// order.  Instead, I propose keeping that from happening by\n"
"\t// guaranteeing, in software, that two peripherals are not accessed\n"
"\t// immediately one after the other.\n"
"\t//\n");
		fprintf(fp,
"\talways @(posedge %s)\n"
"\t\tcase({\t\t%s_ack,\n", c->m_wire->c_str(), (*m_plist)[0]->p_name->c_str());
		for(unsigned k=1; k<m_plist->size(); k++) {
			fprintf(fp, "\t\t\t\t%s_ack", (*m_plist)[k]->p_name->c_str());
			if (k != m_plist->size()-1)
				fprintf(fp, ",\n");
		} fprintf(fp, "})\n");

		fprintf(fp, "\t\t\t%d\'b%0*d: %s_many_ack <= 1\'b0;\n",
			(int)m_plist->size(), (int)m_plist->size(), 0,
			n->c_str());
		for(unsigned k=0; k< m_plist->size(); k++) {
			fprintf(fp, "\t\t\t%d\'b", (int)m_plist->size());
			for(unsigned j=0; j< m_plist->size(); j++)
				fprintf(fp, "%s", (j==k)?"1":"0");
			fprintf(fp, ": %s_many_ack <= 1\'b0;\n",
				n->c_str());
		}

		fprintf(fp, "\t\t\tdefault: %s_many_ack <= (%s_cyc);\n",
			n->c_str(), n->c_str());
		fprintf(fp, "\t\tendcase\n\n");
	}


	// Start with the slist
	if (m_slist) {
		fprintf(fp, "\treg\t\t" PREFIX "r_%s_sio_ack;\n",
				n->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_sio_data;\n\n",
			m_info->data_width()-1, n->c_str());

		fprintf(fp, "\tassign\t" PREFIX "%s_sio_stall = 1\'b0;\n\n", n->c_str());
		fprintf(fp, "\tinitial " PREFIX "r_%s_sio_ack = 1\'b0;\n"
			"\talways\t@(posedge %s)\n"
			"\t\t" PREFIX "r_%s_sio_ack <= (%s_stb)&&(" PREFIX "%s_sio_sel);\n",
				n->c_str(),
				c->m_wire->c_str(),
				n->c_str(),
				n->c_str(), n->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_sio_ack = " PREFIX "r_%s_sio_ack;\n\n",
				n->c_str(), n->c_str());

		unsigned mask = 0, unused_lsbs = 0, lgdw;
		for(unsigned k=0; k<m_slist->size(); k++) {
			mask |= (*m_slist)[k]->p_mask;
		} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0)); tmp >>= 1)
			unused_lsbs++;
		lgdw = nextlg(m_info->data_width())-3;

		fprintf(fp, "\talways\t@(posedge %s)\n"
			// "\t\t// mask        = %08x\n"
			// "\t\t// lgdw        = %d\n"
			// "\t\t// unused_lsbs = %d\n"
			"\tcasez( %s_addr[%d:%d] )\n",
				c->m_wire->c_str(),
				// mask, lgdw, unused_lsbs,
				n->c_str(),
				nextlg(mask)-1, unused_lsbs);
		for(unsigned j=0; j<m_slist->size()-1; j++) {
			fprintf(fp, "\t\t%d'h%lx: " PREFIX "r_%s_sio_data <= %s_data;\n",
				nextlg(mask)-unused_lsbs,
				((*m_slist)[j]->p_base) >> (unused_lsbs + lgdw),
				n->c_str(),
				(*m_slist)[j]->p_name->c_str());
		}

		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_sio_data <= %s_data;\n",
			n->c_str(),
			(*m_slist)[m_slist->size()-1]->p_name->c_str());
		fprintf(fp, "\tendcase\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_sio_data = " PREFIX "r_%s_sio_data;\n\n",
			n->c_str(), n->c_str());
	}

	// Then the dlist
	if (m_dlist) {
		unsigned mask = 0, unused_lsbs = 0, lgdw, maskbits;
		for(unsigned k=0; k<m_dlist->size(); k++) {
			mask |= (*m_dlist)[k]->p_mask;
		} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0)); tmp >>= 1)
			unused_lsbs++;
		maskbits = nextlg(mask)-unused_lsbs;
		if (maskbits < 1)
			maskbits=1;
		// lgdw is the log of the data width in bytes.  We require it
		// here because the bus addresses words rather than the bytes
		// within them
		lgdw = nextlg(m_info->data_width())-3;


		fprintf(fp, "\treg\t[1:0]\t" PREFIX "r_%s_dio_ack;\n",
				n->c_str());
		fprintf(fp, "\t// # dlist = %d, nextlg(#dlist) = %d\n",
			(int)m_dlist->size(),
			nextlg(m_dlist->size()));
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_dio_bus_select;\n",
			nextlg((int)m_dlist->size())-1, n->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_dio_data;\n",
			m_info->data_width()-1, n->c_str());

		//
		// The stall line
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_stall = 1\'b0;\n", n->c_str());
		//
		// The ACK line
		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\tif (i_reset || !%s_cyc)\n"
			"\t\t" PREFIX "r_%s_dio_ack <= 0;\n"
			"\telse\n"
			"\t\t" PREFIX "r_%s_dio_ack <= { " PREFIX "r_%s_dio_ack[0], (%s_stb)&&(" PREFIX "%s_dio_sel) };\n",
				c->m_wire->c_str(), n->c_str(),
				n->c_str(), n->c_str(),
				n->c_str(), n->c_str(),
				n->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_ack = " PREFIX "r_%s_dio_ack[1];\n", n->c_str(), n->c_str());
		fprintf(fp, "\n");

		//
		// The data return lines
		fprintf(fp, "\talways @(posedge %s)\n"
			"\tcasez(%s_addr[%d:%d])\n",
				c->m_wire->c_str(),
				n->c_str(),
				maskbits+unused_lsbs-1, unused_lsbs);
		for(unsigned k=0; k<m_dlist->size(); k++) {
			fprintf(fp, "\t\t%d'b", maskbits);
			for(unsigned b=0; b<maskbits; b++) {
				unsigned	shift = maskbits + unused_lsbs-b-1;
				if (((*m_dlist)[k]->p_mask & (1<<shift))==0)
					fprintf(fp, "?");
				else if ((*m_dlist)[k]->p_base & (1<<(shift+lgdw)))
					fprintf(fp, "1");
				else
					fprintf(fp, "0");

				if ((shift > lgdw)&&(((shift-lgdw)&3)==0)
					&&(b<maskbits-1))
					fprintf(fp, "_");
			}
			fprintf(fp, ": " PREFIX "r_%s_dio_bus_select <= %d\'d%d;\n",
				n->c_str(), nextlg(m_dlist->size()), k);
		}
		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_dio_bus_select <= 0;\n",
			n->c_str());
		fprintf(fp, "\tendcase\n\n");

		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\tcasez(" PREFIX "r_%s_dio_bus_select)\n",
			c->m_wire->c_str(), n->c_str());

		for(unsigned k=0; k<m_dlist->size()-1; k++) {
			fprintf(fp, "\t\t%d'd%d", nextlg(m_dlist->size()), k);
			fprintf(fp, ": " PREFIX "r_%s_dio_data <= %s_data;\n",
				n->c_str(),
				(*m_dlist)[k]->p_name->c_str());
		}

		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_dio_data <= %s_data;\n",
			n->c_str(),
			(*m_dlist)[m_dlist->size()-1]->p_name->c_str());
		fprintf(fp, "\tendcase\n\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_data = " PREFIX "r_%s_dio_data;\n\n",
			n->c_str(), n->c_str());
	} else
		fprintf(fp, "\t//\n\t// No class DOUBLE peripherals on the \"%s\" bus\n\t//\n", n->c_str());


	fprintf(fp, ""
	"\t//\n"
	"\t// Finally, determine what the response is from the %s bus\n"
	"\t// bus\n"
	"\t//\n"
	"\t//\n", n->c_str());


	fprintf(fp, ""
	"\t//\n"
	"\t// %s_ack\n"
	"\t//\n"
	"\t// The returning wishbone ack is equal to the OR of every component that\n"
	"\t// might possibly produce an acknowledgement, gated by the CYC line.\n"
	"\t//\n"
	"\t// To return an ack here, a component must have a @SLAVE.TYPE tag.\n"
	"\t// Acks from any @SLAVE.TYPE of SINGLE and DOUBLE components have been\n"
	"\t// collected together (above) into " PREFIX "%s_sio_ack and " PREFIX "%s_dio_ack\n"
	"\t// respectively, which will appear ahead of any other device acks.\n"
	"\t//\n",
	n->c_str(), n->c_str(), n->c_str());

	fprintf(fp, "\talways @(posedge %s)\n\t\t%s_ack <= "
			"(%s_cyc)&&(|{ ",
			c->m_wire->c_str(),
			n->c_str(), n->c_str());
	for(unsigned k=0; k < m_plist->size()-1; k++)
		fprintf(fp, "%s_ack,\n\t\t\t\t",
			(*m_plist)[k]->p_name->c_str());
	fprintf(fp, "%s_ack });\n",
		(*m_plist)[m_plist->size()-1]->p_name->c_str());


	fprintf(fp, ""
	"\t//\n"
	"\t// %s_idata\n"
	"\t//\n"
	"\t// This is the data returned on the bus.  Here, we select between a\n"
	"\t// series of bus sources to select what data to return.  The basic\n"
	"\t// logic is simply this: the data we return is the data for which the\n"
	"\t// ACK line is high.\n"
	"\t//\n"
	"\t// The last item on the list is chosen by default if no other ACK's are\n"
	"\t// true.  Although we might choose to return zeros in that case, by\n"
	"\t// returning something we can skimp a touch on the logic.\n"
	"\t//\n"
	"\t// Any peripheral component with a @SLAVE.TYPE value of either OTHER\n"
	"\t// or MEMORY will automatically be listed here.  In addition, the\n"
	"\t// bus responses from @SLAVE.TYPE SINGLE (_sio_) and/or DOUBLE\n"
	"\t// (_dio_) may also be listed here, depending upon components are\n"
	"\t// connected to them.\n"
	"\t//\n", n->c_str());

	if (( m_plist)&&( m_plist->size() > 0)) {
		if ( m_plist->size() == 1) {
			fprintf(fp, "\talways @(*)\n\t\t%s_idata <= %s_data;\n",
				n->c_str(),
				(* m_plist)[0]->p_name->c_str());
		} else if ( m_plist->size() == 2) {
			fprintf(fp, "\talways @(posedge %s)\n\t\tif (%s_ack)\n\t\t\t%s_idata <= %s_data;\n\t\telse\n\t\t\t%s_idata <= %s_data;\n",
				c->m_wire->c_str(),
				(*m_plist)[0]->p_name->c_str(),
				n->c_str(),
				(*m_plist)[0]->p_name->c_str(),
				n->c_str(),
				(*m_plist)[1]->p_name->c_str());
		} else {
			int	sbits = nextlg( m_plist->size());
			fprintf(fp,"\treg [%d:0]\t" PREFIX "r_%s_bus_select;\n",
				sbits-1, n->c_str());

			unsigned mask = 0, unused_lsbs = 0, lgdw, maskbits;
			for(unsigned k=0; k< m_plist->size(); k++) {
				mask |= (*m_plist)[k]->p_mask;
			} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0));
					tmp >>= 1)
				unused_lsbs++;
			maskbits = nextlg(mask)-unused_lsbs;
			// lgdw is the log of the data width in bytes.  We
			// require it here because the bus addresses words
			// rather than the bytes within them
			lgdw = nextlg(m_info->data_width())-3;

			fprintf(fp, "\talways\t@(posedge %s)\n"
				"\tif (%s_stb && ! %s_stall)\n"
				"\t\tcasez(" PREFIX "%s_addr[%d:%d])\n",
				c->m_wire->c_str(),
				n->c_str(), n->c_str(),
				n->c_str(),
				unused_lsbs+maskbits-1, unused_lsbs);
			for(unsigned k=0; k< m_plist->size(); k++) {
				fprintf(fp, "\t\t\t// %08lx & %08lx, %s\n",
					((*m_plist)[k]->p_mask << lgdw),
					(*m_plist)[k]->p_base,
					(*m_plist)[k]->p_name->c_str());
				fprintf(fp, "\t\t\t%d'b", maskbits);
				for(unsigned b=0; b<maskbits; b++) {
					unsigned	shift = maskbits + unused_lsbs-b-1;
					if (((*m_plist)[k]->p_mask & (1<<shift))==0)
						fprintf(fp, "?");
					else if ((*m_plist)[k]->p_base & (1<<(shift+lgdw)))
						fprintf(fp, "1");
					else
						fprintf(fp, "0");

					if ((shift > lgdw)&&(((shift-lgdw)&3)==0)
						&&(b<maskbits-1))
						fprintf(fp, "_");
				}
				fprintf(fp, ": " PREFIX "r_%s_bus_select <= %d'd%d;\n",
					n->c_str(), sbits, k);
			}
			fprintf(fp, "\t\t\tdefault: begin end\n");
			fprintf(fp, "\t\tendcase\n\n");

			fprintf(fp, "\talways @(posedge %s)\n"
				"\tcasez(" PREFIX "r_%s_bus_select)\n",
					c->m_wire->c_str(),
					n->c_str());

			for(unsigned i=0; i< m_plist->size(); i++) {
				fprintf(fp, "\t\t%d\'d%d", sbits, i);
				fprintf(fp, ": %s_idata <= %s_data;\n",
					n->c_str(),
					(*m_plist)[i]->p_name->c_str());
			}
			fprintf(fp, "\t\tdefault: %s_idata <= %s_data;\n",
				n->c_str(),
				(*m_plist)[( m_plist->size()-1)]->p_name->c_str());
			fprintf(fp, "\tendcase\n\n");
		}
	} else
		fprintf(fp, "\talways @(posedge %s)\n"
			"\t\t%s_idata <= 32\'h0\n",
			c->m_wire->c_str(), n->c_str());

	// The stall line
	fprintf(fp, "\tassign\t%s_stall =\t((%s_sel)&&(%s_stall))",
		n->c_str(), (*m_plist)[0]->p_name->c_str(),
		(*m_plist)[0]->p_name->c_str());
	if ( m_plist->size() <= 1)
		fprintf(fp, ";\n\n");
	else {
		for(unsigned i=1; i< m_plist->size(); i++)
			fprintf(fp, "\n\t\t\t\t||((%s_sel)&&(%s_stall))",
				(*m_plist)[i]->p_name->c_str(),
				(*m_plist)[i]->p_name->c_str());

		fprintf(fp, ";\n\n");
	}

	// Bus errors
	{
		STRING	err_bus("");
		STRINGP	strp;
		int	ecount = 0;

#ifdef	SLIST
		if (m_slist) for(PLIST::iterator sp=m_slist->begin();
					sp != m_slist->end(); sp++) {
			if (NULL == (strp = getstring((*sp)->p_phash,
						KYERROR_WIRE)))
				continue;
			if (ecount == 0)
				err_bus = STRING("(");
			else
				err_bus = err_bus + STRING(")||(");
			err_bus = err_bus + (*strp);
			ecount++;
		}
#endif

#ifdef	DLIST
		if (m_dlist) for(PLIST::iterator dp=m_dlist->begin();
					dp != m_dlist->end(); dp++) {
			if (NULL == (strp = getstring((*dp)->p_phash,
						KYERROR_WIRE)))
				continue;
			if (ecount == 0)
				err_bus = STRING("(");
			else
				err_bus = err_bus + STRING(")||(");
			err_bus = err_bus + (*strp);
			ecount++;
		}
#endif

		for(PLIST::iterator pp= m_plist->begin();
					pp !=  m_plist->end(); pp++) {
			if (NULL == (strp = getstring((*pp)->p_phash,
						KYERROR_WIRE)))
				continue;
			if (ecount == 0)
				err_bus = STRING("(");
			else
				err_bus = err_bus + STRING(")||(");
			err_bus = err_bus + (*strp);
			ecount++;
		}

		if (ecount > 0)
			err_bus = STRING("(") + err_bus + STRING(")");

		fprintf(fp, "\tassign %s_err = ((%s_stb)&&(%s_none_sel))||(%s_many_ack)",
			n->c_str(), n->c_str(), n->c_str(),
			n->c_str());
		if (ecount == 0) {
			fprintf(fp, ";\n");
		} else if (ecount == 1) {
			fprintf(fp, "||%s);\n", err_bus.c_str());
		} else
			fprintf(fp, "\n\t\t\t||%s; // ecount = %d",
				err_bus.c_str(), ecount);
	}
}

STRINGP	WBBUSCLASS::name(void) {
	return new STRING("wb");
}

STRINGP	WBBUSCLASS::longname(void) {
	return new STRING("Wishbone");
}

bool	WBBUSCLASS::matchtype(STRINGP str) {
	if (str->compare("wb")==0)
		return true;
	if (str->compare("wbp")==0)
		return true;
	return false;
}

bool	WBBUSCLASS::matchfail(MAPDHASH *bhash) {
	return false;
}

GENBUS *WBBUSCLASS::create(BUSINFO *bi) {
	WBBUS	*busclass;

	busclass = new WBBUS(bi);
	return busclass;
}
