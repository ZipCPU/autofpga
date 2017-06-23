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

unsigned	BUSINFO::min_addr_size(unsigned np, unsigned mina) {
	unsigned	start = 0, pa, base;

	for(unsigned i=0; i<np; i++) {
		pa = (*m_plist)[i]->p_awid;
		if ((*m_plist)[i]->p_awid < mina)
			pa = mina;
		base = (start + ((1<<pa)-1));
		base &= (-1<<pa);
		// First valid next address is ...
		start = base + (1<<pa);
	} return nextlg(start);
}

void	BUSINFO::assign_addresses(void) {
	if ((!m_plist)||(m_plist->size() < 1)) {
		m_address_width = 0;
	} else if (!m_addresses_assigned) {
if (gbl_dump) fprintf(gbl_dump, "BUSINFO:Assigning bus addresses for bus: %s\n", m_name->c_str());
if (gbl_dump) fprintf(gbl_dump, " DW = %d, NS = %d\n", m_data_width, m_nullsz);

		m_plist->assign_addresses(m_data_width, m_nullsz);
		m_address_width = m_plist->m_address_width;
	} m_addresses_assigned = true;
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

void	BUSLIST::addperipheral(MAPDHASH *phash) {
	STRINGP	str;
	BUSINFO	*bi;

	if (NULL != (str = getstring(*phash, KYSLAVE_BUS))) {
		if (!find_bus(str)) {
			bi = new BUSINFO();
			push_back(bi);
			bi->m_name = str;
		}
		bi = find_bus(str);
		bi->add(phash);
	} else if (NULL != (str = getstring(*phash, KYSLAVE))) {
		if ((*this)[0])
			(*this)[0]->add(phash);
		else {
			bi = new BUSINFO();
			push_back(bi);
			bi->add(phash);
		}
	}
}
void	BUSLIST::addperipheral(MAPT &map) {
	if (map.m_typ == MAPT_MAP)
		addperipheral(map.u.m_m);
}

void	BUSLIST::adddefault(STRINGP defname) {
	if (gbl_dump) fprintf(stderr, "Adding default bus: %s\n", defname->c_str());
	if (size() == 0) {
		push_back(new BUSINFO());
		(*this)[0]->m_name = defname;
	} else { // if (size() > 0)
		if (NULL != find_bus(defname)) {
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
			if ((*this)[0]->m_name->compare(*defname)!=0) {
				// Create a new BUSINFO struct for
				// this new one
				push_back((*this)[0]);
				(*this)[0] = new BUSINFO();
			}
			// else this is already the default
		} else
			// First spot exists, but has no name
			(*this)[0]->m_name = defname;
	}
}

void	BUSLIST::addbus(MAPDHASH *phash) {
	MAPDHASH	*bp;
	int		value;
	STRINGP		str;
	BUSINFO		*bi;

	bp = getmap(*phash, KYBUS);
	if (!bp)
		return;

	str = getstring(*bp, KY_NAME);
	if (str) {
		bi = find_bus(str);
		if (!bi) {
			bi = new BUSINFO();
			push_back(bi);
			bi->m_name = str;
		}
	} else if (size() > 0) {
		bi = (*this)[0];
	} else {
		bi = new BUSINFO();
		push_back(bi);
	}

	if (NULL != (str = getstring(*bp, KY_TYPE))) {
		if (bi->m_type == NULL) {
			bi->m_type = str;
		} else if (bi->m_type->compare(*str) != 0) {
			fprintf(stderr, "ERR: Conflicting bus types "
					"for %s\n",bi->m_name->c_str());
			gbl_err++;
		}
	}

	if (NULL != (str = getstring(*bp, KY_CLOCK))) {
		bi->m_clock = NULL;
		for(MAPDHASH::iterator kvpair = gbl_hash->begin();
				kvpair != gbl_hash->end(); kvpair++) {
			if (kvpair->second.m_typ != MAPT_MAP)
				continue;
			if (kvpair->first.compare(*str)==0) {
				bi->m_clock = kvpair->second.u.m_m;
				break;
			}
		}
		if (bi->m_clock == NULL) {
			fprintf(stderr, "ERR: Clock %s not defined for %s\n",
				str->c_str(), bi->m_name->c_str());
			gbl_err++;
		}
	}

	if (getvalue(*bp, KY_WIDTH, value)) {
		if (bi->m_data_width <= 0) {
			bi->m_data_width = value;
		} else if (bi->m_data_width != value) {
			fprintf(stderr, "ERR: Conflicting bus width definitions for %s\n", bi->m_name->c_str());
			gbl_err++;
		}
	}

	if (getvalue(*bp, KY_NULLSZ, value)) {
		bi->m_nullsz = value;
		// bi->m_addresses_assigned = false;
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

void	mkselect2(FILE *fp, MAPDHASH &master, BUSINFO *bi) {
	STRING	addrbus = STRING(*bi->m_name)+"_addr";
	unsigned	sbaw = bi->address_width();
	unsigned	dw   = bi->m_data_width;
	unsigned	dalines = nextlg(dw/8);

	for(unsigned i=0; i< bi->m_plist->size(); i++) {
		if ((*bi->m_plist)[i]->p_name) {
			fprintf(fp, "//x2\tassign\t%12s_sel "
					"= ((%s[%2d:0] & %2d\'h%x) == %2d\'h%0*x);\n",
				(*bi->m_plist)[i]->p_name->c_str(),
				addrbus.c_str(),
				sbaw-dalines-1,
				sbaw-dalines, (*bi->m_plist)[i]->p_mask,
				sbaw-dalines,
				(sbaw-dalines+3)/4,
				(*bi->m_plist)[i]->p_base>>dalines);
		} if ((*bi->m_plist)[i]->p_master_bus) {
			fprintf(fp, "//x2\tWas a master bus as well\n");
		}
	}
}

void	mkselect2(FILE *fp, MAPDHASH &master) {
	mkselect2(fp, master, (*gbl_blist)[0]);
}

void	writeout_bus_slave_defns_v(MAPDHASH &master, FILE *fp) {
}

void	writeout_bus_master_defns_v(MAPDHASH &master, FILE *fp) {
}

void	writeout_bus_defns_v(MAPDHASH &master, FILE *fp) {
	fprintf(fp, "//\n//\n// Define bus wires\n//\n//\n");
}

void	writeout_bus_select_v(MAPDHASH &master, FILE *fp) {
}

void	build_bus_list(MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	BUSLIST	*bl = new BUSLIST;
	STRINGP	str;

printf("BUILD-BUS-LIST!!!!!!!!!!!!!!!!!!!!!!\n");
	if (gbl_dump) fprintf(gbl_dump, "BUILD-BUS-LIST!!!!!!!!!!!!!!!!!\n");

	if (NULL != (str = getstring(master, KYDEFAULT_BUS))) {
		if (gbl_dump) fprintf(gbl_dump, "Found default bus: %s\n", str->c_str());
		bl->adddefault(str);
		if (gbl_dump) fprintf(gbl_dump, "Default bus: %s\n", str->c_str());
	}
else { if (gbl_dump) fprintf(gbl_dump, "NO default bus found\n"); }

fflush(gbl_dump);

	//
	if (refbus(master))
		bl->addbus(&master);
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (NULL != getstring(*kvpair->second.u.m_m, KYBUS))
			continue;
		bl->addbus(kvpair->second);
	}

if (gbl_dump) { fprintf(gbl_dump, "Looking for other buses that might be part of peripherals\n"); fflush(gbl_dump); }

	//
	//
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isperipheral(kvpair->second))
			continue;
		bl->addperipheral(kvpair->second);
	}

if (gbl_dump) { fprintf(gbl_dump, "All busses found, assigning addresses\n"); fflush(gbl_dump); }

	for(unsigned i=0; i< bl->size(); i++)
		(*bl)[i]->assign_addresses();

if (gbl_dump) { fprintf(gbl_dump, "All addresses assigned\n"); fflush(gbl_dump); }

	for(unsigned i=0; i< bl->size(); i++) {
		STRINGP	bname = (*bl)[i]->m_name;
		STRING	ky = (*bname) + KYBUS_AWID;
		int	value;
		value = (*bl)[i]->address_width();
		setvalue(master, ky, value);
	}

	printf("BUILT-BLIST, %ld elements\n", bl->size());
	gbl_blist = bl;
}

#ifdef	NOT_YET
void	build_main_v(     MAPDHASH &master, FILE *fp, STRING &fname) {
/*
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str, astr, sellist, acklist, siosel_str, diosel_str;
	int		first;
	unsigned	nsel = 0, nacks = 0, baw;

	legal_notice(master, fp, fname);
	baw = get_address_width(master);

	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	fprintf(fp, "`default_nettype\tnone\n");

	build_access_ifdefs_v(master, fp);

	fprintf(fp, "//\n//\n");
	fprintf(fp,
"// Finally, we define our main module itself.  We start with the list of\n"
"// I/O ports, or wires, passed into (or out of) the main function.\n"
"//\n"
"// These fields are copied verbatim from the respective I/O port lists,\n"
"// from the fields given by @MAIN.PORTLIST\n"
"//\n");

	// Define our external ports within a port list
	fprintf(fp, "module\tmain(i_clk, i_reset,\n");
	str = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, str);
		if (!strp)
			continue;
		if (!first)
			fprintf(fp, ",\n");
		first=0;
		STRING	tmps(*strp);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		fprintf(fp, "%s", tmps.c_str());
	} fprintf(fp, ");\n");

	fprintf(fp, "//\n" "// Any parameter definitions\n//\n"
		"// These are drawn from anything with a MAIN.PARAM definition.\n"
		"// As they aren\'t connected to the toplevel at all, it would\n"
		"// be best to use localparam over parameter, but here we don\'t\n"
		"// check\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp = getstring(kvpair->second, KYMAIN_PARAM);
		if (!strp)
			continue;
		fprintf(fp, "%s", strp->c_str());
	}

	fprintf(fp, "//\n"
"// The next step is to declare all of the various ports that were just\n"
"// listed above.  \n"
"//\n"
"// The following declarations are taken from the values of the various\n"
"// @MAIN.IODECL keys.\n"
"//\n");

// #warning "How do I know these will be in the same order?
//	They have been--because the master always reads its subfields in the
//	same order.
//
	// External declarations (input/output) for our various ports
	fprintf(fp, "\tinput\twire\t\ti_clk, i_reset;\n");
	str = "MAIN.IODECL";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		strp = getstring(*kvpair->second.u.m_m, str);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}

	// Declare Bus master data
	fprintf(fp, "\n\n");
	fprintf(fp, "\t//\n\t// Declaring wishbone master bus data\n\t//\n");
	fprintf(fp, "\twire\t\twb_cyc, wb_stb, wb_we, wb_stall, wb_err;\n");
	if (DELAY_ACK) {
		fprintf(fp, "\treg\twb_ack;\t// ACKs delayed by extra clock\n");
	} else {
		fprintf(fp, "\twire\twb_ack;\n");
	}
	fprintf(fp, "\twire\t[(%d-1):0]\twb_addr;\n", baw);
	fprintf(fp, "\twire\t[31:0]\twb_data;\n");
	fprintf(fp, "\treg\t[31:0]\twb_idata;\n");
	fprintf(fp, "\twire\t[3:0]\twb_sel;\n");
	if (slist.size()>0) {
		fprintf(fp, "\twire\tsio_sel, sio_stall;\n");
		fprintf(fp, "\treg\tsio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tsio_data;\n");
	}
	if (dlist.size()>0) {
		fprintf(fp, "\twire\tdio_sel, dio_stall;\n");
		fprintf(fp, "\treg\tdio_ack;\n");
		fprintf(fp, "\treg\t\tpre_dio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tdio_data;\n");
	}


	fprintf(fp, "\n\n");

	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring interrupt lines\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @INT.<interrupt name>.WIRE key.\n\t//\n");
	// Define any interrupt wires
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		MAPDHASH::iterator	kvint, kvsub, kvwire;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvint = kvpair->second.u.m_m->find(KY_INT);
		if (kvint == kvpair->second.u.m_m->end())
			continue;
		if (kvint->second.m_typ != MAPT_MAP)
			continue;
		for(kvsub=kvint->second.u.m_m->begin();
				kvsub != kvint->second.u.m_m->end(); kvsub++) {
			if (kvsub->second.m_typ != MAPT_MAP)
				continue;


			// Cant use getstring() here, 'cause we need the name of
			// the wire below
			kvwire = kvsub->second.u.m_m->find(KY_WIRE);
			if (kvwire == kvsub->second.u.m_m->end())
				continue;
			if (kvwire->second.m_typ != MAPT_STRING)
				continue;
			if (kvwire->second.u.m_s->size() == 0)
				continue;
			fprintf(fp, "\twire\t%s;\t// %s.%s.%s.%s\n",
				kvwire->second.u.m_s->c_str(),
				kvpair->first.c_str(),
				kvint->first.c_str(),
				kvsub->first.c_str(),
				kvwire->first.c_str());
		}
	}


	// Declare bus-master data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring Bus-Master data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, for those components with\n"
	"\t// an MTYPE flag.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (!isbusmaster(kvpair->second))
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare peripheral data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring Peripheral data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, for those components with a\n"
	"\t// PTYPE key but no MTYPE key.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (isbusmaster(kvpair->second))
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare other data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring other data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, but which have neither PTYPE\n"
	"\t// nor MTYPE keys.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (isbusmaster(kvpair->second))
			continue;
		if (isperipheral(kvpair->second))
			continue;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare interrupt vector wires.
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring interrupt vector wires\n\t//\n"
	"\t// These declarations come from the various components having\n"
	"\t// PIC and PIC.MAX keys.\n\t//\n");
	for(unsigned picid=0; picid < piclist.size(); picid++) {
		STRINGP	defnstr, vecstr;
		MAPDHASH *picmap;

		if (piclist[picid]->i_max <= 0)
			continue;
		picmap = getmap(master, *piclist[picid]->i_name);
		if (!picmap)
			continue;
		vecstr = getstring(*picmap, KYPIC_BUS);
		if (vecstr) {
			fprintf(fp, "\twire\t[%d:0]\t%s;\n",
					piclist[picid]->i_max-1,
					vecstr->c_str());
		} else {
			defnstr = piclist[picid]->i_name;
			if (defnstr)
				fprintf(fp, "\twire\t[%d:0]\t%s_int_vec;\n",
					piclist[picid]->i_max,
					defnstr->c_str());
		}
	}

	// Declare wishbone lines
	fprintf(fp, "\n"
	"\t// Declare those signals necessary to build the bus, and detect\n"
	"\t// bus errors upon it.\n"
	"\t//\n"
	"\twire\tnone_sel;\n"
	"\treg\tmany_sel, many_ack;\n"
	"\treg\t[31:0]\tr_bus_err;\n");
	fprintf(fp, "\n"
	"\t//\n"
	"\t// Wishbone master wire declarations\n"
	"\t//\n"
	"\t// These are given for every configuration file with an @MTYPE\n"
	"\t// tag, and the names are prefixed by whatever is in the @PREFIX tag.\n"
	"\t//\n"
	"\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isbusmaster(kvpair->second))
			continue;

		STRINGP	strp = getstring(*kvpair->second.u.m_m, KYPREFIX);
		const char	*pfx = strp->c_str();

		fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we, %s_ack, %s_stall, %s_err;\n",
			pfx, pfx, pfx, pfx, pfx, pfx);
		fprintf(fp, "\twire\t[(%d-1):0]\t%s_addr;\n", baw, pfx);
		fprintf(fp, "\twire\t[31:0]\t%s_data, %s_idata;\n", pfx, pfx);
		fprintf(fp, "\twire\t[3:0]\t%s_sel;\n", pfx);
		fprintf(fp, "\n");
	}

	fprintf(fp, "\n"
	"\t//\n"
	"\t// Wishbone slave wire declarations\n"
	"\t//\n"
	"\t// These are given for every configuration file with a @PTYPE\n"
	"\t// tag, and the names are given by the @PREFIX tag.\n"
	"\t//\n"
	"\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(*kvpair->second.u.m_m))
			continue;

		STRINGP	strp = getstring(*kvpair->second.u.m_m, KYPREFIX);
		const char	*pfx = strp->c_str();

		fprintf(fp, "\twire\t%s_ack, %s_stall, %s_sel;\n", pfx, pfx, pfx);
		fprintf(fp, "\twire\t[31:0]\t%s_data;\n", pfx);
		fprintf(fp, "\n");
	}

	// Define the select lines
	fprintf(fp, "\n"
	"\t// Wishbone peripheral address decoding\n"
	"\t// This particular address decoder decodes addresses for all\n"
	"\t// peripherals (components with a @PTYPE tag), based upon their\n"
	"\t// NADDR (number of addresses required) tag\n"
	"\t//\n");
	fprintf(fp, "\n");

	if (slist.size() > 1)
		mkselect(fp, master, slist, KYSIO_SEL, "sio_skip");
	if (dlist.size() > 1)
		mkselect(fp, master, dlist, KYDIO_SEL, "dio_skip");
	mkselect(fp, master, plist, STRING(""), "wb_skip");

	if (plist.size()>0) {
		if (sellist == "")
			sellist = (*plist[0]->p_name) + "_sel";
		else
			sellist = (*plist[0]->p_name) + "_sel, " + sellist;
	} for(unsigned i=1; i<plist.size(); i++)
		sellist = (*plist[i]->p_name)+"_sel, "+sellist;
	nsel += plist.size();

	// Define none_sel
	fprintf(fp, "\tassign\tnone_sel = (wb_stb)&&({ ");
	fprintf(fp, "%s", sellist.c_str());
	fprintf(fp, "} == 0);\n");

	fprintf(fp, ""
	"\t//\n"
	"\t// many_sel\n"
	"\t//\n"
	"\t// This should *never* be true .... unless the address decoding logic\n"
	"\t// is somehow broken.  Given that a computer is generating the\n"
	"\t// addresses, that should never happen.  However, since it has\n"
	"\t// happened to me before (without the computer), I test/check for it\n"
	"\t// here.\n"
	"\t//\n"
	"\t// Devices are placed here as a natural result of the address\n"
	"\t// decoding logic.  Thus, any device with a sel_ line will be\n"
	"\t// tested here.\n"
	"\t//\n");
	fprintf(fp, "`ifdef\tVERILATOR\n\n");

	fprintf(fp, "\talways @(*)\n");
	fprintf(fp, "\t\tcase({%s})\n", sellist.c_str());
	fprintf(fp, "\t\t\t%d\'h0: many_sel = 1\'b0;\n", nsel);
	for(unsigned i=0; i<nsel; i++) {
		fprintf(fp, "\t\t\t%d\'b", nsel);
		for(unsigned j=0; j<nsel; j++)
			fprintf(fp, (i==j)?"1":"0");
		fprintf(fp, ": many_sel = 1\'b0;\n");
	}
	fprintf(fp, "\t\t\tdefault: many_sel = (wb_stb);\n");
	fprintf(fp, "\t\tendcase\n");

	fprintf(fp, "\n`else\t// VERILATOR\n\n");

	fprintf(fp, "\talways @(*)\n");
	fprintf(fp, "\t\tcase({%s})\n", sellist.c_str());
	fprintf(fp, "\t\t\t%d\'h0: many_sel <= 1\'b0;\n", nsel);
	for(unsigned i=0; i<nsel; i++) {
		fprintf(fp, "\t\t\t%d\'b", nsel);
		for(unsigned j=0; j<nsel; j++)
			fprintf(fp, (i==j)?"1":"0");
		fprintf(fp, ": many_sel <= 1\'b0;\n");
	}
	fprintf(fp, "\t\t\tdefault: many_sel <= (wb_stb);\n");
	fprintf(fp, "\t\tendcase\n");

	fprintf(fp, "\n`endif\t// VERILATOR\n\n");




	// Build a list of ACK signals
	acklist = ""; nacks = 0;
	if (plist.size() > 0) {
		if (nacks > 0)
			acklist = (*plist[0]->p_name) + "_ack, " + acklist;
		else
			acklist = (*plist[0]->p_name) + "_ack";
		nacks++;
	} for(unsigned i=1; i < plist.size(); i++) {
		acklist = (*plist[i]->p_name) + "_ack, " + acklist;
		nacks++;
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// many_ack\n"
	"\t//\n"
	"\t// It is also a violation of the bus protocol to produce multiply\n"
	"\t// acks at once and on the same clock.  In that case, the bus\n"
	"\t// can\'t decide which result to return.  Worse, if someone is waiting\n"
	"\t// for a return value, that value will never come since another ack\n"
	"\t// masked it.\n"
	"\t//\n"
	"\t// The other error that isn\'t tested for here, no would I necessarily\n"
	"\t// know how to test for it, is when peripherals return values out of\n"
	"\t// order.  Instead, I propose keeping that from happening by\n"
	"\t// guaranteeing, in software, that two peripherals are not accessed\n"
	"\t// immediately one after the other.\n"
	"\t//\n");
	{
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\t\tcase({%s})\n", acklist.c_str());
		fprintf(fp, "\t\t\t%d\'h0: many_ack <= 1\'b0;\n", nacks);
		for(unsigned i=0; i<nacks; i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":"0");
			fprintf(fp, ": many_ack <= 1\'b0;\n");
		}
		fprintf(fp, "\t\tdefault: many_ack <= (wb_cyc);\n");
		fprintf(fp, "\t\tendcase\n");
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// wb_ack\n"
	"\t//\n"
	"\t// The returning wishbone ack is equal to the OR of every component that\n"
	"\t// might possibly produce an acknowledgement, gated by the CYC line.  To\n"
	"\t// add new components, OR their acknowledgements in here.\n"
	"\t//\n"
	"\t// To return an ack here, a component must have a @PTYPE.  Acks from\n"
	"\t// any @PTYPE SINGLE and DOUBLE components have been collected\n"
	"\t// together into sio_ack and dio_ack respectively, which will appear.\n"
	"\t// ahead of any other device acks.\n"
	"\t//\n");
	if (slist.size() > 0)
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tsio_ack <= (wb_stb)&&(sio_sel);\n");
	if (dlist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tpre_dio_ack <= (wb_stb)&&(dio_sel);\n");
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tdio_ack <= pre_dio_ack;\n");
	}

	if (DELAY_ACK) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\twb_ack <= "
			"(wb_cyc)&&(|{%s});\n\n\n", acklist.c_str());
	} else {
		if (acklist.size() > 0) {
			fprintf(fp, "\tassign\twb_ack = (wb_cyc)&&(|{%s});\n\n\n",
				acklist.c_str());
		} else
			fprintf(fp, "\tassign\twb_ack = 1\'b0;\n\n\n");
	}


	// Define the stall line
	if (plist.size() > 0) {
		fprintf(fp,
	"\t//\n"
	"\t// wb_stall\n"
	"\t//\n"
	"\t// The returning wishbone stall line really depends upon what device\n"
	"\t// is requested.  Thus, if a particular device is selected, we return \n"
	"\t// the stall line for that device.\n"
	"\t//\n"
	"\t// Stall lines come from any component with a @PTYPE key and a\n"
	"\t// @NADDR > 0.  Since those components of @PTYPE SINGLE or DOUBLE\n"
	"\t// are not allowed to stall, they have been removed from this list\n"
	"\t// here for simplicity.\n"
	"\t//\n");

		if (slist.size() > 0)
			fprintf(fp, "\tassign\tsio_stall = 1\'b0;\n");
		if (dlist.size() > 0)
			fprintf(fp, "\tassign\tdio_stall = 1\'b0;\n");
		fprintf(fp, "\tassign\twb_stall = \n"
			"\t\t  (wb_stb)&&(%6s_sel)&&(%6s_stall)",
			plist[0]->p_name->c_str(), plist[0]->p_name->c_str());
		for(unsigned i=1; i<plist.size(); i++)
			fprintf(fp, "\n\t\t||(wb_stb)&&(%6s_sel)&&(%6s_stall)",
				plist[i]->p_name->c_str(), plist[i]->p_name->c_str());
		fprintf(fp, ";\n\n\n");
	} else
		fprintf(fp, "\tassign\twb_stall = 1\'b0;\n\n\n");

	// Define the bus error
	fprintf(fp, ""
	"\t//\n"
	"\t// wb_err\n"
	"\t//\n"
	"\t// This is the bus error signal.  It should never be true, but practice\n"
	"\t// teaches us otherwise.  Here, we allow for three basic errors:\n"
	"\t//\n"
	"\t// 1. STB is true, but no devices are selected\n"
	"\t//\n"
	"\t//	This is the null pointer reference bug.  If you try to access\n"
	"\t//	something on the bus, at an address with no mapping, the bus\n"
	"\t//	should produce an error--such as if you try to access something\n"
	"\t//	at zero.\n"
	"\t//\n"
	"\t// 2. STB is true, and more than one device is selected\n"
	"\t//\n"
	"\t//	(This can be turned off, if you design this file well.  For\n"
	"\t//	this line to be true means you have a design flaw.)\n"
	"\t//\n"
	"\t// 3. If more than one ACK is every true at any given time.\n"
	"\t//\n"
	"\t//	This is a bug of bus usage, combined with a subtle flaw in the\n"
	"\t//	WB pipeline definition.  You can issue bus requests, one per\n"
	"\t//	clock, and if you cross device boundaries with your requests,\n"
	"\t//	you may have things come back out of order (not detected here)\n"
	"\t//	or colliding on return (detected here).  The solution to this\n"
	"\t//	problem is to make certain that any burst request does not cross\n"
	"\t//	device boundaries.  This is a requirement of whoever (or\n"
	"\t//	whatever) drives the bus.\n"
	"\t//\n"
	"\tassign	wb_err = ((wb_stb)&&(none_sel || many_sel))\n"
				"\t\t\t\t|| ((wb_cyc)&&(many_ack));\n\n");

	if (baw < 30) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { {(%d){1\'b0}}, wb_addr, 2\'b00 };\n\n", 30-baw);
	} else if (baw == 30) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { wb_addr, 2\'b00 };\n\n");
	} else if (baw == 31)
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { wb_addr, 1\'b0 };\n\n");
	else
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= wb_addr;\n\n");

	fprintf(fp, "\t//Now we turn to defining all of the parts and pieces of what\n"
	"\t// each of the various peripherals does, and what logic it needs.\n"
	"\t//\n"
	"\t// This information comes from the @MAIN.INSERT and @MAIN.ALT tags.\n"
	"\t// If an @ACCESS tag is available, an ifdef is created to handle\n"
	"\t// having the access and not.  If the @ACCESS tag is `defined above\n"
	"\t// then the @MAIN.INSERT code is executed.  If not, the @MAIN.ALT\n"
	"\t// code is exeucted, together with any other cleanup settings that\n"
	"\t// might need to take place--such as returning zeros to the bus,\n"
	"\t// or making sure all of the various interrupt wires are set to\n"
	"\t// zero if the component is not included.\n"
	"\t//\n");

	fprintf(fp, "\t//\n\t// Declare the interrupt busses\n\t//\n");
	for(unsigned picid=0; picid < piclist.size(); picid++) {
		STRINGP	defnstr, vecstr;
		MAPDHASH *picmap;

		if (piclist[picid]->i_max <= 0)
			continue;
		picmap = getmap(master, *piclist[picid]->i_name);
		if (!picmap)
			continue;
		vecstr = getstring(*picmap, KYPIC_BUS);
		if (vecstr) {
			fprintf(fp, "\tassign\t%s = {\n",
					vecstr->c_str());
		} else {
			defnstr = piclist[picid]->i_name;
			if (defnstr)
				fprintf(fp, "\tassign\t%s_int_vec = {\n",
					defnstr->c_str());
			else {
				gbl_err++;
				fprintf(stderr, "ERR: PIC has no associated name\n");
				continue;
			}
		}

		for(int iid=piclist[picid]->i_max-1; iid>=0; iid--) {
			INTP	iip = piclist[picid]->getint(iid);
			if ((iip == NULL)||(iip->i_wire == NULL)
					||(iip->i_wire->size() == 0)) {
				fprintf(fp, "\t\t1\'b0%s\n",
					(iid != 0)?",":"");
				continue;
			} fprintf(fp, "\t\t%s%s\n",
				iip->i_wire->c_str(),
				(iid == 0)?"":",");
		}
		fprintf(fp, "\t};\n");
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
	// MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
		MAPDHASH::iterator	kvint, kvsub, kvwire;
		bool			nomain, noaccess, noalt;
		STRINGP			insert, alt, access;

		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		insert = getstring(kvpair->second, KYMAIN_INSERT);
		access = getstring(kvpair->second, KYACCESS);
		alt    = getstring(kvpair->second, KYMAIN_ALT);

		nomain   = false;
		noaccess = false;
		noalt    = false;
		if (NULL == insert)
			nomain = true;
		if (NULL == access)
			noaccess= true;
		if (NULL == alt)
			noalt = true;

		if (noaccess) {
			if (!nomain)
				fputs(insert->c_str(), fp);
		} else if ((!nomain)||(!noalt)) {
			if (nomain) {
				fprintf(fp, "`ifndef\t%s\n", access->c_str());
			} else {
				fprintf(fp, "`ifdef\t%s\n", access->c_str());
				fputs(insert->c_str(), fp);
				fprintf(fp, "`else\t// %s\n", access->c_str());
			}

			if (!noalt) {
				fputs(alt->c_str(), fp);
			}

			if (isbusmaster(kvpair->second)) {
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYPREFIX);
				if (pfx) {
					fprintf(fp, "\n");
					fprintf(fp, "\tassign\t%s_cyc = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_stb = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_we  = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_sel = 4\'b0000;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_addr = 0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_data = 0;\n", pfx->c_str());
					fprintf(fp, "\n");
				}
			}

			if (isperipheral(kvpair->second)) {
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYPREFIX);
				if (pfx) {
					fprintf(fp, "\n");
					fprintf(fp, "\treg\tr_%s_ack;\n", pfx->c_str());
					fprintf(fp, "\talways @(posedge i_clk)\n\t\tr_%s_ack <= (wb_stb)&&(%s_sel);\n",
						pfx->c_str(),
						pfx->c_str());
					fprintf(fp, "\n");
					fprintf(fp, "\tassign\t%s_ack   = r_%s_ack;\n",
						pfx->c_str(),
						pfx->c_str());
					fprintf(fp, "\tassign\t%s_stall = 1\'b0;\n",
						pfx->c_str());
					fprintf(fp, "\tassign\t%s_data  = 32\'h0;\n",
						pfx->c_str());
					fprintf(fp, "\n");
				}
			}
			kvint    = kvpair->second.u.m_m->find(KY_INT);
			if ((kvint != kvpair->second.u.m_m->end())
					&&(kvint->second.m_typ == MAPT_MAP)) {
				MAPDHASH	*imap = kvint->second.u.m_m,
						*smap;
				for(kvsub=imap->begin(); kvsub != imap->end();
						kvsub++) {
					// p.INT.SUB
					if (kvsub->second.m_typ != MAPT_MAP)
						continue;
					smap = kvsub->second.u.m_m;
					kvwire = smap->find(KY_WIRE);
					if (kvwire == smap->end())
						continue;
					if (kvwire->second.m_typ != MAPT_STRING)
						continue;
					fprintf(fp, "\tassign\t%s = 1\'b0;\t// %s.%s.%s.%s\n",
						kvwire->second.u.m_s->c_str(),
						kvpair->first.c_str(),
						kvint->first.c_str(),
						kvsub->first.c_str(),
						kvwire->first.c_str());
				}
			}
			fprintf(fp, "`endif\t// %s\n\n", access->c_str());
		}
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// Finally, determine what the response is from the wishbone\n"
	"\t// bus\n"
	"\t//\n"
	"\t//\n");

	fprintf(fp, ""
	"\t//\n"
	"\t// wb_idata\n"
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
	"\t// Any peripheral component with a @PTYPE value will be listed\n"
	"\t// here.\n"
	"\t//\n");

	if (slist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\tcasez({ ");
		for(unsigned i=0; i<slist.size()-1; i++)
			fprintf(fp, "%s_sel, ", slist[i]->p_name->c_str());
		fprintf(fp, "%s_sel })\n", slist[slist.size()-1]->p_name->c_str());
		for(unsigned i=0; i<slist.size(); i++) {
			fprintf(fp, "\t\t%2ld\'b", slist.size());
			for(unsigned j=0; j<slist.size(); j++) {
				if (j < i)
					fprintf(fp, "0");
				else if (j==i)
					fprintf(fp, "1");
				else
					fprintf(fp, "?");
			}
			fprintf(fp, ": sio_data <= %s_data;\n",
				slist[i]->p_name->c_str());
		} fprintf(fp, "\t\tdefault: sio_data <= 32\'h0;\n");
		fprintf(fp, "\tendcase\n\n");
	} else
		fprintf(fp, "\tinitial\tsio_data = 32\'h0;\n"
			"\talways @(posedge i_clk)\n\t\tsio_data = 32\'h0;\n");

	if (dlist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\tcasez({ ");
		for(unsigned i=0; i<dlist.size()-1; i++)
			fprintf(fp, "%s_ack, ", dlist[i]->p_name->c_str());
		fprintf(fp, "%s_ack })\n", dlist[dlist.size()-1]->p_name->c_str());
		for(unsigned i=0; i<dlist.size(); i++) {
			fprintf(fp, "\t\t%2ld\'b", dlist.size());
			for(unsigned j=0; j<dlist.size(); j++) {
				if (j < i)
					fprintf(fp, "0");
				else if (j==i)
					fprintf(fp, "1");
				else
					fprintf(fp, "?");
			}
			fprintf(fp, ": dio_data <= %s_data;\n",
				dlist[i]->p_name->c_str());
		} fprintf(fp, "\t\tdefault: dio_data <= 32\'h0;\n\tendcase\n");
	}

	if (DELAY_ACK) {
		fprintf(fp, "\talways @(posedge i_clk)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
	} else {
		fprintf(fp, "`ifdef\tVERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
				for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata = %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata = 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata = 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "\n`else\t// VERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "`endif\t// VERILATOR\n");
	}

	fprintf(fp, "\n\nendmodule // main.v\n");

*/
}
#endif


