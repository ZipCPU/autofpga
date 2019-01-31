////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	ifdefs.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Components we have can contain an ACCESS keyword that can then
//		be used by logic to determine if the component exists or not.
//
//	These ACCESS lines may also depend upon the ACCESS lines of other
//	components.
//
//	main.v		Gets all of our work.  The ACCESS lines are used at
//			the top of this component to turn things on (or off)
//
//	toplevel.v	Doesn't get touched.  If you include your component
//			in the autofpga list, it will define external ports
//			for it, independent of the ACCESS lines.
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

extern	bool	isperipheral(MAPT &pmap);
extern	bool	isperipheral(MAPDHASH &phash);

void	build_access_ifdefs_v(MAPDHASH &master, FILE *fp) {
	MAPDHASH::iterator	kvpair;
	STRING		already_defined;
	MAPDHASH	dephash;

	fprintf(fp,
"//\n"
"//\n"
"// Here is a list of defines which may be used, post auto-design\n"
"// (not post-build), to turn particular peripherals (and bus masters)\n"
"// on and off.  In particular, to turn off support for a particular\n"
"// design component, just comment out its respective `define below.\n"
"//\n"
"// These lines are taken from the respective @ACCESS tags for each of our\n"
"// components.  If a component doesn\'t have an @ACCESS tag, it will not\n"
"// be listed here.\n"
"//\n");
	fprintf(fp, "// First, the independent access fields for any bus masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		STRINGP	dep, accessp;
		dep = getstring(*kvpair->second.u.m_m, KYDEPENDS);
		accessp = getstring(*kvpair->second.u.m_m, KYACCESS);
		if (NULL == accessp)
			continue;
		if (NULL != dep) {
			dephash.insert(*kvpair);
		} else {
			fprintf(fp, "`define\t%s\n", accessp->c_str());
			already_defined = already_defined + " " + (*accessp);
		}
	}

	fprintf(fp, "// And then for the independent peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		STRINGP	dep, accessp;
		dep = getstring(kvpair->second, KYDEPENDS);
		accessp = getstring(kvpair->second, KYACCESS);
		if (NULL == accessp)
			continue;
		else if (NULL != dep) {
			dephash.insert(*kvpair);
		} else {
			fprintf(fp, "`define\t%s\n", accessp->c_str());
			already_defined = already_defined + " " + STRING(*accessp);
		}
	}

	if (dephash.begin() != dephash.end()) {
		fprintf(fp, "//\n//\n// The list of those things that have @DEPENDS tags\n//\n//\n");
		fprintf(fp,
"// Dependencies are listed within the @DEPENDS tag\n"
"// Values prefixed by a !, yet with no spaces between the ! and the\n"
"// dependency, are ifndef dependencies.  As an example, an\n"
"// an access and depends tag such as:\n"
"//\n"
"// @ACCESS=  THIS_COMPONENT\n"
"// @DEPENDS= MUST_HAVE_A !MUST_NOT_HAVE_B\n"
"//\n"
"// will turn into:\n"
"//\n"
"// `ifdef MUST_HAVE_A\n"
"// `ifndef MUST_NOT_HAVE_B\n"
"// `define THIS_COMPONENT\n"
"// `endif // MUST_NOT_HAVE_B\n"
"// `endif // MUST_HAVE_A\n"
"//\n");

		bool	done;
		do {
			done = true;
			STRING	depstr, endstr;

			for(kvpair=dephash.begin(); kvpair != dephash.end(); kvpair++) {
				const char	DELIMITERS[] = ", \t\n";
				if (kvpair->second.m_typ != MAPT_MAP)
					continue;
				STRINGP	dep, accessp;
				dep = getstring(kvpair->second, KYDEPENDS);
				accessp = getstring(kvpair->second, KYACCESS);


				bool	depsmet = true;
				char	*deplist, *dependency;
				deplist = strdup(dep->c_str());

				depstr = "";
				endstr = "";

				dependency = strtok(deplist, DELIMITERS);
				while(dependency) {
					char	*rawdep;

					rawdep = dependency;
					STRING	mstr = STRING(" ")+STRING(rawdep)
						+STRING(" ");
					if (NULL == strstr(already_defined.c_str(),
							mstr.c_str())) {
						depsmet = false;
						break;
					}

					depstr += STRING("`ifdef\t")
						+STRING(rawdep)
						+STRING("\n");
					endstr += STRING("`endif\n");
					dependency = strtok(NULL, DELIMITERS);
				}


				if (depsmet) {
					fprintf(fp, "%s", depstr.c_str());
					fprintf(fp, "`define\t%s\n", accessp->c_str());
					fprintf(fp, "%s", endstr.c_str());

					already_defined = already_defined + STRING(" ")
						+ (*accessp) + STRING(" ");
					dephash.erase(kvpair);
					kvpair = dephash.begin();
					if (kvpair == dephash.end())
						break;

					// We changed something, so ...
					done = false;
				}
			}


			if (dephash.begin() == dephash.end())
				done = true;
		} while(!done);
	}

	if (dephash.begin() != dephash.end()) {
		fprintf(fp, "// The following have unmet dependencies.  They are listed\n"
			"// here for reference, but their dependencies cannot be met.\n");


		for(kvpair=dephash.begin(); kvpair != dephash.end(); kvpair++) {
			const char	DELIMITERS[] = ", \t\n";
			if (kvpair->second.m_typ != MAPT_MAP)
				continue;
			STRINGP	dep, accessp;
			dep = getstring(kvpair->second, KYDEPENDS);
			accessp = getstring(kvpair->second, KYACCESS);


			char	*deplist, *dependency;
			deplist = strdup(dep->c_str());

			STRING	depstr, endstr;

			depstr = "";
			endstr = "";

			dependency = strtok(deplist, DELIMITERS);
			while(dependency) {
				depstr += STRING("`ifdef\t")
					+STRING(dependency)
					+STRING("\n");
				endstr += STRING("`endif\n");
				dependency = strtok(NULL, DELIMITERS);
			}

			fprintf(fp, "%s", depstr.c_str());
			fprintf(fp, "`define\t%s\n", accessp->c_str());
			fprintf(fp, "%s\n", endstr.c_str());
		}
	}

	fprintf(fp, "//\n// End of dependency list\n//\n//\n");
}
