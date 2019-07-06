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
#include "msgs.h"

BUSLIST	*gbl_blist;

#define	PREFIX

// Look up the number of bits in the address bus of the given hash
int	BUSINFO::address_width(void) {
	if (!m_addresses_assigned) {
		gbl_msg.info("Requesting address width of %s\n", m_name->c_str());
		assign_addresses();

		gbl_msg.info("Address width of %s is %d\n", m_name->c_str(), m_address_width);
	}
	return m_address_width;
}

int	BUSINFO::data_width(void) {
	int	value;

	if ((m_hash)&&(getvalue(m_hash, KY_WIDTH, value))) {
		m_data_width = value;
	} if (m_data_width > 0)
		return m_data_width;
	return	32;
}

bool	BUSINFO::get_base_address(MAPDHASH *phash, unsigned &base) {
	/*
	if ((m_slist)&&(m_slist->get_base_address(base)))
		return;
	if ((m_dlist)&&(m_dlist->get_base_address(base)))
		return;
	*/
	if (!m_plist) {
		gbl_msg.error("BUS[%s] has no peripherals!\n",
			m_name->c_str());
		return false;
	} else
		return m_plist->get_base_address(phash, base);
}

void	BUSINFO::assign_addresses(void) {

	gbl_msg.info("Assigning addresses for bus %s\n", m_name->c_str());
	if ((!m_plist)||(m_plist->size() < 1)) {
		m_address_width = 0;
	} else if (!m_addresses_assigned) {
		int	dw = data_width();

		if (m_slist)
			m_slist->assign_addresses(dw, 0);
		if (m_dlist)
			m_dlist->assign_addresses(dw, 0);
		m_plist->assign_addresses(dw, m_nullsz);
		m_address_width = m_plist->get_address_width();
		if (m_hash) {
			setvalue(*m_hash, KY_AWID, m_address_width);
			REHASH;
		}
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

		name = new STRING(STRING("" PREFIX) + (*m_name) + "_sio");
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
		// subp->p_master_bus = set by the slave to be sbi
		m_plist->add(subp);
		// m_plist->integrity_check();
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

		name = new STRING(STRING("" PREFIX) + (*m_name) + "_dio");
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
		// subp->p_master_bus = set by the slave to be dbi
		m_plist->add(subp);
		dbi->add();
		m_dlist = dbi->m_plist;
	} return m_dlist;
}


void BUSINFO::add(void) {
	if (!m_plist) {
		m_plist = new PLIST;
		m_plist->integrity_check();
	}
}

PERIPH *BUSINFO::add(PERIPHP p) {
	STRINGP	strp;
	int	pi;
	PLISTP	plist;

	if (!m_plist)
		m_plist = new PLIST();
	else
		m_plist->integrity_check();
	plist = m_plist;

	if ((p->p_phash)&&(NULL != (strp = getstring(*p->p_phash, KYSLAVE_TYPE)))) {
		if (strp->compare(KYSINGLE)==0) {
			plist = create_sio();
		} else if (strp->compare(KYDOUBLE)==0) {
			plist = create_dio();
		}
	}

	pi = plist->add(p);
	plist->integrity_check();
	if (pi >= 0) {
		p = (*plist)[pi];
		p->p_slave_bus = this;
assert(NULL != p->p_phash);
		return p;
	} else {
		gbl_msg.fatal("Could not add %s\n",
			getstring(p->p_phash, KYPREFIX)->c_str());
		return NULL;
	}
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

	plist->integrity_check();
	pi = plist->add(phash);
	plist->integrity_check();
	m_plist->integrity_check();
	if (pi >= 0) {
		p = (*plist)[pi];
		p->p_slave_bus = this;
		return p;
	} else {
		gbl_msg.error("Could not add %s\n", getstring(phash, KYPREFIX)->c_str());
		return NULL;
	}
}

void	BUSINFO::post_countsio(void) {
	gbl_msg.info("BUSINFO::POST-COUNTSIO[%4s]: SINGLE=%2d, DOUBLE=%2d, TOTAL=%2d (PRE)\n",
		m_name->c_str(), m_num_single, m_num_double, m_num_total);
	if (m_num_total == m_num_single + m_num_double) {
		m_num_double = 0;
	} if (m_num_total + m_num_single + m_num_double < 8) {
		m_num_single = 0;
		m_num_double = 0;
	}
	gbl_msg.info("BUSINFO::POST-COUNTSIO[%4s]: SINGLE=%2d, DOUBLE=%2d, TOTAL=%2d\n",
		m_name->c_str(), m_num_single, m_num_double, m_num_total);
}

void	BUSINFO::countsio(MAPDHASH *phash) {
	STRINGP	strp;

	strp = getstring(phash, KYSLAVE_TYPE);
	if (NULL != strp) {
		if (0==strp->compare(KYSINGLE)) {
			m_num_single++;
		} else if (0==strp->compare(KYDOUBLE)) {
			m_num_double++;
		} m_num_total++;
		// else if (str->compare(KYMEMORY))
		//	m_num_memory++;
	} else
		m_num_total++;	// Default to OTHER if no type is given
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

PERIPHP	BUSINFO::operator[](unsigned k) {
	unsigned	idx = k;

	if (m_slist) {
		if (idx < m_slist->size())
			return (*m_slist)[idx];
		else
			idx -= m_slist->size();
	}

	if (m_dlist) {
		if (idx < m_dlist->size())
			return (*m_dlist)[idx];
		else
			idx -= m_dlist->size();
	}

	if (idx < m_plist->size())
		return (*m_plist)[idx];

	return NULL;
}

unsigned	BUSINFO::size(void) {
	unsigned sz = m_plist->size();
	if (m_slist)
		sz += m_slist->size();
	if (m_dlist)
		sz += m_dlist->size();

	return sz;
}

void	BUSINFO::init(MAPDHASH *phash, MAPDHASH *bp) {
	int	value;
	MAPT	elm;
	STRINGP	strp, prefix;

	prefix = getstring(*phash, KYPREFIX);
	if (NULL == prefix)
		prefix = getstring(*phash, KY_NAME);
	if (NULL == prefix)
		prefix = new STRING("(Unknown prefix)");
	for(MAPDHASH::iterator kvpair=bp->begin(); kvpair != bp->end();
				kvpair++) {
		if (0 == KY_WIDTH.compare(kvpair->first)) {
			if (getvalue(*bp, KY_WIDTH, value)) {
				if (m_data_width <= 0) {
					m_data_width = value;
					//
					elm.m_typ = MAPT_INT;
					elm.u.m_v = value;
					m_hash->insert(KEYVALUE(KY_WIDTH, elm));
					gbl_msg.info("Setting bus width for %s bus to %d, in %s\n", m_name->c_str(), value, prefix->c_str());

					// Calculate the number of select lines
					elm.m_typ = MAPT_INT;
					elm.u.m_v = (value+7)/8;
					m_hash->insert(KEYVALUE(KY_NSELECT, elm));
					REHASH;
					kvpair = bp->begin();
				} else if (m_data_width != value) {
					gbl_msg.error("Conflicting bus width definitions for %s: %d != %d in %s\n", m_name->c_str(), m_data_width, value, prefix->c_str());
				}
			}
			continue;
		} if (0== KY_NULLSZ.compare(kvpair->first)) {
			if ((getvalue(*bp, KY_NULLSZ, value))&&(m_nullsz != value)) {
				gbl_msg.info("BUSINFO::INIT(%s).NULLSZ "
					"FOUND: %d\n", prefix->c_str(), value);
				m_nullsz = value;
				// m_addresses_assigned = false;
				//
				elm.m_typ = MAPT_INT;
				elm.u.m_v = value;
				m_hash->insert(KEYVALUE(KY_NULLSZ, elm));

				REHASH;
				kvpair = bp->begin();
			}
			continue;
		}

		if ((kvpair->second.m_typ == MAPT_STRING)
				&&(NULL != kvpair->second.u.m_s)) {
			strp = kvpair->second.u.m_s;
			gbl_msg.info("BUSINFO::INIT(%s) @%s=%s\n",
				prefix->c_str(), kvpair->first.c_str(),
				kvpair->second.u.m_s->c_str());
			if (KY_TYPE.compare(kvpair->first)==0) {
				if (m_type == NULL) {
					m_type = strp;
					elm.m_typ = MAPT_STRING;
					elm.u.m_s = strp;
					if (NULL == m_hash) {
						gbl_msg.error("Undefined bus as a part of %s (no name given?)\n",
							prefix->c_str());
					} else
						m_hash->insert(KEYVALUE(KY_TYPE, elm));

					REHASH;
					kvpair = bp->begin();
				} else if (m_type->compare(*strp) != 0) {
					gbl_msg.error("Conflicting bus types "
						"for %s\n",m_name->c_str());
				}
				continue;
			} else if ((KY_CLOCK.compare(kvpair->first)==0)
					&&(NULL == m_clock)) {
				gbl_msg.info("BUSINFO::INIT(%s)."
					"CLOCK(%s)\n", prefix->c_str(),
					strp->c_str());
				assert(strp);
				m_clock = getclockinfo(strp);
				elm.m_typ = MAPT_MAP;
				if (m_clock != NULL) {
					gbl_msg.info("BUSINFO::INIT(%s)."
						"CLOCK(%s) FOUND, FREQ = %d\n",
						prefix->c_str(), strp->c_str(),
						m_clock->frequency());
					elm.m_typ = MAPT_MAP;
					elm.u.m_m = m_clock->m_hash;

					if (NULL == m_hash) {
						gbl_msg.error("Undefined bus as a part of %s (no name given?)\n",
							prefix->c_str());
					} else
						m_hash->insert(KEYVALUE(KY_CLOCK, elm));
					kvpair->second.m_typ = MAPT_MAP;
					kvpair->second.u.m_m = m_clock->m_hash;
					REHASH;
					kvpair = bp->begin();
				}
				continue;
			}
		}

		// Anything else, we copy into our defining hash
		if ((bp != m_hash)&&(m_hash->end()
				!= findkey(*m_hash, kvpair->first))) {
			m_hash->insert(*kvpair);
			// REHASH;
		}
	}
}

void	BUSINFO::integrity_check(void) {
	if (!m_name) {
		gbl_msg.error("ERR: Bus with no name! (No BUS.NAME tag)\n");
		return;
	}

	if (m_data_width <= 0) {
		gbl_msg.error("ERR: BUS width not defined for %s\n",
			m_name->c_str());
	}

	if (m_clock == NULL) {
		gbl_msg.error("ERR: BUS %s has no defined clock\n",
			m_name->c_str());
	}
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
	if (!hash) {
		if ((*this)[0])
			return ((*this)[0]);
		return NULL;
	}
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
	// If no name is given, return the default bus
	if (NULL == name) {
		if ((*this)[0])
			return ((*this)[0]);
		return NULL;
	}
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
		if (!bi) {
			gbl_msg.fatal("Could not find %s bus, needed by %s\n", kvpair->second.u.m_s->c_str(), prefix->c_str());
		}
	} else {
		gbl_msg.fatal("Internal error\n");
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
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		bi = find_bus(kvpair->second.u.m_m);
		if (bi == NULL) {
			MAPDHASH	*blmap = kvpair->second.u.m_m;
			for(kvbus = blmap->begin(); kvbus != blmap->end();
					kvbus++) {
				if (kvbus->second.m_typ != MAPT_MAP)
					continue;
				bi = find_bus(kvbus->second.u.m_m);
				if (NULL == bi)
					continue;
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

	if (NULL != bi->m_slist)
		bi->m_slist->integrity_check();
	if (NULL != bi->m_dlist)
		bi->m_dlist->integrity_check();
	bi->m_plist->integrity_check();
}
void	BUSLIST::addperipheral(MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		addperipheral(map.u.m_m);
}

void	BUSLIST::adddefault(MAPDHASH &master, STRINGP defname) {
	BUSINFO	*bi;

	gbl_msg.userinfo("ADDING BUS: %s (default)\n", defname->c_str());
	if (size() == 0) {
		// If there are no elements (yet) in our BUSLIST, then create
		// a first one, and call it our default.
		push_back(bi = new BUSINFO());
		bi->m_name = defname;
		bi->m_hash = new MAPDHASH();
		setstring(*bi->m_hash, KY_NAME, defname);
	} else { // if (size() > 0)
		//
		// If there are already busses in our bus list, shuffle them
		// so that the bus named defname is the first.
		//
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

BUSINFO *BUSLIST::addbus_aux(MAPDHASH *phash, STRINGP pname, MAPDHASH *bp) {
	if (NULL == bp)
		return NULL;

	STRINGP	str;
	BUSINFO	*bi;

	str = getstring(*bp, KY_NAME);
	if (str) {
		bi = find_bus(str);
		if (!bi) {
			MAPDHASH::iterator	kvpair;

			gbl_msg.info("ADDBUS(%s) ... BUS.NAME KEY NOT FOUND(%s)\n",
			pname->c_str(), str->c_str());

			bi = new BUSINFO();
			push_back(bi);
			gbl_msg.info("ADDING BUS: %s\n", str->c_str());
			bi->m_name = str;
			bi->m_hash = bp;
			// setstring(*bi->m_hash, KY_NAME, str);
		// } else {
		//	fprintf(gbl_dump, "ADDBUS(%s) ... BUS FOUND(%s)\n",
		//		pname->c_str(), bi->m_name->c_str());
		}
	} else if (size() > 0) {
		gbl_msg.info("ADDING BUS: (Unnamed--becomes default)\n");
		bi = (*this)[0];
	} else {
		gbl_msg.error("Unnamed bus as a part of %s\n",
			(NULL == getstring(*phash, KYPREFIX)) ? "(Null-no-name)"
			: getstring(*phash, KYPREFIX)->c_str());
		bi = new BUSINFO();
		push_back(bi);
		bi->m_hash = NULL;
	}

	gbl_msg.info("ADDBUS-AUX(%s)--calling INIT for %s\n",
		pname->c_str(), bi->m_name->c_str());
	bi->init(phash, bp);

	return bi;
}

void	BUSLIST::addbus(MAPDHASH *phash) {
	MAPDHASH	*bp;
	STRINGP		str, pname;
	BUSINFO		*bi;
	MAPDHASH::iterator	kvbus;

	pname = getstring(phash, KYPREFIX);
	if (NULL == pname)
		pname = getstring(phash, KY_NAME);
	if (NULL == pname)
		pname = new  STRING("(Null/No name)");

	str = getstring(*phash, KYSLAVE_TYPE);
	if (true) {
		gbl_msg.info("ADDBUS request for \'%s\' ...", pname->c_str());
		if (isbusmaster(*phash))
			gbl_msg.info("MASTER ");
		if (isperipheral(*phash))
			gbl_msg.info("SLAVE ");
		if (NULL == str) {
			gbl_msg.info("NULL-TYPE ");
		} else if (KYBUS.compare(*str)==0)
			gbl_msg.info("BUS ");
		else if (KYOTHER.compare(*str)==0)
			gbl_msg.info("OTHER ");
		gbl_msg.info("\n");
	}

	kvbus = findkey(*phash, KYBUS);
	if (kvbus != phash->end()) {
		STRING tmps = STRING("(unknown)");
		if (MAPT_MAP == kvbus->second.m_typ)
			tmps = STRING("map");
		else if (MAPT_STRING == kvbus->second.m_typ)
			tmps = STRING("string");
		gbl_msg.info("ADDBUS(%s)...@BUS FOUND (%s)\n",
			pname->c_str(), tmps.c_str());
		if (MAPT_MAP == kvbus->second.m_typ) {
			bp = kvbus->second.u.m_m;
			bi = addbus_aux(phash, pname, bp);
			if ((NULL != bi)&&(kvbus->second.u.m_m != bi->m_hash))
				kvbus->second.u.m_m = bi->m_hash;
		} else if (MAPT_STRING == kvbus->second.m_typ) {
			bi = find_bus(kvbus->second.u.m_s);
			if (NULL == bi) {
				bi = new BUSINFO();
				push_back(bi);
				bi->m_name = kvbus->second.u.m_s;
				bi->m_hash = new MAPDHASH();
				setstring(bi->m_hash, KY_NAME, bi->m_name);
			}
			kvbus->second.m_typ = MAPT_MAP;
			kvbus->second.u.m_m = bi->m_hash;
		}
	}

	kvbus = findkey(*phash, KYSLAVE_BUS);
	if (kvbus != phash->end()) {
		if (MAPT_MAP == kvbus->second.m_typ) {
			bp = kvbus->second.u.m_m;
			bi = addbus_aux(phash, pname, bp);
			if ((NULL != bi)&&(kvbus->second.u.m_m != bi->m_hash))
				kvbus->second.u.m_m = bi->m_hash;
		} else if (MAPT_STRING == kvbus->second.m_typ) {
			bi = find_bus(kvbus->second.u.m_s);
			if (NULL == bi) {
				bi = new BUSINFO();
				push_back(bi);
				bi->m_name = kvbus->second.u.m_s;
				bi->m_hash = new MAPDHASH();
				setstring(bi->m_hash, KY_NAME, bi->m_name);
			}

			kvbus->second.m_typ = MAPT_MAP;
			kvbus->second.u.m_m = bi->m_hash;
		}
	}

	kvbus = findkey(*phash, KYMASTER_BUS);
	if (kvbus != phash->end()) {
		if (MAPT_MAP == kvbus->second.m_typ) {
			bp = kvbus->second.u.m_m;
			bi = addbus_aux(phash, pname, bp);
			if ((NULL != bi)&&(kvbus->second.u.m_m != bi->m_hash))
				kvbus->second.u.m_m = bi->m_hash;
		} else if (MAPT_STRING == kvbus->second.m_typ) {
			bi = find_bus(kvbus->second.u.m_s);
			if (NULL == bi) {
				bi = new BUSINFO();
				push_back(bi);
				bi->m_name = kvbus->second.u.m_s;
				bi->m_hash = new MAPDHASH();
				setstring(bi->m_hash, KY_NAME, bi->m_name);
			}
			kvbus->second.m_typ = MAPT_MAP;
			kvbus->second.u.m_m = bi->m_hash;
		}
	}
}

void	BUSLIST::addbus(MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		addbus(map.u.m_m);
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

void	BUSINFO::writeout_slave_defn_v(FILE *fp, const char* pname, const char *errwire, const char *btyp) {
	fprintf(fp, "\t// Wishbone slave definitions for bus %s%s, slave %s\n",
		m_name->c_str(), btyp, pname);
	fprintf(fp, "\twire\t\t%s_sel, %s_ack, %s_stall",
			pname, pname, pname);
	if ((errwire)&&(errwire[0] != '\0'))
		fprintf(fp, ", %s;\n", errwire);
	else
		fprintf(fp, ";\n");
	fprintf(fp, "\twire\t[%d:0]\t%s_data;\n\n", data_width()-1, pname);
}

void	BUSINFO::writeout_bus_slave_defns_v(FILE *fp) {
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

	if (m_plist) {
		for(PLIST::iterator pp=m_plist->begin(); pp != m_plist->end(); pp++) {
			STRINGP	errwire = getstring((*pp)->p_phash, KYERROR_WIRE);
			writeout_slave_defn_v(fp, (*pp)->p_name->c_str(),
				(errwire) ? errwire->c_str() : NULL);
		}
	} else {
		gbl_msg.error("%s has no slaves\n", m_name->c_str());
	}
}

void	BUSINFO::writeout_bus_master_defns_v(FILE *fp) {
	unsigned aw = address_width();
	fprintf(fp, "\t// Wishbone master wire definitions for bus: %s\n",
		m_name->c_str());
	fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we, %s_stall, %s_err,\n"
			"\t\t\t%s_none_sel;\n"
			"\treg\t\t%s_many_ack;\n"
			"\twire\t[%d:0]\t%s_addr;\n"
			"\twire\t[%d:0]\t%s_data;\n"
			"\treg\t[%d:0]\t%s_idata;\n"
			"\twire\t[%d:0]\t%s_sel;\n"
			"\treg\t\t%s_ack;\n\n",
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str(),
			aw-1,
			m_name->c_str(),
			data_width()-1, m_name->c_str(),
			data_width()-1, m_name->c_str(),
			(data_width()/8)-1,
			m_name->c_str(), m_name->c_str());
}

void	BUSINFO::writeout_bus_defns_v(FILE *fp) {
	writeout_bus_master_defns_v(fp);
	writeout_bus_slave_defns_v(fp);
}

void	writeout_bus_defns_v(FILE *fp) {
	fprintf(fp, "\t//\n\t//\n\t// Define bus wires\n\t//\n\t//\n\n");
	BUSLIST	*bl = gbl_blist;
	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++) {
		fprintf(fp, "\t// Bus %s\n", (*bp)->m_name->c_str());
		(*bp)->writeout_bus_defns_v(fp);
	}
}

void	BUSINFO::write_addr_range(FILE *fp, const PERIPHP p, const int dalines) {
	unsigned w = address_width();
	w = (w+3)/4;
	if (p->p_naddr == 1)
		fprintf(fp, " // 0x%0*lx", w, p->p_base);
	else
		fprintf(fp, " // 0x%0*lx - 0x%0*lx",
			w, p->p_base, w, p->p_base + (p->p_naddr<<(dalines))-1);

}

void	BUSINFO::writeout_bus_select_v(FILE *fp) {
	STRING	addrbus = STRING(*m_name)+"_addr";
	unsigned	sbaw = address_width();
	unsigned	dw   = data_width();
	unsigned	dalines = nextlg(dw/8);
	unsigned	unused_lsbs = 0, mask = 0;

	if (NULL == m_plist) {
		gbl_msg.error("Bus[%s] has no peripherals\n", m_name->c_str());
		return;
	}

	fprintf(fp, "\t//\n\t//\n\t//\n\t// Select lines for bus: %s\n\t//\n", m_name->c_str());
	fprintf(fp, "\t// Address width: %d\n",
		m_plist->get_address_width());
	fprintf(fp, "\t// Data width:    %d\n\t//\n\t//\n\t\n",
		m_data_width);
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
					(*m_slist)[i]->p_name->c_str(), m_name->c_str(),
					m_name->c_str(),
					sbaw-1, unused_lsbs,
					sbaw-unused_lsbs,
					(sbaw-unused_lsbs+3)/4,
					(*m_slist)[i]->p_base >> (unused_lsbs+dalines));
					write_addr_range(fp, (*m_slist)[i], dalines);
					fprintf(fp, "\n");
			}
		}

	} if (m_dlist) {
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
				m_name->c_str(),
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
	for(unsigned i=0; i<m_plist->size(); i++)
		mask |= (*m_plist)[i]->p_mask;
	for(unsigned tmp=mask; ((tmp)&&((tmp&1)==0)); tmp>>=1)
		unused_lsbs++;

	if (m_plist->size() == 1) {
		if ((*m_plist)[0]->p_name)
			fprintf(fp, "\tassign\t%12s_sel = (%s_cyc); "
					"// Only one peripheral on this bus\n",
				(*m_plist)[0]->p_name->c_str(),
				m_name->c_str());
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
	fprintf(fp, "\n");
	fprintf(fp, "\t// In the case that there is no %s peripheral responding on the %s bus\n", prefix->c_str(), m_name->c_str());
	fprintf(fp, "\tassign\t%s_ack   = (%s_stb) && (%s_sel);\n",
			prefix->c_str(), m_name->c_str(), prefix->c_str());
	fprintf(fp, "\tassign\t%s_stall = 0;\n", prefix->c_str());
	fprintf(fp, "\tassign\t%s_data  = 0;\n", prefix->c_str());
	fprintf(fp, "\n");
}

void	BUSINFO::writeout_no_master_v(FILE *fp) {
	if (!m_name)
		gbl_msg.error("(Unnamed bus) has no name!\n");
	fprintf(fp, "\n");
	fprintf(fp, "\t// In the case that nothing drives the %s bus ...\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_cyc = 1\'b0;\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_stb = 1\'b0;\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_we  = 1\'b0;\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_sel = 0;\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_addr= 0;\n", m_name->c_str());
	fprintf(fp, "\tassign\t%s_data= 0;\n", m_name->c_str());

	fprintf(fp, "\t// verilator lint_off UNUSED\n");
	fprintf(fp, "\twire\t[%d:0]\tunused_bus_%s;\n",
			3+m_data_width, m_name->c_str());
	fprintf(fp, "\tassign\tunused_bus_%s = "
		"{ %s_ack, %s_stall, %s_err, %s_data };\n",
		m_name->c_str(), m_name->c_str(), m_name->c_str(),
		m_name->c_str(), m_name->c_str());
	fprintf(fp, "\t// verilator lint_on  UNUSED\n");
	fprintf(fp, "\n");
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

	if (NULL == m_plist)
		return;

	if (m_plist->size() == 0) {
		// Since this bus has no slaves, any attempt to access it
		// needs to cause a bus error.
		fprintf(fp,
			"\t\t// No peripherals attached\n"
			"\t\tassign\t%s_err   = %s_stb;\n"
			"\t\tassign\t%s_stall = 1\'b0;\n"
			"\t\talways @(*)\n\t\t\t%s_ack   <= 1\'b0;\n"
			"\t\tassign\t%s_idata = 0;\n"
			"\tassign\t%s_none_sel = 1\'b1;\n",
			m_name->c_str(), m_name->c_str(),
			m_name->c_str(), m_name->c_str(),
			m_name->c_str(), m_name->c_str());
		return;
	} else if (m_plist->size() == 1) {
		STRINGP	strp;

		fprintf(fp,
			"\t\t// Only one peripheral attached\n"
			"\tassign\t%s_none_sel = 1\'b0;\n"
			"\talways @(*)\n\t\t%s_many_ack = 1\'b0;\n",
			m_name->c_str(), m_name->c_str());
		if (NULL != (strp = getstring((*m_plist)[0]->p_phash,
						KYERROR_WIRE))) {
			fprintf(fp, "\tassign\t%s_err = %s;\n",
				m_name->c_str(), strp->c_str());
		} else
			fprintf(fp, "\tassign\t%s_err = 1\'b0;\n",
				m_name->c_str());
		fprintf(fp,
			"\tassign\t%s_stall = %s_stall;\n"
			"\talways @(*)\n\t\t%s_ack = %s_ack;\n"
			"\talways @(*)\n\t\t%s_idata = %s_data;\n",
			m_name->c_str(), (*m_plist)[0]->p_name->c_str(),
			m_name->c_str(), (*m_plist)[0]->p_name->c_str(),
			m_name->c_str(), (*m_plist)[0]->p_name->c_str());
		return;
	}

	// none_sel
	fprintf(fp, "\tassign\t%s_none_sel = (%s_stb)&&({\n",
		m_name->c_str(), m_name->c_str());
	for(unsigned k=0; k<m_plist->size(); k++) {
		fprintf(fp, "\t\t\t\t%s_sel", (*m_plist)[k]->p_name->c_str());
		if (k != m_plist->size()-1)
			fprintf(fp, ",\n");

	} fprintf(fp, "} == 0);\n\n");

	// many_ack
	if (m_plist->size() < 2) {
		fprintf(fp, "\talways @(*)\n\t\t%s_many_ack <= 1'b0;\n",
			m_name->c_str());
	} else {
		fprintf(fp,
"\t//\n"
"\t// many_ack\n"
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
"\t\tcase({\t\t%s_ack,\n", m_clock->m_wire->c_str(), (*m_plist)[0]->p_name->c_str());
		for(unsigned k=1; k<m_plist->size(); k++) {
			fprintf(fp, "\t\t\t\t%s_ack", (*m_plist)[k]->p_name->c_str());
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
		fprintf(fp, "\treg\t\t" PREFIX "r_%s_sio_ack;\n",
				m_name->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_sio_data;\n\n",
			m_data_width-1, m_name->c_str());

		fprintf(fp, "\tassign\t" PREFIX "%s_sio_stall = 1\'b0;\n\n", m_name->c_str());
		fprintf(fp, "\tinitial " PREFIX "r_%s_sio_ack = 1\'b0;\n"
			"\talways\t@(posedge %s)\n"
			"\t\t" PREFIX "r_%s_sio_ack <= (%s_stb)&&(" PREFIX "%s_sio_sel);\n",
				m_name->c_str(),
				m_clock->m_wire->c_str(),
				m_name->c_str(),
				m_name->c_str(), m_name->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_sio_ack = " PREFIX "r_%s_sio_ack;\n\n",
				m_name->c_str(), m_name->c_str());

		unsigned mask = 0, unused_lsbs = 0, lgdw;
		for(unsigned k=0; k<m_slist->size(); k++) {
			mask |= (*m_slist)[k]->p_mask;
		} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0)); tmp >>= 1)
			unused_lsbs++;
		lgdw = nextlg(data_width())-3;

		fprintf(fp, "\talways\t@(posedge %s)\n"
			// "\t\t// mask        = %08x\n"
			// "\t\t// lgdw        = %d\n"
			// "\t\t// unused_lsbs = %d\n"
			"\tcasez( %s_addr[%d:%d] )\n",
				m_clock->m_wire->c_str(),
				// mask, lgdw, unused_lsbs,
				m_name->c_str(),
				nextlg(mask)-1, unused_lsbs);
		for(unsigned j=0; j<m_slist->size()-1; j++) {
			fprintf(fp, "\t\t%d'h%lx: " PREFIX "r_%s_sio_data <= %s_data;\n",
				nextlg(mask)-unused_lsbs,
				((*m_slist)[j]->p_base) >> (unused_lsbs + lgdw),
				m_name->c_str(),
				(*m_slist)[j]->p_name->c_str());
		}

		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_sio_data <= %s_data;\n",
			m_name->c_str(),
			(*m_slist)[m_slist->size()-1]->p_name->c_str());
		fprintf(fp, "\tendcase\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_sio_data = " PREFIX "r_%s_sio_data;\n\n",
			m_name->c_str(), m_name->c_str());
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
		lgdw = nextlg(data_width())-3;


		fprintf(fp, "\treg\t[1:0]\t" PREFIX "r_%s_dio_ack;\n",
				m_name->c_str());
		fprintf(fp, "\t// # dlist = %d, nextlg(#dlist) = %d\n",
			(int)m_dlist->size(),
			nextlg(m_dlist->size()));
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_dio_bus_select;\n",
			nextlg((int)m_dlist->size())-1, m_name->c_str());
		fprintf(fp, "\treg\t[%d:0]\t" PREFIX "r_%s_dio_data;\n",
			m_data_width-1, m_name->c_str());

		//
		// The stall line
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_stall = 1\'b0;\n", m_name->c_str());
		//
		// The ACK line
		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\tif (i_reset || !%s_cyc)\n"
			"\t\t" PREFIX "r_%s_dio_ack <= 0;\n"
			"\telse\n"
			"\t\t" PREFIX "r_%s_dio_ack <= { " PREFIX "r_%s_dio_ack[0], (%s_stb)&&(" PREFIX "%s_dio_sel) };\n",
				m_clock->m_wire->c_str(), m_name->c_str(),
				m_name->c_str(), m_name->c_str(),
				m_name->c_str(), m_name->c_str(),
				m_name->c_str());
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_ack = " PREFIX "r_%s_dio_ack[1];\n", m_name->c_str(), m_name->c_str());
		fprintf(fp, "\n");

		//
		// The data return lines
		fprintf(fp, "\talways @(posedge %s)\n"
			"\tcasez(%s_addr[%d:%d])\n",
				m_clock->m_wire->c_str(),
				m_name->c_str(),
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
				m_name->c_str(), nextlg(m_dlist->size()), k);
		}
		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_dio_bus_select <= 0;\n",
			m_name->c_str());
		fprintf(fp, "\tendcase\n\n");

		fprintf(fp, "\talways\t@(posedge %s)\n"
			"\tcasez(" PREFIX "r_%s_dio_bus_select)\n",
			m_clock->m_wire->c_str(), m_name->c_str());

		for(unsigned k=0; k<m_dlist->size()-1; k++) {
			fprintf(fp, "\t\t%d'd%d", nextlg(m_dlist->size()), k);
			fprintf(fp, ": " PREFIX "r_%s_dio_data <= %s_data;\n",
				m_name->c_str(),
				(*m_dlist)[k]->p_name->c_str());
		}

		fprintf(fp, "\t\tdefault: " PREFIX "r_%s_dio_data <= %s_data;\n",
			m_name->c_str(),
			(*m_dlist)[m_dlist->size()-1]->p_name->c_str());
		fprintf(fp, "\tendcase\n\n");
		fprintf(fp, "\tassign\t" PREFIX "%s_dio_data = " PREFIX "r_%s_dio_data;\n\n",
			m_name->c_str(), m_name->c_str());
	} else
		fprintf(fp, "\t//\n\t// No class DOUBLE peripherals on the \"%s\" bus\n\t//\n", m_name->c_str());



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
	"\t// collected together (above) into " PREFIX "%s_sio_ack and " PREFIX "%s_dio_ack\n"
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
	"\t// Any peripheral component with a @SLAVE.TYPE value of either OTHER\n"
	"\t// or MEMORY will automatically be listed here.  In addition, the\n"
	"\t// bus responses from @SLAVE.TYPE SINGLE (_sio_) and/or DOUBLE\n"
	"\t// (_dio_) may also be listed here, depending upon components are\n"
	"\t// connected to them.\n"
	"\t//\n", m_name->c_str());

	if ((m_plist)&&(m_plist->size() > 0)) {
		if (m_plist->size() == 1) {
			fprintf(fp, "\talways @(*)\n\t\t%s_idata <= %s_data;\n",
				m_name->c_str(),
				(*m_plist)[0]->p_name->c_str());
		} else if (m_plist->size() == 2) {
			fprintf(fp, "\talways @(posedge %s)\n\t\tif (%s_ack)\n\t\t\t%s_idata <= %s_data;\n\t\telse\n\t\t\t%s_idata <= %s_data;\n",
				m_clock->m_wire->c_str(),
				(*m_plist)[0]->p_name->c_str(),
				m_name->c_str(),
				(*m_plist)[0]->p_name->c_str(),
				m_name->c_str(),
				(*m_plist)[1]->p_name->c_str());
		} else {
			int	sbits = nextlg(m_plist->size());
			fprintf(fp,"\treg [%d:0]\t" PREFIX "r_%s_bus_select;\n",
				sbits-1, m_name->c_str());

			unsigned mask = 0, unused_lsbs = 0, lgdw, maskbits;
			for(unsigned k=0; k<m_plist->size(); k++) {
				mask |= (*m_plist)[k]->p_mask;
			} for(unsigned tmp=mask; ((tmp!=0)&&((tmp & 1)==0));
					tmp >>= 1)
				unused_lsbs++;
			maskbits = nextlg(mask)-unused_lsbs;
			// lgdw is the log of the data width in bytes.  We
			// require it here because the bus addresses words
			// rather than the bytes within them
			lgdw = nextlg(data_width())-3;

			fprintf(fp, "\talways\t@(posedge %s)\n"
				"\tif (%s_stb && ! %s_stall)\n"
				"\t\tcasez(" PREFIX "%s_addr[%d:%d])\n",
				m_clock->m_wire->c_str(),
				m_name->c_str(), m_name->c_str(),
				m_name->c_str(),
				unused_lsbs+maskbits-1, unused_lsbs);
			for(unsigned k=0; k<m_plist->size(); k++) {
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
					m_name->c_str(), sbits, k);
			}
			fprintf(fp, "\t\t\tdefault: begin end\n");
			fprintf(fp, "\t\tendcase\n\n");

			fprintf(fp, "\talways @(posedge %s)\n"
				"\tcasez(" PREFIX "r_%s_bus_select)\n",
					m_clock->m_wire->c_str(),
					m_name->c_str());

			for(unsigned i=0; i<m_plist->size(); i++) {
				fprintf(fp, "\t\t%d\'d%d", sbits, i);
				fprintf(fp, ": %s_idata <= %s_data;\n",
					m_name->c_str(),
					(*m_plist)[i]->p_name->c_str());
			}
			fprintf(fp, "\t\tdefault: %s_idata <= %s_data;\n",
				m_name->c_str(),
				(*m_plist)[(m_plist->size()-1)]->p_name->c_str());
			fprintf(fp, "\tendcase\n\n");
		}
	} else
		fprintf(fp, "\talways @(posedge %s)\n"
			"\t\t%s_idata <= 32\'h0\n",
			m_clock->m_wire->c_str(), m_name->c_str());

	// The stall line
	fprintf(fp, "\tassign\t%s_stall =\t((%s_sel)&&(%s_stall))",
		m_name->c_str(), (*m_plist)[0]->p_name->c_str(),
		(*m_plist)[0]->p_name->c_str());
	if (m_plist->size() <= 1)
		fprintf(fp, ";\n\n");
	else {
		for(unsigned i=1; i<m_plist->size(); i++)
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

		for(PLIST::iterator pp=m_plist->begin();
					pp != m_plist->end(); pp++) {
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
			m_name->c_str(), m_name->c_str(), m_name->c_str(),
			m_name->c_str());
		if (ecount == 0) {
			fprintf(fp, ";\n");
		} else if (ecount == 1) {
			fprintf(fp, "||%s);\n", err_bus.c_str());
		} else
			fprintf(fp, "\n\t\t\t||%s; // ecount = %d",
				err_bus.c_str(), ecount);
	}
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
	MAPDHASH	*shash, *parent;

	pname = getstring(*bus_slave, KYPREFIX);
	if (pname == NULL)
		pname = new STRING("(Null)");

	// Check for a more generic BUS tag
	if ((NULL != (strp = getstring(*bus_slave, KYBUS_NAME)))
		||(NULL != (strp = getstring(*bus_slave, KYBUS)))) {
		MAPDHASH::iterator	kvpair;

		bi = gbl_blist->find_bus(strp);
		if (!bi)
			bi = (*gbl_blist)[0];
		kvpair = findkey(*bus_slave, KYBUS);
		if (kvpair->second.m_typ == MAPT_MAP) {
			if (kvpair->second.u.m_m != bi->m_hash) {
				bi->init(bus_slave, kvpair->second.u.m_m);
				kvpair->second.u.m_m = bi->m_hash;
			}
		} else {
			kvpair->second.m_typ = MAPT_MAP;
			kvpair->second.u.m_m = bi->m_hash;
		}
		reeval(master);
	}

	shash = getmap(*bus_slave, KYSLAVE);
	if (NULL == shash)
		return;

	sbus = findkey(*shash, KYBUS);
	if (sbus == shash->end()) {
		// No SLAVE.BUS tag
		//
	} else if (sbus->second.m_typ == MAPT_STRING) {
		BUSINFO	*bi;
		char	*cstr, *tok, *nxt;
		const char	*DELIMITERS=", \t\r\n";

		strp = sbus->second.u.m_s;
		cstr = strdup(strp->c_str());

		tok = strtok(cstr, DELIMITERS);
		nxt = strtok(NULL, DELIMITERS);
		if ((tok)&&(!nxt)) {
			bi = gbl_blist->find_bus(strp);
			if (NULL == bi) {
				gbl_msg.error("BUS %s not found in %s\n",
					tok, pname->c_str());
			} else if (sbus->second.m_typ == MAPT_MAP) {
				if (sbus->second.u.m_m != bi->m_hash) {
					bi->init(bus_slave, sbus->second.u.m_m);
					sbus->second.u.m_m = bi->m_hash;
				}
			} else {
				sbus->second.m_typ = MAPT_MAP;
				sbus->second.u.m_m = bi->m_hash;
			}
		} else if (tok) {
			assert(0);
			sbus->second.m_typ = MAPT_MAP;
			sbus->second.u.m_m = new MAPDHASH();
			reeval(master);
			parent = sbus->second.u.m_m;

			cstr = strdup(strp->c_str());
			tok = strtok(cstr, DELIMITERS);
			while(tok) {
				bi = gbl_blist->find_bus(new STRING(tok));
				if (NULL == bi) {
					gbl_msg.error("ERR: BUS %s not found in %s\n",
						tok, pname->c_str());
				} else {
					MAPT	elm;

					elm.m_typ = MAPT_MAP;
					elm.u.m_m = bi->m_hash;
					reeval(master);
					parent->insert(KEYVALUE(STRING(tok),
						elm));
				}
				//
				tok = strtok(NULL, DELIMITERS);
			} free(cstr);
			reeval(master);
		} free(cstr);
	} else if (sbus->second.m_typ == MAPT_MAP) {
		if (NULL == (strp = getstring(*sbus->second.u.m_m, KY_NAME))) {
			gbl_msg.error("%s SLAVE.BUS "
				"exists, is a map, but has no name\n",
				pname->c_str());
		} else if (NULL == (bi = find_bus(strp))) {
			gbl_msg.error("%s SLAVE.BUS "
				"exists, is a map, but name %s not found\n",
				pname->c_str(), strp->c_str());
		} else if ((bi->m_hash != sbus->second.u.m_m)
					&&(strp->compare(*bi->m_name)==0)) {
			bi->init(bus_slave, sbus->second.u.m_m);
				sbus->second.u.m_m = bi->m_hash;
		}
	} else {
		gbl_msg.error("%s SLAVE.BUS exists, but isn't a string!\n",
			pname->c_str());
	}
}

void	assign_bus_master(MAPDHASH &master, MAPDHASH *bus_master) {
	MAPDHASH::iterator mbus;
	BUSINFO		*bi;
	STRINGP		pname, strp;
	MAPDHASH	*mhash, *parent;

	pname = getstring(*bus_master, KYPREFIX);
	if (pname == NULL)
		pname = new STRING("(Null)");

	// Check for a generic BUS tag
	if ((NULL != (strp = getstring(*bus_master, KYBUS_NAME)))
		||(NULL != (strp = getstring(*bus_master, KYBUS)))) {
		MAPDHASH::iterator	kvpair;

		bi = gbl_blist->find_bus(strp);
		if (!bi)
			bi = (*gbl_blist)[0];

		kvpair = findkey(*bus_master, KYBUS);
		if (kvpair->second.m_typ == MAPT_MAP) {
			if (kvpair->second.u.m_m != bi->m_hash) {
				bi->init(bus_master, kvpair->second.u.m_m);
				kvpair->second.u.m_m = bi->m_hash;
			}
		} else {
			kvpair->second.m_typ = MAPT_MAP;
			kvpair->second.u.m_m = bi->m_hash;
		}
		reeval(master);
	}

	mhash = getmap(*bus_master, KYMASTER);
	if (NULL == mhash)
		return;

	mbus = findkey(*mhash, KYBUS);
	if (mbus == mhash->end()) {
		// No MASTER.BUS tag
		//
	} else if (mbus->second.m_typ == MAPT_STRING) {
		char	*cstr, *tok, *nxt;
		const char	*DELIMITERS=", \t\r\n";

		strp = mbus->second.u.m_s;
		cstr = strdup(strp->c_str());

		tok = strtok(cstr, DELIMITERS);
		nxt = (tok) ? strtok(NULL, DELIMITERS) : NULL;
		if ((tok)&&(!nxt)) {
			bi = gbl_blist->find_bus(strp);
			if (NULL == bi) {
				gbl_msg.error("BUS %s not found in %s\n",
					tok, pname->c_str());
			} else if (mbus->second.m_typ == MAPT_MAP) {
				if (mbus->second.u.m_m != bi->m_hash) {
					bi->init(bus_master, mbus->second.u.m_m);
					mbus->second.u.m_m = bi->m_hash;
				}
			} else {
				mbus->second.m_typ = MAPT_MAP;
				mbus->second.u.m_m = bi->m_hash;
			}
			reeval(master);
		} else if (tok) {
			assert(0);
			mbus->second.m_typ = MAPT_MAP;
			mbus->second.u.m_m = new MAPDHASH();
			parent = mbus->second.u.m_m;

			cstr = strdup(strp->c_str());
			tok = strtok(cstr, DELIMITERS);
			while(tok) {
				bi = gbl_blist->find_bus(new STRING(tok));
				MAPT	elm;

				if (!bi) {
					gbl_msg.error("ERR: BUS %s not found in %s\n",
						tok, pname->c_str());
					continue;
				}

				elm.m_typ = MAPT_MAP;
				elm.u.m_m = bi->m_hash;
				reeval(master);
				parent->insert(KEYVALUE(STRING(tok), elm));
				//
				tok = strtok(NULL, DELIMITERS);
				reeval(master);
			}
		}
		free(cstr);
	} else if (mbus->second.m_typ == MAPT_MAP) {
		if (NULL == (strp = getstring(*mbus->second.u.m_m, KY_NAME))) {
			gbl_msg.error("%s MASTER.BUS "
				"exists, is a map, but has no name\n",
				pname->c_str());
		} else if (NULL == (bi = find_bus(strp))) {
			gbl_msg.error("ERR: %s MASTER.BUS "
				"exists, is a map, but name %s not found\n",
				pname->c_str(), strp->c_str());
		} else if ((bi->m_hash != mbus->second.u.m_m)
					&&(strp->compare(*bi->m_name)==0)) {
				bi->init(bus_master, mbus->second.u.m_m);
				mbus->second.u.m_m = bi->m_hash;
		}
	} else {
		gbl_msg.error("%s MASTER.BUS exists, but isn't a string!\n",
			pname->c_str());
	}
}

void	build_bus_list(MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	BUSLIST	*bl = new BUSLIST;
	STRINGP	str;

	gbl_blist = bl;

	gbl_msg.info("------------ BUILD-BUS-LIST------------\n");


	if (NULL != (str = getstring(master, KYDEFAULT_BUS))) {
		bl->adddefault(master, str);
	}

	//
	if (refbus(master)) {
		gbl_msg.info("Adding a refbus (master)\n");
		bl->addbus(&master);
	}
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		MAPDHASH	*mp;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (NULL != (mp = getmap(*kvpair->second.u.m_m, KYBUS)))
			bl->addbus(kvpair->second);
		else if (NULL != (mp = getmap(*kvpair->second.u.m_m, KYSLAVE_BUS)))
			bl->addbus(kvpair->second);
		else if (NULL != (mp = getmap(*kvpair->second.u.m_m, KYMASTER_BUS)))
			bl->addbus(kvpair->second);
	}

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
			MAPDHASH	*map;
			if (NULL != (str = getstring((*bp)->m_hash, KYCLOCK))) {
				(*bp)->m_clock = getclockinfo(str);
				MAPDHASH::iterator kvclock;
				kvclock = findkey(*(*bp)->m_hash, KYCLOCK);
				assert((*bp)->m_hash->end() != kvclock);
				kvclock->second.m_typ = MAPT_MAP;
				kvclock->second.u.m_m = (*bp)->m_clock->m_hash;
			} else if (NULL != (map = getmap((*bp)->m_hash, KYCLOCK))) {
				(*bp)->m_clock = getclockinfo(getstring(map, KY_NAME));
				MAPDHASH::iterator kvclock;
				kvclock = findkey(*(*bp)->m_hash, KYCLOCK);
				assert((*bp)->m_hash->end() != kvclock);
				kvclock->second.m_typ = MAPT_MAP;
				kvclock->second.u.m_m = (*bp)->m_clock->m_hash;
			} else {
				gbl_msg.fatal("Bus %s has no defined clock\n",
					(*bp)->m_name->c_str());
				(*bp)->m_clock = getclockinfo(NULL);
				assert((*bp)->m_hash);
				setstring((*bp)->m_hash, KYCLOCK, (*bp)->m_clock->m_name);
			}
		}
	}

	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++)
		(*bp)->integrity_check();

	//
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isperipheral(kvpair->second))
			continue;
		bl->countsio(kvpair->second);
	}

	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++) {
		(*bp)->post_countsio();
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

	reeval(master);
}

#ifdef	DUMP_BUS_TREE
void	BUSINFO::dump_bus_tree(int tab=0) {

	gbl_msg.info("%*sDUMP BUS-TREE: %s\n",
		tab, "", b_name->c_str());

	for(m_plist::iterator pp=m_plist->begin(); pp != m_plist->end(); pp++) {
		gbl_msg.info("%*s%s\n", tab+1, "", (*pp)->p_name->c_str());
	}
}

void	dump_global_buslist(void) {
	find_bus(0)->dump_bus_tree(0);
}
#endif
