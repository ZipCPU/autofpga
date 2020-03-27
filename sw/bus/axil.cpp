////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	axil.cpp
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
//		FULL	(default:slave has no all ports, not just the ports used)
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
// Copyright (C) 2019-2020, Gisselquist Technology, LLC
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

#include "axil.h"

#define	PREFIX

extern	AXILBUSCLASS	axilclass;
const	unsigned	AXI_MIN_ADDRESS_WIDTH = 12;

AXILBUS::AXILBUS(BUSINFO *bi) {
	m_info = bi;
	m_slist = NULL;
	m_dlist = NULL;

	m_is_single = false;
	m_is_double = false;

	m_num_single = 0;
	m_num_double = 0;
	m_num_total = 0;
}

void	AXILBUS::allocate_subbus(void) {
	PLIST	*pl = m_info->m_plist;
	BUSINFO	*sbi = NULL, *dbi = NULL;

	if (NULL == pl || pl->size() == 0) {
		gbl_msg.warning("Bus %s has no attached slaves\n",
			(name()) ? name()->c_str() : "(No-name)");
	}

	gbl_msg.info("Generating AXI-Lite bus logic generator for %s\n",
		(name()) ? name()->c_str() : "(No-name)");
	countsio();

	if (m_num_single == m_num_total) {
		m_num_single = 0;
		m_is_single = true;
	} else if (m_num_single <= 2) {
		if (m_num_double > 0) {
			// Merge single and double busses, making single
			// slaves into double slaves
			m_num_double += m_num_single;
			m_num_single = 0;
		}
		// else if (m_num_total > m_num_single) {
		//	Merge single slaves into full fledged peripherals
		//	  This doesn't work, though, if the slaves have been
		//	  simplified
		//	m_num_single = 0;
		// }
	}

	// if (m_num_double <= 2)
	//	Merge our double peripherals into the regular set
	//	  Doesn't work in case the slaves have been simplified
	//	  They might not work on a full bus
	//	m_num_double = 0;
	// else
	if (m_num_double == m_num_total) {
		m_num_double = 0;
		m_is_double  = true;
	} else if (m_num_single + m_num_double == m_num_total) {
		m_is_double  = true;
		m_num_double = 0;
	}

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

	if (!m_is_single && !m_is_double && pl) {
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
					&& ((ptyp->compare(KYDOUBLE) == 0)
					|| (ptyp->compare(KYSINGLE) == 0))) {
				m_info->m_plist->erase(pl->begin()+pi);
				pi--;
				m_dlist->add(p);
			} else {
			// Leave this peripheral in m_info->m_plist
			}
		}
	}

	if (sbi)
		setstring(sbi->m_hash, KY_TYPE, axilclass.name());
	if (dbi)
		setstring(dbi->m_hash, KY_TYPE, axilclass.name());
	REHASH;
}

int	AXILBUS::address_width(void) {
	assert(m_info);
	return m_info->m_address_width;
}

bool	AXILBUS::get_base_address(MAPDHASH *phash, unsigned &base) {
	if (!m_info || !m_info->m_plist) {
		gbl_msg.error("BUS[%s] has no peripherals!\n",
			(name()) ? name()->c_str() : "(No name)");
		return false;
	} else
		return m_info->m_plist->get_base_address(phash, base);
}

void	AXILBUS::assign_addresses(void) {
	int	address_width;

	if (m_info->m_addresses_assigned)
		return;
	if ((NULL == m_slist)&&(NULL == m_dlist))
		allocate_subbus();

	if (!m_info)
		return;
	gbl_msg.info("AXIL: Assigning addresses for bus %s\n",
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

		m_info->m_plist->assign_addresses(dw, nullsz, AXI_MIN_ADDRESS_WIDTH);
		address_width = m_info->m_plist->get_address_width();
		m_info->m_address_width = address_width;
		if (m_info->m_hash) {
			setvalue(*m_info->m_hash, KY_AWID, m_info->m_address_width);
			REHASH;
		}
	} m_info->m_addresses_assigned = true;
}

BUSINFO *AXILBUS::create_sio(void) {
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
	setstring(bushash, KY_TYPE, new STRING("axil"));
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

BUSINFO *AXILBUS::create_dio(void) {
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
	setstring(bushash, KY_TYPE, new STRING("axil"));
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

void	AXILBUS::countsio(void) {
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

void	AXILBUS::integrity_check(void) {
	// GENBUS::integrity_check();

	if (m_info && m_info->m_data_width <= 0) {
		gbl_msg.error("ERR: BUS width not defined for %s\n",
			name()->c_str());
	}
}

void	AXILBUS::writeout_defn_v(FILE *fp, const char *pname,
		const char* busp, const char *btyp){
	STRINGP	n = name();
	int	aw = address_width();

	fprintf(fp, "\t//\t// AXI-lite slave definitions for bus %s%s,\n"
		"\t// component %s, with prefix %s\n\t//\n",
		n->c_str(), btyp, pname, busp);
	fprintf(fp, "\t// Verilator lint_off UNUSED\n");
	fprintf(fp, "\twire\t\t%s_awready, %s_wready,\n\t\t\t%s_arready;\n",
			busp, busp, busp);
	fprintf(fp, "\twire\t\t%s_bvalid, %s_rvalid;\n",
			busp, busp);
	fprintf(fp, "\twire\t[1:0]\t%s_bresp, %s_rresp;\n",
			busp, busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_rdata;\n\n",
			m_info->data_width()-1, busp);
	fprintf(fp, "\twire\t\t%s_awvalid, %s_wvalid,\n\t\t\t%s_arvalid,\n"
		"\t\t\t%s_bready, %s_rready;\n"
		"\twire\t[%d:0]\t%s_araddr, %s_awaddr;\n"
		"\twire\t[2:0]\t%s_arprot, %s_awprot;\n"
		"\twire\t[%d:0]\t%s_wdata;\n"
		"\twire\t[%d:0]\t%s_wstrb;\n\n",
		busp, busp, busp,
		busp, busp,
			aw-1, busp, busp,
		busp, busp,
			m_info->data_width()-1, busp,
			(m_info->data_width()/8)-1, busp);
	fprintf(fp, "\t// Verilator lint_on  UNUSED\n");
}

void	AXILBUS::writeout_bus_slave_defns_v(FILE *fp) {
	PLIST	*p = m_info->m_plist;
	STRINGP	n = name();

	if (m_slist) {
		for(PLIST::iterator pp=m_slist->begin();
				pp != m_slist->end(); pp++) {
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str(), "(SIO)");
		}
	} else if (!m_is_single)
		fprintf(fp, "\n\t// Bus %s has no SINGLE slaves\n\t//\n", n->c_str());
	else
		fprintf(fp, "\n\t// Bus %s is all SINGLE slaves\n\t//\n", n->c_str());

	if (m_dlist) {
		for(PLIST::iterator pp=m_dlist->begin();
				pp != m_dlist->end(); pp++) {
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str(), "(DIO)");
		}
	} else if (!m_is_double)
		fprintf(fp, "\n\t// Bus %s has no DOUBLE slaves\n\t//\n", n->c_str());
	else
		fprintf(fp, "\n\t// Bus %s is all DOUBLE slaves\n\t//\n", n->c_str());


	if (p) {
		for(PLIST::iterator pp=p->begin(); pp != p->end(); pp++) {
			writeout_defn_v(fp, (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str());
		}
	} else {
		gbl_msg.error("%s has no slaves\n", n->c_str());
	}
}

void	AXILBUS::writeout_bus_master_defns_v(FILE *fp) {
	MLIST	*m = m_info->m_mlist;
	STRINGP	n = name();

	if (m) {
		for(MLIST::iterator pp=m->begin(); pp != m->end(); pp++) {
			writeout_defn_v(fp, (*pp)->name()->c_str(),
			(*pp)->bus_prefix()->c_str());
		}
	} else {
		gbl_msg.error("Bus %s has no masters\n", n->c_str());
	}
}

void	AXILBUS::write_addr_range(FILE *fp, const PERIPHP p, const int dalines) {
	unsigned	w = address_width();
	w = (w+3)/4;
	if (p->p_naddr == 1)
		fprintf(fp, " // 0x%0*lx", w, p->p_base);
	else
		fprintf(fp, " // 0x%0*lx - 0x%0*lx", w, p->p_base,
			w, p->p_base + (p->p_naddr << (dalines))-1);
}

void	AXILBUS::writeout_no_slave_v(FILE *fp, STRINGP prefix) {
}

void	AXILBUS::writeout_no_master_v(FILE *fp) {
	if (!m_info || !name())
		gbl_msg.error("(Unnamed bus) has no name!\n");
}

STRINGP	AXILBUS::master_name(int k) {
	MLISTP ml = m_info->m_mlist;

	return (*ml)[k]->name();
}

//
// Connect this master to the crossbar.  Specifically, we want to output
// a list of master connections to fill the given port.
//
void AXILBUS::xbarcon_master(FILE *fp, const char *tabs,
			const char *pfx,const char *sig, bool comma) {
	STRING lcase = STRING(sig);

	for(unsigned k=0; k<lcase.size(); k++)
		lcase[k] = tolower(lcase[k]);

	fprintf(fp, "%s%s%s({\n", tabs, pfx, sig);
	for(unsigned k=m_info->m_mlist->size()-1; k> 0; k--) {
		BMASTER *m = (*m_info->m_mlist)[k];
		STRINGP busp = m->bus_prefix();
		fprintf(fp, "%s\t%s_%s,\n", tabs, busp->c_str(), lcase.c_str());
	}
	fprintf(fp, "%s\t%s_%s\n", tabs, (*m_info->m_mlist)[0]->bus_prefix()->c_str(),
		lcase.c_str());
	fprintf(fp, "%s})%s\n", tabs, comma ? ",":"");
}

//
// Output a list of connections to slave bus wires.  Used in connecting the
// slaves to the various crossbar inputs.
//
void AXILBUS::xbarcon_slave(FILE *fp, PLIST *pl, const char *tabs,
			const char *pfx,const char *sig, bool comma) {
	STRING lcase = STRING(sig);

	for(unsigned k=0; k<lcase.size(); k++)
		lcase[k] = tolower(lcase[k]);

	fprintf(fp, "%s%s%s({\n", tabs, pfx, sig);
	for(unsigned k=pl->size()-1; k> 0; k--)
		fprintf(fp, "%s\t%s_%s,\n", tabs, (*pl)[k]->bus_prefix()->c_str(), lcase.c_str());
	fprintf(fp, "%s\t%s_%s\n", tabs, (*pl)[0]->bus_prefix()->c_str(), lcase.c_str());
	fprintf(fp, "%s})%s\n", tabs, comma ? ",":"");
}

void	AXILBUS::writeout_bus_logic_v(FILE *fp) {
	STRINGP		n = name(), rst;
	CLOCKINFO	*c = m_info->m_clock;
	PLIST::iterator	pp;
	PLISTP		pl;
	unsigned	unused_lsbs;
	MLISTP		m_mlist = m_info->m_mlist;

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


	unused_lsbs = nextlg(m_info->data_width())-3;
	if (m_info->m_plist->size() == 0) {
		fprintf(fp, "\t//\n"
			"\t// Bus %s has no slaves\n"
			"\t//\n\n", n->c_str());

		// Since this bus has no slaves, any attempt to access it
		// needs to cause a bus error.
		//
		// Need to loop through all possible masters ...
		for(unsigned m=0; m_info && m < m_info->m_mlist->size(); m++) {
			STRINGP	mstr = master_name(m);
			const	char *mp = mstr->c_str();
			fprintf(fp,
				"\t//\n"
				"\t// The %s bus has no slaves assigned to it\n"
				"\t//\n"
		"\tassign	%s_awready = %s_bvalid && %s_bready;\n"
		"\tassign	%s_wready  = %s_awready;\n"
		"\tassign	%s_bvalid = %s_awvalid && %s_wvalid;\n"
		"\tassign	%s_bresp = 2'b11;\n"
		"\t//\n"
		"\tassign	%s_arready = %s_rready;\n"
		"\tassign	%s_rvalid = %s_arvalid;\n"
		"\tassign	%s_rresp = 2'b11;\n\n",
				mp,
				//
				mp, mp, mp,
				mp, mp,
				mp, mp, mp,
				mp,
				//
				mp, mp,
				mp, mp,
				mp);
		}

		return;
	} else if (NULL == m_mlist || m_mlist->size() == 0) {
		for(unsigned p=0; p < m_info->m_plist->size(); p++) {
			STRINGP	pstr = (*m_info->m_plist)[p]->bus_prefix();
			fprintf(fp,
		"\t//\n"
		"\t// The %s bus has no masters assigned to it\n"
		"\t//\n"
		"\tassign	%s_awready = %s_bvalid && %s_bready;\n"
		"\tassign	%s_wready  = %s_awready;\n"
		"\tassign	%s_bvalid = %s_awvalid && %s_wvalid;\n"
		"\tassign	%s_bresp = 2'b11;\n"
		"\t//\n"
		"\tassign	%s_arready = %s_rready;\n"
		"\tassign	%s_rvalid = %s_arvalid;\n"
		"\tassign	%s_rresp = 2'b11;\n\n",
		pstr->c_str(),
		//
		pstr->c_str(), pstr->c_str(), pstr->c_str(),
		pstr->c_str(), pstr->c_str(),
		pstr->c_str(), pstr->c_str(), pstr->c_str(),
		pstr->c_str(),
		//
		pstr->c_str(), pstr->c_str(),
		pstr->c_str(), pstr->c_str(),
		pstr->c_str());
		}

		return;
	} else if ((m_info->m_plist->size() == 1)&&(m_mlist && m_mlist->size() == 1)) {
		// Only one master connected to only one slave--skip all the
		// extra connection logic.
		//
		// Can only simplify if there's only one peripheral and only
		// one master
		//
		STRINGP	slv  = (*m_info->m_plist)[0]->bus_prefix();
		STRINGP	mstr = (*m_mlist)[0]->bus_prefix();

		fprintf(fp,
		"//\n"
		"// Bus %s has only one master (%s) and one slave (%s)\n"
		"// connected to it -- skipping the interconnect\n"
		"//\n"
		"assign	%s_awvalid = %s_awvalid;\n"
		"assign	%s_awready = %s_awready;\n"
		"assign	%s_awaddr  = %s_awaddr;\n"
		"assign	%s_awprot  = %s_awprot;\n"
		"//\n"
		"assign	%s_wvalid = %s_wvalid;\n"
		"assign	%s_wready = %s_wready;\n"
		"assign	%s_wdata  = %s_wdata;\n"
		"assign	%s_wstrb  = %s_wstrb;\n"
		"//\n"
		"assign	%s_bvalid = %s_bvalid;\n"
		"assign	%s_bready = %s_bready;\n"
		"assign	%s_bresp  = %s_bresp;\n"
		"//\n"
		"assign	%s_arvalid = %s_arvalid;\n"
		"assign	%s_arready = %s_arready;\n"
		"assign	%s_araddr  = %s_araddr;\n"
		"assign	%s_arprot  = %s_arprot;\n"
		"//\n"
		"assign	%s_rvalid = %s_rvalid;\n"
		"assign	%s_rready = %s_rready;\n"
		"assign	%s_rdata  = %s_rdata;\n"
		"assign	%s_rresp  = %s_rresp;\n"
		"\n\n",
		n->c_str(), master_name(0)->c_str(),
		(*m_info->m_plist)[0]->p_name->c_str(),
		//
		slv->c_str(),  mstr->c_str(),
		mstr->c_str(), slv->c_str(),
		slv->c_str(),  mstr->c_str(),
		slv->c_str(),  mstr->c_str(),
		//
		slv->c_str(),  mstr->c_str(),
		mstr->c_str(), slv->c_str(),
		slv->c_str(),  mstr->c_str(),
		slv->c_str(),  mstr->c_str(),
		//
		mstr->c_str(), slv->c_str(),
		slv->c_str(),  mstr->c_str(),
		mstr->c_str(), slv->c_str(),
		//
		slv->c_str(),  mstr->c_str(),
		mstr->c_str(), slv->c_str(),
		slv->c_str(),  mstr->c_str(),
		slv->c_str(),  mstr->c_str(),
		//
		mstr->c_str(), slv->c_str(),
		slv->c_str(),  mstr->c_str(),
		mstr->c_str(), slv->c_str(),
		mstr->c_str(), slv->c_str()
		);

		return;
	}

	// Start with the slist
	if (m_is_single) {
		assert(1 == m_info->m_mlist->size());

		PLIST	*slist = m_info->m_plist;
		const char *np = n->c_str();
		STRING	s = STRING(*(*m_mlist)[0]->bus_prefix());
		int	aw = nextlg(slist->size())+nextlg(m_info->data_width())-3;

		fprintf(fp,
			"\t// Some extra wires to capture combined values--values\n"
			"\t// that will be the same across all slaves of the\n"
			"\t// class\n"
			"\twire [2:0]\t%s_siow_awprot;\n"
			"\twire [%d:0]\t%s_siow_wdata;\n"
			"\twire [%d:0]\t%s_siow_wstrb;\n"
			"\twire [2:0]\t%s_siow_arprot;\n\n",
			n->c_str(), m_info->data_width()-1, n->c_str(),
			m_info->data_width()/8-1, n->c_str(), n->c_str());

		fprintf(fp,
		"\taxilsingle #(\n"
			"\t\t// .C_AXI_ADDR_WIDTH(%d), // must be only one word address\n"
			"\t\t.C_AXI_DATA_WIDTH(%d),\n"
			"\t\t.NS(%ld)",
			aw, m_info->data_width(),
			slist->size());
		xbar_option(fp, KY_OPT_LOWPOWER,  ",\n\t\t.OPT_LOWPOWER(%)",
			"1\'b1");
		fprintf(fp,
		"\n\t) %s_axilsingle(\n", np);
		fprintf(fp,
			"\t\t.S_AXI_ACLK(%s),\n"
			"\t\t.S_AXI_ARESETN(%s),\n",
				c->m_wire->c_str(), rst->c_str());

		fprintf(fp, "\t\t//\n");
		fprintf(fp,
			"\t\t.S_AXI_AWVALID(%s_awvalid),\n"
			"\t\t.S_AXI_AWREADY(%s_awready),\n"
			"\t\t.S_AXI_AWADDR( %s_awaddr[%d:0]),\n"
			"\t\t.S_AXI_AWPROT( %s_awprot),\n"
			"\t\t//\n"
			"\t\t.S_AXI_WVALID( %s_wvalid),\n"
			"\t\t.S_AXI_WREADY( %s_wready),\n"
			"\t\t.S_AXI_WDATA(  %s_wdata),\n"
			"\t\t.S_AXI_WSTRB(  %s_wstrb),\n"
			"\t\t//\n"
			"\t\t.S_AXI_BVALID( %s_bvalid),\n"
			"\t\t.S_AXI_BREADY( %s_bready),\n"
			"\t\t.S_AXI_BRESP(  %s_bresp),\n"
			"\t\t// Read connections\n"
			"\t\t.S_AXI_ARVALID(%s_arvalid),\n"
			"\t\t.S_AXI_ARREADY(%s_arready),\n"
			"\t\t.S_AXI_ARADDR( %s_araddr[%d:0]),\n"
			"\t\t.S_AXI_ARPROT( %s_arprot),\n"
			"\t\t//\n"
			"\t\t.S_AXI_RVALID( %s_rvalid),\n"
			"\t\t.S_AXI_RREADY( %s_rready),\n"
			"\t\t.S_AXI_RDATA(  %s_rdata),\n"
			"\t\t.S_AXI_RRESP(  %s_rresp),\n"
			"\t\t//\n"
			"\t\t// Connections to slaves\n"
			"\t\t//\n",
			s.c_str(), s.c_str(), s.c_str(), aw-1, s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), aw-1, s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), s.c_str());

		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","AWVALID");
		fprintf(fp,
			"\t\t.M_AXI_AWPROT(%s_siow_awprot),\n"
			"\t\t.M_AXI_WDATA( %s_siow_wdata),\n"
			"\t\t.M_AXI_WSTRB( %s_siow_wstrb),\n",
			n->c_str(), n->c_str(), n->c_str());
		fprintf(fp, "\t\t//\n");
		fprintf(fp, "\t\t//\n");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","BRESP");
		fprintf(fp, "\t\t// Read connections\n");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","ARVALID");
		fprintf(fp,
			"\t\t.M_AXI_ARPROT(%s_siow_arprot),\n"
			"\t\t//\n", n->c_str());
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","RDATA");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","RRESP", false);
		fprintf(fp, "\t);\n\n");
		fprintf(fp,"\t//\n\t// Now connecting the extra slaves wires "
			"to the AXISINGLE controller\n\t//\n");

		for(int k=slist->size(); k>0; k--) {
			PERIPHP p = (*slist)[k-1];
			const char *pn = p->p_name->c_str(),
				*ns = n->c_str();

			fprintf(fp, "\t// %s\n", pn);
			pn = p->bus_prefix()->c_str();
			fprintf(fp, "\tassign %s_awaddr = 0;\n", pn);
			fprintf(fp, "\tassign %s_awprot = %s_siow_awprot;\n", pn, ns);
			fprintf(fp, "\tassign %s_wvalid = %s_awvalid;\n", pn, pn);
			fprintf(fp, "\tassign %s_wdata = %s_siow_wdata;\n", pn, ns);
			fprintf(fp, "\tassign %s_wstrb = %s_siow_wstrb;\n", pn, ns);
			fprintf(fp, "\tassign %s_bready = 1\'b1;\n", pn);
			fprintf(fp, "\tassign %s_araddr = 0;\n", pn);
			fprintf(fp, "\tassign %s_arprot = %s_siow_arprot;\n", pn, ns);
			fprintf(fp, "\tassign %s_rready = 1\'b1;\n", pn);
		}

		return;
	} else if (!m_slist)
		fprintf(fp, "\t//\n\t// No class SINGLE peripherals on the \"%s\" bus\n\t//\n\n", n->c_str());

	// Then the dlist
	if (m_is_double) {
		if (m_dlist)
			pl = m_dlist;
		else
			pl = m_info->m_plist;
		STRING	s = STRING(*(*m_mlist)[0]->bus_prefix());
		int	aw = address_width();

		fprintf(fp,
			"\t//\n"
			"\t// %s Bus logic to handle %ld DOUBLE slaves\n"
			"\t//\n"
			"\t//\n", n->c_str(), pl->size());

		fprintf(fp,
			"\t// Some extra wires to capture combined values--values\n"
			"\t// that will be the same across all slaves of the\n"
			"\t// class\n"
			"\twire [%d:0]\t%s_diow_awaddr;\n"
			"\twire [2:0]\t%s_diow_awprot;\n"
			"\twire [%d:0]\t%s_diow_wdata;\n"
			"\twire [%d:0]\t%s_diow_wstrb;\n"
			"\twire [%d:0]\t%s_diow_araddr;\n"
			"\twire [2:0]\t%s_diow_arprot;\n\n",
			address_width()-1, n->c_str(),
			n->c_str(),
			m_info->data_width()-1, n->c_str(),
			m_info->data_width()/8-1, n->c_str(),
			address_width()-1, n->c_str(),
			n->c_str());

		fprintf(fp,
		"\taxildouble #(\n"
			"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
			"\t\t.C_AXI_DATA_WIDTH(%d),\n"
			"\t\t.NS(%ld),\n",
			address_width(), m_info->data_width(),
			pl->size());
		xbar_option(fp, KY_OPT_LOWPOWER,  "\t\t.OPT_LOWPOWER(%),\n",
			"1\'b1");
		slave_addr(fp, pl); fprintf(fp, ",\n");
		slave_mask(fp, pl); fprintf(fp, "\n");
		fprintf(fp,
		"\t) %s_axildouble(\n",
			n->c_str());
		fprintf(fp,
			"\t\t.S_AXI_ACLK(%s),\n"
			"\t\t.S_AXI_ARESETN(%s),\n",
				c->m_wire->c_str(), rst->c_str());

		fprintf(fp, "\t\t//\n");
		fprintf(fp,
			"\t\t.S_AXI_AWVALID(%s_awvalid),\n"
			"\t\t.S_AXI_AWREADY(%s_awready),\n"
			"\t\t.S_AXI_AWADDR( %s_awaddr[%d:0]),\n"
			"\t\t.S_AXI_AWPROT( %s_awprot),\n"
			"\t\t//\n"
			"\t\t.S_AXI_WVALID( %s_wvalid),\n"
			"\t\t.S_AXI_WREADY( %s_wready),\n"
			"\t\t.S_AXI_WDATA(  %s_wdata),\n"
			"\t\t.S_AXI_WSTRB(  %s_wstrb),\n"
			"\t\t//\n"
			"\t\t.S_AXI_BVALID( %s_bvalid),\n"
			"\t\t.S_AXI_BREADY( %s_bready),\n"
			"\t\t.S_AXI_BRESP(  %s_bresp),\n"
			"\t\t//\n"
			"\t\t// Read connections\n"
			"\t\t.S_AXI_ARVALID(%s_arvalid),\n"
			"\t\t.S_AXI_ARREADY(%s_arready),\n"
			"\t\t.S_AXI_ARADDR( %s_araddr[%d:0]),\n"
			"\t\t.S_AXI_ARPROT( %s_arprot),\n"
			"\t\t//\n"
			"\t\t.S_AXI_RVALID( %s_rvalid),\n"
			"\t\t.S_AXI_RREADY( %s_rready),\n"
			"\t\t.S_AXI_RDATA(  %s_rdata),\n"
			"\t\t.S_AXI_RRESP(  %s_rresp),\n"
			"\t\t//\n"
			"\t\t// Connections to slaves\n"
			"\t\t//\n",
			s.c_str(), s.c_str(), s.c_str(), aw-1, s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), aw-1, s.c_str(),
			s.c_str(), s.c_str(), s.c_str(), s.c_str());

		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWVALID");
		fprintf(fp,
			"\t\t.M_AXI_AWADDR(%s_diow_awaddr),\n"
			"\t\t.M_AXI_AWPROT(%s_diow_awprot),\n"
			"\t\t.M_AXI_WDATA( %s_diow_wdata),\n"
			"\t\t.M_AXI_WSTRB( %s_diow_wstrb),\n",
			n->c_str(), n->c_str(), n->c_str(), n->c_str());
		fprintf(fp, "\t\t//\n");
		fprintf(fp, "\t\t//\n");
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BRESP");
		fprintf(fp, "\t\t// Read connections\n");
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARVALID");
		fprintf(fp,
			"\t\t.M_AXI_ARADDR(%s_diow_araddr),\n"
			"\t\t.M_AXI_ARPROT(%s_diow_arprot),\n"
			"\t\t//\n", n->c_str(), n->c_str());
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RDATA");
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RRESP", false);
		fprintf(fp, "\t\t);\n\n");
		fprintf(fp,"\t//\n\t// Now connecting the extra slaves wires "
			"to the AXILDOUBLE controller\n\t//\n");

		for(int k=pl->size(); k>0; k--) {
			PERIPHP p = (*pl)[k-1];
			const char *pn = p->p_name->c_str(),
				*pfx = p->bus_prefix()->c_str();
			int	aw = p->p_awid + unused_lsbs;

			fprintf(fp, "\t// %s\n", pn);
			fprintf(fp, "\tassign %s_awaddr = %s_diow_awaddr;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awprot = %s_diow_awprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_wvalid = %s_awvalid;\n", pfx, pfx);
			fprintf(fp, "\tassign %s_wdata = %s_diow_wdata;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_wstrb = %s_diow_wstrb;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_bready = 1\'b1;\n", pfx);
			fprintf(fp, "\tassign %s_araddr = %s_diow_araddr;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arprot = %s_diow_arprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_rready = 1\'b1;\n", pfx);
		}

		return;
	} else if (!m_dlist)
		fprintf(fp, "\t//\n\t// No class DOUBLE peripherals on the \"%s\" bus\n\t//\n\n", n->c_str());

	//
	//
	// Now for the main set of slaves
	//
	//
	pl = m_info->m_plist;

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
	// Now create the crossbar interconnect
	//
	// if ((m_mlist->size() == 1)&&(m_info->m_plist->size() == 1))
	//	special case we dealt with above

	fprintf(fp,
	"\t//\n"
	"\t// Connect the %s bus components together using the axilxbar()\n"
	"\t//\n"
	"\t//\n", n->c_str());

	fprintf(fp,
	"\taxilxbar #(\n"
	"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
	"\t\t.C_AXI_DATA_WIDTH(%d),\n"
	"\t\t.NM(%ld), .NS(%ld),\n",
		address_width(),
		m_info->data_width(),
		m_mlist->size(),
		m_info->m_plist->size());
	slave_addr(fp, m_info->m_plist); fprintf(fp, ",\n");
	slave_mask(fp, m_info->m_plist);

	xbar_option(fp, KY_OPT_LOWPOWER,  ",\n\t\t.OPT_LOWPOWER(%)", "1\'b1");
	xbar_option(fp, KY_OPT_LINGER,    ",\n\t\t.OPT_LINGER(%)");
	xbar_option(fp, KY_OPT_LGMAXBURST,",\n\t\t.LGMAXBURST(%)");
	//
	fprintf(fp,
	"\n\t) %s_xbar(\n"
		"\t\t.S_AXI_ACLK(%s),\n",
		n->c_str(), m_info->m_clock->m_name->c_str());
	fprintf(fp, "\t\t.S_AXI_ARESETN(%s),\n", rst->c_str());

	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWPROT");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWADDR");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","WVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","WREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","WDATA");
	xbarcon_master(fp, "\t\t",".S_AXI_","WSTRB");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","BVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","BREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","BRESP");
	fprintf(fp, "\t\t// Read connections\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARPROT");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARADDR");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","RVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","RREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","RDATA");
	xbarcon_master(fp, "\t\t",".S_AXI_","RRESP");
	fprintf(fp, "\t\t//\n");
	fprintf(fp, "\t\t// Connections to slaves\n");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWPROT");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWADDR");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WDATA");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WSTRB");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BRESP");
	fprintf(fp, "\t\t// Read connections\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARPROT");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARADDR");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RDATA");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RRESP", false);
	fprintf(fp, "\t\t);\n\n");

	for(unsigned k=0; k < m_info->m_plist->size(); k++) {
		PERIPHP p = (*m_info->m_plist)[k];
		STRINGP	busp = p->bus_prefix();

		if (p->write_only()) {
			fprintf(fp,
			"\tassign %s_awready = %s_bvalid & %s_bready;\n"
			"\tassign %s_wready = %s_awready;\n"
			"\tassign %s_bvalid = %s_awvalid & %s_wvalid;\n"
			"\tassign %s_bresp  = 2\'b10; // SLVERR\n",
				busp->c_str(), busp->c_str(), busp->c_str(),
				busp->c_str(), busp->c_str(),
				busp->c_str(), busp->c_str(), busp->c_str(),
				busp->c_str());
		}
		if (p->read_only()) {
			fprintf(fp,
			"\tassign %s_arready = %s_rready;\n"
			"\tassign %s_rvalid  = %s_arvalid;\n"
			"\tassign %s_bresp  = 2\'b10; // SLVERR\n",
					busp->c_str(), busp->c_str(),
					busp->c_str(), busp->c_str(),
					busp->c_str());
		}
	}
}

STRINGP	AXILBUS::master_portlist(BMASTERP m) {
	STRING	str;

	if (m->read_only())
		str = str + STRING("// Master is read only\n\t\t");
	else
		str = str + STRING(
	"//\n"
	"\t\t@$(MASTER.PREFIX)_awvalid,\n"
	"\t\t@$(MASTER.PREFIX)_awready,\n"
	"\t\t@$(MASTER.PREFIX)_awaddr[@$(MASTER.BUS.AWID)-1:0],\n"
	"\t\t@$(MASTER.PREFIX)_awprot,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_wvalid,\n"
	"\t\t@$(MASTER.PREFIX)_wready,\n"
	"\t\t@$(MASTER.PREFIX)_wdata,\n"
	"\t\t@$(MASTER.PREFIX)_wstrb,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_bvalid,\n"
	"\t\t@$(MASTER.PREFIX)_bready,\n"
	"\t\t@$(MASTER.PREFIX)_bresp,\n\t\t");
	if (!m->read_only() && !m->write_only())
		str = str + STRING("// Read connections\n\t\t");
	if (m->write_only())
		str = str + STRING("// Master is write-only\n");
	else
		str = str + STRING(
	"@$(MASTER.PREFIX)_arvalid,\n"
	"\t\t@$(MASTER.PREFIX)_arready,\n"
	"\t\t@$(MASTER.PREFIX)_araddr[@$(MASTER.BUS.AWID)-1:0],\n"
	"\t\t@$(MASTER.PREFIX)_arprot,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_rvalid,\n"
	"\t\t@$(MASTER.PREFIX)_rready,\n"
	"\t\t@$(MASTER.PREFIX)_rdata,\n"
	"\t\t@$(MASTER.PREFIX)_rresp");

	return new STRING(str);
}

STRINGP	AXILBUS::iansi(BMASTERP m) {
	return new STRING("");
}

STRINGP	AXILBUS::oansi(BMASTERP m) {
	return new STRING("");
}

STRINGP	AXILBUS::master_ansprefix(BMASTERP m) {
	return new STRING("M_AXI_");
}

STRINGP	AXILBUS::master_ansi_portlist(BMASTERP m) {
	STRING	str;

	if (!m->read_only())
		str = str + STRING(
	"//\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWVALID(@$(MASTER.PREFIX)_awvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)AWREADY(@$(MASTER.PREFIX)_awready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWADDR( @$(MASTER.PREFIX)_awaddr[@$(MASTER.BUS.AWID)-1:0]),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWPROT( @$(MASTER.PREFIX)_awprot),\n"
	"//\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WVALID(@$(MASTER.PREFIX)_wvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)WREADY(@$(MASTER.PREFIX)_wready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WDATA( @$(MASTER.PREFIX)_wdata),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WSTRB( @$(MASTER.PREFIX)_wstrb),\n"
	"//\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)BVALID(@$(MASTER.PREFIX)_bvalid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)BREADY(@$(MASTER.PREFIX)_bready),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)BRESP( @$(MASTER.PREFIX)_bresp),\n");
	if (!m->read_only() && !m->write_only())
		str = str + STRING("\t\t// Read connections\n");
	if (!m->write_only())
		str = str + STRING(
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARVALID(@$(MASTER.PREFIX)_arvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)ARREADY(@$(MASTER.PREFIX)_arready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARADDR( @$(MASTER.PREFIX)_araddr[@$(MASTER.BUS.AWID)-1:0]),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARPROT( @$(MASTER.PREFIX)_arprot),\n"
	"//\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RVALID(@$(MASTER.PREFIX)_rvalid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)RREADY(@$(MASTER.PREFIX)_rready),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RDATA( @$(MASTER.PREFIX)_rdata),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RRESP( @$(MASTER.PREFIX)_rresp)");

	return new STRING(str);
}

STRINGP	AXILBUS::slave_ansprefix(PERIPHP m) {
	return new STRING("S_AXI_");
}

STRINGP	AXILBUS::slave_portlist(PERIPHP p) {
	STRING	str;

	if (p->read_only())
		str = str + STRING("// Slave is read only\n\t\t");
	else {
		str = str + STRING(
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_awvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_awready,\n"
	"\t\t@$(SLAVE.PREFIX)_awaddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0],\n"
	"\t\t@$(SLAVE.PREFIX)_awprot,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_wvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_wready,\n"
	"\t\t@$(SLAVE.PREFIX)_wdata,\n"
	"\t\t@$(SLAVE.PREFIX)_wstrb,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_bvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_bready,\n"
	"\t\t@$(SLAVE.PREFIX)_bresp,\n\t\t");
	}
	if (!p->read_only() && !p->write_only())
		str = str + STRING("// Read connections\n");
	if (p->write_only())
		str = str + STRING("// Slave is write-only\n");
	else {
		str = str + STRING(
	"\t\t@$(SLAVE.PREFIX)_arvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_arready,\n"
	"\t\t@$(SLAVE.PREFIX)_araddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0],\n"
	"\t\t@$(SLAVE.PREFIX)_arprot,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_rvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_rready,\n"
	"\t\t@$(SLAVE.PREFIX)_rdata,\n"
	"\t\t@$(SLAVE.PREFIX)_rresp");
	}

	return new STRING(str);
}

STRINGP	AXILBUS::slave_ansi_portlist(PERIPHP p) {
	STRING	str;

	if (!p->read_only())
		str = str + STRING(
	"//\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWVALID(@$(SLAVE.PREFIX)_awvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)AWREADY(@$(SLAVE.PREFIX)_awready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWADDR( @$(SLAVE.PREFIX)_awaddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0]),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWPROT( @$(SLAVE.PREFIX)_awprot),\n"
	"//\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WVALID(@$(SLAVE.PREFIX)_wvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)WREADY(@$(SLAVE.PREFIX)_wready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WDATA( @$(SLAVE.PREFIX)_wdata),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WSTRB( @$(SLAVE.PREFIX)_wstrb),\n"
	"//\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)BVALID(@$(SLAVE.PREFIX)_bvalid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)BREADY(@$(SLAVE.PREFIX)_bready),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)BRESP( @$(SLAVE.PREFIX)_bresp),\n");
	if (!p->read_only() && !p->write_only())
		str = str + STRING("\t\t// Read connections\n");
	if (!p->write_only())
		str = str + STRING(
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARVALID(@$(SLAVE.PREFIX)_arvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)ARREADY(@$(SLAVE.PREFIX)_arready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARADDR( @$(SLAVE.PREFIX)_araddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0]),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARPROT( @$(SLAVE.PREFIX)_arprot),\n"
	"//\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RVALID(@$(SLAVE.PREFIX)_rvalid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)RREADY(@$(SLAVE.PREFIX)_rready),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RDATA( @$(SLAVE.PREFIX)_rdata),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RRESP( @$(SLAVE.PREFIX)_rresp)");

	return new STRING(str);
}

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
STRINGP	AXILBUSCLASS::name(void) {
	return new STRING("axil");
}

STRINGP	AXILBUSCLASS::longname(void) {
	return new STRING("AXI-lite");
}

bool	AXILBUSCLASS::matchtype(STRINGP str) {
	if (!str)
		// We are not the default
		return false;
	if (strcasecmp(str->c_str(), "axil")==0)
		return true;
	if (strcasecmp(str->c_str(), "axi-lite")==0)
		return true;
	if (strcasecmp(str->c_str(), "axi lite")==0)
		return true;
// printf("ERR: No match for bus type %s\n",str->c_str());
	return false;
}

bool	AXILBUSCLASS::matchfail(MAPDHASH *bhash) {
	return false;
}

GENBUS *AXILBUSCLASS::create(BUSINFO *bi) {
	AXILBUS	*busclass;

	busclass = new AXILBUS(bi);
	return busclass;
}
