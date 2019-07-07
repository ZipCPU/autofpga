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
	return m_genbus->address_width();
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
	if (!m_genbus) {
		gbl_msg.error("BUS[%s] has no type\n",
			m_name->c_str());
		return false;
	} else
		return m_genbus->get_base_address(phash, base);
}

void	BUSINFO::assign_addresses(void) {
	if (!m_genbus)
		gbl_msg.fatal("Bus %s has no generator type defined\n",
			m_name->c_str());

	gbl_msg.info("Assigning addresses for bus %s\n", m_name->c_str());
	m_genbus->assign_addresses();
	m_addresses_assigned = true;
}

void BUSINFO::add(void) {
	if (!m_plist) {
		m_plist = new PLIST;
		m_plist->integrity_check();
	}
}

PERIPH *BUSINFO::add(PERIPHP p) {
	int	pi;
	PLISTP	plist;

	if (!m_plist)
		m_plist = new PLIST();
	else
		m_plist->integrity_check();
	plist = m_plist;

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
	PERIPH	*p;
	int	pi;
	PLISTP	plist;

	if (!m_plist)
		m_plist = new PLIST();
	plist = m_plist;
	assert(plist);

	pi = plist->add(phash);
	if (pi >= 0) {
		p = (*plist)[pi];
		p->p_slave_bus = this;
		return p;
	} else {
		gbl_msg.error("Could not add %s\n", getstring(phash, KYPREFIX)->c_str());
		return NULL;
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

PERIPHP	BUSINFO::operator[](unsigned k) {
	unsigned	idx = k;

	if (idx < m_plist->size())
		return (*m_plist)[idx];

	return NULL;
}

unsigned	BUSINFO::size(void) {
	unsigned sz = m_plist->size();

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
				m_nullsz = (value > m_nullsz) ? value : m_nullsz;
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

void	BUSINFO::writeout_bus_defns_v(FILE *fp) {
	if (!m_genbus)
		gbl_msg.error("No bus type defined for bus %s\n", m_name->c_str());
	m_genbus->writeout_bus_defns_v(fp);
}

void	writeout_bus_defns_v(FILE *fp) {
	fprintf(fp, "\t//\n\t//\n\t// Define bus wires\n\t//\n\t//\n\n");
	BUSLIST	*bl = gbl_blist;
	for(BUSLIST::iterator bp=bl->begin(); bp != bl->end(); bp++) {
		fprintf(fp, "\t// Bus %s\n", (*bp)->m_name->c_str());
		(*bp)->writeout_bus_defns_v(fp);
	}
}

void	BUSINFO::writeout_no_slave_v(FILE *fp, STRINGP prefix) {
	if (!m_genbus)
		gbl_msg.error("No slaves assigned to bus %s\n", m_name->c_str());
	else
		m_genbus->writeout_no_slave_v(fp, prefix);
}

void	BUSINFO::writeout_no_master_v(FILE *fp) {
	if (!m_genbus)
		gbl_msg.error("No slaves assigned to bus %s\n",
			(m_name) ? m_name->c_str() : "(Unnamed-bus)");
	else
		m_genbus->writeout_no_master_v(fp);
}

bool	BUSINFO::ismember_of(MAPDHASH *phash) {
	for(unsigned i=0; i<m_plist->size(); i++)
		if ((*m_plist)[i]->p_phash == phash)
			return true;

	return false;
}

void	BUSINFO::writeout_bus_logic_v(FILE *fp) {
	if (!m_genbus)
		gbl_msg.error("No slaves assigned to bus %s\n", m_name->c_str());
	else
		m_genbus->writeout_bus_logic_v(fp);
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

void	BUSLIST::assign_bus_types(void) {
	if (num_bus_classes == 0)
		gbl_msg.warning("No bus classes defined!\n");
	for(unsigned k = 0; k < size(); k++) {
		bool	found= false;
		for(unsigned tst=0; tst<num_bus_classes; tst++) {
			if (busclass_list[tst]->matches((*this)[k])) {
				found = true;
				(*this)[k]->m_genbus = busclass_list[tst]->create((*this)[k]);
				break;
			}
		} if (!found) {
			gbl_msg.error("No bus logic generator found for bus %s\n", (*this)[k]->m_name->c_str());
		}
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
		bl->addperipheral(kvpair->second);
	}
	reeval(master);

	bl->assign_bus_types();

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
