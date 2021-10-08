////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	axil.cpp
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
// }}}
// Copyright (C) 2019-2021, Gisselquist Technology, LLC
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

#include "axil.h"
#include "axi.h"

#define	PREFIX

extern	AXIBUSCLASS	axiclass;
const	unsigned	AXI_MIN_ADDRESS_WIDTH = 12;

AXIBUS::AXIBUS(BUSINFO *bi) : AXILBUS(bi) {
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

void	AXIBUS::allocate_subbus(void) {
	// {{{
	PLIST	*pl = m_info->m_plist;
	BUSINFO	*sbi = NULL, *dbi = NULL;

	if (NULL == pl || pl->size() == 0) {
		gbl_msg.warning("Bus %s has no attached slaves\n",
			(name()) ? name()->c_str() : "(No-name)");
	}

	gbl_msg.info("Generating AXI4 bus logic generator for %s\n",
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
						&& ptyp->compare(KYDOUBLE) == 0) {
				m_info->m_plist->erase(pl->begin()+pi);
				pi--;
				m_dlist->add(p);
			} else {
			// Leave this peripheral in m_info->m_plist
			}
		}
	}

	if (sbi)
		setstring(sbi->m_hash, KY_TYPE, axiclass.name());
	if (dbi)
		setstring(dbi->m_hash, KY_TYPE, axiclass.name());
	REHASH;
}
// }}}

int	AXIBUS::address_width(void) {
	// {{{
	assert(m_info);
	return m_info->m_address_width;
}
// }}}

bool	AXIBUS::get_base_address(MAPDHASH *phash, unsigned &base) {
	// {{{
	if (!m_info || !m_info->m_plist) {
		gbl_msg.error("BUS[%s] has no peripherals!\n",
			(name()) ? name()->c_str() : "(No name)");
		return false;
	} else
		return m_info->m_plist->get_base_address(phash, base);
}
// }}}

bool	compare_idwidths(BMASTERP a, BMASTERP b) {
	int	aidw, bidw;

	assert(a);
	assert(b);

	if (!getvalue(a->m_hash, KYMASTER_IDWIDTH, aidw)) {
		aidw = 0;
		gbl_msg.error("Bus master %s has no required ID width specified, assuming 0 bits\n", a->name()->c_str());
	}
	if (!getvalue(a->m_hash, KYMASTER_IDWIDTH, bidw)) {
		bidw = 0;
		gbl_msg.error("Bus master %s has no required ID width specified, assuming 0 bits\n", b->name()->c_str());
	}

	return (aidw < bidw);
}

/*
void	AXIBUS::assign_ids(void) {
	int	id_width;

	if (!m_info)
		return;
	if (m_info->m_ids_assigned)
		return;
	gbl_msg.info("AXI4: Assigning IDs for bus %s\n",
		(name()) ? name()->c_str() : "(No name bus)");
	if (m_info->m_list->size() == 0) {
		m_id_width = 0;
		return;
	} else if (m_info->m_list->size() == 0) {
		BMASTERP	m = (*m_info->m_list)[0];
		if (!getvalue(m->m_hash, KYMASTER_IDWIDTH, m_id_width)) {
			gbl_msg.warning("Bus master %s has no required ID width specified, assuming 0 bits\n", m->name()->c_str());
			gbl_msg.info("This is a warning since bus %s has only one master", name()->c_str());
			m_id_width = 0;
		}
		return;
	}

	sort(m_info->m_list->begin(), m_info->m_list->end(), 
		compare_idwidths);
	base = 0;
	for(auto p=m_info->m_list->begin(); p!=m_info->m_list->end(); p++) {
		if (!getvalue((*m_info->m_list)[0]->m_hash, KYMASTER_IDWIDTH, iw)
			iw = 1;
		id = base + ((1<<iw)-1) & (-1l<<iw);
		base = id + (1<<iw);

		setvalue((*m_info->m_list)[0]->m_hash, KYMASTER_BUSID, id);
	}

	setvalue(m_info->m_hash, KY_IDWIDTH, nextlg(base));
}
*/

void	AXIBUS::assign_addresses(void) {
	// {{{
	int	address_width;

	if (m_info->m_addresses_assigned)
		return;
	if ((NULL == m_slist)&&(NULL == m_dlist))
		allocate_subbus();

	if (!m_info)
		return;
	gbl_msg.info("AXI4: Assigning addresses for bus %s\n",
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
// }}}

BUSINFO *AXIBUS::create_sio(void) {
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
	setstring(bushash, KY_TYPE, new STRING("axi4"));
	sbi->m_data_width = m_info->m_data_width;
	sbi->m_clock      = m_info->m_clock;
	sbi->addmaster(m_info->m_hash);
	subp = new SUBBUS(shash, sioname, sbi);
	subp->p_slave_bus = m_info;
	// subp->p_master_bus = set by the SUBBUS to be sbi
	m_info->m_plist->add(subp);
	// m_plist->integrity_check();
	sbi->add();
	m_slist = sbi->m_plist;

	return sbi;
}
// }}}

BUSINFO *AXIBUS::create_dio(void) {
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
	setstring(bushash, KY_TYPE, new STRING("axi4"));
	setvalue(*bushash, KY_WIDTH, m_info->data_width());
	dbi->m_data_width = m_info->m_data_width;
	dbi->m_clock      = m_info->m_clock;
	dbi->addmaster(m_info->m_hash);
	subp = new SUBBUS(shash, dioname, dbi);
	subp->p_slave_bus = m_info;
	m_info->m_plist->add(subp);
	// subp->p_master_bus = set by the slave to be dbi
	dbi->add();
	m_dlist = dbi->m_plist;

	return dbi;
}
// }}}

int	AXIBUS::id_width(void) {
	int	value;

	if (m_info && getvalue(m_info->m_hash, KY_IDWIDTH, value))
		return m_id_width = value;
	return m_id_width;
}

void	AXIBUS::writeout_defn_v(FILE *fp, const char *mstype,
		const char *pname, const char* busp, const char *btyp){
	// {{{
	STRINGP	n = name();
	int	aw = address_width();
	int	iw = id_width();

	fprintf(fp, "\t// AXI4 %s definitions for bus %s%s,\n"
		"\t// component %s, with prefix %s\n"
		"\t// {{{\n",
		mstype, n->c_str(), btyp, pname, busp);
	fprintf(fp, "\t// Verilator lint_off UNUSED\n");
	fprintf(fp, "\twire\t\t%s_awvalid;\n", busp);
	fprintf(fp, "\twire\t\t%s_awready;\n", busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_awid;\n",   iw-1, busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_awaddr;\n", aw-1, busp);
	fprintf(fp, "\twire\t[7:0]\t%s_awlen;\n", busp);
	fprintf(fp, "\twire\t[2:0]\t%s_awsize;\n", busp);
	fprintf(fp, "\twire\t[1:0]\t%s_awburst;\n", busp);
	fprintf(fp, "\twire\t\t%s_awlock;\n", busp);
	fprintf(fp, "\twire\t[3:0]\t%s_awcache;\n", busp);
	fprintf(fp, "\twire\t[2:0]\t%s_awprot;\n", busp);
	fprintf(fp, "\twire\t[3:0]\t%s_awqos;\n", busp);
	fprintf(fp, "\t//\n");
	//
	fprintf(fp, "\twire\t\t%s_wvalid;\n", busp);
	fprintf(fp, "\twire\t\t%s_wready;\n", busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_wdata;\n\n", m_info->data_width()-1, busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_wstrb;\n\n", m_info->data_width()/8-1, busp);
	fprintf(fp, "\twire\t\t%s_wlast;\n\n", busp);
	//
	fprintf(fp, "\twire\t\t%s_bvalid;\n", busp);
	fprintf(fp, "\twire\t\t%s_bready;\n", busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_bid;\n", iw-1, busp);
	fprintf(fp, "\twire\t[1:0]\t%s_bresp;\n", busp);
	//
	fprintf(fp, "\twire\t\t%s_arvalid;\n", busp);
	fprintf(fp, "\twire\t\t%s_arready;\n", busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_arid;\n",   iw-1, busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_araddr;\n", aw-1, busp);
	fprintf(fp, "\twire\t[7:0]\t%s_arlen;\n", busp);
	fprintf(fp, "\twire\t[2:0]\t%s_arsize;\n", busp);
	fprintf(fp, "\twire\t[1:0]\t%s_arburst;\n", busp);
	fprintf(fp, "\twire\t\t%s_arlock;\n", busp);
	fprintf(fp, "\twire\t[3:0]\t%s_arcache;\n", busp);
	fprintf(fp, "\twire\t[2:0]\t%s_arprot;\n", busp);
	fprintf(fp, "\twire\t[3:0]\t%s_arqos;\n", busp);
	fprintf(fp, "\t//\n");
	//
	fprintf(fp, "\twire\t\t%s_rvalid;\n", busp);
	fprintf(fp, "\twire\t\t%s_rready;\n", busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_rid;\n", iw-1, busp);
	fprintf(fp, "\twire\t[%d:0]\t%s_rdata;\n\n", m_info->data_width()-1, busp);
	fprintf(fp, "\twire\t\t%s_rlast;\n\n", busp);
	fprintf(fp, "\twire\t[1:0]\t%s_rresp;\n", busp);
	fprintf(fp,
		"\t// Verilator lint_on  UNUSED\n"
		"\t// }}}\n");
}
// }}}

// Inherited from AXI-lite
void	AXIBUS::writeout_bus_slave_defns_v(FILE *fp) {
	// {{{
	PLIST	*p = m_info->m_plist;
	STRINGP	n = name();

	if (m_slist) {
		for(PLIST::iterator pp=m_slist->begin();
				pp != m_slist->end(); pp++) {
			writeout_defn_v(fp, "slave", (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str(), "(SIO)");
		}
	} else if (!m_is_single)
		fprintf(fp, "\n\t// Bus %s has no SINGLE slaves\n\t//\n", n->c_str());
	else
		fprintf(fp, "\n\t// Bus %s is all SINGLE slaves\n\t//\n", n->c_str());

	if (m_dlist) {
		for(PLIST::iterator pp=m_dlist->begin();
				pp != m_dlist->end(); pp++) {
			writeout_defn_v(fp, "slave", (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str(), "(DIO)");
		}
	} else if (!m_is_double)
		fprintf(fp, "\n\t// Bus %s has no DOUBLE slaves\n\t//\n", n->c_str());
	else
		fprintf(fp, "\n\t// Bus %s is all DOUBLE slaves\n\t//\n", n->c_str());


	if (p) {
		for(PLIST::iterator pp=p->begin(); pp != p->end(); pp++) {
			writeout_defn_v(fp, "slave", (*pp)->p_name->c_str(),
				(*pp)->bus_prefix()->c_str());
		}
	} else {
		gbl_msg.error("%s has no slaves\n", n->c_str());
	}
}
// }}}

void	AXIBUS::writeout_bus_master_defns_v(FILE *fp) {
	// {{{
	MLIST	*m = m_info->m_mlist;
	STRINGP	n = name();

	if (m) {
		for(MLIST::iterator pp=m->begin(); pp != m->end(); pp++) {
			writeout_defn_v(fp, "master", (*pp)->name()->c_str(),
			(*pp)->bus_prefix()->c_str());
		}
	} else {
		gbl_msg.warning("Bus %s has no masters\n", n->c_str());
	}
}
// }}}

// void	AXIBUS::write_addr_range(FILE *fp, const PERIPHP p, const int dalines)
// void	AXIBUS::writeout_no_slave_v(FILE *fp, STRINGP prefix)
// void	AXIBUS::writeout_no_master_v(FILE *fp)
// STRINGP	AXIBUS::master_name(int k)

//
// Connect this master to the crossbar.  Specifically, we want to output
// a list of master connections to fill the given port.
//
// void AXILBUS::xbarcon_master(FILE *fp, const char *tabs,

//
// Output a list of connections to slave bus wires.  Used in connecting the
// slaves to the various crossbar inputs.
//
// void AXILBUS::xbarcon_slave(FILE *fp, PLIST *pl, const char *tabs,
//			const char *pfx,const char *sig, bool comma)

void	AXIBUS::writeout_bus_logic_v(FILE *fp) {
	STRINGP		n = name(), rst;
	CLOCKINFO	*c = m_info->m_clock;
	PLIST::iterator	pp;
	PLISTP		pl;
	// unsigned	unused_lsbs;
	MLISTP		m_mlist = m_info->m_mlist;

	if (NULL == m_info->m_plist)
		return;

	if (NULL == m_info->m_mlist) {
		gbl_msg.warning("No masters assigned to bus %s\n",
				n->c_str());
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


	// unused_lsbs = nextlg(m_info->data_width())-3;
	if (m_info->m_plist->size() == 0) {	// No slaves
		// {{{
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
			// const char *pfx = (*m_info->m_mlist)[m]->bus_prefix()->c_str();
			fprintf(fp,
				"\t//\n"
				"\t// The %s bus has no slaves assigned to it\n"
				"\t//\n", mp);

			fprintf(fp,
				"\t// The no-slave peripheral\n"
				"\t//\n"
	"\taxiempty #(\n"
	"\t\t// {{{\n"
	"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
	"\t\t.C_AXI_DATA_WIDTH(%d),\n"
	"\t\t.C_AXI_ID_WIDTH(%d),\n"
	"\t\t// }}}\n",
				address_width(),
				m_info->data_width(),
				id_width());
			fprintf(fp,
	"\t) %s_no_slavep (\n"
		"\t\t// {{{\n"
		"\t\t.S_AXI_ACLK(%s), .S_AXI_ARESETN(%s),\n"
		"\t\t//\n",
			mp, c->m_wire->c_str(), rst->c_str());
			fprintf(fp,
		"\t\t.S_AXI_AWVALID(%s_awvalid), .S_AXI_AWREADY(%s_awready),\n"
		"\t\t\t.S_AXI_AWID(%s_awid),\n"
		"\t\t//\n"
		"\t\t.S_AXI_WVALID(%s_wvalid),   .S_AXI_WREADY(%s_wready),\n"
		"\t\t\t.S_AXI_WLAST(%s_wlast),\n"
		"\t\t//\n"
		"\t\t.S_AXI_BVALID(%s_bvalid),   .S_AXI_BREADY(%s_bready),\n"
		"\t\t\t.S_AXI_BID(%s_bid),       .S_AXI_BRESP(%s_bresp),\n"
		"\t\t//\n"
		"\t\t.S_AXI_ARVALID(%s_arvalid), .S_AXI_ARREADY(%s_arready),\n"
		"\t\t\t.S_AXI_ARLEN(%s_arlen),   .S_AXI_ARID(%s_awid),\n"
		"\t\t//\n"
		"\t\t.S_AXI_RVALID(%s_rvalid),   .S_AXI_RREADY(%s_rready),\n"
		"\t\t\t.S_AXI_RID(%s_rid),       .S_AXI_RDATA(%s_rdata),\n"
		"\t\t\t.S_AXI_RLAST(%s_rlast),   .S_AXI_RRESP(%s_rresp)\n"
		"\t);\n\n",
				mp, mp, mp,
				mp, mp, mp,
				mp, mp, mp, mp,
				mp, mp, mp, mp,
				mp, mp, mp, mp, mp, mp);
		}

		return;
		// }}}
	} else if (NULL == m_mlist || m_mlist->size() == 0) { // No masters
		// {{{
		for(unsigned p=0; p < m_info->m_plist->size(); p++) {
			STRINGP	pstr = (*m_info->m_plist)[p]->bus_prefix();
			fprintf(fp,
		"\t//\n"
		"\t// The %s bus has no masters assigned to it\n"
		"\t//\n"
		"\tassign	%s_awvalid = 1\'b0;\n"
		"\tassign	%s_awid    = 0;\n"
		"\tassign	%s_awaddr  = 0;\n"
		"\tassign	%s_awlen   = 8\'h00;\n"
		"\tassign	%s_awsize  = 3\'b000;\n"
		"\tassign	%s_awburst = 2\'b00;\n"
		"\tassign	%s_awlock  = 1\'b0;\n"
		"\tassign	%s_awcache = 4\'h0;\n"
		"\tassign	%s_awprot  = 3\'h0;\n"
		"\tassign	%s_awqos   = 4\'h0;\n"
		"\t//\n",
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str());

			fprintf(fp,
		"\tassign	%s_wvalid  = 1\'b0;\n"
		"\tassign	%s_wdata   = 0;\n"
		"\tassign	%s_wstrb   = 0;\n"
		"\tassign	%s_wlast   = 1\'b1;\n"
		"\t//\n",
			pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str());

			fprintf(fp,
		"\tassign	%s_bready  = 1\'b0;\n",
			pstr->c_str());

			fprintf(fp,
		"\t//\n"
		"\tassign	%s_arvalid = 1\'b0;\n"
		"\tassign	%s_arid    = 0;\n"
		"\tassign	%s_araddr  = 0;\n"
		"\tassign	%s_arlen   = 8\'h00;\n"
		"\tassign	%s_arsize  = 3\'b000;\n"
		"\tassign	%s_arburst = 2\'b00;\n"
		"\tassign	%s_arlock  = 1\'b0;\n"
		"\tassign	%s_arcache = 4\'h0;\n"
		"\tassign	%s_arprot  = 3\'h0;\n"
		"\tassign	%s_arqos   = 4\'h0;\n"
		"\t//\n",
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str());

			fprintf(fp,
		"\tassign	%s_rready  = 1\'b1;\n",
			pstr->c_str());

			fprintf(fp,
		"\t//\n"
		"\t//\n"
		"\twire	%s_unused;\n"
		"\tassign	%s_unused = &{ 1\'b0, %s_awready, %s_wready,\n"
			"\t\t%s_bvalid, %s_bid, %s_bresp,\n"
			"\t\t%s_arready,\n"
			"\t\t%s_rvalid, %s_rid, %s_rdata,\n"
				"\t\t\t%s_rlast, %s_rresp\n"
			"\t\t};\n",
			pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
			pstr->c_str(),
			pstr->c_str(), pstr->c_str(), pstr->c_str(),
				pstr->c_str(), pstr->c_str()
			);
		}

		return;
		// }}}
	} else if ((m_info->m_plist->size() == 1)&&(m_mlist && m_mlist->size() == 1)) {
		// {{{
		// Only one master connected to only one slave--skip all the
		// extra connection logic.
		//
		// Can only simplify if there's only one peripheral and only
		// one master
		//
		STRINGP	slv  = (*m_info->m_plist)[0]->bus_prefix();
		STRINGP	mstr = (*m_mlist)[0]->bus_prefix();
		const char *sp = slv->c_str(),
			*mp = mstr->c_str();

		fprintf(fp,
		"\t//\n"
		"\t// Bus %s has only one master (%s) and one slave (%s)\n"
		"\t// connected to it -- skipping the interconnect\n"
		"\t//\n",
		n->c_str(), master_name(0)->c_str(), sp);

			fprintf(fp,
		"\tassign	%s_awvalid = %s_awvalid;\n"
		"\tassign	%s_awready = %s_awready;\n"
		"\tassign	%s_awid    = %s_awid;\n"
		"\tassign	%s_awaddr  = %s_awaddr;\n"
		"\tassign	%s_awlen   = %s_awlen;\n"
		"\tassign	%s_awsize  = %s_awsize;\n"
		"\tassign	%s_awburst = %s_awburst;\n"
		"\tassign	%s_awlock  = %s_awlock;\n"
		"\tassign	%s_awcache = %s_awcache;\n"
		"\tassign	%s_awprot  = %s_awprot;\n"
		"\tassign	%s_awqos   = %s_awqos;\n"
		"\t//\n",
			sp, mp, mp, sp,
			sp, mp, sp, mp, sp, mp, sp, mp,
			sp, mp, sp, mp, sp, mp, sp, mp,
			sp, mp);

			fprintf(fp,
		"\tassign	%s_wvalid  = %s_wvalid;\n"
		"\tassign	%s_wready  = %s_wready;\n"
		"\tassign	%s_wdata   = %s_wdata;\n"
		"\tassign	%s_wstrb   = %s_wstrb;\n"
		"\tassign	%s_wlast   = %s_wlast;\n"
		"\t//\n",
			sp, mp, mp, sp,
			sp, mp, sp, mp, sp, mp);

			fprintf(fp,
		"\tassign	%s_bvalid  = %s_bvalid;\n"
		"\tassign	%s_bready  = %s_bready;\n"
		"\tassign	%s_bid     = %s_bid;\n"
		"\tassign	%s_bresp   = %s_bresp;\n",
			mp, sp, sp, mp,
			mp, sp, mp, sp);

			fprintf(fp,
		"\t//\n"
		"\t//\n"
		"\tassign	%s_arvalid = %s_arvalid;\n"
		"\tassign	%s_arready = %s_arready;\n"
		"\tassign	%s_arid    = %s_arid;\n"
		"\tassign	%s_araddr  = %s_araddr;\n"
		"\tassign	%s_arlen   = %s_arlen;\n"
		"\tassign	%s_arsize  = %s_arsize;\n"
		"\tassign	%s_arburst = %s_arburst;\n"
		"\tassign	%s_arlock  = %s_arlock;\n"
		"\tassign	%s_arcache = %s_arcache;\n"
		"\tassign	%s_arprot  = %s_arprot;\n"
		"\tassign	%s_arqos   = %s_arqos;\n"
		"\t//\n",
			sp, mp, mp, sp,
			sp, mp, sp, mp,
			sp, mp, sp, mp, sp, mp, sp, mp,
			sp, mp, sp, mp, sp, mp);

			fprintf(fp,
		"\tassign	%s_rvalid  = %s_rvalid;\n"
		"\tassign	%s_rready  = %s_rready;\n"
		"\tassign	%s_rid     = %s_rid;\n"
		"\tassign	%s_rdata   = %s_rdata;\n"
		"\tassign	%s_rlast   = %s_rlast;\n"
		"\tassign	%s_rresp   = %s_rresp;\n\n",
			mp, sp, sp, mp,
			mp, sp, mp, sp, mp, sp, mp, sp);
		return;
		// }}}
	}

	// Start with the slist
	if (m_is_single || m_slist) {
		// {{{
		assert(1 == m_info->m_mlist->size());

		PLIST	*slist = (m_is_single) ? m_info->m_plist : m_slist;
		const char *np = n->c_str();
		STRING	s = STRING(*(*m_mlist)[0]->bus_prefix());
		int	aw = nextlg(slist->size())+nextlg(m_info->data_width())-3;

		gbl_msg.error("AXI-Single logic hasn\'t yet been implemented\n");
		fprintf(fp,
		"\t// (AXI-Single isn\'t yet written)\n"
		"\taxisingle #(\n"
			"\t\t// {{{\n"
			"\t\t// .C_AXI_ADDR_WIDTH(%d),\n"
			"\t\t.C_AXI_DATA_WIDTH(%d),\n"
			"\t\t.NS(%ld)",
			aw, m_info->data_width(),
			slist->size());
		xbar_option(fp, KY_OPT_LOWPOWER,  ",\n\t\t.OPT_LOWPOWER(%)",
			"1\'b1");
		fprintf(fp,
		"\n\t\t// }}}\n"
		"\t) %s_axisingle(\n", np);
		fprintf(fp,
			"\t\t// {{{\n"
			"\t\t.S_AXI_ACLK(%s),\n"
			"\t\t.S_AXI_ARESETN(%s),\n",
				c->m_wire->c_str(), rst->c_str());

		fprintf(fp, "\t\t// Connect our SIO slave wires to axisingle\n"
			"\t\t// {{{\n");
		fprintf(fp,
			"\t\t.S_AXI_AWVALID(%s_awvalid),\n"
			"\t\t.S_AXI_AWREADY(%s_awready),\n"
			"\t\t.S_AXI_AWID(   %s_awid),\n"
			"\t\t.S_AXI_AWADDR( %s_awaddr[%d:0]),\n"
			"\t\t.S_AXI_AWLEN(  %s_awlen),\n"
			"\t\t.S_AXI_AWSIZE( %s_awsize),\n"
			"\t\t.S_AXI_AWBURST(%s_awburst),\n"
			"\t\t.S_AXI_AWLOCK( %s_awlock),\n"
			"\t\t.S_AXI_AWCACHE(%s_awcache),\n"
			"\t\t.S_AXI_AWPROT( %s_awprot),\n"
			"\t\t.S_AXI_AWQOS(  %s_awqos),\n"
			"\t\t//\n"
			"\t\t.S_AXI_WVALID( %s_wvalid),\n"
			"\t\t.S_AXI_WREADY( %s_wready),\n"
			"\t\t.S_AXI_WDATA(  %s_wdata),\n"
			"\t\t.S_AXI_WSTRB(  %s_wstrb),\n"
			"\t\t.S_AXI_WLAST(  %s_wlast),\n"
			"\t\t//\n"
			"\t\t.S_AXI_BVALID( %s_bvalid),\n"
			"\t\t.S_AXI_BREADY( %s_bready),\n"
			"\t\t.S_AXI_BID(    %s_bid),\n"
			"\t\t.S_AXI_BRESP(  %s_bresp),\n"
			"\t\t//\n",
			s.c_str(), s.c_str(), s.c_str(), s.c_str(), aw-1,
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			//
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(),
			//
			s.c_str(), s.c_str(), s.c_str(), s.c_str());

		fprintf(fp,
			"\t\t// Read connections\n"
			"\t\t.S_AXI_ARVALID(%s_arvalid),\n"
			"\t\t.S_AXI_ARREADY(%s_arready),\n"
			"\t\t.S_AXI_ARID(   %s_arid),\n"
			"\t\t.S_AXI_ARADDR( %s_araddr[%d:0]),\n"
			"\t\t.S_AXI_ARLEN(  %s_arlen),\n"
			"\t\t.S_AXI_ARSIZE( %s_arsize),\n"
			"\t\t.S_AXI_ARBURST(%s_arburst),\n"
			"\t\t.S_AXI_ARLOCK( %s_arlock),\n"
			"\t\t.S_AXI_ARCACHE(%s_arcache),\n"
			"\t\t.S_AXI_ARPROT( %s_arprot),\n"
			"\t\t.S_AXI_ARQOS(  %s_arqos),\n"
			"\t\t//\n"
			"\t\t.S_AXI_RVALID( %s_rvalid),\n"
			"\t\t.S_AXI_RREADY( %s_rready),\n"
			"\t\t.S_AXI_RID(    %s_rid),\n"
			"\t\t.S_AXI_RDATA(  %s_rdata),\n"
			"\t\t.S_AXI_RLAST(  %s_rlast),\n"
			"\t\t.S_AXI_RRESP(  %s_rresp),\n"
			"\t\t// }}}\n"
			"\t\t// Connections to varous AXI (SINGLE) slaves\n"
			"\t\t// {{{\n",
			s.c_str(), s.c_str(), s.c_str(), s.c_str(), aw-1,
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			//
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str());

		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","AWVALID");
		fprintf(fp,
			"\t\t.M_AXI_AWPROT(%s_siow_awprot),\n"
			"\t\t.M_AXI_WDATA( %s_siow_wdata),\n"
			"\t\t.M_AXI_WSTRB( %s_siow_wstrb),\n",
			n->c_str(), n->c_str(), n->c_str());
		fprintf(fp, "\t\t//\n"
			"\t\t//\n");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","BRESP");
		fprintf(fp, "\t\t// Read connections\n");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","ARVALID");
		fprintf(fp,
			"\t\t.M_AXI_ARPROT(%s_siow_arprot),\n"
			"\t\t//\n", n->c_str());
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","RDATA");
		xbarcon_slave(fp, slist, "\t\t",".M_AXI_","RRESP", false);
		fprintf(fp, "\t\t// }}}\n"
			"\t\t// }}}\n"
			"\t);\n\n");
		fprintf(fp,"\t//\n\t// Now connecting the extra slaves wires "
			"to the AXISINGLE controller\n"
			"\t// {{{\n");

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
		} fprintf(fp, "\t// }}}\n");
		// }}}
		return;
		// }}}
	} else if (!m_slist)
		fprintf(fp, "\t//\n\t// No class SINGLE peripherals on the \"%s\" bus\n\t//\n\n", n->c_str());

	// Then the dlist
	if (m_is_double || m_dlist) {
		// {{{
		if (m_dlist)
			pl = m_dlist;
		else
			pl = m_info->m_plist;
		STRING	s = STRING(*(*m_mlist)[0]->bus_prefix());
		int	aw = address_width();

		if (!m_is_double)
			s = (*n) + "_dio";

		fprintf(fp,
			"\t//\n"
			"\t// %s Bus logic to handle %ld DOUBLE slaves\n"
			"\t//\n"
			"\t//\n", n->c_str(), pl->size());

		fprintf(fp,
			"\t// Some extra wires to capture combined values--values\n"
			"\t// that will be the same across all slaves of the\n"
			"\t// class\n"
			"\twire [0:0]\t%s_diow_awid;\n"
			"\twire [%d:0]\t%s_diow_awaddr;\n"
			"\twire [7:0]\t%s_diow_awlen;\n"
			"\twire [2:0]\t%s_diow_awsize;\n"
			"\twire [1:0]\t%s_diow_awburst;\n"
			"\twire\t%s_diow_awlock;\n"
			"\twire [3:0]\t%s_diow_awcache;\n"
			"\twire [2:0]\t%s_diow_awprot;\n"
			"\twire [3:0]\t%s_diow_awqos;\n"
			"\t//\n",
			n->c_str(),
			address_width()-1, n->c_str(),
			n->c_str(), n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str());

		fprintf(fp,
			"\twire [%d:0]\t%s_diow_wdata;\n"
			"\twire [%d:0]\t%s_diow_wstrb;\n"
			"\twire\t%s_diow_wlast;\n"
			"\t//\n"
			"\twire\t%s_diow_bready;\n"
			"\t//\n",
			m_info->data_width()-1, n->c_str(),
			m_info->data_width()/8-1, n->c_str(),
			n->c_str(), n->c_str());

		fprintf(fp,
			"\twire [0:0]\t%s_diow_arid;\n"
			"\twire [%d:0]\t%s_diow_araddr;\n"
			"\twire [7:0]\t%s_diow_arlen;\n"
			"\twire [2:0]\t%s_diow_arsize;\n"
			"\twire [1:0]\t%s_diow_arburst;\n\n"
			"\twire\t%s_diow_arlock;\n"
			"\twire [3:0]\t%s_diow_arcache;\n"
			"\twire [2:0]\t%s_diow_arprot;\n"
			"\twire [3:0]\t%s_diow_arqos;\n"
			"\t//\n"
			"\twire\t%s_diow_rready;\n"
			"\t//\n",
			n->c_str(),
			address_width()-1, n->c_str(),
			n->c_str(), n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str(), n->c_str());

		fprintf(fp,
		"\taxidouble #(\n"
			"\t\t// {{{\n"
			"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
			"\t\t.C_AXI_DATA_WIDTH(%d),\n"
			"\t\t.C_AXI_ID_WIDTH(%d),\n"
			"\t\t.NS(%ld),\n",
			address_width(), m_info->data_width(),
			id_width(), pl->size());
		xbar_option(fp, KY_OPT_LOWPOWER,  "\t\t.OPT_LOWPOWER(%),\n",
			"1\'b1");
		slave_addr(fp, pl); fprintf(fp, ",\n");
		slave_mask(fp, pl); fprintf(fp, "\n");
		fprintf(fp,
		"\t\t// }}}\n"
		"\t) %s_axidouble(\n",
			n->c_str());
		fprintf(fp,
			"\t\t// {{{\n"
			"\t\t.S_AXI_ACLK(%s),\n"
			"\t\t.S_AXI_ARESETN(%s),\n",
				c->m_wire->c_str(), rst->c_str());

		fprintf(fp, "\t\t//\n");
		fprintf(fp,
			"\t\t// Slave port\n"
			"\t\t// {{{\n"
			"\t\t.S_AXI_AWVALID(%s_awvalid),\n"
			"\t\t.S_AXI_AWREADY(%s_awready),\n"
			"\t\t.S_AXI_AWID(   %s_awid),\n"
			"\t\t.S_AXI_AWADDR( %s_awaddr[%d:0]),\n"
			"\t\t.S_AXI_AWLEN(  %s_awlen),\n"
			"\t\t.S_AXI_AWSIZE( %s_awsize),\n"
			"\t\t.S_AXI_AWBURST(%s_awburst),\n"
			"\t\t.S_AXI_AWLOCK( %s_awlock),\n"
			"\t\t.S_AXI_AWCACHE(%s_awcache),\n"
			"\t\t.S_AXI_AWPROT( %s_awprot),\n"
			"\t\t.S_AXI_AWQOS(  %s_awqos),\n"
			"\t\t//\n"
			"\t\t.S_AXI_WVALID( %s_wvalid),\n"
			"\t\t.S_AXI_WREADY( %s_wready),\n"
			"\t\t.S_AXI_WDATA(  %s_wdata),\n"
			"\t\t.S_AXI_WSTRB(  %s_wstrb),\n"
			"\t\t.S_AXI_WLAST(  %s_wlast),\n"
			"\t\t//\n"
			"\t\t.S_AXI_BVALID( %s_bvalid),\n"
			"\t\t.S_AXI_BREADY( %s_bready),\n"
			"\t\t.S_AXI_BID(    %s_bid),\n"
			"\t\t.S_AXI_BRESP(  %s_bresp),\n"
			"\t\t//\n",
			// AW
			s.c_str(), s.c_str(), s.c_str(), s.c_str(), aw-1,
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			// W
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(),
			// B
			s.c_str(), s.c_str(), s.c_str(), s.c_str());

		fprintf(fp,
			"\t\t// Read connections\n"
			"\t\t.S_AXI_ARVALID(%s_arvalid),\n"
			"\t\t.S_AXI_ARREADY(%s_arready),\n"
			"\t\t.S_AXI_ARID(   %s_arid),\n"
			"\t\t.S_AXI_ARADDR( %s_araddr[%d:0]),\n"
			"\t\t.S_AXI_ARLEN(  %s_arlen),\n"
			"\t\t.S_AXI_ARSIZE( %s_arsize),\n"
			"\t\t.S_AXI_ARBURST(%s_arburst),\n"
			"\t\t.S_AXI_ARLOCK( %s_arlock),\n"
			"\t\t.S_AXI_ARCACHE(%s_arcache),\n"
			"\t\t.S_AXI_ARPROT( %s_arprot),\n"
			"\t\t.S_AXI_ARQOS(  %s_arqos),\n"
			"\t\t//\n"
			"\t\t.S_AXI_RVALID( %s_rvalid),\n"
			"\t\t.S_AXI_RREADY( %s_rready),\n"
			"\t\t.S_AXI_RID(    %s_rid),\n"
			"\t\t.S_AXI_RDATA(  %s_rdata),\n"
			"\t\t.S_AXI_RLAST(  %s_rlast),\n"
			"\t\t.S_AXI_RRESP(  %s_rresp),\n"
			"\t\t// }}}\n"
			"\t\t// Connections to slaves\n"
			"\t\t// {{{\n",
			// AR
			s.c_str(), s.c_str(), s.c_str(), s.c_str(), aw-1,
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str(), s.c_str(),
			// R
			s.c_str(), s.c_str(), s.c_str(), s.c_str(),
			s.c_str(), s.c_str());

		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWVALID");
		fprintf(fp,
			"\t\t.M_AXI_AWID(   %s_diow_awid),  // == 1\'b0\n"
			"\t\t.M_AXI_AWADDR( %s_diow_awaddr),\n"
			"\t\t.M_AXI_AWLEN(  %s_diow_awlen), // == 0\n"
			"\t\t.M_AXI_AWSIZE( %s_diow_awsize),\n"
			"\t\t.M_AXI_AWBURST(%s_diow_awburst), // ==INC==2\'b01\n"
			"\t\t.M_AXI_AWLOCK( %s_diow_awlock), // == 0\n"
			"\t\t.M_AXI_AWCACHE(%s_diow_awcache),\n"
			"\t\t.M_AXI_AWPROT( %s_diow_awprot),\n"
			"\t\t.M_AXI_AWQOS(  %s_diow_awqos),\n",
			n->c_str(), n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str(), n->c_str(),
			n->c_str());

		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WVALID");
		fprintf(fp,
			"\t\t.M_AXI_WDATA(  %s_diow_wdata),\n"
			"\t\t.M_AXI_WSTRB(  %s_diow_wstrb),\n"
			"\t\t.M_AXI_WLAST(  %s_diow_wlast), // == 1\n",
			n->c_str(), n->c_str(), n->c_str());
		fprintf(fp, "\t\t//\n");
		fprintf(fp, "\t\t//\n");
		fprintf(fp,
			"\t\t.M_AXI_BREADY(  %s_diow_bready),\n", n->c_str());
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BRESP");
		fprintf(fp, "\t\t// Read connections\n");
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARVALID");
		fprintf(fp,
			"\t\t.M_AXI_ARID(   %s_diow_arid),  // == 1\'b0\n"
			"\t\t.M_AXI_ARADDR( %s_diow_araddr),\n"
			"\t\t.M_AXI_ARLEN(  %s_diow_arlen), // == 0\n"
			"\t\t.M_AXI_ARSIZE( %s_diow_arsize),\n"
			"\t\t.M_AXI_ARBURST(%s_diow_arburst),//==INC==2\'b01\n"
			"\t\t.M_AXI_ARLOCK( %s_diow_arlock),// == 0\n"
			"\t\t.M_AXI_ARCACHE(%s_diow_arcache),//== 0\n"
			"\t\t.M_AXI_ARPROT( %s_diow_arprot),\n"
			"\t\t.M_AXI_ARQOS(  %s_diow_arqos),  //== 0\n"
			"\t\t//\n",
			n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str(),
			n->c_str(), n->c_str(), n->c_str());

		// Read returns
		fprintf(fp,
			"\t\t.M_AXI_RREADY(  %s_diow_rready),\n", n->c_str());
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RDATA");
		xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RRESP", false);
		fprintf(fp,
			"\t\t// }}}\n"
			"\t\t// }}}\n"
			"\t);\n\n");
		fprintf(fp,"\t//\n\t// Now connecting the extra slaves wires "
			"to the AXIDOUBLE controller\n\t//\n");

		for(int k=pl->size(); k>0; k--) {
			PERIPHP p = (*pl)[k-1];
			const char *pn = p->p_name->c_str(),
				*pfx = p->bus_prefix()->c_str();
			int	// aw = p->p_awid + unused_lsbs,
				iw = id_width();

			fprintf(fp, "\t// %s\n"
				"\t// {{{\n", pn);
			fprintf(fp, "\tassign %s_awid   = {(%d){1\'b0}};\n", pfx, iw);
			fprintf(fp, "\tassign %s_awaddr = %s_diow_awaddr;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awlen  = %s_diow_awlen;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awsize = %s_diow_awsize;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awburst= %s_diow_awburst;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awlock = %s_diow_awlock;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awcache= %s_diow_awcache;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awprot = %s_diow_awprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_awqos  = %s_diow_awqos;\n", pfx, n->c_str());
			//
			fprintf(fp, "\tassign %s_wdata  = %s_diow_wdata;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_wstrb  = %s_diow_wstrb;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_wlast  = %s_diow_wlast;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_bready = 1\'b1;\n", pfx);
			// arready is set by the slave ... and ignored
			fprintf(fp, "\tassign %s_arid   = {(%d){1\'b0}};\n", pfx, iw);
			fprintf(fp, "\tassign %s_araddr = %s_diow_araddr;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arlen  = %s_diow_arlen;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arsize = %s_diow_arsize;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arburst= %s_diow_arburst;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arlock = %s_diow_arlock;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arcache= %s_diow_arcache;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arprot = %s_diow_arprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arqos  = %s_diow_arqos;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arprot = %s_diow_arprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_arprot = %s_diow_arprot;\n", pfx, n->c_str());
			fprintf(fp, "\tassign %s_rready = 1\'b1;\n"
				"\t// }}}\n", pfx);
		}

		if (m_is_double)
			return;
		// }}}
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
	fprintf(fp,
	"\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n"
	"\t// Connect the %s bus components together using the axixbar()\n"
	"\t// {{{\n"
	"\t//\n", n->c_str());

	fprintf(fp,
	"\taxixbar #(\n"
	"\t\t// {{{\n"
	"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
	"\t\t.C_AXI_DATA_WIDTH(%d),\n"
	"\t\t.C_AXI_ID_WIDTH(%d),\n"
	"\t\t.NM(%ld), .NS(%ld),\n",
		address_width(),
		m_info->data_width(),
		id_width(),
		m_mlist->size(),
		m_info->m_plist->size());
	/*
	fprintf(fp,
	"\t\t.READ_ACCESS(%ld\'b", m_info->m_plist->size());
	for(unsigned k=0; k<m_info->m_plist->size(); k++) {
		PERIPHP p = (*m_info->m_plist)[k];
		if (p->write_only()) {
			putc('0', fp);
			if (p->read_only())
				gbl_msg.error("Slave %s cannot be both write-only and read-only\n", p->name()->c_str());
		} else
			putc('1', fp);
	} fprintf(fp, "),\n\t\t.WRITE_ACCESS(%ld\'b", m_info->m_plist->size());
	for(unsigned k=0; k<m_info->m_plist->size(); k++) {
		PERIPHP p = (*m_info->m_plist)[k];
		if (p->write_only())
			putc('0', fp);
		else
			putc('1', fp);
	} fprintf(fp, "),\n");
	*/

	slave_addr(fp, m_info->m_plist); fprintf(fp, ",\n");
	slave_mask(fp, m_info->m_plist);

	xbar_option(fp, KY_OPT_LOWPOWER,  ",\n\t\t.OPT_LOWPOWER(%)", "1\'b1");
	xbar_option(fp, KY_OPT_LINGER,    ",\n\t\t.OPT_LINGER(%)");
	xbar_option(fp, KY_OPT_LGMAXBURST,",\n\t\t.LGMAXBURST(%)");
	//
	fprintf(fp,
	"\n\t\t// }}}\n"
	"\t) %s_xbar(\n"
		"\t\t// {{{\n"
		"\t\t.S_AXI_ACLK(%s),\n",
		n->c_str(), m_info->m_clock->m_wire->c_str());
	fprintf(fp, "\t\t.S_AXI_ARESETN(%s),\n", rst->c_str());

	fprintf(fp, "\t\t// Connections from masters\n"
		"\t\t// {{{\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWID");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWADDR");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWLEN");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWSIZE");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWBURST");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWLOCK");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWCACHE");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWPROT");
	xbarcon_master(fp, "\t\t",".S_AXI_","AWQOS");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","WVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","WREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","WDATA");
	xbarcon_master(fp, "\t\t",".S_AXI_","WSTRB");
	xbarcon_master(fp, "\t\t",".S_AXI_","WLAST");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","BVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","BREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","BID");
	xbarcon_master(fp, "\t\t",".S_AXI_","BRESP");

	fprintf(fp, "\t\t//\n");
	fprintf(fp, "\t\t// Read connections\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARID");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARADDR");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARLEN");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARSIZE");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARBURST");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARLOCK");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARCACHE");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARPROT");
	xbarcon_master(fp, "\t\t",".S_AXI_","ARQOS");
	fprintf(fp, "\t\t//\n");
	xbarcon_master(fp, "\t\t",".S_AXI_","RVALID");
	xbarcon_master(fp, "\t\t",".S_AXI_","RREADY");
	xbarcon_master(fp, "\t\t",".S_AXI_","RID");
	xbarcon_master(fp, "\t\t",".S_AXI_","RDATA");
	xbarcon_master(fp, "\t\t",".S_AXI_","RLAST");
	xbarcon_master(fp, "\t\t",".S_AXI_","RRESP");
	fprintf(fp, "\t\t// }}}\n");
	fprintf(fp, "\t\t// Connections to slaves\n");
	fprintf(fp, "\t\t// {{{\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWADDR");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWLEN");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWSIZE");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWBURST");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWLOCK");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWCACHE");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWPROT");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","AWQOS");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WDATA");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WSTRB");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","WLAST");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","BRESP");
	fprintf(fp, "\t\t//\n");
	fprintf(fp, "\t\t// Read connections\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARADDR");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARLEN");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARSIZE");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARBURST");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARLOCK");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARCACHE");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARPROT");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","ARQOS");
	fprintf(fp, "\t\t//\n");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RVALID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RREADY");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RID");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RDATA");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RLAST");
	xbarcon_slave(fp, pl, "\t\t",".M_AXI_","RRESP", false);
	fprintf(fp, "\t\t// }}}\n"
		"\t\t// }}}\n\t);\n\n");

	for(unsigned k=0; k < m_info->m_plist->size(); k++) {
		// Handle read only or write only slaves
		// {{{
		PERIPHP p = (*m_info->m_plist)[k];
		STRINGP	busp = p->bus_prefix();
		const char	*bp = busp->c_str();

		if (p->write_only()) {
			fprintf(fp,
	"\taxiempty #(\n"
	"\t\t// {{{\n"
	"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
	"\t\t.C_AXI_DATA_WIDTH(%d)\n"
	"\t\t// }}}\n",
				address_width(),
				m_info->data_width());
			fprintf(fp,
	"\t) %s_no_slavep (\n"
		"\t\t// {{{\n"
		"\t\t.S_AXI_ACLK(%s), .S_AXI_ARESETN(%s),\n"
		"\t\t//\n",
			bp, c->m_wire->c_str(), rst->c_str());

			fprintf(fp,
		"\t\t// Verilator lint_off PINCONNECTEMPTY\n"
		"\t\t.S_AXI_AWVALID(1\'b0), .S_AXI_AWREADY(),\n"
		"\t\t.S_AXI_WVALID(1\'b0),  .S_AXI_WREADY(),\n"
		"\t\t.S_AXI_WLAST(1\'b0),\n"
		"\t\t//\n"
		"\t\t.S_AXI_BVALID(),   .S_AXI_BREADY(1\'b1),\n"
		"\t\t.S_AXI_BID(), .S_AXI_BRESP(),\n"
		"\t\t// Verilator lint_on PINCONNECTEMPTY\n"
		"\t\t//\n"
		"\t\t.S_AXI_ARVALID(%s_arvalid), .S_AXI_ARREADY(%s_arready),\n"
		"\t\t.S_AXI_ARID(%s_arid), .S_AXI_ARLEN(%s_arlen),\n"
		"\t\t.S_AXI_RVALID(%s_rvalid),   .S_AXI_RREADY(%s_rready),\n"
		"\t\t.S_AXI_RDATA(%s_rdata),   .S_AXI_RLAST(%s_rlast),\n"
		"\t\t.S_AXI_RRESP(%s_rresp)\n"
		"\t);\n\n",
				// AW, W, B
				//
				bp, bp, bp, bp,
				bp, bp, bp, bp, bp);

		}
		if (p->read_only()) {

			fprintf(fp,
	"\taxiempty #(\n"
	"\t\t// {{{\n"
	"\t\t.C_AXI_ADDR_WIDTH(%d),\n"
	"\t\t.C_AXI_DATA_WIDTH(%d)\n"
	"\t\t// }}}\n",
				address_width(),
				m_info->data_width());
			fprintf(fp,
	"\t) %s_no_slavep (\n"
		"\t\t// {{{\n"
		"\t\t.S_AXI_ACLK(%s), .S_AXI_ARESETN(%s),\n"
		"\t\t//\n",
			bp, c->m_wire->c_str(), rst->c_str());

			fprintf(fp,
		"\t\t.S_AXI_AWVALID(%s_awvalid), .S_AXI_AWREADY(%s_awready),\n"
		"\t\t.S_AXI_WVALID(%s_wvalid),  .S_AXI_WREADY(%s_wready),\n"
		"\t\t.S_AXI_WLAST(%s_wlast),\n"
		"\t\t//\n"
		"\t\t.S_AXI_BVALID(%s_bvalid),   .S_AXI_BREADY(%s_bready),\n"
		"\t\t.S_AXI_BID(%s_bid), .S_AXI_BRESP(%s_bresp),\n"
		"\t\t//\n"
		"\t\t// Verilator lint_off PINCONNECTEMPTY\n"
		"\t\t.S_AXI_ARVALID(1\'b0), .S_AXI_ARREADY(),\n"
		"\t\t.S_AXI_ARID(0), .S_AXI_ARLEN(8'h00),\n"
		"\t\t//\n"
		"\t\t.S_AXI_RVALID(),   .S_AXI_RREADY(1\'b1),\n"
		"\t\t.S_AXI_RDATA(),   .S_AXI_RLAST(),\n"
		"\t\t.S_AXI_RRESP()\n"
		"\t\t// Verilator lint_on PINCONNECTEMPTY\n"
		"\t);\n\n",
				// AW, W
				bp, bp, bp, bp, bp,
				// B
				bp, bp, bp, bp);
		}
		// }}}
	}
	fprintf(fp, "\t\t// }}}\n");
}

STRINGP	AXIBUS::master_portlist(BMASTERP m) {
	// {{{
	STRING	str;

	if (m->write_only())
		str = str + STRING("// Master is write-only\n");
	if (m->read_only())
		str = str + STRING("// Master is read only\n\t\t");
	else
		str = str + STRING(
	"@$(MASTER.PREFIX)_awvalid,\n"
	"\t\t@$(MASTER.PREFIX)_awready,\n"
	"\t\t@$(MASTER.PREFIX)_awid,\n"
	"\t\t@$(MASTER.PREFIX)_awaddr[@$(MASTER.BUS.AWID)-1:0],\n"
	"\t\t@$(MASTER.PREFIX)_awlen,\n"
	"\t\t@$(MASTER.PREFIX)_awsize,\n"
	"\t\t@$(MASTER.PREFIX)_awburst,\n"
	"\t\t@$(MASTER.PREFIX)_awlock,\n"
	"\t\t@$(MASTER.PREFIX)_awcache,\n"
	"\t\t@$(MASTER.PREFIX)_awprot,\n"
	"\t\t@$(MASTER.PREFIX)_awqos,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_wvalid,\n"
	"\t\t@$(MASTER.PREFIX)_wready,\n"
	"\t\t@$(MASTER.PREFIX)_wdata,\n"
	"\t\t@$(MASTER.PREFIX)_wstrb,\n"
	"\t\t@$(MASTER.PREFIX)_wlast,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_bvalid,\n"
	"\t\t@$(MASTER.PREFIX)_bready,\n"
	"\t\t@$(MASTER.PREFIX)_bid,\n"
	"\t\t@$(MASTER.PREFIX)_bresp");
	if (!m->read_only() && !m->write_only())
		str = str + STRING(",\n\t\t// Read connections\n\t\t");
	if (!m->write_only())
		str = str + STRING(
	"@$(MASTER.PREFIX)_arvalid,\n"
	"\t\t@$(MASTER.PREFIX)_arready,\n"
	"\t\t@$(MASTER.PREFIX)_arid,\n"
	"\t\t@$(MASTER.PREFIX)_araddr[@$(MASTER.BUS.AWID)-1:0],\n"
	"\t\t@$(MASTER.PREFIX)_arlen,\n"
	"\t\t@$(MASTER.PREFIX)_arsize,\n"
	"\t\t@$(MASTER.PREFIX)_arburst,\n"
	"\t\t@$(MASTER.PREFIX)_arlock,\n"
	"\t\t@$(MASTER.PREFIX)_arcache,\n"
	"\t\t@$(MASTER.PREFIX)_arprot,\n"
	"\t\t@$(MASTER.PREFIX)_arqos,\n"
	"\t\t//\n"
	"\t\t@$(MASTER.PREFIX)_rvalid,\n"
	"\t\t@$(MASTER.PREFIX)_rready,\n"
	"\t\t@$(MASTER.PREFIX)_rid,\n"
	"\t\t@$(MASTER.PREFIX)_rdata,\n"
	"\t\t@$(MASTER.PREFIX)_rlast,\n"
	"\t\t@$(MASTER.PREFIX)_rresp");

	return new STRING(str);
}
// }}}

STRINGP	AXIBUS::iansi(BMASTERP m) {
	return new STRING("");
}

STRINGP	AXIBUS::oansi(BMASTERP m) {
	return new STRING("");
}

STRINGP	AXIBUS::master_ansprefix(BMASTERP m) {
	return new STRING("M_AXI_");
}

STRINGP	AXIBUS::master_ansi_portlist(BMASTERP m) {
	// {{{
	STRING	str;

	if (!m->read_only())
		str = str + STRING(
	".@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWVALID(@$(MASTER.PREFIX)_awvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)AWREADY(@$(MASTER.PREFIX)_awready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWID(   @$(MASTER.PREFIX)_awid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWADDR( @$(MASTER.PREFIX)_awaddr[@$(MASTER.BUS.AWID)-1:0]),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWLEN(  @$(MASTER.PREFIX)_awlen),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWSIZE( @$(MASTER.PREFIX)_awsize),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWBURST(@$(MASTER.PREFIX)_awburst),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWLOCK( @$(MASTER.PREFIX)_awlock),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWCACHE(@$(MASTER.PREFIX)_awcache),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWPROT( @$(MASTER.PREFIX)_awprot),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)AWQOS(  @$(MASTER.PREFIX)_awqos),\n"
	"\t\t//\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WVALID(@$(MASTER.PREFIX)_wvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)WREADY(@$(MASTER.PREFIX)_wready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WDATA( @$(MASTER.PREFIX)_wdata),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WSTRB( @$(MASTER.PREFIX)_wstrb),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)WLAST( @$(MASTER.PREFIX)_wlast),\n"
	"\t\t//\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)BVALID(@$(MASTER.PREFIX)_bvalid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)BREADY(@$(MASTER.PREFIX)_bready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)BID(   @$(MASTER.PREFIX)_bid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)BRESP( @$(MASTER.PREFIX)_bresp)");
	if (!m->read_only() && !m->write_only())
		str = str + STRING(",\n\t\t// Read connections\n\t\t");
	if (!m->write_only())
		str = str + STRING(
	".@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARVALID(@$(MASTER.PREFIX)_arvalid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)ARREADY(@$(MASTER.PREFIX)_arready),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARID(   @$(MASTER.PREFIX)_arid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARADDR( @$(MASTER.PREFIX)_araddr[@$(MASTER.BUS.AWID)-1:0]),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARLEN(  @$(MASTER.PREFIX)_arlen),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARSIZE( @$(MASTER.PREFIX)_arsize),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARBURST(@$(MASTER.PREFIX)_arburst),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARLOCK( @$(MASTER.PREFIX)_arlock),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARCACHE(@$(MASTER.PREFIX)_arcache),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARPROT( @$(MASTER.PREFIX)_arprot),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)ARQOS(  @$(MASTER.PREFIX)_arqos),\n"
	"//\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RVALID(@$(MASTER.PREFIX)_rvalid),\n"
	"\t\t.@$(MASTER.IANSI)@$(MASTER.ANSPREFIX)RREADY(@$(MASTER.PREFIX)_rready),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RID(   @$(MASTER.PREFIX)_rid),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RDATA( @$(MASTER.PREFIX)_rdata),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RLAST( @$(MASTER.PREFIX)_rlast),\n"
	"\t\t.@$(MASTER.OANSI)@$(MASTER.ANSPREFIX)RRESP( @$(MASTER.PREFIX)_rresp)");

	return new STRING(str);
}
// }}}

STRINGP	AXIBUS::slave_ansprefix(PERIPHP p) {
	return new STRING("S_AXI_");
}

STRINGP	AXIBUS::slave_portlist(PERIPHP p) {
	// {{{
	STRING	str;

	if (p->write_only())
		str = str + STRING("// Slave is write-only\n\t\t");
	if (p->read_only())
		str = str + STRING("// Slave is read only\n\t\t");
	else {
		str = str + STRING(
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_awvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_awready,\n"
	"\t\t@$(SLAVE.PREFIX)_awid,\n"
	"\t\t@$(SLAVE.PREFIX)_awaddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0],\n"
	"\t\t@$(SLAVE.PREFIX)_awlen,\n"
	"\t\t@$(SLAVE.PREFIX)_awsize,\n"
	"\t\t@$(SLAVE.PREFIX)_awburst,\n"
	"\t\t@$(SLAVE.PREFIX)_awlock,\n"
	"\t\t@$(SLAVE.PREFIX)_awcache,\n"
	"\t\t@$(SLAVE.PREFIX)_awprot,\n"
	"\t\t@$(SLAVE.PREFIX)_awqos,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_wvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_wready,\n"
	"\t\t@$(SLAVE.PREFIX)_wdata,\n"
	"\t\t@$(SLAVE.PREFIX)_wstrb,\n"
	"\t\t@$(SLAVE.PREFIX)_wlast,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_bvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_bready,\n"
	"\t\t@$(SLAVE.PREFIX)_bid,\n"
	"\t\t@$(SLAVE.PREFIX)_bresp");
	}
	if (!p->read_only() && !p->write_only())
		str = str + STRING(",\n\t\t// Read connections\n");
	if (!p->write_only())
		str = str + STRING(
	"\t\t@$(SLAVE.PREFIX)_arvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_arready,\n"
	"\t\t@$(SLAVE.PREFIX)_arid,\n"
	"\t\t@$(SLAVE.PREFIX)_araddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0],\n"
	"\t\t@$(SLAVE.PREFIX)_arlen,\n"
	"\t\t@$(SLAVE.PREFIX)_arsize,\n"
	"\t\t@$(SLAVE.PREFIX)_arburst,\n"
	"\t\t@$(SLAVE.PREFIX)_arlock,\n"
	"\t\t@$(SLAVE.PREFIX)_arcache,\n"
	"\t\t@$(SLAVE.PREFIX)_arprot,\n"
	"\t\t@$(SLAVE.PREFIX)_arqos,\n"
	"//\n"
	"\t\t@$(SLAVE.PREFIX)_rvalid,\n"
	"\t\t@$(SLAVE.PREFIX)_rready,\n"
	"\t\t@$(SLAVE.PREFIX)_rid,\n"
	"\t\t@$(SLAVE.PREFIX)_rdata,\n"
	"\t\t@$(SLAVE.PREFIX)_rlast,\n"
	"\t\t@$(SLAVE.PREFIX)_rresp");

	return new STRING(str);
}
// }}}

STRINGP	AXIBUS::slave_ansi_portlist(PERIPHP p) {
	// {{{
	STRING	str;

	if (!p->read_only())
		str = str + STRING(
	".@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWVALID(@$(SLAVE.PREFIX)_awvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)AWREADY(@$(SLAVE.PREFIX)_awready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWID(   @$(SLAVE.PREFIX)_awid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWADDR( @$(SLAVE.PREFIX)_awaddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0]),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWLEN(  @$(SLAVE.PREFIX)_awlen),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWSIZE( @$(SLAVE.PREFIX)_awsize),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWBURST(@$(SLAVE.PREFIX)_awburst),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWLOCK( @$(SLAVE.PREFIX)_awlock),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWCACHE(@$(SLAVE.PREFIX)_awcache),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWPROT( @$(SLAVE.PREFIX)_awprot),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)AWQOS(  @$(SLAVE.PREFIX)_awqos),\n"
	"\t\t//\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WVALID(@$(SLAVE.PREFIX)_wvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)WREADY(@$(SLAVE.PREFIX)_wready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WDATA( @$(SLAVE.PREFIX)_wdata),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WSTRB( @$(SLAVE.PREFIX)_wstrb),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)WLAST( @$(SLAVE.PREFIX)_wlast),\n"
	"\t\t//\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)BVALID(@$(SLAVE.PREFIX)_bvalid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)BREADY(@$(SLAVE.PREFIX)_bready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)BID(   @$(SLAVE.PREFIX)_bid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)BRESP( @$(SLAVE.PREFIX)_bresp)");
	if (!p->read_only() && !p->write_only())
		str = str + STRING(",\n\t\t// Read connections\n");
	if (!p->write_only())
		str = str + STRING(
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARVALID(@$(SLAVE.PREFIX)_arvalid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)ARREADY(@$(SLAVE.PREFIX)_arready),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARID(   @$(SLAVE.PREFIX)_arid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARADDR( @$(SLAVE.PREFIX)_araddr[")
		+ std::to_string(p->get_slave_address_width())
		+ STRING("-1:0]),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARLEN(  @$(SLAVE.PREFIX)_arlen),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARSIZE( @$(SLAVE.PREFIX)_arsize),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARBURST(@$(SLAVE.PREFIX)_arburst),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARLOCK( @$(SLAVE.PREFIX)_arlock),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARCACHE(@$(SLAVE.PREFIX)_arcache),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARPROT( @$(SLAVE.PREFIX)_arprot),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)ARQOS(  @$(SLAVE.PREFIX)_arqos),\n"
	"//\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RVALID(@$(SLAVE.PREFIX)_rvalid),\n"
	"\t\t.@$(SLAVE.IANSI)@$(SLAVE.ANSPREFIX)RREADY(@$(SLAVE.PREFIX)_rready),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RID(   @$(SLAVE.PREFIX)_rid),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RDATA( @$(SLAVE.PREFIX)_rdata),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RLAST( @$(SLAVE.PREFIX)_rlast),\n"
	"\t\t.@$(SLAVE.OANSI)@$(SLAVE.ANSPREFIX)RRESP( @$(SLAVE.PREFIX)_rresp)");

	return new STRING(str);
}
// }}}

////////////////////////////////////////////////////////////////////////
//
// Bus class logic
//
// The bus class describes the logic above in a way that it can be
// chosen, selected, and enacted within a design.  Hence if a design
// has four wishbone buses and one AXI4 bus, the bus class logic
// will recognize the four WB buses and generate WB bus generators,
// and then an AXI4 bus and bus generator, etc.
//
////////////////////////////////////////////////////////////////////////
STRINGP	AXIBUSCLASS::name(void) {
	return new STRING("axil");
}

STRINGP	AXIBUSCLASS::longname(void) {
	return new STRING("AXI4");
}

bool	AXIBUSCLASS::matchtype(STRINGP str) {
	if (!str)
		// We are not the default
		return false;
	if (strcasecmp(str->c_str(), "axi")==0)
		return true;
	if (strcasecmp(str->c_str(), "axi4")==0)
		return true;
	if (strcasecmp(str->c_str(), "axi4full")==0)
		return true;
// printf("ERR: No match for bus type %s\n",str->c_str());
	return false;
}

bool	AXIBUSCLASS::matchfail(MAPDHASH *bhash) {
	return false;
}

GENBUS *AXIBUSCLASS::create(BUSINFO *bi) {
	AXIBUS	*busclass;

	busclass = new AXIBUS(bi);
	return busclass;
}
