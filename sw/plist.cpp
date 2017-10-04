////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	plist.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A PLIST is a list of peripherals.  This file defines the methods
//		associated with such a list.
//
//
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
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <ctype.h>

#include "parser.h"
#include "keys.h"
#include "kveval.h"
#include "plist.h"
#include "bitlib.h"
#include "businfo.h"
#include "predicates.h"
#include "subbus.h"
#include "globals.h"
#include "msgs.h"


bool	PERIPH::issingle(void) {
	STRINGP	str;

	if (!p_phash)
		return false;
	if (NULL == (str = getstring(*p_phash, KYSLAVE_TYPE)))
		return false;
	if (str->compare(KYSINGLE)!=0)
		return false;
	return true;
}

bool	PERIPH::isdouble(void) {
	STRINGP	str;

	if (!p_phash)
		return false;
	if (NULL == (str = getstring(*p_phash, KYSLAVE_TYPE)))
		return false;
	if (str->compare(KYDOUBLE)!=0)
		return false;
	return true;
}

bool	PERIPH::isbus(void) {
	STRINGP	str;

	if (!p_phash)
		return false;
	if (NULL == (str = getstring(*p_phash, KYSLAVE_TYPE)))
		return false;
	if (str->compare(KYBUS)!=0)
		return false;
	return true;
}

bool	PERIPH::ismemory(void) {
	STRINGP	str;

	if (!p_phash)
		return false;
	if (NULL == (str = getstring(*p_phash, KYSLAVE_TYPE)))
		return false;
	if (str->compare(KYMEMORY)!=0)
		return false;
	return true;
}

unsigned PERIPH::get_slave_address_width(void) {
	/*
	MAPDHASH::iterator	kvbus, kvvalue;
	kvbus = findkey(*p_phash, KYMASTER_BUS);
	if ((kvbus != p_phash->end())&&(kvbus->second.m_typ == MAPT_MAP)) {
		if (getstring(kvbus->
	}
	*/
	if (p_awid != nextlg(naddr()))
		p_awid = nextlg(naddr());
	return p_awid;
}

unsigned PERIPH::naddr(void) {
	int	value;

	assert(p_phash);
	if (getvalue(*p_phash, KYNADDR, value)) {
		if (0 == p_naddr) {
			p_naddr = value;
			p_awid  = nextlg(p_naddr);
		} else if ((int)p_naddr != value) {
			gbl_msg.warning("%s's number of addresses changed from %ld to %d\n",
				p_name->c_str(), p_naddr, value);
			p_naddr = value;
			p_awid  = nextlg(p_naddr);
		}
	}
	return p_naddr;
}

//
// compare_naddr
//
// This is part of the peripheral sorting mechanism, whereby peripherals are
// sorted by the numbers of addresses they use.  Peripherals using fewer
// addresses are placed first, with peripherals using more addresses placed
// later.
//
bool	compare_naddr(PERIPHP a, PERIPHP b) {
	if (!a)
		return (b)?false:true;
	else if (!b)
		return true;

	// Unordered items come before ordered items.

	bool		have_order = false;
	int		aorder, border;

	if (a->p_phash == NULL) {
		gbl_msg.fatal("Peripheral %s has a null hash!\n", a->p_name->c_str());
	} if (b->p_phash == NULL) {
		gbl_msg.fatal("ERR: Peripheral %s has a null hash!\n", b->p_name->c_str());
	}

	have_order = getvalue(*a->p_phash, KYSLAVE_ORDER, aorder);
	if (have_order) {
		have_order = getvalue(*b->p_phash, KYSLAVE_ORDER, border);
		if (have_order)
			return (aorder < border);
		return false;
	} else if (getvalue(*b->p_phash, KYSLAVE_ORDER, border))
		return true;
		
	unsigned	anaddr, bnaddr;

	anaddr = a->get_slave_address_width();
	bnaddr = b->get_slave_address_width();

	if (anaddr != bnaddr)
		return (anaddr < bnaddr);
	// Otherwise ... the two address types are equal.
	if ((a->p_name)&&(b->p_name))
		return (a->p_name->compare(*b->p_name) < 0) ? true:false;
	else if (!a->p_name)
		return true;
	else
		return true;
}

bool	compare_address(PERIPHP a, PERIPHP b) {
	if (!a)
		return (b)?false:true;
	else if (!b)
		return true;
	return (a->p_base < b->p_base);
}

bool	compare_regaddr(PERIPHP a, PERIPHP b) {
	return (a->p_regbase < b->p_regbase);
}

//
// Add a peripheral to a given list of peripherals
int	PLIST::add(MAPDHASH *phash) {
	PERIPHP p;
	STRINGP	pname;
	int	naddr;

	pname  = getstring(*phash, KYPREFIX);
	if (!pname) {
		gbl_msg.warning("Skipping unnamed peripheral\n");
		return -1;
	}
	if (!getvalue(*phash, KYNADDR, naddr)) {
		naddr = 0;
	}
	if (issubbus(*phash)) {
		BUSINFO		*bi;
		MAPDHASH::iterator	kvmbus;

		kvmbus = findkey(*phash, KYMASTER_BUS);
		if (kvmbus != phash->end()) {
			bi = NULL;
			if (kvmbus->second.m_typ == MAPT_STRING)
				bi = find_bus(kvmbus->second.u.m_s);
			else if (kvmbus->second.m_typ == MAPT_MAP)
				bi = find_bus(kvmbus->second.u.m_m);
			assert(bi);
		}
		p = new SUBBUS(phash, bi->m_name, bi);
	} else {
		p = new PERIPH;
		p->p_master_bus = NULL;
	}

	p->p_base = 0;
	p->p_naddr = naddr;
	p->p_awid  = (0 == naddr) ? nextlg(p->p_naddr) : 0;
	p->p_phash = phash;
	p->p_name  = pname;

	{
		BUSINFO		*bi;
		MAPDHASH::iterator	kvsbus;

		kvsbus = findkey(*phash, KYSLAVE_BUS);
		if (kvsbus != phash->end()) {
			bi = NULL;
			if (kvsbus->second.m_typ == MAPT_STRING) {
				bi = find_bus(kvsbus->second.u.m_s);
				kvsbus->second.m_typ = MAPT_MAP;
				kvsbus->second.u.m_m = bi->m_hash;
			} else if (kvsbus->second.m_typ == MAPT_MAP)
				bi = find_bus(kvsbus->second.u.m_m);
			assert(bi);
		} else {
			MAPT	elm;
			bi = find_bus((STRINGP)NULL);
			assert(NULL != bi);
			assert(NULL != bi->m_hash);
			elm.m_typ = MAPT_MAP;
			elm.u.m_m = bi->m_hash;
			assert(bi->m_hash);
			phash->insert(KEYVALUE(KYSLAVE_BUS, elm));
		}
		p->p_slave_bus = bi;
	}


	push_back(p);
	return size()-1;
}

void	PERIPH::integrity_check(void) {
	assert(NULL != p_name);
	assert(NULL != p_phash);
	assert(NULL != p_slave_bus);
}

void	PLIST::integrity_check(void) {
	assert(NULL != this);
	for(unsigned k=0; k<size(); k++) {
		PERIPHP	pp = (*this)[k];
		pp->integrity_check();
	}
}

int	PLIST::add(PERIPHP p) {
	push_back(p);
	return size()-1;
}

bool	PLIST::get_base_address(MAPDHASH *phash, unsigned &base) {
	for(iterator p=begin(); p!=end(); p++) {
		if ((*p)->p_phash == phash) {
			base = (*p)->p_base;
			return true;
		} else if ((*p)->p_master_bus) {
			if ((*p)->p_master_bus->get_base_address(phash, base)){
				base += (*p)->p_base;
				return true;
			}
		}
	} return false;
}

unsigned	PLIST::min_addr_size_bytes(const unsigned np,
				const unsigned mina_bytes,
				const unsigned nullsz_bytes) {
	unsigned	start = nullsz_bytes, pa, base_bytes;

	for(unsigned i=0; i<np; i++) {
		pa = (*this)[i]->get_slave_address_width();
		if (pa <= 0)
			continue;
		if (pa < mina_bytes)
			pa = mina_bytes;
		base_bytes = (start + ((1<<pa)-1));
		base_bytes &= (-1<<pa);
		// First valid next address is ...
		start = base_bytes + (1<<pa);
	} 
	// Next address
	return nextlg(start);
}

unsigned	PLIST::min_addr_size_octets(unsigned np,
				unsigned mina, unsigned nullsz,
				unsigned daddr) {
	unsigned	mna = mina - daddr,
			nullszb;

	if (mna < 1)
		mna = 1;
	nullszb = nullsz >> daddr;
	mna = min_addr_size_bytes(np, mna, nullszb);
	mna += daddr;
	return mna;
}


void	PLIST::assign_addresses(unsigned dwidth, unsigned nullsz) {
	unsigned daddr_abits = nextlg(dwidth/8);

	// Use daddr_abits to convert our addresses between bus addresses and
	// byte addresses.  The address width involved is in bus words,
	// whereeas the base address needs to be in octets.  NullSz is also
	// in octets.
	if (size() < 1) {
		if (nullsz > 0)
			m_address_width = nextlg(nullsz);
		else
			m_address_width = 0;
		return;
	} else if ((size() < 2)&&(nullsz == 0)) {
		(*this)[0]->p_base = 0;
		(*this)[0]->p_mask = 0;
		setvalue(*(*this)[0]->p_phash, KYBASE, (*this)[0]->p_base);
		m_address_width = (*this)[0]->get_slave_address_width();
		if (m_address_width <= 0) {
			gbl_msg.error("Slave %s has zero NADDR (now address assigned)\n",
				(*this)[0]->p_name->c_str());
		}
	} else {

		// We'll need a minimum of nextlg(p->size()) bits to address
		// p->size() separate peripherals.  While we'd like to minimize
		// the number of bits we use to do this, adding one extra bit
		// in to the minimum number of bits required gives us a little
		// bit of flexibility while also attempting to keep our bus
		// width to a minimum.
		unsigned long start_address = nullsz;
		unsigned long min_awd, min_asz;

		// Sort our peripherals by the number of address lines they
		// will be using.  At the end of this, we'll know that we'll
		// need a minimum of (*p)[p->size()-1]->p_awid+1 address lines.
		sort(begin(), end(), compare_naddr);

		//
		// We've got two Goals:
		//
		// #1 Keep the bus address width to a minimum
		// #2 Keep the LUTs required to a minimum
		//
		// Hence, let's try adjusting the minimum address difference
		// between peripherals to find the minimum #2, subject to
		// having found the minimum of #1

		// Let's start by calculating the number of bits we will need
		// if we don't do anything smart
		min_awd = (*this)[size()-1]->get_slave_address_width()
				+ daddr_abits;
		min_asz = min_addr_size_octets(size(), min_awd,
				nullsz, daddr_abits);
		// Our goal will be to do better than this

		for(iterator p=begin(); p!=end(); p++) {
			if ((*p)->naddr() <= 0) {
				gbl_msg.error("Slave %s has zero "
					"NADDR (now address assigned)\n",
					(*p)->p_name->c_str());
			}
		}

		unsigned	min_relevant = 32-daddr_abits;
		for(unsigned mina = daddr_abits+1; mina < 32-daddr_abits;
						mina++) {
			//
			unsigned	total_address_width,
					relevant_address_bits;

			//
			total_address_width = min_addr_size_octets(size(),
					mina, nullsz, daddr_abits);
			relevant_address_bits = total_address_width - mina;

			if (total_address_width > min_asz) {
				// Never increase our # of address lines
			} else if (relevant_address_bits < min_relevant) {
				min_asz = total_address_width;
				min_relevant = relevant_address_bits;
				min_awd = mina;
			}
		}

		// Do our calculation in octets
		start_address = nullsz;
		for(unsigned i=0; i<size(); i++) {
			unsigned	pa;

			// Number of address lines ... were the bus to be
			// done in octets
			pa = (*this)[i]->get_slave_address_width()+daddr_abits;
			if (pa <= 0) {
				// p_base is in octets
				(*this)[i]->p_base = start_address;
				(*this)[i]->p_mask = 0;
			} else {
				if (pa < min_awd)
					pa = min_awd;

				// p_base is in octets
				(*this)[i]->p_base = start_address + ((1ul<<pa)-1);
				(*this)[i]->p_base &= (-1l<<pa);
				start_address = (*this)[i]->p_base + (1ul<<pa);

				(*this)[i]->p_mask = (-1)<<(pa-daddr_abits);
				assert((*this)[i]->p_mask != 0);
			}
		}
		assert(start_address != 0);

		unsigned master_mask = nextlg(start_address);
		master_mask = (1u << (master_mask-daddr_abits))-1;

		// One more pass, now that we know the last address
		for(unsigned i=0; i<size(); i++) {

			(*this)[i]->p_mask &= master_mask;

			gbl_msg.info("  %20s -> %08lx & 0x%08lx\n",
					(*this)[i]->p_name->c_str(),
					(*this)[i]->p_base,
					(*this)[i]->p_mask << daddr_abits);

			if ((*this)[i]->p_phash) {
				MAPDHASH	*ph = (*this)[i]->p_phash;
				setvalue(*ph, KYBASE, (*this)[i]->p_base);
				setvalue(*ph, KYMASK, (*this)[i]->p_mask << daddr_abits);
			}
		} m_address_width = nextlg(start_address)-daddr_abits;
	}

	reeval(gbl_hash);
}
