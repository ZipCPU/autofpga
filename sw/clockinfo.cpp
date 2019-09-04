////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	clockinfo.cpp
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
#include <vector>
#include "parser.h"
#include "mapdhash.h"
#include "keys.h"
#include "bldtestb.h"
#include "legalnotice.h"
#include "clockinfo.h"
#include "globals.h"
#include "msgs.h"

const	unsigned long	CLOCKINFO::UNKNOWN_PS = 2ul;
const	unsigned long	CLOCKINFO::PICOSECONDS_PER_SECOND = 1000000000000ul;

CLKLIST	cklist;

CLOCKINFO::CLOCKINFO(void) {
	m_hash  = new MAPDHASH();
	m_name  = NULL;
	m_reset = NULL;
	m_wire  = NULL;
	m_top   = NULL;
	m_simclass = NULL;
	m_interval_ps = UNKNOWN_PS;
}

CLOCKINFO *CLOCKINFO::new_clock(STRINGP name) {
	CLOCKINFO *ci;
	if (NULL != (ci = getclockinfo(name)))
		return ci;

	unsigned	id = cklist.size();
	cklist.push_back(CLOCKINFO());
	ci = &cklist[id];
	ci->setname(new STRING(*name));

	return	ci;
}

unsigned long CLOCKINFO::setfrequency(unsigned long frequency_hz) {

	if (frequency_hz != 0l) {
		unsigned long ps;

		setvalue(*m_hash, KY_FREQUENCY, (int)frequency_hz);
		ps = PICOSECONDS_PER_SECOND / (unsigned long)frequency_hz;
		m_interval_ps = ps;
		return m_interval_ps;
	} else if (m_interval_ps != UNKNOWN_PS)
		return m_interval_ps;
	return 0l;
}

unsigned	CLOCKINFO::frequency(void) {
	double	v = 1e12 / m_interval_ps;
	return (unsigned)v;
}

void	CLOCKINFO::setname(STRINGP name) {
	STRINGP	strp;

	strp = getstring(*m_hash, KY_NAME);
	if (NULL == strp) {
		setstring(*m_hash, KY_NAME, name);
		m_name = name;
	} else if (strp->compare(*name) != 0) {
		fprintf(stderr, "ERR: Clock with multiple names: %s and %s\n",
			strp->c_str(), name->c_str());
		m_name = strp;
	} else if (!m_name)
		m_name = name;
}

STRINGP	CLOCKINFO::reset(void) {
	STRINGP	strp;

	strp = getstring(*m_hash, KY_RESET);
	return strp;
}

void	CLOCKINFO::setwire(STRINGP wire) {
	STRINGP	strp;

	strp = getstring(*m_hash, KY_WIRE);
	if (NULL == strp) {
		setstring(*m_hash, KY_WIRE, wire);
		m_wire = wire;
	} else if (strp->compare(*wire) != 0) {
		fprintf(stderr, "ERR: Clock with multiple wire ID\'s: %s and %s\n",
			strp->c_str(), wire->c_str());
		m_wire = strp;
	} else if (!m_wire) {
		// Case #3: This is a new WIRE tag
		MAPT	subfm;

		subfm.m_typ = MAPT_STRING;
		subfm.u.m_s = wire;
		m_hash->insert(KEYVALUE(KY_WIRE, subfm));
		m_wire = wire;
	}
}

void	CLOCKINFO::settop(STRINGP top) {
	STRINGP	strp;

	strp = getstring(*m_hash, KY_TOP);
	if (NULL == strp) {
		// Case #1: it's not (yet) in our hash
		setstring(*m_hash, KY_TOP, top);
		m_top = top;
	} else if (strp->compare(*top) != 0) {
		// Case #2: it's in the has, but conflicts
		fprintf(stderr, "ERR: Clock with multiple top-level wire ID\'s: %s and %s\n",
			strp->c_str(), top->c_str());
		m_top = strp;
	} else if (!m_top) {
		// Case #3: This is a new TOP tag
		MAPT	subfm;

		subfm.m_typ = MAPT_STRING;
		subfm.u.m_s = top;
		m_hash->insert(KEYVALUE(KY_TOP, subfm));
		m_top = top;
	}
}

void	CLOCKINFO::setclass(STRINGP simclass) {
	STRINGP	strp;

	strp = getstring(*m_hash, KY_CLASS);
	if (NULL == strp) {
		setstring(*m_hash, KY_CLASS, simclass);
		m_simclass = simclass;
	} else if (strp->compare(*simclass) != 0) {
		fprintf(stderr, "ERR: Clock with multiple simulation classes: %s and %s\n",
			strp->c_str(), simclass->c_str());
		m_simclass = strp;
	} else if (!m_simclass) {
		// Case #3: This is a new CLASS tag
		m_simclass = simclass;
		MAPT	subfm;

		subfm.m_typ = MAPT_STRING;
		subfm.u.m_s = simclass;
		m_hash->insert(KEYVALUE(KY_CLASS, subfm));
		m_simclass = simclass;
	}
}

void	add_to_clklist(MAPDHASH *ckmap) {
	const	char	DELIMITERS[] = " \t\n,";
	int	ifreq;

	STRINGP	sname, swire, sfreq, simclass, stop;
	char	*dname, *dwire, *dfreq, *dsimclass, *dtop;
	char	*pname, *pwire, *pfreq, *psimclass, *ptop;
	char	*tname, *twire, *tfreq, *tsimclass, *ttop;

	sname    = getstring(*ckmap, KY_NAME);
	swire    = getstring(*ckmap, KY_WIRE);
	simclass = getstring(*ckmap, KY_CLASS);
	stop     = getstring(*ckmap, KY_TOP);

	// strtok requires a writable string
	if (sname) dname = strdup(sname->c_str());
	else	  dname = NULL;
	if (swire) dwire = strdup(swire->c_str());
	else	  dwire = NULL;
	if (simclass) dsimclass = strdup(simclass->c_str());
	else	  dsimclass = NULL;
	if (stop) dtop = strdup(stop->c_str());
	else	  dtop = NULL;

	{
		MAPDHASH::iterator	kvfreq;

		sfreq = NULL;
		dfreq = NULL;
		ifreq = 0;

		kvfreq = findkey(*ckmap, KY_FREQUENCY);
		if (kvfreq != ckmap->end()) switch(kvfreq->second.m_typ) {
		case MAPT_STRING:
			sfreq = kvfreq->second.u.m_s;
			dfreq = strdup(sfreq->c_str());
			break;
		case MAPT_INT: case MAPT_AST: case MAPT_MAP:
		default:
			if (!getvalue(*ckmap, KY_FREQUENCY, ifreq)) {
				gbl_msg.error("Could not evaluate the "
					"frequency of clock %s\n",
					(sname)?(sname->c_str())
						: "(Unnamed-clock)");
			} break;
		}
	}

	pname = (dname) ? strtok_r(dname, DELIMITERS, &tname) : NULL;
	pwire = (dwire) ? strtok_r(dwire, DELIMITERS, &twire) : NULL;
	pfreq = (dfreq) ? strtok_r(dfreq, DELIMITERS, &tfreq) : NULL;
	ptop  = (dtop ) ? strtok_r(dtop , DELIMITERS, &ttop) : NULL;
	psimclass = (dsimclass) ? strtok_r(dsimclass, DELIMITERS, &tsimclass) : NULL;

	if (!pname)
		fprintf(stderr, "ERR: CLOCK has no name!\n");
	// if ((!pfreq)&&(ifreq == 0))
		// fprintf(stderr, "ERR: CLOCK has no frequency\n");

	while(pname) {
		unsigned	id = cklist.size();
		unsigned long	clocks_per_second;
		STRINGP		wname, wsimclass = NULL;
		bool		already_defined = false;

		gbl_msg.info("Examining clock: %s %s %s\n",
				pname, (pwire)?pwire:"(Unspec)",
				(pfreq)?pfreq:"(Unspec)",
				(ptop)?ptop:"(Unspec)");

		for(unsigned i=0; i<id; i++) {
			if (cklist[i].m_name->compare(pname)==0) {
				//
				// Update an existing clocks information
				//
				already_defined = true;
				gbl_msg.info("Clock %s is already defined: %s %ld\n",
						cklist[i].m_name->c_str(),
						(cklist[i].m_wire)
						  ? cklist[i].m_wire->c_str()
						  : "(Unspec)",
						cklist[i].m_interval_ps);

				//
				// Clock's wire name, such as i_clk
				//
				if ((pwire)&&(cklist[i].m_wire == NULL)) {
					cklist[i].m_wire = new STRING(pwire);
					gbl_msg.info("Clock %s\'s wire set to %s\n", pname, pwire);
				} else if ((pwire)&&(cklist[i].m_wire->compare(pwire) != 0)) {
					gbl_msg.error("Clock %s has a conflicting wire definition: %s and %s\n", pname, pwire, cklist[i].m_wire->c_str());
				}

				//
				// Name of the top level incoming port, if
				// present
				//
				if ((ptop)&&(cklist[i].m_top == NULL)) {
					cklist[i].m_top = new STRING(ptop);
					gbl_msg.info("Clock %s\'s top-level wire set to %s\n", pname, ptop);
				} else if ((ptop)&&(cklist[i].m_top->compare(ptop) != 0)) {
					gbl_msg.error("Clock %s has a conflicting toplevel wire definition: %s and %s\n", pname, ptop, cklist[i].m_top->c_str());
				}


				//
				// Name of the simulation class, if present
				//
				if ((psimclass)&&(cklist[i].m_simclass == NULL)) {
					cklist[i].m_simclass = new STRING(psimclass);
					gbl_msg.info("Clock %s\'s simulation class set to %s\n", pname, psimclass);
				} else if ((psimclass)&&(cklist[i].m_simclass->compare(psimclass) != 0)) {
					gbl_msg.error("Clock %s has a conflicting simulation class definition: %s and %s\n", pname, psimclass, cklist[i].m_simclass->c_str());
				}


				if ((pfreq)&&(cklist[i].interval_ps()==CLOCKINFO::UNKNOWN_PS)) {
					clocks_per_second = strtoul(pfreq, NULL, 0);
					gbl_msg.info("Setting %s clock frequency to %ld\n", pname, clocks_per_second);
					cklist[i].setfrequency(
							clocks_per_second);
				} else if ((ifreq)&&(cklist[i].interval_ps() == CLOCKINFO::UNKNOWN_PS)) {
					gbl_msg.info("Setting %s clock frequency to %u\n", pname, ifreq);
					cklist[i].setfrequency(
							(unsigned long)
							((unsigned)ifreq));
				}

				break;
			}
		} if (!already_defined) {
			CLOCKINFO	*cki;

			cklist.push_back(CLOCKINFO());
			cki = &cklist[id];
			cki->setname(new STRING(pname));
			if (pwire)
				wname = new STRING(pwire);
			else
				wname = new STRING(STRING("i_")+STRING(pname));
			cki->setwire(wname);
			if (ptop)
				cki->settop(new STRING(ptop));
			if (psimclass) {
				wsimclass = new STRING(psimclass);
				cki->setclass(wsimclass);
			}
			clocks_per_second = (unsigned)ifreq;
			if (pfreq) {
				clocks_per_second = strtoul(pfreq, NULL, 0);
				cki->setfrequency(clocks_per_second);
			} else if (ifreq != 0) {
				// clocks_per_second = (unsigned)ifreq;
				cki->setfrequency(clocks_per_second);
			}

			if (clocks_per_second != 0)
				gbl_msg.userinfo("Clock: %s, %s, at %lu Hz\n",
					pname, wname->c_str(),
					clocks_per_second);
			else
				gbl_msg.userinfo("Clock: %s, %s\n",
					pname, wname->c_str());
		}

		if (pname) pname = strtok_r(NULL, DELIMITERS, &tname);
		if (pwire) pwire = strtok_r(NULL, DELIMITERS, &twire);
		if (pfreq) pfreq = strtok_r(NULL, DELIMITERS, &tfreq);
		if (ptop)  ptop  = strtok_r(NULL, DELIMITERS, &ttop);
		if (psimclass) psimclass = strtok_r(NULL, DELIMITERS, &tsimclass);
		ifreq = 0;
	}

	free(dname);
	free(dwire);
	free(dfreq);
	free(dtop);
	free(dsimclass);
}

CLOCKINFO	*getclockinfo(STRING &clock_name) {
	CLKLIST::iterator	ckp;

	for(ckp = cklist.begin(); ckp != cklist.end(); ckp++) {
		if (ckp->m_name->compare(clock_name)==0) {
			CLOCKINFO	*ckresult;
			ckresult = &(*ckp);
			return ckresult;
		}
	} return NULL;
}

CLOCKINFO	*getclockinfo(STRINGP clock_name) {
	if (!clock_name) {
		// Get the default clock
		assert(0);
		gbl_msg.fatal("No clock name given (might have assumed a default clock of clk)\n");
		STRING	str("clk");
		return getclockinfo(str);
	}
	return getclockinfo(*clock_name);
}

void	expand_clock(MAPDHASH &info) {
	MAPDHASH::iterator	kyclock;
	MAPDHASH	*ckmap;
	CLOCKINFO	*cki;
	STRINGP		sname;

	kyclock = findkey(info, KYCLOCK);
	if (info.end() == kyclock)
		return;
	if (kyclock->second.m_typ != MAPT_MAP)
		return;
	ckmap = kyclock->second.u.m_m;
	sname = getstring(ckmap, KY_NAME);

	if (!sname) {
		STRINGP ckwire;
		MAPDHASH::iterator	kyprefix;
		kyprefix = findkey(info, KYPREFIX);
		if ((info.end() == kyprefix)
				||(kyprefix->second.m_typ != MAPT_STRING)) {
			gbl_msg.error("ERR: Clock in %s has been given no name",
				kyprefix->second.u.m_s->c_str());
			return;
		}
		ckwire = getstring(ckmap, KY_WIRE);
		if (ckwire) {
			gbl_msg.error("ERR: Clock with no name, using wire named %s\n", ckwire->c_str());
			return;
		}
	} assert(sname);

	// This will fail if multiple clocks are defined on the same line
	cki = getclockinfo(sname);

	if (cki) {
		if (cki->m_hash != kyclock->second.u.m_m)
			kyclock->second.u.m_m = cki->m_hash;
	}
}

void	expand_clock(MAPT &elm) {
	if(elm.m_typ == MAPT_MAP)
		expand_clock(*elm.u.m_m);
}

void	find_clocks(MAPDHASH &master) {
	MAPDHASH	*ckkey;
	MAPDHASH::iterator	kypair;

	gbl_msg.info("------------ FIND-CLOCKS!! ------------\n");

	// If we already have at least one clock, then we must've been called
	// before.  Do nothing more.
	if (cklist.size() > 0)
		return;

	if (NULL != (ckkey = getmap(master, KYCLOCK))) {
		add_to_clklist(ckkey);
	}

	for(kypair = master.begin(); kypair != master.end(); kypair++) {
		MAPDHASH	*p;

		if (kypair->second.m_typ != MAPT_MAP)
			continue;

		p = kypair->second.u.m_m;
		if (NULL != (ckkey = getmap(*p, KYCLOCK)))
			add_to_clklist(ckkey);
	}

	CLOCKINFO	*cki;
	STRINGP		sclk;

	sclk = new STRING("clk");
	if (NULL == (cki = getclockinfo(new STRING("clk")))) {
		cklist.push_back(CLOCKINFO());
		// Default clock, if not given, is 100MHz
		cklist[0].set(sclk, new STRING("i_clk"), 100000000ul);
	} else
		delete sclk;

	expand_clock(master);
	for(kypair = master.begin(); kypair != master.end(); kypair++)
		expand_clock(kypair->second);

	fprintf(stderr, "All clocks enumerated\n");
}
