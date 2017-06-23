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

extern	FILE	*gbl_dump;
const	unsigned long	PICOSECONDS_PER_SECOND = 1000000000000ul;

CLKLIST	cklist;

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

		for(unsigned i=0; i<id; i++) {
			if (cklist[i].m_name->compare(pname)==0) {
				already_defined = true;
				break;
			}
		} if (already_defined)
			continue;
		cklist.push_back(CLOCKINFO());
		cklist[id].m_name = new STRING(pname);
		if (pwire)
			wname = new STRING(pwire);
		else
			wname = new STRING(STRING("i_")+STRING(pname));
		clocks_per_second = strtoul(pfreq, NULL, 0);


		cklist[id].set(new STRING(pname), wname,
				PICOSECONDS_PER_SECOND
					/ (unsigned long)clocks_per_second);

		if (gbl_dump) {
			fprintf(gbl_dump, "ADDING CLOCK: %s, %s, at %lu Hz\n",
				pname, wname->c_str(), clocks_per_second);
		}
		printf("ADDING CLOCK: %s, %s, at %lu Hz\n",
				pname, wname->c_str(), clocks_per_second);

		if (pname) pname = strtok_r(NULL, DELIMITERS, &tname);
		if (pwire) pwire = strtok_r(NULL, DELIMITERS, &twire);
		if (pfreq) pfreq = strtok_r(NULL, DELIMITERS, &tfreq);
	}

	free(dname);
	free(dwire);
	free(dfreq);
}

void	find_clocks(MAPDHASH &master) {
	MAPDHASH	*ckkey;
	MAPDHASH::iterator	kypair;

	// If we already have at least one clock, then we must've been called
	// before.  Do nothing more.
	if (cklist.size() > 0)
		return;

	if (NULL != (ckkey = getmap(master, KYCLOCK))) {
		add_to_clklist(ckkey);
	} else {
		cklist.push_back(CLOCKINFO());
		cklist[0].set(new STRING("clk"), new STRING("i_clk"), 10000ul);
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
}

