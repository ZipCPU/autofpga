////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldregdefs.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To build the regdefs.h and regdefs.cpp files.
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
#include "kveval.h"
#include "legalnotice.h"
#include "plist.h"
#include "bldregdefs.h"
#include "businfo.h"
#include "subbus.h"
#include "globals.h"
#include "gather.h"
#include "msgs.h"

extern	bool	isperipheral(MAPT &pmap);
extern	bool	isperipheral(MAPDHASH &phash);

int	get_longest_defname(APLIST *alist) {
	unsigned	longest_defname = 0;
	STRING		str;

	for(unsigned i=0; i<alist->size(); i++) {
		MAPDHASH::iterator	kvp;
		int	nregs = 0;
		MAPDHASH	*ph;

		ph = (*alist)[i]->p_phash;
		/*
		if ((*alist)[i]->isbus()) {
			SUBBUS	*sbp;
			sbp = (SUBBUS *)(*alist[i]);
			assert(sbp->p_slave_bus);
			assert(sbp->p_master_bus);
			if (!sbp->p_slave_bus->need_translator(sbp->p_master_bus)) {
				unsigned k = get_longest_defname(*sbp->p_master_bus->m_plist);
				if (k > longest_defname)
					longest_defname = k;
			}
		}*/


		if (!getvalue(*ph, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];

			sprintf(nstr, "%d", j);
			kvp = findkey(*ph,str=STRING("REGS.")+nstr);
			if (kvp == ph->end()) {
				fprintf(stderr, "%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				gbl_msg.info("%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname;

			// 1. Read the number (Not used)
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				fprintf(stderr, "No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);
		}
	}

	return longest_defname;
}

//
// write_regdefs
//
// This writes out the definitions of all the registers found within the plist
// to the C++ header file given by fp.  It takes as a parameter the longest
// name definition, which we use to try to line things up in a prettier fashion
// than we could without it.
//
void write_regdefs(FILE *fp, APLIST *alist, unsigned longest_defname) {
	STRING	str;

	gbl_msg.info("WRITE-REGDEFS\n");
	// Walk through this peripheral list one peripheral at a time
	for(unsigned i=0; i<alist->size(); i++) {
		MAPDHASH::iterator	kvp;
		int	nregs = 0;
		MAPDHASH	*ph;
		STRINGP		pname;

		ph = (*alist)[i]->p_phash;
		pname = (*alist)[i]->p_name;
		gbl_msg.info("WRITE-REGDEFS(%d, %s)\n", i, (NULL != pname)?pname->c_str() : "(No-name)");

		// If there is a note key for this peripheral, place it into
		// the output without modifications.
		kvp = findkey(*ph,KYREGS_NOTE);
		if ((kvp != ph->end())&&(kvp->second.m_typ == MAPT_STRING))
			fprintf(fp, "%s\n", kvp->second.u.m_s->c_str());


		// Walk through each of the defined registers.  There will be
		// @REGS.N registers defined.
		if (!getvalue(ph, KYREGS_N, nregs)) {
			gbl_msg.info("REGS.N not found in %s\n", pname->c_str());
			continue;
		}

		gbl_msg.info("Looking for REGS in %s\n", pname->c_str());
		// Now, walk through all of the register definitions
		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			STRINGP	strp;

			// Look for a @REGS.%d tag, defining each of the named
			// registers within this set.
			sprintf(nstr, "%d", j);
			strp = getstring(ph, str=STRING("REGS.")+nstr);
			if (!strp) {
				fprintf(stderr, "%s not found\n", str.c_str());
				continue;
			}

			STRING	scpy = *strp;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			int roff = strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				gbl_msg.info("No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			fprintf(fp, "#define\t%-*s\t0x%08lx", longest_defname,
				rname, (roff<<2)+(*alist)[i]->p_regbase);

			fprintf(fp, "\t// %08lx, wbregs names: ", (*alist)[i]->p_regbase);
			int	first = 1;
			// 3. Get the various user names
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (!first)
					fprintf(fp, ", ");
				first = 0;
				fprintf(fp, "%s", rv);
			} fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n\n");
}

//
// build_regdefs_h
//
//
// This builds a regdefs.h file, a file that can be used on a host in order
// to access our design.
//
void	build_regdefs_h(  MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess;
	STRING	str;
	STRINGP	strp;
	APLIST	*alist;

	legal_notice(master, fp, fname);

	fprintf(fp, "#ifndef\tREGDEFS_H\n");
	fprintf(fp, "#define\tREGDEFS_H\n");
	fprintf(fp, "\n\n");

	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.INCLUDE tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.INCLUDE for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INCLUDE);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INCLUDE for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INCLUDE);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And finally any master REGDEFS.H.INCLUDE tags\n");
	strp = getstring(master, KYREGDEFS_H_INCLUDE);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.INCLUDE\n\n\n");


	fprintf(fp, "//\n// Register address definitions, from @REGS.#d\n//\n");

	unsigned	longest_defname = 0;

	// Get the list of all peripherals
	alist = full_gather();

	longest_defname = get_longest_defname(alist);

	write_regdefs(fp, alist, longest_defname);


	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.DEFNS tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.DEFNS for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_DEFNS);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.DEFNS for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_DEFNS);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.DEFNS at the top level\n");
	strp = getstring(master, KYREGDEFS_H_DEFNS);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.DEFNS\n");



	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.INSERT tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.INSERT for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INSERT for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INSERT from the top level\n");
	strp = getstring(master, KYREGDEFS_H_INSERT);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.INSERT\n");

	fprintf(fp, "\n\n");

	fprintf(fp, "#endif\t// REGDEFS_H\n");
}

//
// get_longest_uname
//
// This is very similar to the get longest defname (get length of the longest
// variable definition name) above, save that this is applied to the user
// names within regdefs.cpp
//
unsigned	get_longest_uname(APLIST *alist) {
	unsigned	longest_uname = 0;
	STRING	str;
	STRINGP	strp;

	for(unsigned i=0; i<alist->size(); i++) {
		MAPDHASH::iterator	kvp;
		MAPDHASH	*ph;

		ph = (*alist)[i]->p_phash;

		int nregs = 0;
		if (!getvalue(*ph, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			strp = getstring(*ph, str=STRING("REGS.")+nstr);
			STRING	scpy = *strp;

			char	*nxtp, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				continue;
			}

			// 2. Get (and ignore) the C definition name
			strtok(nxtp, " \t\n,");

			// 3. Every token that follows will be a user (string)
			// name.  Find the one of these with the longest length.
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (strlen(rv) > longest_uname)
					longest_uname = strlen(rv);
			}
		}
	}

	return longest_uname;
}

//
// write_regnames
//
//
void write_regnames(FILE *fp, APLIST *alist,
		unsigned longest_defname, unsigned longest_uname) {
	STRING	str;
	STRINGP	strp;
	int	first = 1;

	for(unsigned i=0; i<alist->size(); i++) {
		int nregs = 0;
		MAPDHASH	*ph;

		ph = (*alist)[i]->p_phash;

		if (!getvalue(*ph, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			strp = getstring(*ph,str=STRING("REGS.")+nstr);
			STRING	scpy = *strp;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				gbl_msg.info("No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			// 3. Get the various user names and ... define them
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (!first)
					fprintf(fp, ",\n");
				first = 0;
				fprintf(fp, "\t{ %-*s,\t\"%s\"%*s\t}",
					longest_defname, rname, rv,
					(int)(longest_uname-strlen(rv)), "");
			}
		}
	}
}


//
// build_regdefs_cpp
//
void	build_regdefs_cpp(MAPDHASH &master, FILE *fp, STRING &fname) {
	STRINGP	strp;
	STRING	str;
	APLIST	*alist;

	legal_notice(master, fp, fname);

	if (NULL != (strp = getstring(master, KYREGDEFS_CPP_INCLUDE))) {
		fputs(strp->c_str(), fp);
	} else {
		fprintf(fp, "// No default include list found\n");
		fprintf(fp, "#include <stdio.h>\n");
		fprintf(fp, "#include <stdlib.h>\n");
		fprintf(fp, "#include <strings.h>\n");
		fprintf(fp, "#include <ctype.h>\n");
	}

	fprintf(fp, "#include \"regdefs.h\"\n\n");
	fprintf(fp, "const\tREGNAME\traw_bregs[] = {\n");

	// Get the list of peripherals
	alist = full_gather();

	// First, find out how long our longest definition name is.
	// This will help to allow us to line things up later.
	unsigned	longest_defname = 0;
	unsigned	longest_uname = 0;

	// Find the length of the longest register name
	longest_defname = get_longest_defname(alist);

	// Find the length of the longest user name string
	longest_uname = get_longest_uname(alist);

	write_regnames(fp, alist, longest_defname, longest_uname);

	fprintf(fp, "\n};\n\n");

	fprintf(fp, "// REGSDEFS.CPP.INSERT for any bus masters\n");
	for(MAPDHASH::iterator kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_CPP_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And then from the peripherals\n");
	for(MAPDHASH::iterator kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_CPP_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And finally any master REGS.CPP.INSERT tags\n");
	if (NULL != (strp = getstring(master, KYREGDEFS_CPP_INSERT))) {
		fputs(strp->c_str(), fp);
	}
}

