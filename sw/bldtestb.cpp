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
// Copyright (C) 2017-2018, Gisselquist Technology, LLC
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

void	build_testb_h(MAPDHASH &master, FILE *fp, STRING &fname) {
	bool	multiclock = false;
	// Find all the clocks in the design, and categorize them
	find_clocks(master);

	legal_notice(master, fp, fname);

	fprintf(fp, ""
"#ifndef	TESTB_H\n"
"#define	TESTB_H\n"
"\n"
"#include <stdio.h>\n"
"#include <stdint.h>\n"
"#include <verilated_vcd_c.h>\n"
"#include \"tbclock.h\"\n");

	if (cklist.size() == 0) {
		fprintf(stderr, "ERR: No clocks defined!\n");
		exit(EXIT_FAILURE);
	}

	if (cklist.size() > 1)
		multiclock = true;
	else if (cklist[0].m_simclass != NULL)
		multiclock = true;
	else
		multiclock = false;
	if (multiclock)
		fprintf(fp, "#include <tbclock.h>\n");

	fprintf(fp, "\n"
"template <class VA>	class TESTB {\n"
"public:\n"
"	VA	*m_core;\n"
"	bool		m_changed;\n"
"	VerilatedVcdC*	m_trace;\n"
"	bool		m_done;\n"
"	uint64_t	m_time_ps;\n");

	if (multiclock) {
		fprintf(fp, "\t// TBCLOCK is a clock support class, enabling"
				" multiclock simulation\n\t// operation.\n");
		for(unsigned i=0; i<cklist.size(); i++)
			fprintf(fp, "\t%s\tm_%s;\n",
				(cklist[i].m_simclass)
					? cklist[i].m_simclass->c_str()
					: "TBCLOCK",
				cklist[i].m_name->c_str());
	}

	fprintf(fp, "\n"
"	TESTB(void) {\n"
"		m_core = new VA;\n"
"		m_time_ps  = 0ul;\n"
"		m_trace    = NULL;\n"
"		m_done     = false;\n"
"		Verilated::traceEverOn(true);\n");

	if (multiclock) {
		fprintf(fp, "// Set the initial clock periods\n");
		for(unsigned i=0; i<cklist.size(); i++) {
			double	freq;
			fprintf(fp, "\t\tm_%s.init(%ld);",
				cklist[i].m_name->c_str(),
				cklist[i].interval_ps());
			freq = 1e6 / cklist[i].interval_ps();
			fprintf(fp, "\t//%8.2f MHz\n", freq);
		}
	}

	fprintf(fp,
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
"			m_trace->spTrace()->set_time_resolution(\"ps\");\n"
"			m_trace->spTrace()->set_time_unit(\"ps\");\n"
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
"			delete m_trace;\n"
"			m_trace = NULL;\n"
"		}\n"
"	}\n"
"\n"
"	virtual	void	eval(void) {\n"
"		m_core->eval();\n"
"	}\n"
"\n"
"	virtual	void	tick(void) {\n");

	if (multiclock) {
		fprintf(fp, ""
			"\t\tunsigned	mintime = m_%s.time_to_edge();\n\n",
				cklist[0].m_name->c_str());

		for(unsigned i=1; i<cklist.size(); i++)
			fprintf(fp, ""
				"\t\tif (m_%s.time_to_edge() < mintime)\n"
				"\t\t\tmintime = m_%s.time_to_edge();\n\n",
				cklist[i].m_name->c_str(),
				cklist[i].m_name->c_str());

		fprintf(fp, "\t\tassert(mintime > 1);\n\n");

		fprintf(fp, "\t\t// Pre-evaluate, to give verilator a chance"
			" to settle any\n\t\t// combinatorial logic that"
			"that may have changed since the\n\t\t// last clock"
			"evaluation, and then record that in the trace.\n");
		fprintf(fp, "\t\teval();\n"
			"\t\tif (m_trace) m_trace->dump(m_time_ps+1);\n\n");

		fprintf(fp, "\t\t// Advance each clock\n");
		for(unsigned i=0; i<cklist.size(); i++)
			fprintf(fp, "\t\tm_core->%s = m_%s.advance(mintime);\n",
				cklist[i].m_wire->c_str(),
				cklist[i].m_name->c_str());

		fprintf(fp, "\n\t\tm_time_ps += mintime;\n");

	} else {
		unsigned long	clock_duration_ps, quarter_tick, half_tick;

		clock_duration_ps = cklist[0].interval_ps();
		half_tick    = clock_duration_ps / 2;
		quarter_tick = half_tick / 2;

		fprintf(fp, "\t\t// Pre-evaluate, to give verilator a chance\n"
			"\t\t// to settle any combinatorial logic that\n"
			"\t\t// that may have changed since the last clock\n"
			"\t\t// evaluation, and then record that in the\n"
			"\t\t// trace.\n");
		fprintf(fp, "\t\teval();\n"
			"\t\tif (m_trace) m_trace->dump(m_time_ps+%ld);\n\n",
			quarter_tick);

		fprintf(fp, "\t\t// Advance the one simulation clock, %s\n",
				cklist[0].m_name->c_str());
		fprintf(fp, "\t\tm_core->%s = 1;\n", cklist[0].m_wire->c_str());

		fprintf(fp, "\t\tm_time_ps+= %ld;\n", half_tick);
	}

	fprintf(fp, "\t\teval();\n"
		"\t\t// If we are keeping a trace, dump the current state to "
		"that\n\t\t// trace now\n"
		"\t\tif (m_trace) {\n"
			"\t\t\tm_trace->dump(m_time_ps);\n"
			"\t\t\tm_trace->flush();\n"
		"\t\t}\n\n");


	if (multiclock) {
		for(unsigned i=0; i<cklist.size(); i++)
			fprintf(fp, "\t\tif (m_%s.falling_edge()) {\n"
				"\t\t\tm_changed = true;\n"
				"\t\t\tsim_%s_tick();\n"
				"\t\t}\n",
				cklist[i].m_name->c_str(),
				cklist[i].m_name->c_str());
	} else {
		unsigned long	clock_duration_ps, previous;

		clock_duration_ps = cklist[0].interval_ps();
		previous = clock_duration_ps/2;

		fprintf(fp,
			"\t\t// <SINGLE CLOCK ONLY>:\n"
			"\t\t// Advance the clock again, so that it has "
			"its negative edge\n");
		fprintf(fp, "\t\tm_core->%s = 0;\n"
			"\t\tm_time_ps+= %ld;\n"
			"\t\teval();\n"
			"\t\tif (m_trace) m_trace->dump(m_time_ps);\n\n",
				cklist[0].m_wire->c_str(),
				clock_duration_ps-previous);

		fprintf(fp,
			"\t\t// Call to see if any simulation components need\n"
			"\t\t// to advance their inputs based upon this clock\n");
		fprintf(fp, "\t\tsim_%s_tick();\n",
				cklist[0].m_name->c_str());
	}
	fprintf(fp, "\t}\n\n");


	for(unsigned i=0; i<cklist.size(); i++) {
		fprintf(fp, "\tvirtual	void	sim_%s_tick(void) {\n"
		"\t\t// AutoFPGA will override this method within "
		"main_tb.cpp if any\n\t\t// @SIM.TICK key is present "
		"within a design component also\n\t\t// containing a "
		"@SIM.CLOCK key identifying this clock.  That\n\t\t// "
		"component must also set m_changed to true.\n"
		"\t\tm_changed = false;\n"
		"\t}\n",
			cklist[i].m_name->c_str());
	}

	fprintf(fp, ""
"\tvirtual bool	done(void) {\n"
"\t\tif (m_done)\n"
"\t\t\treturn true;\n"
"\n"
"\t\tif (Verilated::gotFinish())\n"
"\t\t\tm_done = true;\n"
"\n"
"\t\treturn m_done;\n"
"\t}\n\n");

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

