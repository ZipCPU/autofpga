////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldtestb.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To build a test framework that can be used with Verilator for
//		handling multiple clocks.
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
		cklist.push_back(CLKITEM());
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
		cklist.push_back(CLKITEM());
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

void	build_testb_h(MAPDHASH &master, FILE *fp, STRING &fname) {
	// Find all the clocks in the design, and categorize them
	find_clocks(master);

	legal_notice(master, fp, fname);

	fprintf(fp, ""
"#ifndef	TESTB_H\n"
"#define	TESTB_H\n"
"\n"
"#include <stdio.h>\n"
"#include <stdint.h>\n"
"#include <verilated_vcd_c.h>\n");

	if (cklist.size() > 1)
		fprintf(fp, "#include <tbclock.h>\n");

	fprintf(fp, "\n"
"template <class VA>	class TESTB {\n"
"public:\n"
"	VA	*m_core;\n"
"	bool		m_changed;\n"
"	VerilatedVcdC*	m_trace;\n"
"	unsigned long	m_time_ps;\n");

	if (cklist.size() > 1) {
		for(unsigned i=0; i<cklist.size(); i++) {
			fprintf(fp, "\tTBCLOCK\tm_%s;\n",
				cklist[i].m_name->c_str());
		}
	}

	fprintf(fp, "\n"
"	TESTB(void) {\n"
"		m_core = new VA;\n"
"		m_time_ps  = 0ul;\n"
"		m_trace    = NULL;\n"
"		Verilated::traceEverOn(true);\n"
"	}\n"
"	virtual ~TESTB(void) {\n"
"		if (m_trace) m_trace->close();\n"
"		delete m_core;\n"
"		m_core = NULL;\n"
"	}\n"
"\n"
"	virtual	void	opentrace(const char *vcdname) {\n"
"		if (!m_trace) {\n"
"			m_trace = new VerilatedVcdC;\n"
"			m_core->trace(m_trace, 99);\n"
"			m_trace->open(vcdname);\n"
"		}\n"
"	}\n"
"\n"
"	void	trace(const char *vcdname) {\n"
"		opentrace(vcdname);\n"
"	}\n"
"\n"
"	virtual	void	closetrace(void) {\n"
"		if (m_trace) {\n"
"			m_trace->close();\n"
"			m_trace = NULL;\n"
"		}\n"
"	}\n"
"\n"
"	virtual	void	eval(void) {\n"
"		m_core->eval();\n"
"	}\n"
"\n"
"	virtual	void	tick(void) {\n");

	if (cklist.size() > 1) {
		fprintf(fp, ""
		"\t\tunsigned	mintime = m_%s.time_to_tick();\n\n",
			cklist[0].m_name->c_str());

		for(unsigned i=1; i<cklist.size(); i++)
			fprintf(fp, ""
				"\t\tif (m_%s.time_to_tick() < mintime)\n"
				"\t\t\tmintime = m_%s.time_to_tick();\n\n",
				cklist[i].m_name->c_str(),
				cklist[i].m_name->c_str());

		fprintf(fp, "\t\tassert(mintime > 1);\n\n");

		fprintf(fp, "\t\teval();\n"
		"\t\tif (m_trace) m_trace->dump(m_time_ps+1);\n\n");

		for(unsigned i=0; i<cklist.size(); i++)
			fprintf(fp, "\t\tm_core->%s = m_%s.advance(mintime);\n",
				cklist[i].m_wire->c_str(),
				cklist[i].m_name->c_str());

		fprintf(fp, "\n\t\teval();\n"
			"\t\tif (m_trace) m_trace->dump(m_time_ps+1);\n\n");


		fprintf(fp, "\n\t\tm_time_ps += mintime;\n");
		for(unsigned i=0; i<cklist.size(); i++)
			fprintf(fp, "\t\tif (m_%s.falling_edge()) {\n"
				"\t\t\tm_changed = true;\n"
				"\t\t\tsim_%s_tick();\n"
				"\t\t}\n",
				cklist[i].m_name->c_str(),
				cklist[i].m_name->c_str());

		fprintf(fp, "\t}\n\n");

		for(unsigned i=0; i<cklist.size(); i++) {
			fprintf(fp, "\tvirtual	void	sim_%s_tick(void) {}\n",
				cklist[i].m_name->c_str());
		}
	} else {
		unsigned long	clock_duration_ps;

		clock_duration_ps = cklist[0].interval_ps();

		fprintf(fp, "\t\tm_time_ps+= %ld;\n", clock_duration_ps);

		fprintf(fp, ""
		"\n"
		"\t\t// Make sure we have all of our evaluations complete before the top\n"
		"\t\t// of the clock.  This is necessary since some of the \n"
		"\t\t// connection modules may have made changes, for which some\n"
		"\t\t// logic depends.  This forces that logic to be recalculated\n"
		"\t\t// before the top of the clock.\n"
		"\t\tm_changed = true;\n"
		"\t\tsim_%s_tick();\n"
		"\t\tif (m_changed) {\n"
		"\t\t\teval();\n", cklist[0].m_name->c_str());
		fprintf(fp, ""
		"\t\t\tif (m_trace) m_trace->dump(m_time_ps - %ld);\n",
			clock_duration_ps/4);
		fprintf(fp, "\t\t}\n\n");

		fprintf(fp, ""
		"\t\tm_core->%s = 1;\n"
		"\t\teval();\n"
		"\t\tif (m_trace) m_trace->dump(m_time_ps);\n"
		"\t\tm_core->%s = 0;\n"
		"\t\teval();\n"
		"\t\tif (m_trace) m_trace->dump(m_time_ps + %ld);\n"
		"\n\t}\n\n",
			cklist[0].m_wire->c_str(),
			cklist[0].m_wire->c_str(),
			clock_duration_ps/2);

		fprintf(fp, "\t\tvirtual	void	sim_%s_tick(void) {\n"
			"\t\t\t// Your test fixture should over-ride this\n"
			"\t\t\t// method.  If you change any of the inputs,\n"
			"\t\t\t// either ignore m_changed or set it to true.\n"
			"\t\t\tm_changed = false;\n"
			"\t\t}\n",
			cklist[0].m_name->c_str());
	}

	fprintf(fp, ""
"	virtual	void	reset(void) {\n"
"		m_core->i_reset = 1;\n"
"		tick();\n"
"		m_core->i_reset = 0;\n"
"		// printf(\"RESET\\n\");\n"
"	}\n"
"};\n"
"\n"
"#endif\t// TESTB\n"
"\n");
}

