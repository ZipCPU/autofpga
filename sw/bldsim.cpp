////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldsim.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	This is the main/master program for the autofpga project.  All
//		other components server this one.
//
//	The purpose of autofpga.cpp is to read a group of configuration files
//	(.txt currently), and to generate code from those files to connect
//	the various parts and pieces within them into a design.
//
//	Currently that design is dependent upon the Wishbone B4/Pipelined
//	bus, but that's likely to change in the future.
//
//	Design files produced include:
//
//	toplevel.v
//	main.v
//	regdefs.h
//	regdefs.cpp
//	board.h		(Built, but ... not yet tested)
//	* dev.tex	(Not yet included)
//	* kernel device tree file
//
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
#include "legalnotice.h"
#include "bldtestb.h"

extern	void	writeout(FILE *fp, MAPDHASH &master, const STRING &ky);

bool	tb_same_clock(MAPDHASH &info, STRINGP ckname) {
	STRINGP	simclk;
	if (NULL != (simclk = getstring(info, KYSIM_CLOCK))) {
		return (ckname->compare(*simclk)==0);
	} if (NULL != (simclk = getstring(info, KYCLOCK_NAME))) {
		const	char	DELIMITERS[] = " \t\n,";
		char	*dstr, *tok;
		bool	result;


		dstr = strdup(simclk->c_str());
		tok = strtok(dstr, DELIMITERS);
		result = (ckname->compare(tok)==0);
		free(dstr);

		return result;
	}

	if ((cklist.size() > 0)&&(ckname->compare(*cklist[0].m_name)==0))
		return true;
	return false;
}

bool	tb_tick(MAPDHASH &info, STRINGP ckname, FILE *fp) {
	MAPDHASH::iterator	kvpair;
	bool	result = false;
	const STRING *ky = &KYSIM_TICK;

	if ((ckname)&&(fp))
		fprintf(fp, "\t\t//\n\t\t// SIM.TICK tags go here for SIM.CLOCK=%s\n\t\t//\n", ckname->c_str());
	else if (fp)
		fprintf(fp, "\t\t//\n\t\t// SIM.TICK tags go here for the default clock\n\t\t//\n");

	if (tb_same_clock(info, ckname)) {
		STRINGP	tick = getstring(info, *ky);
		if (tick) {
			if (fp) {
				fprintf(fp, "\t\t// %s from master\n", ky->c_str());
				fprintf(fp, "%s", tick->c_str());
			}
			result = true;
		}
	}

	for(kvpair=info.begin(); kvpair!=info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		MAPDHASH	*p = kvpair->second.u.m_m;
		if (!tb_same_clock(*p, ckname))
			continue;
		STRINGP	tick = getstring(*p, *ky);
		if (tick) {
			if (fp) {
				fprintf(fp, "\t\t// %s from %s\n", ky->c_str(),
					kvpair->first.c_str());
				fprintf(fp, "%s", tick->c_str());
			}
			result = true;
		}
	}

	if ((fp)&&(!result))
		fprintf(fp, "\t\t// No %s tags defined\n", ky->c_str());
	return result;
}

bool	tb_dbg_condition(MAPDHASH &info, STRINGP ckname, FILE *fp) {
	MAPDHASH::iterator	kvpair;
	bool	result = true;
	const STRING *ky = &KYSIM_DBGCONDITION;

	if (fp) fprintf(fp, "\t\t//\n\t\t// SIM.DBGCONDITION\n"
		"\t\t// Set writeout to true here for debug by printf access\n"
		"\t\t// to this routine\n"
		"\t\t//\n");

	if (tb_same_clock(info, ckname)) {
		STRINGP	tick = getstring(info, *ky);
		if (tick) {
			if (fp) {
				fprintf(fp, "\t\t// %s from master\n", ky->c_str());
				fprintf(fp, "%s", tick->c_str());
			}
			result= true;
		}
	}

	for(kvpair=info.begin(); kvpair!=info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		MAPDHASH	*p = kvpair->second.u.m_m;
		if (!tb_same_clock(*p, ckname))
			continue;
		STRINGP	tick = getstring(*p, *ky);
		if (tick) {
			if (fp) {
				fprintf(fp, "%s", tick->c_str());
			} result = true;
		}
	}

	if ((fp)&&(!result))
		fprintf(fp, "\t\t// No %s tags defined\n", ky->c_str());

	return result;
}

bool	tb_debug(MAPDHASH &info, STRINGP ckname, FILE *fp) {
	MAPDHASH::iterator	kvpair;
	bool	result = false;
	const STRING *ky = &KYSIM_DEBUG;

	if (fp)
		fprintf(fp, "\t\t\t//\n\t\t\t// SIM.DEBUG tags can print here, supporting\n"
			"\t\t\t// any attempts to debug by printf.  Following any\n"
			"\t\t\t// code you place here, a newline will close the\n"
			"\t\t\t// debug section.\n"
			"\t\t\t//\n");
	if (tb_same_clock(info, ckname)) {
		STRINGP	tick = getstring(info, *ky);
		if (tick) {
			if (fp) {
				fprintf(fp, "\t\t\t// %s from master\n", ky->c_str());
				fprintf(fp, "%s", tick->c_str());
			}
			result = true;
		}
	}

	for(kvpair=info.begin(); kvpair!=info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		MAPDHASH	*p = kvpair->second.u.m_m;
		if (!tb_same_clock(*p, ckname))
			continue;
		STRINGP	tick = getstring(*p, KYSIM_DEBUG);
		if (tick) {
			if (fp) {
				fprintf(fp, "\t\t\t//    %s from %s\n",
					ky->c_str(),
					kvpair->first.c_str());
				fprintf(fp, "%s", tick->c_str());
			}
			result = true;
		}
	}

	if ((fp)&&(!result))
		fprintf(fp, "\t\t\t// No %s tags defined\n", ky->c_str());
	return result;
}

void	build_main_tb_cpp(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;

	legal_notice(master, fp, fname);

	fprintf(fp, "//\n// SIM.INCLUDE\n//\n");
	fprintf(fp, "// Any SIM.INCLUDE tags you define will be pasted here.\n");
	fprintf(fp, "// This is useful for guaranteeing any include functions\n");
	fprintf(fp, "// your simulation needs are called.\n//\n");
	writeout(fp, master, KYSIM_INCLUDE);
	fprintf(fp, "//\n// SIM.DEFINES\n//\n");
	fprintf(fp, "// This tag is useful fr pasting in any #define values that\n");
	fprintf(fp, "// might then control the simulation following.\n//\n");
	writeout(fp, master, KYSIM_DEFINES);

	// Class definitions
	fprintf(fp, "class\tMAINTB : public TESTB<Vmain> {\npublic:\n");
	fprintf(fp, "\t\t// SIM.DEFNS\n\t\t//\n");
	fprintf(fp, "\t\t// If you have any simulation components, create a\n");
	fprintf(fp, "\t\t// SIM.DEFNS tag to have those components defined here\n");
	fprintf(fp, "\t\t// as part of the main_tb.cpp function.\n");
	writeout(fp, master, KYSIM_DEFNS);


	fprintf(fp, "\tMAINTB(void) {\n");
/*
	// Pre-Initial stuffs ....
	{
		bool	have_initial = false;

		str = getstring(master, KYSIM_PREINITIAL);
		if (NULL != str) {
			fprintf(fp, ":\n%s", str->c_str());
			have_initial = true;
		}

		for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
			if (kvpair->second.m_typ != MAPT_MAP)
				continue;
			str = getstring(kvpair->second, KYSIM_PREINITIAL);
			if (str == NULL)
				continue;

			if (!have_initial)
				fprintf(fp, ":\n");
			else	fprintf(fp, ",\n");
			have_initial = true;
			fprintf(fp, "\t// From %s\n%s", kvpair->first.c_str(),
				str->c_str());
		}
	} fprintf(fp, "\t{\n");
*/

	fprintf(fp, "\t\t// SIM.INIT\n\t\t//\n");
	fprintf(fp, "\t\t// If your simulation components need to be initialized,\n");
	fprintf(fp, "\t\t// create a SIM.INIT tag.  That tag\'s value will be pasted\n");
	fprintf(fp, "\t\t// here.\n\t\t//\n");
	str = getstring(master, KYSIM_INIT);
	if (NULL != str) {
		fprintf(fp, "\t%s", str->c_str());
	}

	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
				continue;
		str = getstring(kvpair->second, KYSIM_INIT);
		if (str == NULL)
			continue;

		fprintf(fp, "\t\t// From %s\n%s", kvpair->first.c_str(),
			str->c_str());
	}

	fprintf(fp, "\t}\n\n");

	fprintf(fp, "\tvoid\treset(void) {\n"
		"\t\t// SIM.SETRESET\n"
		"\t\t// If your simulation component needs logic before the\n"
		"\t\t// tick with reset set, that logic can be placed into\n"
		"\t\t// the SIM.SETRESET tag and thus pasted here.\n"
		"\t\t//\n");

	writeout(fp, master, KYSIM_SETRESET);

	fprintf(fp, "\t\tTESTB<Vmain>::reset();\n");

	fprintf(fp,
		"\t\t// SIM.CLRRESET\n"
		"\t\t// If your simulation component needs logic following the\n"
		"\t\t// reset tick, that logic can be placed into the\n"
		"\t\t// SIM.CLRRESET tag and thus pasted here.\n"
		"\t\t//\n");
	writeout(fp, master, KYSIM_CLRRESET);

	fprintf(fp, "\t}\n");

	fprintf(fp, "\n"
"\tvoid	trace(const char *vcd_trace_file_name) {\n"
"\t\tfprintf(stderr, \"Opening TRACE(%%s)\\n\",\n"
"\t\t		vcd_trace_file_name);\n"
"\t\topentrace(vcd_trace_file_name);\n"
"\t\tm_time_ps = 0;\n"
"\t}\n\n");

	fprintf(fp,
"	void	close(void) {\n"
"		m_done = true;\n"
"	}\n\n");

	fprintf(fp,
"	void	tick(void) {\n");
	fprintf(fp, "\t\tTESTB<Vmain>::tick(); // Clock.size = %ld\n\t}\n\n", cklist.size());

	for(unsigned i=0; i<cklist.size(); i++) {
		bool	have_sim_tick = false, have_debug = false,
			have_condition = false;

		fprintf(fp, "\n\t// Evaluating clock %s\n",
				cklist[i].m_name->c_str());
		have_debug = tb_debug(master, cklist[i].m_name, NULL);
		have_condition = tb_dbg_condition(master,
						cklist[i].m_name, NULL);
		have_sim_tick = tb_tick(master, cklist[i].m_name, NULL);

		if ((!have_sim_tick)&&(!have_condition)&&(!have_debug))
			continue;


		fprintf(fp, "\n\t// sim_%s_tick() will be called from"
				" TESTB<Vmain>::tick()\n"
				"\t//   following any falling edge of clock "
				"%s\n",
				cklist[i].m_name->c_str(),
				cklist[i].m_name->c_str());
		fprintf(fp, "\tvirtual\tvoid\tsim_%s_tick(void) {\n",
			cklist[i].m_name->c_str());
		if (0 == i)
			fprintf(fp, "\t\t// Default clock tick\n");

		if ((have_debug)&&(have_condition))
			fprintf(fp, "\t\tbool\twriteout;\n\n");
		tb_tick(master, cklist[i].m_name, fp);
		if (!have_sim_tick)
			fprintf(fp, "\t\tm_changed = false;\n");

		if ((have_debug)&&(have_condition)) {
			fprintf(fp, "\t\twriteout = false;\n");

			tb_dbg_condition(master, cklist[i].m_name, fp);

			fprintf(fp, "\t\tif (writeout) {\n");
			tb_debug(master, cklist[i].m_name, fp);
			fprintf(fp, "\t\t}\n");
		}
		fprintf(fp, "\t}\n");
	}

	if (cklist.size()>1) {
		for(unsigned i=0; i<cklist.size(); i++) {
			fprintf(fp, "\t//\n\t// Step until clock %s ticks\n\t//\n",
				cklist[i].m_name->c_str());

			fprintf(fp, "\tvirtual\tvoid\ttick_%s(void) {\n",
				cklist[i].m_name->c_str());
			if (0 == i)
				fprintf(fp, "\t\t// Advance until the default clock ticks\n");

			fprintf(fp, "\t\tdo {\n"
				"\t\t\ttick();\n"
				"\t\t} while(!m_%s.rising_edge());\n",
				cklist[i].m_name->c_str());

			fprintf(fp, "\t}\n\n");
		}
	} else {
		fprintf(fp,
			"\tinline\tvoid\ttick_%s(void) {\ttick();\t}\n\n",
			cklist[0].m_name->c_str());
	}

	fprintf(fp, "\t//\n\t// The load function\n"
		"\t//\n"
		"\t// This function is required by designs that need the flash or memory\n"
		"\t// set prior to run time.  The test harness should be able to call\n"
		"\t// this function to load values into any (memory-type) location\n"
		"\t// on the bus.\n"
		"\t//\n"
		"\tbool\tload(uint32_t addr, const char *buf, uint32_t len) {\n");
	STRING	prestr = STRING("\t\tuint32_t\tstart, offset, wlen, base, adrln;\n\n");

	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
				continue;
		MAPDHASH *dev = kvpair->second.u.m_m;
		int	base, naddr,
			lgnb = 2; // Log of the byte width of this bus
		STRINGP	accessp;
		const	char *accessptr;

		str = getstring(kvpair->second, KYSIM_LOAD);
		if (str == NULL)
			continue;
		if (!getvalue(*dev, KYBASE, base))
			continue;
		if (!getvalue(*dev, KYNADDR, naddr))
			continue;
		accessp = getstring(*dev, KYACCESS);

		if (prestr[0]) {
			fprintf(fp, "%s", prestr.c_str());
			prestr[0] = '\0';
		}

		fprintf(fp, "\t\t//\n\t\t// Loading the %s component\n\t\t//\n",
			kvpair->first.c_str());
		fprintf(fp, "\t\tbase  = 0x%08x; // in octets\n"
				"\t\tadrln = 0x%08x;\n\n",
			base, naddr << lgnb);
		fprintf(fp, "\t\tif ((addr >= base)&&(addr < base + adrln)) {\n");
		fprintf(fp, "\t\t\t// If the start access is in %s\n", kvpair->first.c_str());
		fprintf(fp, "\t\t\tstart = (addr > base) ? (addr-base) : 0;\n");
		fprintf(fp, "\t\t\toffset = (start + base) - addr;\n");
		fprintf(fp, "\t\t\twlen = (len-offset > adrln - start)\n\t\t\t\t? (adrln - start) : len - offset;\n");


		if (accessp) {
			accessptr = accessp->c_str();
			if (accessptr[0] == '!')
				accessptr++;
			fprintf(fp, "#ifdef\t%s\n", accessptr);
		} else accessptr = NULL;
		fprintf(fp, "\t\t\t// FROM %s.%s\n", kvpair->first.c_str(), KYSIM_LOAD.c_str());
		fprintf(fp, "%s", str->c_str());
		fprintf(fp, "\t\t\t// AUTOFPGA::Now clean up anything else\n");
		fprintf(fp, "\t\t\t// Was there more to write than we wrote?\n");
		fprintf(fp, "\t\t\tif (addr + len > base + adrln)\n");
		fprintf(fp, "\t\t\t\treturn load(base + adrln, &buf[offset+wlen], len-wlen);\n");
		fprintf(fp, "\t\t\treturn true;\n");
		if (accessp) {
			fprintf(fp, "#else\t// %s\n", accessptr);
			fprintf(fp, "\t\t\treturn false;\n");
			fprintf(fp, "#endif\t// %s\n", accessptr);
		}

		fprintf(fp, "\t\t//\n"
			"\t\t// End of components with a SIM.LOAD tag, and a\n"
			"\t\t// non-zero number of addresses (NADDR)\n"
			"\t\t//\n");
		fprintf(fp, "\t\t}\n\n");
	}
	fprintf(fp, "\t\treturn false;\n");
	fprintf(fp, "\t}\n\n");

	fprintf(fp, "\t//\n\t// KYSIM.METHODS\n\t//\n"
		"\t// If your simulation code will need to call any of its own function\n"
		"\t// define this tag by those functions (or other sim code), and\n"
		"\t// it will be pasated here.\n"
		"\t//\n");
	writeout(fp, master, KYSIM_METHODS);

	fprintf(fp, "\n};\n");
}

