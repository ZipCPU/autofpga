////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	plist.cpp
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A PLIST is a list of peripherals (i.e. bus slaves, or components
//		with a SLAVE.BUS tag).  This file defines the methods associated
//	with such a list.
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
	if (NULL != (str = getstring(*p_phash, KYERROR_WIRE)))
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
	unsigned	awid, lgdw = 0;

	awid = nextlg(naddr());
	if (p_slave_bus != NULL && !p_slave_bus->word_addressing())
		lgdw = nextlg(p_slave_bus->data_width()/8);
	awid += lgdw;

	if (p_awid != (unsigned)awid) {
		p_awid = awid;
		setvalue(*p_phash, KYSLAVE_AWID, awid);
	}
	return p_awid;
}

unsigned PERIPH::naddr(void) {
	int	value;

	assert(p_phash);

	if (getvalue(*p_phash, KYNADDR, value)) {
		bool rebuild = false;
		int	lgdw = 0;

		if (p_slave_bus != NULL && !p_slave_bus->word_addressing())
			lgdw = nextlg(p_slave_bus->data_width()/8);

		if ((int)p_naddr != value) {
			gbl_msg.warning("%s's number of addresses changed from %ld to %d\n",
				p_name->c_str(), p_naddr, value);
			p_naddr = 0;
		}
		if (0 == p_naddr) {
			// Set p_naddr from value

			int	lgaddr = nextlg(value);

			p_naddr = value;
			lgaddr += lgaddr + lgdw;
			rebuild = true;
			p_awid  = lgaddr;
		}
		if (rebuild) {
			setstring(p_phash, KYSLAVE_PORTLIST,
					p_slave_bus->slave_portlist(this));
			setstring(p_phash, KYSLAVE_ANSIPORTLIST,
					p_slave_bus->slave_ansi_portlist(this));
		}
		if (!getstring(p_phash, KYSLAVE_IANSI)) {
			setstring(p_phash, KYSLAVE_IANSI,
					p_slave_bus->slave_iansi(this));
			rebuild = true;
		}
		if (!getstring(p_phash, KYSLAVE_OANSI)) {
			setstring(p_phash, KYSLAVE_OANSI,
					p_slave_bus->slave_oansi(this));
			rebuild = true;
		}
		if (!getstring(p_phash, KYSLAVE_ANSPREFIX)) {
			setstring(p_phash, KYSLAVE_ANSPREFIX,
					p_slave_bus->slave_ansprefix(this));
			rebuild = true;
		}

		if (rebuild)
			reeval(p_phash);
	}
	return p_naddr;
}

void	PERIPH::integrity_check(void) {
	assert(NULL != p_name);
	assert(NULL != p_phash);
	assert(NULL != p_slave_bus);
}

STRINGP	PERIPH::bus_prefix(void) {
	STRINGP	pfx;
	pfx = getstring(p_phash, KYSLAVE_PREFIX);
	if (NULL == pfx) {
		STRINGP	bus = p_slave_bus->name();
		if (NULL == bus)
			return NULL;
		// Assume a prefix if it isnt given
		pfx = new STRING(STRING(*bus)+STRING("_")+STRING(*p_name));
		setstring(p_phash, KYSLAVE_PREFIX, pfx);
	}

	return pfx;
}

bool	PERIPH::read_only(void) {
	STRINGP	options;

	options = getstring(*p_phash, KYSLAVE_OPTIONS);
	if (NULL != options)
		return read_only_option(options);
	return false;
}

bool	PERIPH::write_only(void) {
	STRINGP	options;

	options = getstring(*p_phash, KYSLAVE_OPTIONS);
	if (NULL != options)
		return write_only_option(options);
	return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// Logic on sets of peripherals
//
////////////////////////////////////////////////////////////////////////////////
void	PLIST::integrity_check(void) {
	assert(NULL != this);
	for(unsigned k=0; k<size(); k++) {
		PERIPHP	pp = (*this)[k];
		pp->integrity_check();
	}
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

int	PLIST::add(PERIPHP p) {
	// To get here, the component must already have a valid hash
	assert(p->p_phash);
	push_back(p);
	return size()-1;
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
		p = new SUBBUS(phash, bi->name(), bi);
	} else {
		p = new PERIPH;
		p->p_master_bus = NULL;
	}

	p->p_base = 0;
	p->p_naddr = naddr;
	p->p_phash = phash;
	p->p_name  = pname;
	{
		BUSINFO		*bi;
		MAPDHASH::iterator	kvsbus;

		kvsbus = findkey(*phash, KYSLAVE_BUS);
		assert(kvsbus != phash->end());
		assert(kvsbus->second.m_typ == MAPT_MAP);
		bi = find_bus(kvsbus->second.u.m_m);
		assert(bi);
		p->p_slave_bus = bi;
	}
	if (0 != naddr) {
		// Calculate and set p->p_awid
		p->get_slave_address_width();
	} else
		p->p_awid = 0;


	(void)p->bus_prefix();

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
	}
	return false;
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

extern bool gbl_ready_for_address_assignment;
void	PLIST::assign_addresses(unsigned dwidth, unsigned nullsz,
		unsigned bus_min_address_width) {
	unsigned daddr_abits = nextlg(dwidth/8);

	assert(gbl_ready_for_address_assignment);

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
		PERIPHP	p = (*this)[0];
		MAPDHASH	*ph = p->p_phash;
		GENBUS		*g = p->p_slave_bus->generator();

		p->p_base = 0;
		p->p_mask = 0;
		setvalue(*p->p_phash, KYBASE, p->p_base);
		setvalue(*p->p_phash, KYMASK, p->p_mask);
		m_address_width = p->get_slave_address_width();
		if (m_address_width <= 0) {
			gbl_msg.error("Slave %s has zero NADDR (now address assigned)\n",
				p->p_name->c_str());
		}

		if (g) {
			setstring(*ph, KYSLAVE_PORTLIST,
				g->slave_portlist(p));
			setstring(*ph, KYSLAVE_ANSIPORTLIST,
				g->slave_ansi_portlist(p));
			if (!getstring(*ph, KYSLAVE_IANSI))
				setstring(*ph, KYSLAVE_IANSI, g->iansi(NULL));
			if (!getstring(*ph, KYSLAVE_OANSI))
				setstring(*ph, KYSLAVE_OANSI, g->oansi(NULL));
			if (!getstring(*ph, KYSLAVE_ANSPREFIX))
				setstring(*ph, KYSLAVE_ANSPREFIX,
							g->slave_ansprefix(p));
			reeval(*ph);
		}
	} else {
		bool	m_full_decode = false;

		assert((*this)[0]->p_slave_bus);
		if (!(*this)[0]->p_slave_bus->word_addressing())
			daddr_abits = 0;

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
		if (min_asz < bus_min_address_width)
			min_asz = bus_min_address_width;
		// Our goal will be to do better than this

		for(iterator p=begin(); p!=end(); p++) {
			// Make certain all of our slaves have at least
			// one address location assigned to them, generate
			// an error if not
			if ((*p)->naddr() <= 0) {
				gbl_msg.error("Slave %s has zero "
					"NADDR (now address assigned)\n",
					(*p)->p_name->c_str());
			}
		}

		//
		// Minimize the address width
		//
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

		//
		// Now we're ready to do the actual address assignment
		//
		// We'll do the calculation in calculation in octets, where
		// the conversion from the one to the next is daddr_abits--the
		// number of address bits required to advance one bus word.

		// We'll start assigning addresses after skipping some number
		// of addresses assigned to the null address.  This is user
		// selectable, and may be zero.
		start_address = nullsz;
		for(unsigned i=0; i<size(); i++) {
			unsigned	pa, pfull, pmsk;

			// pa is the width in the number of address bits
			// required to address one octet of this peripheral.
			pfull=(*this)[i]->get_slave_address_width()+daddr_abits;
			pa = pfull;
			if (pa <= 0) {
				// p_base is in octets
				(*this)[i]->p_base = start_address;
				(*this)[i]->p_mask = 0;
			} else {
				//
				// If our address increment is smaller than the
				// minimum increment we calculated above, then
				// bump it up to that minimum increment
				if (pa < min_awd)
					pa = min_awd;

				// p_base is the base address of this
				// peripheral, expressed in octets
				(*this)[i]->p_base =start_address+((1ul<<pa)-1);
				//
				// Trim p_base back down to just the addresses
				// we'd use--that way the upper address bits
				// don't change across this peripherals address
				// range
				(*this)[i]->p_base &= (-1l<<pa);
				//
				// Now, advance the start address to the next
				// open address after this peripheral
				start_address = (*this)[i]->p_base + (1ul<<pa);

				//
				// p_mask are the bits that matter when decoding
				// this peripheral.  It's basically all ones
				// up-shifted by the number of bits this
				// peripheral uses--we'll trim it back down
				// to the minimum required bits in a mask in
				// a moment
				pmsk = (m_full_decode) ? pfull : pa;
				(*this)[i]->p_mask = (-1)<<(pmsk-daddr_abits);
				assert((*this)[i]->p_mask != 0);
			}
		}
		assert(start_address != 0);

		unsigned master_mask = nextlg(start_address);
		master_mask = (1u << (master_mask-daddr_abits))-1;

		// One more pass, now that we know the last address
		for(unsigned i=0; i<size(); i++) {

			//
			// Trim the bits necessary to express the relevant
			// bits of this address
			(*this)[i]->p_mask &= master_mask;

			gbl_msg.info("  %20s -> %08lx & 0x%08lx\n",
					(*this)[i]->p_name->c_str(),
					(*this)[i]->p_base,
					(*this)[i]->p_mask << daddr_abits);

			if ((*this)[i]->p_phash) {
				PERIPHP	p = (*this)[i];
				MAPDHASH	*ph = p->p_phash;
				GENBUS		*g = p->p_slave_bus->generator();
				setvalue(*ph, KYBASE, p->p_base);
				setvalue(*ph, KYMASK, p->p_mask << daddr_abits);
				if (g) {
					setstring(*ph, KYSLAVE_PORTLIST,
						g->slave_portlist(p));
					setstring(*ph, KYSLAVE_ANSIPORTLIST,
						g->slave_ansi_portlist(p));
					if (!getstring(*ph, KYSLAVE_IANSI))
						setstring(*ph, KYSLAVE_IANSI,
							g->iansi(NULL));
					if (!getstring(*ph, KYSLAVE_OANSI))
						setstring(*ph, KYSLAVE_OANSI,
							g->oansi(NULL));
					if (!getstring(*ph, KYSLAVE_ANSPREFIX))
						setstring(*ph, KYSLAVE_ANSPREFIX,
							g->slave_ansprefix(p));
					reeval(*ph);
				}
			}
		} m_address_width = nextlg(start_address)-daddr_abits;
	}

	reeval(gbl_hash);
}
