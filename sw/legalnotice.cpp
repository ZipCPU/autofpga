////////////////////////////////////////////////////////////////////////////////
//
// Filename:	sw/legalnotice.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
// {{{
// Purpose:	Defines a routine that can be used to copy a legal notice
//		into a stream.  Modifications are made to the comments along
//	the way, as appropriate for the type of stream.  Specific modifications
//	include:
//
//	Places the name of the file at the top of the file, assuming a
//	Filename: key (as above) is found within the file.
//
//	Adds the project name from the KYPROJECT key to the Project: line
//	(as above) the project name with the KYPROJECT
//
//	Places a notice in the file that it was automatically generated.  This
//	notice includes the autofpga command line.
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

#include "mapdhash.h"
#include "parser.h"
#include "keys.h"
#include "kveval.h"
#include "msgs.h"

extern	FILE *open_in(MAPDHASH &info, const STRING &fname);

//
// Place a legal notice at the top of every file.  The legal notice is given
// to us as a filename, which we must then open and parse.
//
// We also do several substitutions here, given by the strings below.  This
// allows us to mark all of our produced files with their names within them,
// as well as marking the project that they are associated with.
//
void	legal_notice(MAPDHASH &info, FILE *fp, STRING &fname,
			const char *cline = NULL,
			const char *comment = "//") {
	char	line[512];
	FILE	*lglfp;
	STRING	str;

	STRINGP	legal_fname = getstring(info, str = KYLEGAL);
	if (!legal_fname) {
		fprintf(stderr, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license your project under the GPL\n");
		fprintf(fp, "%s WARNING: NO COPYRIGHT NOTICE\n\n%s Please be considerate and license this under the GPL\n", comment, comment);
		return;
	}

	lglfp = open_in(info, *legal_fname);
	if (!lglfp) {
		gbl_msg.error("Cannot open copyright notice file, %s\n", legal_fname->c_str());
		fprintf(fp, "%s WARNING: NO COPYRIGHT NOTICE\n\n"
			"%s Please be considerate and license this project under the GPL\n", comment, comment);
		perror("O/S Err:");
		return;
	}

	while(fgets(line, sizeof(line), lglfp)) {
		static const char	kyfname[] = "// Filename:",
					kyproject[] = "// Project:",
					kycmdline[] = "// CmdLine:",
					kycomment[] = "//",
					kycline[] = "///////////////////////////////////////////////////////////////////////";
		if (strncasecmp(line, kyfname, strlen(kyfname))==0) {
			fprintf(fp, "%s %s\t%s\n", comment, &kyfname[3],
				fname.c_str());
		} else if (strncasecmp(line, kyproject, strlen(kyproject))==0) {
			STRINGP	strp;
			if (NULL != (strp = getstring(info, KYPROJECT)))
				fprintf(fp, "%s %s\t%s\n", comment,
					&kyproject[3], strp->c_str());
			else
				fputs(line, fp);
		} else if (strncasecmp(line, kycmdline, strlen(kycmdline))==0){
			STRINGP	strp = getstring(info, KYCMDLINE);
			if (strp)
				fprintf(fp, "%s %s\t%s\n", comment,
					&kycmdline[3], strp->c_str());
			else {
				fprintf(fp, "%s\t(No command line data found)\n",
					kycmdline);
			}
		}else if((cline)&&(strncmp(line, kycline, strlen(kycline))==0)){
			fprintf(fp, "%s\n", cline);
		} else if (strncmp(line, kycomment, strlen(kycomment))==0) {
			fprintf(fp, "%s%s", comment, &line[strlen(kycomment)]);
		} else
			fputs(line, fp);
	}
}

