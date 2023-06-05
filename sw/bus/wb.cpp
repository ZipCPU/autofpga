////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	wb.cpp
// {{{
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
//	SLAVE.PREFIX:
//		A string to use to prefix all of the slave's bus wires when
//			naming
//	SLAVE.OPTIONS= (unused)
//		RO (read only slave)
//		WO (write only slave)
//		(default: read and write)
//	SLAVE.SHARE=
//	// slave shares parts of the interface with the other listed slaves
//
//	MASTER.TYPE=
//	(currently ignored)
//	MASTER.PREFIX:
//		A string to use to prefix all of the master's bus wires when
//			naming
//	MASTER.OPTIONS= (unused)
//		RO (read only master)
//		WO (write only master)
//		(default: read and write)
//
//	// INTERCONNECT.TYPE (Not used)
//	// - SINGLE	(Deprecated interconnect)
//	// - CROSSBAR
//
//	BUS.OPT_LOWPOWER
//		If set, forces bus wires to zero when stb (or ack) is low.
//	BUS.OPT_DBLBUFFER
//		If set, uses an extra clock on the return--for better clock
//		speed performance
//	BUS.OPT_LGMAXBURST
//		Log_2 of the maximum number of transactions "in-flight" at any
//		given time.  This sets the bit-width of the internal transaction
//		counter
//	BUS.OPT_TIMEOUT
//		Bus cycles before timing out on any operation
//	BUS.OPT_STARVATION_TIMEOUT
//		(Currently ignored)
//
// Creates tags:
//
//	SLAVE.PORTLIST:	A list of bus interconnect parameters, somewhat like:
//			@$(SLAVE.PREFIX)_cyc,
//			@$(SLAVE.PREFIX)_stb,
//	(if !RO&!WO)	@$(SLAVE.PREFIX)_we,
//	(if AWID>0)	@$(SLAVE.PREFIX)_addr[@$(SLAVE.AWID)-1:0],
//	(if !RO)	@$(SLAVE.PREFIX)_data,
//	(if !RO)	@$(SLAVE.PREFIX)_sel,
//			@$(SLAVE.PREFIX)_stall,
//			@$(SLAVE.PREFIX)_ack,
//	(if !WO)	@$(SLAVE.PREFIX)_data,
//	(and possibly)	@$(SLAVE.PREFIX)_err
//
//	... only, I've currently chosen to do this in a slave independent
//	fashion, and so the ROM, WOM, and AWID strings get set as a default
//
//	SLAVE.ANSIPORTLIST ... same thing,
//
//	SLAVE.IANSI: i_
//	SLAVE.OANSI: o_
//	SLAVE.ANSPREFIX:
//	SLAVE.ANSSUFFIX:
//		.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)cyc@$(SLAVE.ANSSUFFIX)(@$SLAVE.BUS)_cyc),
//		.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)stb@$(SLAVE.ANSSUFFIX)(@$SLAVE.BUS)_stb),
//		.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)we@$(SLAVE.ANSSUFFIX)(@$SLAVE.BUS)_we),
//			.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2019-2022, Gisselquist Technology, LLC
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
	// {{{
	m_info = bi;
	m_slist = NULL;
	m_dlist = NULL;

	m_is_single = false;
	m_is_double = false;

	m_num_single = 0;
	m_num_double = 0;
	m_num_total = 0;
}
// }}}

void	WBBUS::allocate_subbus(void) {
	// {{{
	PLIST	*pl = m_info->m_plist;
	BUSINFO	*sbi = NULL, *dbi = NULL;

	if (NULL == pl || pl->size() == 0) {
		gbl_msg.warning("Bus %s has no attached slaves\n",
			(name()) ? name()->c_str() : "(No-name)");
	}

	gbl_msg.info("Generating WB bus logic generator for %s\n",
		(name()) ? name()->c_str() : "(No-name)");
	countsio();

	if (m_num_single == m_num_total) {
		m_num_single = 0;
		m_is_single = true;
	} else if ((m_num_single <= 2)&&(m_num_double > 0)) {
		m_num_double += m_num_single;
		m_num_single = 0;
	}

	if (!m_is_single && m_num_single + m_num_double == m_num_total) {
		m_num_double = 0;
		m_is_double  = true;
	} else if (m_num_double <= 2)
		m_num_double = 0;

	assert(!m_is_single || !m_is_double);
	assert(m_num_single < 50);
	assert(m_num_double < 50);
	assert(m_num_total >= m_num_single + m_num_double);

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

	if (!m_is_double && !m_is_single && pl) {
		//
		// There exist peripherals that are neither singles nor doubles
		//
		for(unsigned pi = 0; pi< pl->size(); pi++) {
			PERIPHP	p = (*pl)[pi];
			STRINGP	ptyp;

			ptyp = getstring(p->p_phash, KYSLAVE_TYPE);
			if (m_slist && ptyp != NULL
				&& ptyp->compare(KYSINGLE) == 0) {
				m_info->m_plist->erase(pl->begin()+pi);
				pi--;
				m_slist->add(p);
			} else if (m_dlist && ptyp != NULL
						&& ptyp->compare(KYDOUBLE) == 0) {
				m_info->m_plist->erase(pl->begin()+pi);
				pi--;
				m_dlist->add(p);
			} else {
			// Leave this peripheral in m_info->m_plist
			}
		}
	}

//	countsio();

	if (sbi)
		setstring(sbi->m_hash, KY_TYPE, wbclass.name());
	if (dbi)
		setstring(dbi->m_hash, KY_TYPE, wbclass.name());
	REHASH;
}
// }}}

int	WBBUS::address_width(void) {
	assert(m_info);
	return m_info->m_address_width;
}

bool	WBBUS::get_base_address(MAPDHASH *phash, unsigned &base) {
	// {{{
	if (!m_info || !m_info->m_plist) {
		gbl_msg.error("BUS[%s] has no peripherals!\n",
			(name()) ? name()->c_str() : "(No name)");
		return false;
	} else
		return m_info->m_plist->get_base_address(phash, base);
}
// }}}

void	WBBUS::assign_addresses(void) {
	// {{{
	int	address_width;

	if (m_info->m_addresses_assigned)
		return;
	if ((NULL == m_slist)&&(NULL == m_dlist))
		allocate_subbus();

	if (!m_info)
		return;
	gbl_msg.info("WB: Assigning addresses for bus %s\n",
		(name()) ? name()->c_str() : "(No name bus)");
	if (!m_info->m_plist||(m_info->m_plist->size() < 1)) {
		m_info->m_address_width = 0;
	} else if (!m_info->m_addresses_assigned) {
		int	dw = m_info->data_width(),
			nullsz;

		if (m_slist)
			m_slist->assign_addresses(dw, 0);

		if (m_dlist)
			m_dlist->assign_addresses(dw, 0);

		if (!getvalue(*m_info->m_hash, KY_NULLSZ, nullsz))
			nullsz = 0;

		m_info->m_plist->assign_addresses(dw, nullsz);
		address_width = m_info->m_plist->get_address_width();
		m_info->m_address_width = address_width;
		if (m_info->m_hash) {
			setvalue(*m_info->m_hash, KY_AWID, m_info->m_address_width);
			REHASH;
		}
	} m_info->m_addresses_assigned = true;
}
// }}}

BUSINFO *WBBUS::create_sio(void) {
	// {{{
	assert(m_info);

	BUSINFO	*sbi;
	SUBBUS	*subp;
	STRINGP	sioname;
	MAPDHASH *bushash, *shash;
	MAPT	elm;

	sioname = new STRING(STRING("" PREFIX) + (*name()) + "_sio");
	bushash = new MAPDHASH();
	shash   = new MAPDHASH();
	setstring(*shash, KYPREFIX, sioname);
	setstring(*shash, KYSLAVE_TYPE, new STRING(KYDOUBLE));
	setstring(*shash, KYSLAVE_PREFIX, sioname);

	elm.m_typ = MAPT_MAP;
	elm.u.m_m = bushash;
	shash->insert(KEYVALUE(KYSLAVE_BUS, elm));

	elm.u.m_m = m_info->m_hash;
	shash->insert(KEYVALUE(KYMASTER_BUS, elm));
	setstring(shash, KYMASTER_TYPE, KYARBITER);

	sbi  = new BUSINFO(sioname);
	sbi->prefix(new STRING("_sio"));
	setstring(bushash, KY_TYPE, new STRING("wb"));
	sbi->m_data_width = m_info->m_data_width;
	sbi->m_clock      = m_info->m_clock;
	sbi->addmaster(m_info->m_hash);
	subp = new SUBBUS(shash, sioname, sbi);
	subp->p_slave_bus = m_info;
	// subp->p_master_bus = set by the SUBBUS to be sbi
	m_info->m_plist->add(subp);
assert(subp->p_master_bus);
assert(subp->p_slave_bus == m_info);
assert(subp->p_master_bus == sbi);
	// m_plist->integrity_check();
	sbi->add();
	m_slist = sbi->m_plist;

	return sbi;
}
// }}}

BUSINFO *WBBUS::create_dio(void) {
	// {{{
	assert(m_info);

	BUSINFO	*dbi;
	SUBBUS	*subp;
	STRINGP	dioname;
	MAPDHASH	*bushash, *shash;
	MAPT	elm;

	dioname = new STRING(STRING("" PREFIX) + (*name()) + "_dio");
	bushash = new MAPDHASH();
	shash   = new MAPDHASH();
	setstring(*bushash, KY_NAME, dioname);
	setstring(*shash, KYPREFIX, dioname);
	setstring(*shash, KYSLAVE_TYPE, new STRING(KYOTHER));
	setstring(*shash, KYSLAVE_PREFIX, dioname);

	elm.m_typ = MAPT_MAP;
	elm.u.m_m = m_info->m_hash;
	shash->insert(KEYVALUE(KYSLAVE_BUS, elm));

	elm.u.m_m = bushash;
	shash->insert(KEYVALUE(KYMASTER_BUS, elm));
	setstring(shash, KYMASTER_TYPE, KYARBITER);

	dbi  = new BUSINFO(dioname);
	dbi->prefix(new STRING("_dio"));
	setstring(bushash, KY_TYPE, new STRING("wb"));
assert(m_info->data_width() > 0);
	setvalue(*bushash, KY_WIDTH, m_info->data_width());
	dbi->m_data_width = m_info->m_data_width;
	dbi->m_clock      = m_info->m_clock;
	dbi->addmaster(m_info->m_hash);
	subp = new SUBBUS(shash, dioname, dbi);
	subp->p_slave_bus = m_info;
	m_info->m_plist->add(subp);
assert(subp->p_master_bus);
assert(subp->p_master_bus == dbi);
assert(subp->p_slave_bus == m_info);
	// subp->p_master_bus = set by the slave to be dbi
	dbi->add();
	m_dlist = dbi->m_plist;
assert(isbusmaster(*shash));
assert(isarbiter(*shash));

	return dbi;
}
// }}}

void	WBBUS::countsio(void) {
	// {{{
	PLIST	*pl = m_info->m_plist;
	STRINGP	strp;

	m_num_single = 0;
	m_num_double = 0;
	m_num_total = 0;

	if (NULL == pl)
		return;

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
// }}}

void	WBBUS::integrity_check(void) {
	// {{{
	// GENBUS::integrity_check();

	if (m_info && m_info->m_data_width <= 0) {
		gbl_msg.error("ERR: BUS width not defined for %s\n",
			name()->c_str());
	}
}
// }}}

void	WBBUS::writeout_defn_v(FILE *fp, const char* pname,
		const char *pfx,
		const int aw, const int dw,
		const char *errwire, const char *btyp) {
	// {{{
	STRINGP	n = name();

	fprintf(fp, "\t// Wishbone definitions for bus %s%s, component %s\n",
		n->c_str(), btyp, pname);
	fprintf(fp, "\t// Verilator lint_off UNUSED\n");
	fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we;\n",
		pfx, pfx, pfx);
	fprintf(fp, "\twire\t[%d:0]\t%s_addr;\n", address_width()-1, pfx);
	fprintf(fp, "\twire\t[%d:0]\t%s_data;\n", dw-1, pfx);
	fprintf(fp, "\twire\t[%d:0]\t%s_sel;\n", dw/8-1, pfx);
	fprintf(fp, "\twire\t\t%s_stall, %s_ack, %s_err", pfx, pfx, pfx);
	if ((errwire)&&(errwire[0] != '\0')
			&&(STRING(STRING(pfx)+"_err").compare(errwire)!=0))
		fprintf(fp, ", %s;\n"
			"\tassign\t\t%s_err = %s; // P\n", errwire, pfx, errwire);
	else
		fprintf(fp, ";\n");
	fprintf(fp, "\twire\t[%d:0]\t%s_idata;\n", dw-1, pfx);
	fprintf(fp, "\t// Verilator lint_on UNUSED\n");
}
// }}}

void	WBBUS::writeout_bus_slave_defns_v(FILE *fp) {
	// {{{
	PLIST	*p = m_info->m_plist;
	STRINGP	n = name();

	if (m_slist) {
		for(PLIST::iterator pp=m_slist->begin();
				pp != m_slist->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			STRINGP	prefix = (*pp)->bus_prefix();
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				prefix->c_str(),
				0, m_info->data_width(),
				(errwire)?errwire->c_str(): NULL,
				"(SIO)");
		}
	}

	if (m_dlist) {
		for(PLIST::iterator pp=m_dlist->begin();
				pp != m_dlist->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			STRINGP	prefix = (*pp)->bus_prefix();
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				prefix->c_str(),
				(*pp)->p_awid, m_info->data_width(),
				(errwire)?errwire->c_str(): NULL,
				"(DIO)");
		}
	}

	if (p) {
		for(PLIST::iterator pp=p->begin(); pp != p->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			STRINGP	prefix = (*pp)->bus_prefix();
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				prefix->c_str(),
				(*pp)->p_awid, m_info->data_width(),
				(errwire) ? errwire->c_str() : NULL);
		}
	} else {
		gbl_msg.error("%s has no slaves\n", n->c_str());
	}
}
// }}}

void	WBBUS::writeout_bus_master_defns_v(FILE *fp) {
	// {{{
	MLIST	*m = m_info->m_mlist;
	if (m) {
		for(MLIST::iterator pp=m->begin(); pp != m->end(); pp++) {
			writeout_defn_v(fp, (*pp)->name()->c_str(),
				(*pp)->bus_prefix()->c_str(),
				address_width(), m_info->data_width(),
				NULL);
		}
	} else {
		gbl_msg.error("Bus %s has no masters\n", name()->c_str());
	}
}
// }}}

void	WBBUS::write_addr_range(FILE *fp, const PERIPHP p, const int dalines) {
	// {{{
	unsigned	w = address_width();
	w = (w+3)/4;
	if (p->p_naddr == 1)
		fprintf(fp, " // 0x%0*lx", w, p->p_base);
	else
		fprintf(fp, " // 0x%0*lx - 0x%0*lx", w, p->p_base,
			w, p->p_base + (p->p_naddr << (dalines))-1);
}
// }}}

void	WBBUS::writeout_bus_select_v(FILE *fp) {
	// {{{
#ifdef	DEPRECATED_CODE
	// {{{
	STRINGP	n = name();
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

	if (m_info->m_plist->size() == 1) {
		if ((*m_info->m_plist)[0]->p_name)
			fprintf(fp, "\tassign\t%12s_sel = (%s_cyc); "
					"// Only one peripheral on this bus\n",
				(*m_info->m_plist)[0]->p_name->c_str(),
				n->c_str());
	} else for(unsigned i=0; i< m_info->m_plist->size(); i++) {
		PERIPHP p = (*m_info->m_plist)[i];
		const char *pn = p->p_name->c_str();

		if (pn) {
			if (p->p_mask == 0) {
				fprintf(fp, "\tassign\t%12s_sel = 1\'b0; // ERR: address mask == 0\n",
					pn);
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%12s_sel "
					"= ((%s[%2d:%2d] & %2d\'h%lx) == %2d\'h%0*lx);",
					pn, addrbus.c_str(), sbaw-1,
					unused_lsbs,
					sbaw-unused_lsbs, p->p_mask >> unused_lsbs,
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					p->p_base>>(dalines+unused_lsbs));
				write_addr_range(fp, p, dalines);
				fprintf(fp, "\n");
			}
		} if (p->p_master_bus) {
			fprintf(fp, "//x2\tWas a master bus as well\n");
		}
	} fprintf(fp, "\t//\n\n");
	// }}}
#endif
}
// }}}

void	WBBUS::writeout_no_slave_v(FILE *fp, STRINGP prefix) {
	// {{{
	STRINGP	n = m_info->name();

	fprintf(fp, "\n\t//\n");
	fprintf(fp, "\t// In the case that there is no %s peripheral\n"
		"\t// responding on the %s bus\n",
			prefix->c_str(), n->c_str());
	fprintf(fp, "\tassign\t%s_ack   = 1\'b0;\n", prefix->c_str());
	fprintf(fp, "\tassign\t%s_err   = (%s_stb);\n",
			prefix->c_str(), prefix->c_str());
	fprintf(fp, "\tassign\t%s_stall = 0;\n", prefix->c_str());
	fprintf(fp, "\tassign\t%s_idata = 0;\n", prefix->c_str());
	fprintf(fp, "\n");
}
// }}}

void	WBBUS::writeout_no_master_v(FILE *fp) {
	// {{{
/*
	if (!m_info || !m_info->m_name)
		gbl_msg.error("(Unnamed bus) has no name!\n");

	// PLIST	*p = m_info->m_plist;
	STRINGP	p = m_info->bus_prefix();

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
*/
}
// }}}

//
// Connect this master to the crossbar.  Specifically, we want to output
// a list of master connections to fill the given port.
//
void	WBBUS::xbarcon_master(FILE *fp, const char *tabs,
		const char *pfx, const char *sig, bool comma) {
	// {{{
	fprintf(fp, "%s%s({\n", tabs, pfx);
	for(unsigned k = m_info->m_mlist->size()-1; k>0; k--) {
		BMASTERP m = (*m_info->m_mlist)[k];
		STRINGP busp = m->bus_prefix();
		fprintf(fp, "%s\t%s_%s,\n", tabs, busp->c_str(), sig);
	} fprintf(fp, "%s\t%s_%s\n%s})%s\n", tabs,
			 (*m_info->m_mlist)[0]->bus_prefix()->c_str(),
			sig, tabs, (comma) ? ",":"");
}
// }}}

//
// Output a list of connections to slave bus wires.  Used in connecting the
// slaves to the various crossbar inputs.
//
void	WBBUS::xbarcon_slave(FILE *fp, PLIST *pl, const char *tabs,
		const char *pfx, const char *sig, bool comma) {
	// {{{
	fprintf(fp, "%s%s({\n", tabs, pfx);
	for(unsigned k = pl->size()-1; k>0; k--) {
		PERIPHP p = (*pl)[k];
		STRINGP busp = p->bus_prefix();
		fprintf(fp, "%s\t%s_%s,\n", tabs, busp->c_str(), sig);
	} fprintf(fp, "%s\t%s_%s\n%s})%s\n", tabs,
		(*pl)[0]->bus_prefix()->c_str(), sig,
		tabs, (comma) ? ",":"");
}
// }}}

void	WBBUS::writeout_bus_logic_v(FILE *fp) {
	// {{{
	STRINGP		n = name(), rst;
	CLOCKINFO	*c = m_info->m_clock;
	PLIST::iterator	pp;
	PLISTP		pl;
	unsigned	unused_lsbs;

	if (NULL == m_info->m_plist)
		return;

	if (NULL == m_info->m_mlist) {
		gbl_msg.error("No masters assigned to bus %s\n",
				n->c_str());
		return;
	}
	if (NULL == (rst = m_info->reset_wire())) {
		gbl_msg.warning("Bus %s has no associated reset wire, using \'i_reset\'\n", n->c_str());
		rst = new STRING("i_reset");
		setstring(m_info->m_hash, KY_RESET, rst);
		REHASH;
	}

	if (NULL == c || NULL == c->m_wire) {
		gbl_msg.fatal("Bus %s has no associated clock\n", n->c_str());
	}


	if (m_info->m_plist->size() == 0) {
		fprintf(fp, "\t//\n"
			"\t// Bus %s has no slaves\n"
			"\t//\n\n", n->c_str());

		// Since this bus has no slaves, any attempt to access it
		// needs to cause a bus error.
		//
		// Need to loop through all possible masters ...
		for(MLIST::iterator pp=m_info->m_mlist->begin(); pp != m_info->m_mlist->end(); pp++) {
			STRINGP	pfx= (*pp)->bus_prefix();
			const char *pfxc = pfx->c_str();

			fprintf(fp,
				"\t//\n"
				"\t// Master %s has no slaves attached to its bus, %s\n"
				"\t//\n"
				"\t\tassign\t%s_err   = %s_stb;\n"
				"\t\tassign\t%s_stall = 1\'b0;\n"
				"\t\tassign\t%s_ack   = 1\'b0;\n"
				"\t\tassign\t%s_idata = 0;\n",
				pfxc, m_info->name()->c_str(),
				pfxc, pfxc, pfxc, pfxc, pfxc);
		}

		return;
	} else if (NULL == m_info->m_mlist || m_info->m_mlist->size() == 0) {
		for(unsigned p=0; p < m_info->m_plist->size(); p++) {
			STRINGP	pstr = (*m_info->m_plist)[p]->bus_prefix();
			fprintf(fp,
		"\t//\n"
		"\t// The %s bus has no masters assigned to it\n"
		"\t//\n"
		"\tassign	%s_cyc = 1\'b0;\n"
		"\tassign	%s_stb = 1\'b0;\n"
		"\tassign	%s_we  = 1\'b0;\n"
		"\tassign	%s_addr= 0;\n"
		"\tassign	%s_data= 0;\n"
		"\tassign	%s_sel;\n\n",
		pstr->c_str(),
		//
		pstr->c_str(), pstr->c_str(), pstr->c_str(),
		pstr->c_str(), pstr->c_str(), pstr->c_str());
		}

		return;
	} else if ((m_info->m_plist->size() == 1)&&(m_info->m_mlist->size() == 1)) {
		MLIST::iterator	mp = m_info->m_mlist->begin();
		PLIST::iterator	pp = m_info->m_plist->begin();
		STRINGP	strp;
		// Only one master connected to only one slave--skip all the
		// extra connection logic.
		//
		// Can only simplify if there's only one peripheral and only
		// one master
		//
		STRINGP	slv  = (*m_info->m_plist)[0]->bus_prefix();
		STRINGP	mstr = (*m_info->m_mlist)[0]->bus_prefix();

		fprintf(fp,
		"//\n"
		"// Bus %s has only one master (%s) and one slave (%s)\n"
		"// connected to it -- skipping the interconnect\n"
		"//\n", n->c_str(), mstr->c_str(), slv->c_str());

		fprintf(fp,
			"\tassign\t%s_cyc  = %s_cyc;\n"
			"\tassign\t%s_stb  = %s_stb;\n"
			"\tassign\t%s_we   = %s_we;\n"
			"\tassign\t%s_addr = %s_addr;\n"
			"\tassign\t%s_data = %s_data;\n"
			"\tassign\t%s_sel  = %s_sel;\n",
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str(),
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str(),
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str(),
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str(),
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str(),
			(*pp)->bus_prefix()->c_str(),
				(*mp)->bus_prefix()->c_str());

		if (NULL != (strp = getstring((*m_info->m_plist)[0]->p_phash,
						KYERROR_WIRE))) {
			if (strp->compare(STRING(STRING(*(*pp)->bus_prefix())
						+ STRING("_err"))) != 0)
				fprintf(fp, "\tassign\t%s_err = %s; // X\n",
					(*pp)->bus_prefix()->c_str(),
					strp->c_str());
		} else
			fprintf(fp, "\tassign\t%s_err = 1\'b0;\n",
				(*pp)->bus_prefix()->c_str());
		fprintf(fp, "\tassign\t%s_err = %s_err; // Y\n",
			(*mp)->bus_prefix()->c_str(), (*pp)->bus_prefix()->c_str());
		fprintf(fp,
			"\tassign\t%s_stall = %s_stall;\n"
			"\tassign\t%s_ack   = %s_ack;\n"
			"\tassign\t%s_idata = %s_idata;\n",
			(*mp)->bus_prefix()->c_str(),
				(*pp)->bus_prefix()->c_str(),
			(*mp)->bus_prefix()->c_str(),
				(*pp)->bus_prefix()->c_str(),
			(*mp)->bus_prefix()->c_str(),
				(*pp)->bus_prefix()->c_str());
		return;
	}

	// Start with the slist
	if (m_slist) {
		STRING	sio_bus_prefix = STRING(*m_info->name()) + "_sio";
		STRINGP	slp = &sio_bus_prefix;

		fprintf(fp,
			"\t//\n"
			"\t// %s Bus logic to handle SINGLE slaves\n"
			"\t//\n", n->c_str());

		fprintf(fp, "\treg\t\t" PREFIX "r_%s_ack;\n", slp->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_data;\n\n",
			m_info->data_width()-1, slp->c_str());

		fprintf(fp, "\tassign\t" PREFIX "%s_stall = 1\'b0;\n\n", slp->c_str());
		fprintf(fp, "\tinitial " PREFIX "r_%s_ack = 1\'b0;\n"
			"\talways\t@(posedge %s)\n"
			"\t\t" PREFIX "r_%s_ack <= (%s_stb);\n",
				slp->c_str(), c->m_wire->c_str(),
				slp->c_str(), slp->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_ack = " PREFIX "r_%s_ack;\n\n",
				slp->c_str(), slp->c_str());

		unsigned mask = 0, lgdw;
		unused_lsbs = 0;

		for(unsigned k=0; k<m_slist->size(); k++) {
			mask |= (*m_slist)[k]->p_mask;
		} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0)); tmp >>= 1)
			unused_lsbs++;
		lgdw = nextlg(m_info->data_width())-3;

		fprintf(fp, "\talways\t@(posedge %s)\n", c->m_wire->c_str());
			// "\t\t// mask        = %08x\n"
			// "\t\t// lgdw        = %d\n"
			// "\t\t// unused_lsbs = %d\n"
		fprintf(fp, "\tcasez( %s_addr[%d:%d] )\n",
				// mask, lgdw, unused_lsbs,
				slp->c_str(),
				nextlg(mask)-1, unused_lsbs);
		for(unsigned j=0; j<m_slist->size(); j++) {
			fprintf(fp, "\t%d'h%lx: " PREFIX "r_%s_data <= %s_idata;\n",
				nextlg(mask)-unused_lsbs,
				((*m_slist)[j]->p_base) >> (unused_lsbs + lgdw),
				slp->c_str(),
				(*m_slist)[j]->bus_prefix()->c_str());
		}

		if (m_slist->size() != (1u<<nextlg(m_slist->size()))) {
			// We need a default option
		if (bus_option(KY_OPT_LOWPOWER)) {
			int	v;
			STRINGP str;
			if (getvalue(*m_info->m_hash, KY_OPT_LOWPOWER, v))
				fprintf(fp,
				"\tdefault: " PREFIX
					"r_%s_data <= (%d) ? 0 : %s_idata;\n",
				slp->c_str(), v,
				(*m_slist)[m_slist->size()-1]->bus_prefix()->c_str());
			else {
				str = getstring(*m_info->m_hash, KY_OPT_LOWPOWER);
				fprintf(fp, "\tdefault: " PREFIX "r_%s_data <= (%s) ? 0 : %s_idata;\n",
				slp->c_str(), (str) ? str->c_str() : "1\'b0",
				(*m_slist)[m_slist->size()-1]->bus_prefix()->c_str());
			}

		} else {
			fprintf(fp, "\tdefault: " PREFIX "r_%s_data <= %s_idata;\n",
				slp->c_str(),
				(*m_slist)[m_slist->size()-1]->bus_prefix()->c_str());
		}} else {
			fprintf(fp, "\t// No default: SIZE = %d, [Guru meditation: %d != %d]\n",
				(int)m_slist->size(),
				(int)nextlg(m_slist->size()-1),
				(int)nextlg(m_slist->size()));
		}
		fprintf(fp, "\tendcase\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_idata = " PREFIX "r_%s_data;\n\n",
			slp->c_str(), slp->c_str());

		fprintf(fp, "\n\t//\n"
			"\t// Now to translate this logic to the various SIO slaves\n\t//\n"
			"\t// In this case, the SIO bus has the prefix %s\n"
			"\t// and all of the slaves have various wires beginning\n"
			"\t// with their own respective bus prefixes.\n"
			"\t// Our goal here is to make certain that all of\n"
			"\t// the slave bus inputs match the SIO bus wires\n",
			slp->c_str());

		unsigned	sbaw;
		unsigned	dw   = m_info->data_width();
		unsigned	dalines = nextlg(dw/8);
		mask = 0;

		sbaw = m_slist->get_address_width();
		for(unsigned j=0; j<m_slist->size(); j++) {
			const char *pn = (*m_slist)[j]->bus_prefix()->c_str();

			fprintf(fp, "\tassign\t%s_cyc = %s_cyc;\n",
				pn, slp->c_str());

			if ((*m_slist)[j]->p_mask == 0) {
				fprintf(fp, "\tassign\t%s_stb = 1\'b0; // ERR: (SIO) address mask == 0\n",
					pn);
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%s_stb = %s_stb "
					"&& (%s_addr[%2d:%2d] == %2d\'h%0*lx); ",
					pn, slp->c_str(), slp->c_str(),
					sbaw-1, unused_lsbs,
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_slist)[j]->p_base >> (unused_lsbs+dalines));
				write_addr_range(fp, (*m_slist)[j], dalines);
				fprintf(fp, "\n");
			}
			fprintf(fp,
			"\tassign\t%s_we  = %s_we;\n"
			"\tassign\t%s_data= %s_data;\n"
			"\tassign\t%s_sel = %s_sel;\n",
				pn, slp->c_str(),
				pn, slp->c_str(),
				pn, slp->c_str());
		}
	} else
		fprintf(fp, "\t//\n\t// No class SINGLE peripherals on the \"%s\" bus\n\t//\n\n", n->c_str());

	// Then the dlist
	if (m_dlist) {
		STRING	dio_bus_prefix = STRING(*m_info->name()) + "_dio";
		STRINGP	dlp = &dio_bus_prefix;

		fprintf(fp,
			"\t//\n"
			"\t// %s Bus logic to handle %ld DOUBLE slaves\n"
			"\t//\n"
			"\t//\n", n->c_str(), m_dlist->size());

		unsigned mask = 0, lgdw, maskbits;
		unused_lsbs = 0;
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


		fprintf(fp, "\treg\t[1:0]\t" PREFIX "r_%s_ack;\n",
				dlp->c_str());
		fprintf(fp, "\t// # dlist = %d, nextlg(#dlist) = %d\n",
			(int)m_dlist->size(),
			nextlg(m_dlist->size()));
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_bus_select;\n",
			nextlg((int)m_dlist->size())-1, dlp->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_data;\n",
			m_info->data_width()-1, dlp->c_str());
		fprintf(fp, "\n");

		//
		// The stall line
		fprintf(fp, "\t// DOUBLE peripherals are not allowed to stall.\n"
			"\tassign\t" PREFIX "%s_stall = 1\'b0;\n\n",
			dlp->c_str());
		//
		// The ACK line
		fprintf(fp,
		"\t// DOUBLE peripherals return their acknowledgments in two\n"
		"\t// clocks--always, allowing us to collect this logic together\n"
		"\t// in a slave independent manner.  Here, the acknowledgment\n"
		"\t// is treated as a two stage shift register, cleared on any\n"
		"\t// reset, or any time the cycle line drops.  (Dropping the\n"
		"\t// cycle line aborts the transaction.)\n"
		"\tinitial\t" PREFIX "r_%s_ack = 0;\n"
		"\talways\t@(posedge %s)\n"
			"\tif (%s || !%s_cyc)\n",
				dlp->c_str(), c->m_wire->c_str(),
				rst->c_str(), dlp->c_str());
		fprintf(fp,
			"\t\t" PREFIX "r_%s_ack <= 0;\n"
			"\telse\n"
			"\t\t" PREFIX "r_%s_ack <= { " PREFIX "r_%s_ack[0], (%s_stb) };\n",
				dlp->c_str(), dlp->c_str(),
				dlp->c_str(), dlp->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_ack = "
			PREFIX "r_%s_ack[1];\n",
			dlp->c_str(), dlp->c_str());
		fprintf(fp, "\n");

		//
		// The data return lines
		//
		fprintf(fp,
			"\t// Since it costs us two clocks to go through this\n"
			"\t// logic, we'll take one of those clocks here to set\n"
			"\t// a selection index, and then on the next clock we'll\n"
			"\t// use this index to select from among the vaious\n"
			"\t// possible bus return values\n"
			"\talways @(posedge %s)\n"
			"\tcasez(%s_addr[%d:%d])\n",
				c->m_wire->c_str(),
				dlp->c_str(),
				maskbits+unused_lsbs-1, unused_lsbs);
		for(unsigned k=0; k<m_dlist->size(); k++) {
			fprintf(fp, "\t%d'b", maskbits);
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
			fprintf(fp, ": " PREFIX "r_%s_bus_select <= %d\'d%d;\n",
				dlp->c_str(), nextlg(m_dlist->size()), k);
		}
		fprintf(fp, "\tdefault: " PREFIX "r_%s_bus_select <= 0;\n",
			dlp->c_str());
		fprintf(fp, "\tendcase\n\n");

		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\tcasez(" PREFIX "r_%s_bus_select)\n",
			c->m_wire->c_str(), dlp->c_str());

		for(unsigned k=0; k<m_dlist->size(); k++) {
			fprintf(fp, "\t%d'd%d", nextlg(m_dlist->size()), k);
			fprintf(fp, ": " PREFIX "r_%s_data <= %s_idata;\n",
				dlp->c_str(),
				(*m_dlist)[k]->bus_prefix()->c_str());
		}

		if ((1u<<nextlg(m_dlist->size())) != m_dlist->size()) {
			// Only place the default value into the case if there
			// are empty values there.
			if (bus_option(KY_OPT_LOWPOWER)) {
				fprintf(fp, "\tdefault: "
					PREFIX "r_%s_data <= 0;\n", dlp->c_str());
			} else
			fprintf(fp, "\tdefault: "
				PREFIX "r_%s_data <= %s_idata;\n", dlp->c_str(),
			(*m_dlist)[m_dlist->size()-1]->bus_prefix()->c_str());
		}
		fprintf(fp, "\tendcase\n\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_idata = " PREFIX "r_%s_data;\n\n",
			dlp->c_str(), dlp->c_str());

		//
		// Connect the dio bus wires together to send to the various
		// slave buses the dio bus drives
		//
		unsigned	sbaw;
		unsigned	dw   = m_info->data_width();
		unsigned	dalines = nextlg(dw/8);
		mask = 0;

		sbaw = m_dlist->get_address_width();
		for(unsigned j=0; j<m_dlist->size(); j++) {
			const char *pn = (*m_dlist)[j]->bus_prefix()->c_str();

			fprintf(fp, "\tassign\t%s_cyc = %s_cyc;\n",
				pn, dlp->c_str());

			if ((*m_dlist)[j]->p_mask == 0) {
				fprintf(fp, "\tassign\t%s_stb = 1\'b0; // ERR: (DIO) address mask == 0\n",
					pn);
			} else {
				assert(sbaw > unused_lsbs);
				assert(sbaw > 0);
				fprintf(fp, "\tassign\t%s_stb = %s_stb "
					"&& ((%s_addr[%2d:%2d] & %2d\'h%0*lx) == %2d\'h%0*lx); ",
					pn, dlp->c_str(), dlp->c_str(),
					// Relevant address bits
					sbaw-1, unused_lsbs,
					//
					// Mask bits
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_dlist)[j]->p_mask >> unused_lsbs,
					//
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_dlist)[j]->p_base >> (unused_lsbs+dalines));
				write_addr_range(fp, (*m_dlist)[j], dalines);
				fprintf(fp, "\n");
			}
			fprintf(fp,
			"\tassign\t%s_we  = %s_we;\n"
			"\tassign\t%s_addr= %s_addr;\n"
			"\tassign\t%s_data= %s_data;\n"
			"\tassign\t%s_sel = %s_sel;\n",
				pn, dlp->c_str(),
				pn, dlp->c_str(),
				pn, dlp->c_str(),
				pn, dlp->c_str());
		}
	} else
		fprintf(fp, "\t//\n\t// No class DOUBLE peripherals on the \"%s\" bus\n\t//\n\n", n->c_str());

	//
	//
	// Now for the main set of slaves
	//
	//

	unsigned	slave_name_width = 4;
	// Find the maximum width of any slaves name, for our comment tables
	// below
	for(unsigned k=0; k<m_info->m_plist->size(); k++) {
		PERIPHP	p = (*m_info->m_plist)[k];
		unsigned	sz;

		sz = p->name()->size();
		if (slave_name_width < sz)
			slave_name_width = sz;
	}

	//
	// First make certain any slaves that cannot produce error wires
	// can produce
	//
	for(unsigned k=0; k<m_info->m_plist->size(); k++) {
		PERIPHP	p = (*m_info->m_plist)[k];
		const char *pn = (*m_info->m_plist)[k]->bus_prefix()->c_str();
		STRINGP	err;

		if (NULL != (err = getstring(p->p_phash, KYERROR_WIRE))) {
			STRING buserr = STRING(pn) + STRING("_err");
			if (buserr.compare(*err) == 0) {
				fprintf(fp, "\t// info: @ERROR.WIRE for %s "
					"matches the buses error name, "
					"%s_err\n",
					p->name()->c_str(), pn);
			} else {
				fprintf(fp, "\t// info: @ERROR.WIRE %s != %s\n",
					err->c_str(), pn);
				fprintf(fp, "\t// info: @ERROR.WIRE for %s, = %s, doesn\'t match the buses wire %s_err\n",
					p->name()->c_str(), err->c_str(),
					pn);
				fprintf(fp, "\tassign\t%s_err = %s; // Z\n",
					pn, err->c_str());
			}
		} else
			fprintf(fp, "\tassign\t%s_err= 1\'b0;\n", pn);
	}

	//
	// Now create the crossbar interconnect
	//
	fprintf(fp,
	"\t//\n"
	"\t// Connect the %s bus components together using the wbxbar()\n"
	"\t//\n"
	"\t//\n", n->c_str());

	unused_lsbs = nextlg(m_info->data_width())-3;
	fprintf(fp,
	"\twbxbar #(\n"
		"\t\t.NM(%ld), .NS(%ld), .AW(%d), .DW(%d),\n",
		m_info->m_mlist->size(), m_info->m_plist->size(),
		address_width(), m_info->data_width());

	slave_addr(fp, m_info->m_plist, unused_lsbs); fprintf(fp, ",\n");
	slave_mask(fp, m_info->m_plist, unused_lsbs);

	xbar_option(fp, KY_OPT_LOWPOWER,   ",\n\t\t.OPT_LOWPOWER(%)");
	xbar_option(fp, KY_OPT_LGMAXBURST, ",\n\t\t.LGNMAXBURST(%)");
	xbar_option(fp, KY_OPT_TIMEOUT,    ",\n\t\t.OPT_TIMEOUT(%)");
	xbar_option(fp, KY_OPT_DBLBUFFER,  ",\n\t\t.OPT_DBLBUFFER(%)", "1\'b1");
	// OPT_STARVATION_TIMEOUT?

	fprintf(fp,
	")\n\t%s_xbar(\n"
	"\t\t.i_clk(%s), .i_reset(%s),\n",
		n->c_str(), c->m_wire->c_str(), rst->c_str());
	xbarcon_master(fp, "\t\t", ".i_mcyc",  "cyc");
	xbarcon_master(fp, "\t\t", ".i_mstb",  "stb");
	xbarcon_master(fp, "\t\t", ".i_mwe",   "we");
	xbarcon_master(fp, "\t\t", ".i_maddr", "addr");
	xbarcon_master(fp, "\t\t", ".i_mdata", "data");
	xbarcon_master(fp, "\t\t", ".i_msel",  "sel");
	xbarcon_master(fp, "\t\t", ".o_mstall","stall");
	xbarcon_master(fp, "\t\t", ".o_mack",  "ack");
	xbarcon_master(fp, "\t\t", ".o_mdata", "idata");
	xbarcon_master(fp, "\t\t", ".o_merr",  "err");
	fprintf(fp, "\t\t// Slave connections\n");
	pl = m_info->m_plist;
	xbarcon_slave(fp, pl, "\t\t", ".o_scyc",  "cyc");
	xbarcon_slave(fp, pl, "\t\t", ".o_sstb",  "stb");
	xbarcon_slave(fp, pl, "\t\t", ".o_swe",   "we");
	xbarcon_slave(fp, pl, "\t\t", ".o_saddr", "addr");
	xbarcon_slave(fp, pl, "\t\t", ".o_sdata", "data");
	xbarcon_slave(fp, pl, "\t\t", ".o_ssel",  "sel");
	xbarcon_slave(fp, pl, "\t\t", ".i_sstall","stall");
	xbarcon_slave(fp, pl, "\t\t", ".i_sack",  "ack");
	xbarcon_slave(fp, pl, "\t\t", ".i_sdata", "idata");
	xbarcon_slave(fp, pl, "\t\t", ".i_serr",  "err",  false);
	fprintf(fp, "\t\t);\n\n");

}
// }}}

STRINGP	WBBUS::master_portlist(BMASTERP) {
	// {{{
	return new STRING(
	"@$(MASTER.PREFIX)_cyc, "
	"@$(MASTER.PREFIX)_stb, "
	"@$(MASTER.PREFIX)_we,\n"
	"\t\t\t@$(MASTER.PREFIX)_addr[@$(MASTER.BUS.AWID)-1:0],\n"
	"\t\t\t@$(MASTER.PREFIX)_data, // @$(MASTER.BUS.WIDTH) bits wide\n"
	"\t\t\t@$(MASTER.PREFIX)_sel,  // @$(MASTER.BUS.WIDTH)/8 bits wide\n"
	"\t\t@$(MASTER.PREFIX)_stall, "
	"@$(MASTER.PREFIX)_ack, "
	"@$(MASTER.PREFIX)_idata,"
	"@$(MASTER.PREFIX)_err");
}
// }}}

STRINGP	WBBUS::iansi(BMASTERP) {
	return new STRING("i_");
}

STRINGP	WBBUS::oansi(BMASTERP) {
	return new STRING("o_");
}

STRINGP	WBBUS::master_ansprefix(BMASTERP) {
	return new STRING("wb_");
}

STRINGP	WBBUS::master_ansi_portlist(BMASTERP) {
	// {{{
	return new STRING(
	".@$(OANSI)@$(MASTER.ANSPREFIX)cyc(@$(MASTER.PREFIX)_cyc), "
	".@$(OANSI)@$(MASTER.ANSPREFIX)stb(@$(MASTER.PREFIX)_stb), "
	".@$(OANSI)@$(MASTER.ANSPREFIX)we(@$(MASTER.PREFIX)_we),\n"
	"\t\t\t.@$(OANSI)@$(MASTER.ANSPREFIX)addr(@$(MASTER.PREFIX)_addr[@$(MASTER.BUS.AWID)-1:0]),\n"
	"\t\t\t.@$(OANSI)@$(MASTER.ANSPREFIX)data(@$(MASTER.PREFIX)_data), // @$(MASTER.BUS.WIDTH) bits wide\n"
	"\t\t\t.@$(OANSI)@$(MASTER.ANSPREFIX)sel(@$(MASTER.PREFIX)_sel),  // @$(MASTER.BUS.WIDTH)/8 bits wide\n"
	"\t\t.@$(IANSI)@$(MASTER.ANSPREFIX)stall(@$(MASTER.PREFIX)_stall), "
	".@$(IANSI)@$(MASTER.ANSPREFIX)ack(@$(MASTER.PREFIX)_ack), "
	".@$(IANSI)@$(MASTER.ANSPREFIX)data(@$(MASTER.PREFIX)_idata), "
	".@$(IANSI)@$(MASTER.ANSPREFIX)err(@$(MASTER.PREFIX)_err)");
}
// }}}

STRINGP	WBBUS::slave_ansprefix(PERIPHP p) {
	return new STRING("wb_");
}


STRINGP	WBBUS::slave_portlist(PERIPHP p) {
	// {{{
	STRINGP	errp = getstring(p->p_phash, KYERROR_WIRE);
	STRING	str = STRING(
	"@$(SLAVE.PREFIX)_cyc, "
	"@$(SLAVE.PREFIX)_stb,");

	if (!p->read_only() && !p->write_only())
		str = str + STRING(" @$(SLAVE.PREFIX)_we,");
	str = str + STRING("\n");
	if (p->get_slave_address_width() > 0) {
		char	tmp[64];
		STRING	stmp;
		sprintf(tmp, "_addr[%d-1:0],\n", p->get_slave_address_width());
		stmp = STRING("\t\t\t@$(SLAVE.PREFIX)") + STRING(tmp);
		str = str + stmp;
	}
	if (!p->read_only())
		str = str + STRING(
		"\t\t\t@$(SLAVE.PREFIX)_data, // @$(SLAVE.BUS.WIDTH) bits wide\n"
		"\t\t\t@$(SLAVE.PREFIX)_sel,  // @$(SLAVE.BUS.WIDTH)/8 bits wide\n");
	str = str + STRING(
		"\t\t@$(SLAVE.PREFIX)_stall, @$(SLAVE.PREFIX)_ack");
	if (!p->write_only())
		str = str + STRING(", @$(SLAVE.PREFIX)_idata");
	if (NULL != errp)
		str = str + STRING(", @$(SLAVE.PREFIX)_err");

	return new STRING(str);
}
// }}}

STRINGP	WBBUS::slave_ansi_portlist(PERIPHP p) {
	// {{{
	STRINGP	errp = getstring(p->p_phash, KYERROR_WIRE);
	STRING	str = STRING(

	".@$(IANSI)@$(SLAVE.ANSPREFIX)cyc(@$(SLAVE.PREFIX)_cyc), "
	".@$(IANSI)@$(SLAVE.ANSPREFIX)stb(@$(SLAVE.PREFIX)_stb), ");
	if (!p->read_only() && !p->write_only())
		str = str + STRING(".@$(IANSI)@$(SLAVE.ANSPREFIX)we(@$(SLAVE.PREFIX)_we),\n");
	if (p->get_slave_address_width() > 0) {
		char	tmp[64];
		STRING	stmp;
		sprintf(tmp, "_addr[%d-1:0]),\n", p->get_slave_address_width());
		stmp = STRING("\t\t\t.@$(IANSI)@$(SLAVE.ANSPREFIX)addr(@$(SLAVE.PREFIX)") + STRING(tmp);
		str = str + stmp;
	}
	if (!p->read_only())
		str = str + STRING(
	"\t\t\t.@$(IANSI)@$(SLAVE.ANSPREFIX)data(@$(SLAVE.PREFIX)_data), // @$(SLAVE.BUS.WIDTH) bits wide\n"
	"\t\t\t.@$(IANSI)@$(SLAVE.ANSPREFIX)sel(@$(SLAVE.PREFIX)_sel),  // @$(SLAVE.BUS.WIDTH)/8 bits wide\n");
	str = str + STRING(
	"\t\t.@$(OANSI)@$(SLAVE.ANSPREFIX)stall(@$(SLAVE.PREFIX)_stall),"
	".@$(OANSI)@$(SLAVE.ANSPREFIX)ack(@$(SLAVE.PREFIX)_ack)");

	if (!p->write_only())
		str = str + STRING(", .@$(OANSI)@$(SLAVE.ANSPREFIX)data(@$(SLAVE.PREFIX)_idata)");
	if (NULL != errp)
		str = str + STRING(", .@$(OANSI)@$(SLAVE.ANSPREFIX)err(@$(SLAVE.PREFIX)_err)");
	return new STRING(str);
}
// }}}

////////////////////////////////////////////////////////////////////////
//
// Bus class logic
//
// The bus class describes the logic above in a way that it can be
// chosen, selected, and enacted within a design.  Hence if a design
// has four wishbone buses and one AXI-lite bus, the bus class logic
// will recognize the four WB buses and generate WB bus generators,
// and then an AXI-lite bus and bus generator, etc.
//
////////////////////////////////////////////////////////////////////////
STRINGP	WBBUSCLASS::name(void) {
	return new STRING("wb");
}

STRINGP	WBBUSCLASS::longname(void) {
	return new STRING("Wishbone");
}

bool	WBBUSCLASS::matchtype(STRINGP str) {
	if (!str)
		// Wishbone is the default type
		return true;
	if (str->compare("wb")==0)
		return true;
	if (str->compare("wbp")==0)
		return true;
// printf("ERR: No match for bus type %s\n",str->c_str());
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
