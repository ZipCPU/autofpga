////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	parser.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To parse an input file, and create a data structure of key-value
//		pairs from it.  Values of this structure may include other
//	key-value pair sub-structures.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mapdhash.h"
#include "parser.h"
#include "keys.h"

STRING	*rawline(FILE *fp) {
	char		linebuf[512];
	STRING		*r = new STRING;
	char		 *rv, *nl;

	do {
		rv = fgets(linebuf, sizeof(linebuf), fp);
		nl = strchr(linebuf, '\n');
		if (rv)
			r->append(linebuf);
	} while((rv)&&(!nl));

	// If we didn't get anything, such as on an end of file, return NULL
	// that way we can differentiate an EOF or error from an empty line.
	if ((!rv)&&(r->length()==0)) {
		delete r;
		return NULL;
	}

	return r;
}

STRING	*getline(FILE *fp) {
	STRING	*r;
	bool	comment_line;

	do {
		r = rawline(fp);
		comment_line = false;
		if ((r)&&(r->c_str()[0] == '#')) {
			if (isspace(r->c_str()[1]))
				comment_line = true;
			else if (r->c_str()[1] == '#')
				comment_line = true;
		}
	} while(comment_line);

	return r;
}

bool	iskeyline(STRING &s) {
	return (s[0] == '@');
}

MAPDHASH	*parsefile(FILE *fp) {
	STRING	key, value, *ln, prefix;
	MAPDHASH		*fm = new MAPDHASH, *devm = NULL;
	size_t		pos;

	key = ""; value = "";
	while(NULL != (ln = getline(fp))) {
		if (iskeyline(*ln)) {

			// We may have a completed key-value pair that needs
			// to be stored.
			if (key.length()>0) {
				if (key == KYPREFIX) {
					MAPDHASH::iterator	kvpair;
					kvpair = fm->find(value);
					if (kvpair == fm->end()) {
						MAPT	elm;
						STRINGP	kyp = trim(value);

						prefix = value;
						devm = new MAPDHASH;
						elm.m_typ = MAPT_MAP;
						elm.u.m_m = devm;

						fm->insert(KEYVALUE(STRING(*kyp),elm));
						delete kyp;
					} else if (kvpair->second.m_typ == MAPT_MAP) {
						devm = kvpair->second.u.m_m;
					} else {
						fprintf(stderr, "NAME-CONFLICT!!\n");
						exit(EXIT_FAILURE);
					}
				} if (devm)
					addtomap(*devm, key, value);
				else {
					addtomap(*fm, key, value);
				}
			}

			if ((pos = ln->find("="))
				&&(pos != STRING::npos)) {
				int	start, len;
				key   = ln->substr(1, pos-1);

				start = pos+1;
				while(isspace((*ln)[start]))
					start++;
				len = ln->length()-start;
				while((len>start)&&(isspace((*ln)[start+len-1])))
					len--;
				if (len > 0)
					value = ln->substr(start, len);
				else
					value = STRING("");
			} else {
				key = ln->substr(1, ln->length()-1);
				value = "";
			}

			STRING	*s = trim(key);
			key = *s;
			delete s;
		} else if (ln->c_str()[0]) {
			value = value + (*ln);
		}

		delete ln;
	} if (key.length()>0) {
		if (devm) {
			addtomap(*devm, key, value);
		} else {
			addtomap(*fm, key, value);
		}
	}

	return fm;
}

MAPDHASH	*parsefile(const char *fname) {
	FILE	*fp = fopen(fname, "r");
	if (fp == NULL) {
		fprintf(stderr, "ERR: Could not open %s\n", fname);
		return NULL;
	} MAPDHASH *map = parsefile(fp);
	fclose(fp);

	return map;
}

MAPDHASH	*parsefile(const STRING &fname) {
	return parsefile((const char *)fname.c_str());
}

