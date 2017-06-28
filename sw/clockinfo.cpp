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
#include <vector>
#include "parser.h"
#include "mapdhash.h"
#include "keys.h"
#include "bldtestb.h"
#include "legalnotice.h"
#include "clockinfo.h"
#include "globals.h"

const	unsigned long	CLOCKINFO::UNKNOWN_PS = 2ul;
const	unsigned long	CLOCKINFO::PICOSECONDS_PER_SECOND = 1000000000000ul;

CLKLIST	cklist;

CLOCKINFO::CLOCKINFO(void) {
	m_hash = new MAPDHASH();
	m_name = NULL;
	m_wire = NULL;
	m_interval_ps = UNKNOWN_PS;
}

unsigned long CLOCKINFO::setfrequency(unsigned long frequency_hz) {
	unsigned long ps;
	setvalue(*m_hash, KY_FREQUENCY, (int)frequency_hz);
	ps = PICOSECONDS_PER_SECOND / (unsigned long)frequency_hz;
	m_interval_ps = ps;
	return m_interval_ps;
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
	} else if (!m_wire)
		m_wire = wire;
}

void	add_to_clklist(MAPDHASH *ckmap) {
	const	char	DELIMITERS[] = " \t\n,";

	STRINGP	sname, swire, sfreq;
	char	*dname, *dwire, *dfreq;
	char	*pname, *pwire, *pfreq;
	char	*tname, *twire, *tfreq;

	sname = getstring(*ckmap, KY_NAME);
	swire = getstring(*ckmap, KY_WIRE);
	sfreq = getstring(*ckmap, KY_FREQUENCY);

	// strtok requires a writable string
	if (sname) dname = strdup(sname->c_str());
	else	  dname = NULL;
	if (swire) dwire = strdup(swire->c_str());
	else	  dwire = NULL;
	if (sfreq) dfreq = strdup(sfreq->c_str());
	else	  dfreq = NULL;

	pname = (dname) ? strtok_r(dname, DELIMITERS, &tname) : NULL;
	pwire = (dwire) ? strtok_r(dwire, DELIMITERS, &twire) : NULL;
	pfreq = (dfreq) ? strtok_r(dfreq, DELIMITERS, &tfreq) : NULL;

	while((pname)&&(pfreq)) {
		unsigned	id = cklist.size();
		unsigned long	clocks_per_second;
		STRINGP		wname;
		bool		already_defined = false;

		if (gbl_dump) {
			fprintf(gbl_dump, "Examining clock: %s %s %s\n",
				pname, pwire, pfreq);
		}

		for(unsigned i=0; i<id; i++) {
			if (cklist[i].m_name->compare(pname)==0) {
				already_defined = true;
				fprintf(gbl_dump, "Clock %s is already defined: %s %ld\n", cklist[i].m_name->c_str(), cklist[i].m_wire->c_str(), cklist[i].m_interval_ps);
				if ((pwire)&&(cklist[i].m_wire == NULL)) {
					cklist[i].m_wire = new STRING(pwire);
					fprintf(gbl_dump, "Clock %s\'s wire set to %s\n", pname, pwire);
				} else if ((pwire)&&(cklist[i].m_wire->compare(pwire) != 0)) {
					gbl_err++;
					fprintf(gbl_dump, "ERR: Clock %s has a conflicting wire definition: %s and %s\n", pname, pwire, cklist[i].m_wire->c_str());
				}

				if ((pfreq)&&(cklist[i].interval_ps()==CLOCKINFO::UNKNOWN_PS)) {
					fprintf(gbl_dump, "Setting %s clock frequency to %ld\n", pname, strtoul(pfreq, NULL, 0));
					cklist[id].setfrequency(
							clocks_per_second);
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
			clocks_per_second = strtoul(pfreq, NULL, 0);
			cki->setfrequency(clocks_per_second);

			if (gbl_dump) {
				fprintf(gbl_dump, "ADDING CLOCK: %s, %s, at %lu Hz\n",
					pname, wname->c_str(), clocks_per_second);
			}
			printf("ADDING CLOCK: %s, %s, at %lu Hz\n",
					pname, wname->c_str(), clocks_per_second);
		}

		if (pname) pname = strtok_r(NULL, DELIMITERS, &tname);
		if (pwire) pwire = strtok_r(NULL, DELIMITERS, &twire);
		if (pfreq) pfreq = strtok_r(NULL, DELIMITERS, &tfreq);
	}

	free(dname);
	free(dwire);
	free(dfreq);
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
		fprintf(stderr, "WARNING: Assuming a default clock of clk\n");
		STRING	str("clk");
		return getclockinfo(str);
	}
	return getclockinfo(*clock_name);
}

void	find_clocks(MAPDHASH &master) {
	MAPDHASH	*ckkey;
	MAPDHASH::iterator	kypair;

	if (gbl_dump)
		fprintf(gbl_dump, "------------ FIND-CLOCKS!! ------------\n");

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
		if (NULL != (ckkey = getmap(*p, KYCLOCK))) {
			add_to_clklist(ckkey);
		}
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

}

