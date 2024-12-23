////////////////////////////////////////////////////////////////////////////////
//
// Filename:	sw/bldrtlmake.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
// {{{
// Purpose:	To draw the information from the master hash database out in
//		to create a Makefile for an rtl/ directory.
//
// Builds: rtl.make.inc, for include'ing in your rtl Makefile
//
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2024, Gisselquist Technology, LLC
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
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <ctype.h>

#include "parser.h"
#include "keys.h"
#include "kveval.h"
#include "legalnotice.h"
#include "bldtestb.h"
#include "bldboardld.h"
#include "bitlib.h"
#include "plist.h"
#include "bldregdefs.h"
#include "ifdefs.h"
#include "bldsim.h"
#include "predicates.h"
#include "businfo.h"
#include "globals.h"

void	build_rtl_make_inc(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP	mksubd, mkgroup, mkfiles;
	STRING	allgroups, vdirs;
	const char *DELIMITERS=", \t\r\n";
	std::vector<STRINGP>	subdirs;

	legal_notice(master, fp, fname,
		"########################################"
		"########################################", "##");

	for(kvpair=master.begin(); kvpair!=master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		mksubd  = getstring(kvpair->second, KYRTL_MAKE_SUBD);
		mkgroup = getstring(kvpair->second, KYRTL_MAKE_GROUP);
		mkfiles = getstring(kvpair->second, KYRTL_MAKE_FILES);

		if (mksubd && mksubd->size() > 0) {
			bool	prior_dir = false;
			for(auto k : subdirs) {
				if (mksubd->compare(*k)==0) {
					prior_dir = true;
					break;
				}
			} if (!prior_dir)
				subdirs.push_back(mksubd);
		}

		if (!mkfiles)
			continue;

		char	*tokstr = strdup(mkfiles->c_str()), *tok;
		STRING	filstr = "";

		tok = strtok(tokstr, DELIMITERS);
		while(NULL != tok) {
			filstr += STRING(tok) + " ";
			tok = strtok(NULL, DELIMITERS);
		} if (filstr[filstr.size()-1] == ' ')
			filstr[filstr.size()-1] = '\0';
		if (!mkgroup) {
			char	temp[] = "MKGRPXXXXXX";
			mkstemp(temp);
			unlink(temp);
			mkgroup = new STRING(temp);
			setstring(kvpair->second, KYRTL_MAKE_GROUP, mkgroup);

			STRINGP pfx = getstring(kvpair->second.u.m_m, KYPREFIX);
			if (NULL == pfx)
				pfx = new STRING("(Unnamed)");

			fprintf(stderr, "WARNING: Creating a temporary group name for %s\n", pfx->c_str());
		}
		if (mksubd && mksubd->size() > 0) {
			fprintf(fp, "%sD := %s\n", mkgroup->c_str(),
				mksubd->c_str());
			fprintf(fp, "%s  := $(addprefix $(%sD)/,%s)\n",
				mkgroup->c_str(), mkgroup->c_str(),
				filstr.c_str());
		} else {
			fprintf(fp, "%s := %s\n\n", mkgroup->c_str(), filstr.c_str());
		}

		STRING	mkgroupref = STRING("$(")
					+ (*mkgroup)
					+ STRING(")");
		if (std::string::npos == allgroups.find(mkgroupref))
			allgroups = allgroups + STRING(" ") + mkgroupref;
		free(tokstr);
	}

	mksubd  = getstring(master, KYRTL_MAKE_SUBD);
	mkgroup  = getstring(master, KYRTL_MAKE_GROUP);
	if (NULL == mkgroup)
		mkgroup = new STRING(KYVFLIST);
	mkfiles = getstring(master, KYRTL_MAKE_FILES);
	mksubd  = getstring(master, KYRTL_MAKE_SUBD);
	if (NULL != mkfiles) {
		if (mksubd && mksubd->size() > 0) {
			fprintf(fp, "%sD := %s\n", mkgroup->c_str(),
					mksubd->c_str());
			fprintf(fp, "%s  := $(addprefix $(%sD)/,%s) \\\t%s\n",
				mkgroup->c_str(), mkgroup->c_str(),
				mkfiles->c_str(), allgroups.c_str());
		} else {
			fprintf(fp, "%s := %s //\t\t%s\n",
				mkgroup->c_str(), mkfiles->c_str(),
				allgroups.c_str());
		}
	} else if (allgroups.size() > 0) {
		fprintf(fp, "%s := main.v %s\n", mkgroup->c_str(), allgroups.c_str());
	}

	if (mksubd && mksubd->size() > 0) {
		bool	prior_dir = false;
		for(auto k : subdirs) {
			if (mksubd->compare(*k)==0) {
				prior_dir = true;
				break;
			}
		}
		if (!prior_dir)
			subdirs.push_back(mksubd);
	}

	for(auto k : subdirs)
		vdirs = vdirs + STRING(" -y ") + (*k);

	mksubd = getstring(master, KYRTL_MAKE_VDIRS);
	if (NULL == mksubd)
		mksubd = new STRING(KYAUTOVDIRS);
	fprintf(fp, "%s := %s\n", mksubd->c_str(), vdirs.c_str());
}
