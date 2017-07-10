////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	businfo.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "parser.h"
#include "mapdhash.h"
#include "keys.h"
#include "kveval.h"
#include "legalnotice.h"
#include "bldtestb.h"
#include "bitlib.h"
#include "plist.h"
#include "bldregdefs.h"
#include "ifdefs.h"
#include "bldsim.h"
#include "predicates.h"
#include "businfo.h"
#include "globals.h"
#include "subbus.h"

BUSLIST	*gbl_blist;


// Look up the number of bits in the address bus of the given hash
int	BUSINFO::address_width(void) {
	if (!m_addresses_assigned)
		assign_addresses();

	return m_address_width;
}

bool	BUSINFO::get_base_address(MAPDHASH *phash, unsigned &base) {
	return m_plist->get_base_address(phash, base);
}

void	BUSINFO::assign_addresses(void) {
	if ((!m_plist)||(m_plist->size() < 1)) {
		m_address_width = 0;
	} else if (!m_addresses_assigned) {
		if (m_slist)
			m_slist->assign_addresses(m_data_width, 0);
		if (m_dlist)
			m_dlist->assign_addresses(m_data_width, 0);
		m_plist->assign_addresses(m_data_width, m_nullsz);
		m_address_width = m_plist->get_address_width();
		if (m_hash)
			setvalue(*m_hash, KY_AWID, m_address_width);
	} m_addresses_assigned = true;
}

PLIST *BUSINFO::create_sio(void) {
	if (NULL == m_plist)
		m_plist = new PLIST();
	if (m_num_single <= 1)
		return m_plist;
	if (!m_slist) {
		BUSINFO	*sbi;
		SUBBUS	*subp;
		STRINGP	name;
		MAPDHASH *bushash;

		name = new STRING(STRING("_") + (*m_name) + "_sio");
		bushash = new MAPDHASH();
		setstring(*bushash, KYPREFIX, name);
		setstring(*bushash, KYSLAVE_TYPE, new STRING(KYSINGLE));
		sbi  = new BUSINFO();
		sbi->m_name = name;
		sbi->m_prefix = new STRING("_sio");;
		sbi->m_data_width = m_data_width;
		sbi->m_clock      = m_clock;
		subp = new SUBBUS(bushash, name, sbi);
		subp->p_slave_bus = this;
		m_plist->add(subp);
		sbi->add();
		m_slist = sbi->m_plist;
	} return m_slist;
}

PLIST *BUSINFO::create_dio(void) {
	if (NULL == m_plist)
		m_plist = new PLIST();
	if (m_num_double <= 1)
		return m_plist;
	if (!m_dlist) {
		BUSINFO	*dbi;
		SUBBUS	*subp;
		STRINGP	name;
		MAPDHASH	*bushash;

		name = new STRING(STRING("_") + (*m_name) + "_dio");
		bushash = new MAPDHASH();
		setstring(*bushash, KYPREFIX, name);
		setstring(*bushash, KYSLAVE_TYPE, new STRING(KYDOUBLE));
		dbi  = new BUSINFO();
		dbi->m_name = name;
		dbi->m_prefix = new STRING("_dio");;
		dbi->m_data_width = m_data_width;
		dbi->m_clock      = m_clock;
		subp = new SUBBUS(bushash, name, dbi);
		subp->p_slave_bus = this;
		m_plist->add(subp);
		dbi->add();
		m_dlist = dbi->m_plist;
	} return m_dlist;
}


void BUSINFO::add(void) {
	if (!m_plist)
		m_plist = new PLIST;
}

PERIPH *BUSINFO::add(PERIPHP p) {
	STRINGP	strp;
	int	pi;
	PLISTP	plist;

	if (!m_plist)
		m_plist = new PLIST();
	plist = m_plist;

	if ((p->p_phash)&&(NULL != (strp = getstring(*p->p_phash, KYSLAVE_TYPE)))) {
		if (strp->compare(KYSINGLE)==0) {
			plist = create_sio();
		} else if (strp->compare(KYDOUBLE)==0) {
			plist = create_dio();
		}
	}

	pi = plist->add(p);
	if (pi >= 0) {
		p = (*plist)[pi];
		p->p_slave_bus = this;
		return p;
	} else
		return NULL;
}

PERIPH *BUSINFO::add(MAPDHASH *phash) {
	STRINGP	strp;
	PERIPH	*p;
	int	pi;
	PLISTP	plist;

	if (!m_plist)
		m_plist = new PLIST();
	plist = m_plist;
	assert(plist);

	if (NULL != (strp = getstring(phash, KYSLAVE_TYPE))) {
		if (strp->compare(KYSINGLE)==0) {
			plist = create_sio();
			assert(plist);
		} else if (strp->compare(KYDOUBLE)==0) {
			plist = create_dio();
			assert(plist);
		}
	}

	pi = plist->add(phash);
	if (pi >= 0) {
		p = (*plist)[pi];
		p->p_slave_bus = this;
		return p;
	} else {
		gbl_err++;
		fprintf(stderr, "ERR: Could not add %s\n", getstring(phash, KYPREFIX)->c_str());
		return NULL;
	}
}

void	BUSINFO::countsio(MAPDHASH *phash) {
	STRINGP	strp;
	unsigned	total = 0;

	strp = getstring(phash, KYSLAVE_TYPE);
	if (NULL != strp) {
		if (strp->compare(KYSINGLE)) {
			m_num_single++;
		} else if (strp->compare(KYDOUBLE)) {
			m_num_double++;
		} total++;
		// else if (str->compare(KYMEMORY))
		//	m_num_memory++;
	}

	if (total == (unsigned)m_num_single + (unsigned)m_num_double) {
		m_num_double = 0;
	} if (total + m_num_single + m_num_double < 8) {
		m_num_single = 0;
		m_num_double = 0;
	}
}


bool	BUSINFO::need_translator(BUSINFO *m) {
	// If they are the same bus, then no translator is necessary
	if ((m==NULL)||(this == m))
		return false;
	if (m_name->compare(*m->m_name)==0)
		return false;

	// If they are of different clocks, then a translator is required
	if ((m_clock)&&(m->m_clock)&&(m_clock != m->m_clock))
		return true;

	// If they are of different widths
	if ((m_data_width)&&(m->m_data_width)
			&&(m_data_width != m->m_data_width))
		return true;

	// If they are of different bus types
	if ((m_type)&&(m->m_type)
			&&(m_type->compare(*m->m_type) != 0))
		return true;
	//
	//
	return false;
}

bool	need_translator(BUSINFO *s, BUSINFO *m) {
	if (!s)
		return false;
	return s->need_translator(m);
}

BUSINFO *BUSLIST::find_bus_of_peripheral(MAPDHASH *phash) {
	assert(NULL != phash);
	for(iterator bp=begin(); bp!=end(); bp++) {
		if ((*bp)->ismember_of(phash))
			return (*bp);
	} return NULL;
}

BUSINFO *find_bus_of_peripheral(MAPDHASH *phash) {
	return gbl_blist->find_bus_of_peripheral(phash);
}

BUSINFO *BUSLIST::find_bus(MAPDHASH *hash) {
	if (!hash)
		return NULL;
	for(unsigned k=0; k<size(); k++) {
		if (((*this)[k]->m_hash)&&(hash == (*this)[k]->m_hash)) {
			return (*this)[k];
		}
	} return NULL;
}

BUSINFO *find_bus(MAPDHASH *hash) {
	return gbl_blist->find_bus(hash);
}

BUSINFO *BUSLIST::find_bus(STRINGP name) {
	if (!name)
		return NULL;
	for(unsigned k=0; k<size(); k++) {
		if (((*this)[k]->m_name)
			&&((*this)[k]->m_name->compare(*name)==0)) {
			return (*this)[k];
		}
	} return NULL;
}

BUSINFO *find_bus(STRINGP name) {
	return gbl_blist->find_bus(name);
}

unsigned	BUSLIST::get_base_address(MAPDHASH *phash) {
	unsigned	base;
	// Assume everything has only one address, and that the first
	// address is the one we want
	for(unsigned i=0; i<size(); i++)
		if ((*this)[i]->get_base_address(phash, base))
			return base;
	return 0;
}

void	BUSLIST::countsio(MAPDHASH *phash) {
	STRINGP	prefix;
	BUSINFO	*bi;

	prefix = getstring(phash, KYPREFIX);
	if (prefix == NULL)
		prefix = new STRING("(No name)");

	fprintf(gbl_dump, "Looking to count SIO for bus %s\n", prefix->c_str());
	fflush(gbl_dump);

	// Ignore everything that isn't a bus slave
	if (NULL == getmap(phash, KYSLAVE)) {
		return;
	}

	// Insist on the existence of a default bus
	assert((*this)[0]);

	MAPDHASH::iterator	kvpair;
	kvpair = findkey(*phash, KYSLAVE_BUS);
	if (kvpair == phash->end()) {
		// Use the default bus if no bus name is given
		bi = (*this)[0];
		assert(bi);
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		bi = find_bus(kvpair->second.u.m_m);
		if (!bi) {
			MAPDHASH::iterator kvbus;
			for(kvbus = kvpair->second.u.m_m->begin();
					kvbus != kvpair->second.u.m_m->end();
					kvbus++) {
				if ((kvbus->second.m_typ == MAPT_MAP)
					&&(NULL != (bi = find_bus(kvbus->second.u.m_m)))) {
					bi->countsio(phash);
				}
			} return;
		}
		assert(bi);
	} else if (kvpair->second.m_typ == MAPT_STRING) {
		bi = find_bus(kvpair->second.u.m_s);
		assert(bi);
	} else {
		gbl_err++;
		fprintf(stderr, "ERR: Internal error\n");
		assert(0);
	}
		

	// Use this peripheral, and count the number of each peripheral
	// type
	assert(bi);
	bi->countsio(phash);
}

void	BUSLIST::countsio(MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		countsio(map.u.m_m);
}


void	BUSLIST::addperipheral(MAPDHASH *phash) {
	STRINGP	pname;
	BUSINFO	*bi;

	// Ignore everything that isn't a bus slave
	if (NULL == getmap(*phash, KYSLAVE))
		return;
	if (NULL == (pname = getstring(*phash, KYPREFIX)))
		pname = new STRING("(Unnamed-P)");

	// Insist on the existence of a default bus
	assert((*this)[0]);

	MAPDHASH::iterator	kvpair, kvbus;

	kvpair = findkey(*phash, KYSLAVE_BUS);
	if (kvpair == phash->end()) {
		bi = (*this)[0];
	} else if (kvpair->second.m_typ == MAPT_STRING) {
		// If this peripheral is on a named bus, point us to it.
		bi = find_bus(kvpair->second.u.m_s);
		// Insist, though, that the named bus exist already
		assert(bi);
		fprintf(gbl_dump,
			"ADD-PERIPHERAL: Bus found for %s by string name, %s\n",
			pname->c_str(), kvpair->second.u.m_s->c_str());
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		bi = find_bus(kvpair->second.u.m_m);
		fprintf(gbl_dump, "ADD-PERIPHERAL: Looking up bus by map for %s%s%s\n", pname->c_str(), (bi != NULL)?" -- FOUND, ":"", (bi != NULL)?bi->m_name->c_str():"");
		if (bi == NULL) {
			MAPDHASH	*blmap = kvpair->second.u.m_m;
			fprintf(gbl_dump, "ADD-PERIPHERAL: Looking for multiple busses by map for %s\n", pname->c_str());
			for(kvbus = blmap->begin(); kvbus != blmap->end();
					kvbus++) {
				if (kvbus->second.m_typ != MAPT_MAP)
					continue;
				bi = find_bus(kvbus->second.u.m_m);
				if (NULL == bi)
					continue;
			fprintf(gbl_dump, "ADD-PERIPHERAL: Bus %s found\n", bi->m_name->c_str());
				bi->add(phash);
			}
			return;
		}
		assert(bi);
	}

	// Now that we know what bus this peripheral attaches to, add it in
	assert(bi);
	bi->add(phash);
	assert(bi->m_plist);
}
void	BUSLIST::addperipheral(MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		addperipheral(map.u.m_m);
}

void	BUSLIST::adddefault(MAPDHASH &master, STRINGP defname) {
	BUSINFO	*bi;

	if (size() == 0) {
		push_back(new BUSINFO());
		bi = (*this)[0];
		bi->m_name = defname;
		bi->m_hash = new MAPDHASH();
		setstring(*bi->m_hash, KY_NAME, defname);
	} else { // if (size() > 0)
		if (NULL != (bi = find_bus(defname))) {
			// This bus already exists
			for(unsigned k=0; k<size(); k++) {
				if (((*this)[k]->m_name)
					&&((*this)[k]->m_name->compare(*defname)==0)) {
					if (k == 0)
						return;

					// If it's not the zero bus,
					// move it to the zero position
					BUSINFO *bi = (*this)[0];
					(*this)[0] = (*this)[k];
					(*this)[k] = bi;
					return;
				}
			}
		} else if ((*this)[0]->m_name) {
			// The bus doesn't exist, although others do
			if ((*this)[0]->m_name->compare(*defname)!=0) {
				// Create a new BUSINFO struct for
				// this new one
				push_back((*this)[0]);
				(*this)[0] = bi = new BUSINFO();
				bi->m_hash = new MAPDHASH();
				setstring(*bi->m_hash, KY_NAME, defname);
			}
			// else this is already the default
		} else {
			// First spot exists, but has no name
			bi = (*this)[0];
			bi->m_name = defname;
			setstring(*bi->m_hash, KY_NAME, defname);
		}
	}
}

void	BUSLIST::addbus(MAPDHASH &master, MAPDHASH *phash) {
	MAPDHASH	*bp;
	int		value;
	STRINGP		str, pname;
	BUSINFO		*bi;
	MAPT		elm;

	pname = getstring(phash, KYPREFIX);
	if (NULL == pname)
		pname = getstring(phash, KY_NAME);
	if (NULL == pname)
		pname = new  STRING("(Null/No name)");

	fprintf(gbl_dump, "ADDBUS request for %s ...",
		pname->c_str());
	if (isbusmaster(*phash))
		fprintf(gbl_dump, "MASTER");
	if (isperipheral(*phash))
		fprintf(gbl_dump, "SLAVE");
	fprintf(gbl_dump, "\n");

	bp = getmap(*phash, KYBUS);
	if (!bp)
		return;

	str = getstring(*bp, KY_NAME);
	if (str) {
		bi = find_bus(str);
		if (!bi) {
			MAPDHASH::iterator	kvpair;
			STRINGP	busname;

			busname = new STRING(STRING("B[")+(*str) + STRING("]"));
			bi = new BUSINFO();
			push_back(bi);
			bi->m_name = str;

			if ((kvpair = findkey(master,*busname))!=master.end()) {
				if (kvpair->second.m_typ != MAPT_MAP) {
					gbl_err++;
					fprintf(stderr, "ERR: @%s is not a map!\n", str->c_str());
					bi->m_hash = new MAPDHASH();
				} else
					bi->m_hash = kvpair->second.u.m_m;
				delete busname;
			} else {
				bi->m_hash = new MAPDHASH();;
			}

			setstring(*bi->m_hash, KY_NAME, str);
		}
	} else if (size() > 0) {
		bi = (*this)[0];
	} else {
		gbl_err++;
		fprintf(stderr, "ERR: Unnamed bus as a part of %s\n",
			(NULL == getstring(*phash, KYPREFIX)) ? "(Null-no-name)"
			: getstring(*phash, KYPREFIX)->c_str());
		bi = new BUSINFO();
		push_back(bi);
		bi->m_hash = NULL;
	}

	if (NULL != (str = getstring(*bp, KY_TYPE))) {
		if (bi->m_type == NULL) {
			bi->m_type = str;
			elm.m_typ = MAPT_STRING;
			elm.u.m_s = str;
			if (NULL == bi->m_hash) {
				gbl_err++;
				fprintf(stderr, "ERR: Undefined bus as a part of %s (no name given?)\n",
					(NULL == getstring(*phash, KYPREFIX))
					?"(Null-name)"
					:(getstring(*phash, KYPREFIX)->c_str()));
			} else
				bi->m_hash->insert(KEYVALUE(KY_TYPE, elm));
mapdump(gbl_dump, master);
		} else if (bi->m_type->compare(*str) != 0) {
			fprintf(stderr, "ERR: Conflicting bus types "
					"for %s\n",bi->m_name->c_str());
			gbl_err++;
		}
	}

	if (NULL != (str = getstring(*bp, KY_CLOCK))) {
		bi->m_clock = getclockinfo(str);
		if (bi->m_clock == NULL) {
			fprintf(stderr, "ERR: Clock %s not defined for %s\n",
				str->c_str(), bi->m_name->c_str());
			gbl_err++;
		} else {
			MAPDHASH::iterator	clktag;
			clktag = findkey(*bp, KY_CLOCK);
			if (clktag != bp->end()) {
				clktag->second.m_typ = MAPT_MAP;
				clktag->second.u.m_m = bi->m_clock->m_hash;
fprintf(gbl_dump, "Adding clock hash for %s\n", str->c_str());
			}

			elm.m_typ = MAPT_MAP;
			elm.u.m_m = bi->m_clock->m_hash;
			bi->m_hash->insert(KEYVALUE(KY_CLOCK, elm));
		}

		mapdump(gbl_dump, *phash);
	}

	if (getvalue(*bp, KY_WIDTH, value)) {
		if (bi->m_data_width <= 0) {
			bi->m_data_width = value;
			bi->m_type = str;
			//
			elm.m_typ = MAPT_INT;
			elm.u.m_v = value;
			bi->m_hash->insert(KEYVALUE(KY_WIDTH, elm));
mapdump(gbl_dump, master);
		} else if (bi->m_data_width != value) {
			fprintf(stderr, "ERR: Conflicting bus width definitions for %s\n", bi->m_name->c_str());
			gbl_err++;
		}
	}

	if (getvalue(*bp, KY_NULLSZ, value)) {
		bi->m_nullsz = value;
		// bi->m_addresses_assigned = false;
		//
		elm.m_typ = MAPT_INT;
		elm.u.m_v = value;
		bi->m_hash->insert(KEYVALUE(KY_NULLSZ, elm));
mapdump(gbl_dump, master);
	}
}

void	BUSLIST::addbus(MAPDHASH &master, MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		addbus(master, map.u.m_m);
}

//
// assign_addresses
//
// Assign addresses to all of our peripherals, given a first address to start
// from.  The first address helps to insure that the NULL address creates a
// proper error (as it should).
//
void assign_addresses(void) {
	for(unsigned i=0; i<gbl_blist->size(); i++)
		(*gbl_blist)[i]->assign_addresses();
}

void	BUSINFO::writeout_bus_slave_defns_v(FILE *fp) {
	for(PLIST::iterator pp=m_plist->begin(); pp != m_plist->end(); pp++) {
		fprintf(fp, "\twire\t\t%s_ack, %s_stall;\n"
				"\twire\t[%d:0]\t%s_data;\n\n",
				(*pp)->p_name->c_str(), (*pp)->p_name->c_str(),
				m_data_width-1, (*pp)->p_name->c_str());
	}
}

void	BUSINFO::writeout_bus_master_defns_v(FILE *fp) {
	unsigned aw = address_width();
	fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we, %s_stall, %s_err;\n"
			"\twire\t\t%s_none_sel, %s_many_ack;\n"
			"\twire\t[%d:0]\t%s_addr\n"
			"\twire\t[%d:0]\t%s_data, %s_idata;\n\n"
			"\twire\t[%d:0]\t%s_sel;\n\n"
			"\treg\t\t%s_ack;\n\n",
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str(),
			aw-1,
			m_name->c_str(),
			m_data_width-1,
			m_name->c_str(), m_name->c_str(),
			(m_data_width/8)-1,
			m_name->c_str(), m_name->c_str());
}

void	BUSINFO::writeout_bus_defns_v(FILE *fp) {
	writeout_bus_master_defns_v(fp);
	writeout_bus_slave_defns_v(fp);
}

void	writeout_bus_defns_v(FILE *fp) {
	fprintf(fp, "//\n//\n// Define bus wires\n//\n//\n");
	BUSLIST	*bl = gbl_blist;
	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++)
		(*bp)->writeout_bus_defns_v(fp);
}

void	BUSINFO::writeout_bus_select_v(FILE *fp) {
	STRING	addrbus = STRING(*m_name)+"_addr";
	unsigned	sbaw = address_width();
	unsigned	dw   = m_data_width;
	unsigned	dalines = nextlg(dw/8);
	unsigned	unused_lsbs = 0, mask = 0;

	fprintf(fp, "\t//\n\t//\n\t//\n\t// Select lines for bus %s\n\t//\n\t//\n\t\n", m_name->c_str());
	if (m_slist) {
		sbaw = m_slist->get_address_width();
		mask = 0; unused_lsbs = 0;
		for(unsigned i=0; i<m_slist->size(); i++)
			mask |= (*m_slist)[i]->p_mask;
		for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
			unused_lsbs++;
	fprintf(gbl_dump, "// SLIST.SBAW = %d\n// SLIST.DALINES = %d\n// SLIST.UNUSED_LSBS = %d\n", sbaw, dalines, unused_lsbs);
	fflush(gbl_dump);
		for(unsigned i=0; i<m_slist->size(); i++) {
			assert(sbaw > dalines + unused_lsbs);
			assert(sbaw > 0);
			fprintf(fp, "\tassign\t%12s_sel = ((_%s_sio_sel)&&(%s_addr[%2d:%2d] == %2d\'h%0*lx));\n",
				(*m_slist)[i]->p_name->c_str(), m_name->c_str(),
				m_name->c_str(),
				sbaw-dalines-1, unused_lsbs,
				sbaw-dalines-unused_lsbs, 
				(sbaw-dalines-unused_lsbs+3)/4,
				(*m_slist)[i]->p_base >> (unused_lsbs+dalines));
		}
			
	} if (m_dlist) {
		sbaw = m_dlist->get_address_width();
		mask = 0; unused_lsbs = 0;
		for(unsigned i=0; i<m_dlist->size(); i++)
			mask |= (*m_dlist)[i]->p_mask;
		for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
			unused_lsbs++;
	fprintf(gbl_dump, "// DLIST.SBAW = %d\n// DLIST.DALINES = %d\n// DLIST.UNUSED_LSBS = %d\n", sbaw, dalines, unused_lsbs);
	fflush(gbl_dump);
		for(unsigned i=0; i<m_dlist->size(); i++) {
			assert(sbaw > dalines + unused_lsbs);
			assert(sbaw > 0);
			fprintf(fp, "\tassign\t%12s_sel "
					"= ((_%s_dio_sel)&&((%s[%2d:%2d] & %2d\'h%lx) == %2d\'h%0*lx));\n",
				(*m_dlist)[i]->p_name->c_str(),
				m_name->c_str(),
				addrbus.c_str(),
				sbaw-dalines-1,
				unused_lsbs,
				sbaw-dalines-unused_lsbs, (*m_dlist)[i]->p_mask >> unused_lsbs,
				sbaw-dalines-unused_lsbs,
				(sbaw-dalines-unused_lsbs+3)/4,
				(*m_dlist)[i]->p_base>>(dalines+unused_lsbs));
		}
			
	}

	sbaw = address_width();
	mask = 0; unused_lsbs = 0;
	for(unsigned i=0; i<m_plist->size(); i++)
		mask |= (*m_plist)[i]->p_mask;
	for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
		unused_lsbs++;

	fprintf(gbl_dump, "// SBAW = %d\n// DALINES = %d\n// UNUSED_LSBS = %d\n", sbaw, dalines, unused_lsbs);
	fflush(gbl_dump);
	for(unsigned i=0; i< m_plist->size(); i++) {
		if ((*m_plist)[i]->p_name) {
			assert(sbaw > dalines + unused_lsbs);
			assert(sbaw > 0);
			fprintf(fp, "\tassign\t%12s_sel "
					"= ((%s[%2d:%2d] & %2d\'h%lx) == %2d\'h%0*lx);\n",
				(*m_plist)[i]->p_name->c_str(),
				addrbus.c_str(),
				sbaw-dalines-1,
				unused_lsbs,
				sbaw-dalines-unused_lsbs, (*m_plist)[i]->p_mask >> unused_lsbs,
				sbaw-dalines-unused_lsbs,
				(sbaw-dalines-unused_lsbs+3)/4,
				(*m_plist)[i]->p_base>>(dalines+unused_lsbs));
		} if ((*m_plist)[i]->p_master_bus) {
			fprintf(fp, "//x2\tWas a master bus as well\n");
		}
	} fprintf(fp, "\t//\n\n");

}

void	writeout_bus_select_v(FILE *fp) {
	BUSLIST	*bl = gbl_blist;
	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++)
		(*bp)->writeout_bus_select_v(fp);
}

void	mkselect(FILE *fp) {
	for(BUSLIST::iterator bp=gbl_blist->begin(); bp != gbl_blist->end(); bp++)
		(*bp)->writeout_bus_select_v(fp);
}

void	BUSINFO::writeout_no_slave_v(FILE *fp, STRINGP prefix) {
	fprintf(fp, "\treg\t_r_%s_ack;\n", prefix->c_str());
	fprintf(fp, "\tinitial\t_r_%s_ack = 1\'b0;\n", prefix->c_str());
	fprintf(fp, "\talways @(posedge %s)\t_r_%s_ack <= %s_sel;\n",
		m_clock->m_wire->c_str(), prefix->c_str(), prefix->c_str());
	fprintf(fp, "\tassign\t%s_ack   = _r_%s_ack;\n",
			prefix->c_str(), prefix->c_str());
	fprintf(fp, "\tassign\t%s_stall = 0;\n", prefix->c_str());
	fprintf(fp, "\tassign\t%s_data  = 0;\n", prefix->c_str());
}

void	BUSINFO::writeout_no_master_v(FILE *fp, STRINGP master) {
	fprintf(fp, "\tassign\t%s_cyc = 1\'b0;\n", master->c_str());
	fprintf(fp, "\tassign\t%s_stb = 1\'b0;\n", master->c_str());
	fprintf(fp, "\tassign\t%s_we  = 1\'b0;\n", master->c_str());
	fprintf(fp, "\tassign\t%s_sel = 0;\n", master->c_str());
	fprintf(fp, "\tassign\t%s_addr= 0;\n", master->c_str());
	fprintf(fp, "\tassign\t%s_data= 0;\n", master->c_str());
}

bool	BUSINFO::ismember_of(MAPDHASH *phash) {
	if ((m_slist)&&(m_slist->size() > 0)) {
		for(unsigned i=0; i<m_slist->size(); i++)
			if ((*m_slist)[i]->p_phash == phash)
				return true;
	}

	if ((m_dlist)&&(m_dlist->size() > 0)) {
		for(unsigned i=0; i<m_dlist->size(); i++)
			if ((*m_dlist)[i]->p_phash == phash)
				return true;
	}

	for(unsigned i=0; i<m_plist->size(); i++)
		if ((*m_plist)[i]->p_phash == phash)
			return true;

	return false;
}

void	BUSINFO::writeout_bus_logic_v(FILE *fp) {
	PLIST::iterator	pp;

	if (m_plist->size() == 0) {
		fprintf(fp, 
			"\t\tassign\t%s_err   = 1\'b0;\n"
			"\t\tassign\t%s_stall = 1\'b0;\n"
			"\t\tinitial\t%s_ack   = 1\'b0;\n"
			"\t\talways @(posedge %s)\n\t\t\t%s_ack   <= 1\'b0;\n"
			"\t\tassign\t%s_idata = 1\'b0;\n",
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_clock->m_name->c_str(),
			m_name->c_str(), m_name->c_str());
		return;
	} else if (m_plist->size() == 1) {
		fprintf(fp,
			"\tassign\t%s_none_sel = 1\'b0;\n"
			"\tassign\t%s_many_sel = 1\'b0;\n"
			"\tinitial\t%s_many_ack = 1\'b0;\n"
			"\talways @(*)\n\t\t%s_many_ack = 1\'b0;\n",
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str());
		fprintf(fp,
			"\tassign\t%s_err = 1\'b0;\n"
			"\tassign\t%s_stall = %s_stall;\n"
			"\tinitial\t%s_ack = 1\'b0;\n"
			"\talways @(*)\n\t\t%s_ack = %s_ack;\n"
			"\talways @(*)\n\t\t%s_idata = %s_data;\n",
			m_name->c_str(),
			m_name->c_str(), (*m_plist)[0]->p_name->c_str(),
			m_name->c_str(),
			m_name->c_str(), (*m_plist)[0]->p_name->c_str(),
			m_name->c_str(), (*m_plist)[0]->p_name->c_str());
		return;
	}

	// none_sel
	fprintf(fp, "\tassign\t%s_none_sel = (%s_stb)&&({\n",
		m_name->c_str(), m_name->c_str());
	for(unsigned k=0; k<m_plist->size(); k++) {
		fprintf(fp, "\t\t\t\t%s", (*m_plist)[k]->p_name->c_str());
		if (k != m_plist->size()-1)
			fprintf(fp, ",\n");

	} fprintf(fp, "} == 0);\n\n");

	// many_ack
	if (m_plist->size() < 2) {
		fprintf(fp, "\tinitial %s_many_ack = 1'b0;\n\talways @(posedge %s)\n\t\t%s_many_ack <= 1'b0;\n",
			m_name->c_str(),
			m_clock->m_wire->c_str(),
			m_name->c_str());
	} else {
		fprintf(fp,
"\t//\n"
"\t// many_ack\n"
"\t//\n"
"\t// It is also a violation of the bus protocol to produce multiply\n"
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
"\t\tcase({\t\t%s,\n", m_clock->m_wire->c_str(), (*m_plist)[0]->p_name->c_str());
		for(unsigned k=1; k<m_plist->size(); k++) {
			fprintf(fp, "\t\t\t\t%s", (*m_plist)[k]->p_name->c_str());
			if (k != m_plist->size()-1)
				fprintf(fp, ",\n");
		} fprintf(fp, "})\n");

		fprintf(fp, "\t\t\t%d\'b%0*d: %s_many_ack <= 1\'b0;\n",
			(int)m_plist->size(), (int)m_plist->size(), 0,
			m_name->c_str());
		for(unsigned k=0; k<m_plist->size(); k++) {
			fprintf(fp, "\t\t\t%d\'b", (int)m_plist->size());
			for(unsigned j=0; j<m_plist->size(); j++)
				fprintf(fp, "%s", (j==k)?"1":"0");
			fprintf(fp, ": %s_many_ack <= 1\'b0;\n",
				m_name->c_str());
		}

		fprintf(fp, "\t\t\tdefault: %s_many_ack <= (%s_cyc);\n",
			m_name->c_str(), m_name->c_str());
		fprintf(fp, "\t\tendcase\n\n");
	}



	// Start with the slist
	if (m_slist) {
		fprintf(fp, "\tassign\t_%s_sio_stall = 1\'b0;\n", m_name->c_str());
		fprintf(fp, "\tinitial _r_%s_sio_ack = 1\'b0;\n"
			"\talways\t@(posedge %s)\n"
			"\t\t_r_%s_sio_ack <= (%s_stb)&&(_%s_sio_sel);\n",
				m_name->c_str(),
				m_clock->m_wire->c_str(),
				m_name->c_str(),
				m_name->c_str(), m_name->c_str());
		fprintf(fp, "\tassign\t_%s_sio_ack = _r_%s_sio_ack;\n",
				m_name->c_str(), m_name->c_str());
		fprintf(fp, "\treg\t_r_%s_sio_ack;\n",
				m_name->c_str());

		unsigned mask = 0, unused_lsbs = 0, lgdw;
		for(unsigned k=0; k<m_slist->size(); k++) {
			mask |= (*m_slist)[k]->p_mask;
		} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0)); tmp >>= 1)
			unused_lsbs++;
		lgdw = nextlg(m_data_width)-3;

		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\t\t// mask        = %08x\n"
			"\t\t// lgdw        = %d\n"
			"\t\t// unused_lsbs = %d\n"
			"\t\tcasez( %s_addr[%d:%d] )\n",
				m_clock->m_wire->c_str(),
				mask, lgdw, unused_lsbs,
				m_name->c_str(),
				nextlg(mask)-1, unused_lsbs);
		for(unsigned j=0; j<m_slist->size()-1; j++) {
			fprintf(fp, "\t\t\t%d'h%lx: _%s_sio_idata <= %s_data;\n",
				nextlg(mask)-unused_lsbs,
				((*m_slist)[j]->p_base) >> (unused_lsbs + lgdw),
				m_name->c_str(),
				(*m_slist)[j]->p_name->c_str());
		}

		fprintf(fp, "\t\t\tdefault: %s_idata <= %s_data;\n",
			m_name->c_str(),
			(*m_slist)[m_slist->size()-1]->p_name->c_str());
		fprintf(fp, "\t\tendcase\n\n");
	}

	// Then the dlist
	if (m_dlist) {
		fprintf(fp, "\tassign\t_%s_dio_stall = 1\'b0;\n", m_name->c_str());
		fprintf(fp, "\treg\t[1:0]\t_r_%s_dio_ack;\n",
				m_name->c_str());
		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\t\t_r%s_ack <= { _r_%s_dio_ack[0], (%s_stb)&&(_r_%s_dio_sel) };\n",
				m_clock->m_wire->c_str(), m_name->c_str(),
				m_name->c_str(), m_name->c_str(),
				m_name->c_str());
		fprintf(fp, "\tassign\t_%s_dio_ack = _r_%s_dio_ack[1];\n", m_name->c_str(), m_name->c_str());
		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\t\tcasez({\n",
			m_clock->m_wire->c_str());
		for(unsigned k=0; k<m_dlist->size()-1; k++) {
			fprintf(fp, "\t\t\t\t%s%s", (*m_dlist)[k]->p_name->c_str(),
				(k == m_dlist->size()-2)?"":",\n");
		} fprintf(fp, "\t}) // %s default\n",
			(*m_dlist)[m_dlist->size()-1]->p_name->c_str());
		for(unsigned k=0; k<m_dlist->size()-1; k++) {
			fprintf(fp, "\t\t\t%d\'b", (int)m_dlist->size()-1);
			for(unsigned j=0; j<m_dlist->size()-1; j++)
				fprintf(fp, (k==j)?"1":((k>j)?"0":"?"));
			fprintf(fp, ": %s_idata <= %s_data;\n", m_name->c_str(),
				(*m_dlist)[k]->p_name->c_str());
		} fprintf(fp, "\t\t\tdefault: %s_idata <= %s_data;\n\n",
			m_name->c_str(),
			(*m_dlist)[m_dlist->size()-1]->p_name->c_str());
		fprintf(fp, "\t\tendcase\n\n");
	}



	fprintf(fp, ""
	"\t//\n"
	"\t// Finally, determine what the response is from the %s bus\n"
	"\t// bus\n"
	"\t//\n"
	"\t//\n", m_name->c_str());


	fprintf(fp, ""
	"\t//\n"
	"\t// %s_ack\n"
	"\t//\n"
	"\t// The returning wishbone ack is equal to the OR of every component that\n"
	"\t// might possibly produce an acknowledgement, gated by the CYC line.\n"
	"\t//\n"
	"\t// To return an ack here, a component must have a @SLAVE.TYPE tag.\n"
	"\t// Acks from any @SLAVE.TYPE of SINGLE and DOUBLE components have been\n"
	"\t// collected together (above) into _%s_sio_ack and _%s_dio_ack\n"
	"\t// respectively, which will appear ahead of any other device acks.\n"
	"\t//\n",
	m_name->c_str(), m_name->c_str(), m_name->c_str());

	fprintf(fp, "\talways @(posedge %s)\n\t\t%s_ack <= "
			"(%s_cyc)&&(|{ ",
			m_clock->m_wire->c_str(),
			m_name->c_str(), m_name->c_str());
	for(unsigned k=0; k<m_plist->size()-1; k++)
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
	"\t// Any peripheral component with a @SLAVE.TYPE value will be listed\n"
	"\t// here.\n"
	"\t//\n", m_name->c_str());

	if ((m_plist)&&(m_plist->size() > 0)) {
		fprintf(fp, "\talways @(posedge %s)\n"
			"\tbegin\n"
			"\t\tcasez({\t%s_ack%s\n\t\t\t\t",
				m_clock->m_wire->c_str(),
			(*m_plist)[0]->p_name->c_str(),
			((m_plist->size() > 1)?",":""));
			for(unsigned i=1; i<m_plist->size(); i++) {
				fprintf(fp, "%s_ack",
					(*m_plist)[i]->p_name->c_str());
				if (i != m_plist->size()-1)
					fprintf(fp, ",\n\t\t\t\t");
			} fprintf(fp, "\t})\n");
				
		for(unsigned i=0; i<m_plist->size()-1; i++) {
			fprintf(fp, "\t\t\t%d\'b", (int)m_plist->size()-1);
			for(unsigned j=0; j<m_plist->size()-1; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			fprintf(fp, ": %s_idata <= %s_data;\n",
				m_name->c_str(),
				(*m_plist)[i]->p_name->c_str());
		}
		fprintf(fp, "\t\t\tdefault: %s_idata <= %s_data;\n",
			m_name->c_str(),
			(*m_plist)[(m_plist->size()-1)]->p_name->c_str());
		fprintf(fp, "\t\tendcase\n\tend\n");
	} else
		fprintf(fp, "\talways @(posedge %s)\n"
			"\t\t%s_idata <= 32\'h0\n",
			m_clock->m_wire->c_str(),
			m_name->c_str());
}

void	writeout_bus_logic_v(FILE *fp) {
	for(unsigned i=0; i<gbl_blist->size(); i++) {
		fprintf(fp, "\t//\n\t// BUS-LOGIC for %s\n\t//\n",
			(*gbl_blist)[i]->m_name->c_str());
		(*gbl_blist)[i]->writeout_bus_logic_v(fp);
	}
}

void	assign_bus_slave(MAPDHASH &master, MAPDHASH *bus_slave) {
	MAPDHASH::iterator sbus;
	BUSINFO	*bi;
	STRINGP	pname, strp;
	MAPT	elm;
	MAPDHASH	*shash, *parent;

	pname = getstring(*bus_slave, KYPREFIX);
	if (pname == NULL)
		pname = new STRING("(Null)");

	shash = getmap(*bus_slave, KYSLAVE);
	if (NULL == shash)
		return;

	sbus = findkey(*shash, KYBUS);
	if (sbus == shash->end()) {
		bi = (*gbl_blist)[0];
		elm.m_typ = MAPT_MAP;
		elm.u.m_m = bi->m_hash;
		shash->insert(KEYVALUE(KYBUS, elm));
	} else if (sbus->second.m_typ == MAPT_STRING) {
		BUSINFO	*bi;
		char	*cstr, *tok, *nxt;
		const char	*DELIMITERS=", \t\r\n";

		strp = sbus->second.u.m_s;
		cstr = strdup(strp->c_str());

		tok = strtok(cstr, DELIMITERS);
		nxt = strtok(NULL, DELIMITERS);
		free(cstr);
		if ((tok)&&(!nxt)) {
			bi = gbl_blist->find_bus(strp);
			sbus->second.m_typ = MAPT_MAP;
			sbus->second.u.m_m = bi->m_hash;
		} else if (tok) {
			sbus->second.m_typ = MAPT_MAP;
			sbus->second.u.m_m = new MAPDHASH();
			parent = sbus->second.u.m_m;

			cstr = strdup(strp->c_str());
			tok = strtok(cstr, DELIMITERS);
			while(tok) {
				bi = gbl_blist->find_bus(new STRING(tok));
				if (NULL == bi) {
					fprintf(stderr, "ERR: BUS %s not found\n", tok);
					gbl_err++;
				} else {
					MAPT	elm;

					elm.m_typ = MAPT_MAP;
					elm.u.m_m = bi->m_hash;
					parent->insert(KEYVALUE(STRING(tok),
						elm));
				}
				//
				tok = strtok(NULL, DELIMITERS);
			} free(cstr);
		}
	} else {
		gbl_err++;
		fprintf(gbl_dump,
			"ERR: %s MASTER.BUS exists, but isn't a string!\n",
			pname->c_str());
		fprintf(stderr,
			"ERR: %s MASTER.BUS exists, but isn't a string!\n",
			pname->c_str());
	}
}

void	assign_bus_master(MAPDHASH &master, MAPDHASH *bus_master) {
	MAPDHASH::iterator mbus;
	BUSINFO		*bi;
	STRINGP		pname, strp;
	MAPDHASH	*mhash, *parent;
	MAPT		elm;

	pname = getstring(*bus_master, KYPREFIX);
	if (pname == NULL)
		pname = new STRING("(Null)");

fprintf(gbl_dump, "ASSIGN-BUS-MASTER: Attempting to change the BUS MASTER of %s\n", pname->c_str());
fflush(gbl_dump);

	mhash = getmap(*bus_master, KYMASTER);
	if (NULL == mhash)
		return;

	mbus = findkey(*mhash, KYBUS);
	if (mbus == mhash->end()) {
		bi = (*gbl_blist)[0];
		elm.m_typ = MAPT_MAP;
		elm.u.m_m = bi->m_hash;
		mhash->insert(KEYVALUE(KYBUS, elm));
	} else if (mbus->second.m_typ == MAPT_STRING) {
		char	*cstr, *tok, *nxt;
		const char	*DELIMITERS=", \t\r\n";

		strp = mbus->second.u.m_s;
		cstr = strdup(strp->c_str());

		tok = strtok(cstr, DELIMITERS);
		nxt = strtok(NULL, DELIMITERS);
		free(cstr);
		if ((tok)&&(!nxt)) {
fprintf(gbl_dump, "ASSIGN-BUS-MASTER: Single bus master only\n");
fflush(gbl_dump);
			bi = gbl_blist->find_bus(strp);
			assert(bi);
			mbus->second.m_typ = MAPT_MAP;
			mbus->second.u.m_m = bi->m_hash;
fprintf(gbl_dump, "ASSIGN-BUS-MASTER: HASH WAS\n");
mapdump(gbl_dump, *bi->m_hash);
		} else if (tok) {
fprintf(gbl_dump, "ASSIGN-BUS-MASTER: Multiple bus masters\n");
fflush(gbl_dump);
			mbus->second.m_typ = MAPT_MAP;
			mbus->second.u.m_m = new MAPDHASH();
			parent = mbus->second.u.m_m;

			cstr = strdup(strp->c_str());
			tok = strtok(cstr, DELIMITERS);
			while(tok) {
				bi = gbl_blist->find_bus(new STRING(tok));
				MAPT	elm;

				elm.m_typ = MAPT_MAP;
				elm.u.m_m = bi->m_hash;
				parent->insert(KEYVALUE(STRING(tok), elm));
				//
				tok = strtok(NULL, DELIMITERS);
			} free(cstr);
		}
	} else {
		gbl_err++;
		fprintf(gbl_dump,
			"ERR: %s MASTER.BUS exists, but isn't a string!\n",
			pname->c_str());
		fprintf(stderr,
			"ERR: %s MASTER.BUS exists, but isn't a string!\n",
			pname->c_str());
	}

fprintf(gbl_dump, "ASSIGN-BUS-MASTER: Completed\n");
fflush(gbl_dump);
}

void	build_bus_list(MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	BUSLIST	*bl = new BUSLIST;
	STRINGP	str;

	gbl_blist = bl;

	if (gbl_dump)
		fprintf(gbl_dump, "------------ BUILD-BUS-LIST------------\n");


	if (NULL != (str = getstring(master, KYDEFAULT_BUS))) {
		if (gbl_dump) fprintf(gbl_dump, "Found default bus: %s\n", str->c_str());
		bl->adddefault(master, str);
		if (gbl_dump) fprintf(gbl_dump, "Default bus: %s\n", str->c_str());
	}
else { if (gbl_dump) fprintf(gbl_dump, "NO default bus found\n"); }

fflush(gbl_dump);

	//
	if (refbus(master))
		bl->addbus(master, &master);
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (NULL != getstring(*kvpair->second.u.m_m, KYBUS))
			continue;
		bl->addbus(master, kvpair->second);
	}

fprintf(gbl_dump, "POST-ADD-BUS-------------------------------\n");
mapdump(gbl_dump, master);

	// Let's now go back through our components, and create a SLAVE.BUS
	// and (possibly) a MASTER.BUS tags for each component.
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isbusmaster(kvpair->second)) {
			assign_bus_master(master, kvpair->second.u.m_m);
		} if (isperipheral(kvpair->second)) {
			assign_bus_slave(master, kvpair->second.u.m_m);
		}
	}

	reeval(master);

	// Set any unset clocks to the default
	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++) {
		if (NULL == (*bp)->m_clock) {
			(*bp)->m_clock = getclockinfo(NULL);
			setstring((*bp)->m_hash, KYCLOCK, (*bp)->m_clock->m_name);
		}
	}

	//
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isperipheral(kvpair->second))
			continue;
		bl->countsio(kvpair->second);
	}

	//
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isperipheral(kvpair->second))
			continue;
		bl->addperipheral(kvpair->second);
	}
	reeval(master);

	for(unsigned i=0; i< bl->size(); i++) {
		(*bl)[i]->assign_addresses();
		reeval(master);
	}

	for(unsigned i=0; i< bl->size(); i++) {
		STRINGP	bname = (*bl)[i]->m_name;
		STRING	ky = (*bname) + KYBUS_AWID;
		int	value;
		value = (*bl)[i]->address_width();
		setvalue(master, ky, value);
	} gbl_blist = bl;

	reeval(master);
}
