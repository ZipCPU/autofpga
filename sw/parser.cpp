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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "mapdhash.h"
#include "parser.h"
#include "keys.h"

extern	int	gbl_err;

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
	return ((s[0] == '@')&&(s[1] != '@'));
}

MAPDHASH	*parsefile(FILE *fp, const STRING &search) {
	STRING	key, value, *ln, prefix;
	MAPDHASH	*fm = new MAPDHASH, *devm = NULL;
	size_t		pos;

	key = ""; value = "";
	while(NULL != (ln = getline(fp))) {
		if (iskeyline(*ln)) {

			// We may have a completed key-value pair that needs
			// to be stored.
			if (key.length()>0) {
				// A key exists.  Let's store it.
				if (key == KYPREFIX) {
					// This isn't any old key, this is a
					// prefix key.  All keys following will
					// be placed in the hierarchy beneath
					// this key.
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
						elm.m_typ = MAPT_STRING;
						elm.u.m_s = kyp;
						devm->insert(KEYVALUE(KYPREFIX,elm));
					} else if (kvpair->second.m_typ == MAPT_MAP) {
						devm = kvpair->second.u.m_m;
					} else {
						fprintf(stderr, "NAME-CONFLICT!!\n");
						exit(EXIT_FAILURE);
					}
				}

				{
					MAPDHASH	*parent;
					if (devm)
						parent = devm;
					else
						parent = fm;

					if (key == KYINCLUDEFILE) {
						MAPDHASH	*submap,
								*plusmap;
						MAPDHASH::iterator subp;
						submap = parsefile(value.c_str(), search);
						if (submap != NULL) {
						subp = parent->find(KYPLUSDOT);
						if (subp == parent->end()) {
							MAPT	elm;
							elm.m_typ = MAPT_MAP;
							plusmap = new MAPDHASH;
							elm.u.m_m = plusmap;
							parent->insert(KEYVALUE(KYPLUSDOT, elm));
						} else if (subp->second.m_typ != MAPT_MAP) {
							fprintf(stderr, "ERR: KEY(+) EXISTS, AND ISN\'T A MAP\n");
							// exit(EXIT_FAILURE);
							gbl_err++;
						} else {
							plusmap = subp->second.u.m_m;
						} if (plusmap)
							mergemaps(*plusmap, *submap);
						}
					} else {
						addtomap(*parent, key, value);
						// mapdump(*devm);
					}
				}
			}

			if ((pos = ln->find("="))
				&&(pos != STRING::npos)) {
				key   = ln->substr(1, pos-1);

				if (key.c_str()[(key.size())-1] == '+') {
					key = STRING("+")+key.substr(0,key.size()-1);
				}

				STRINGP	trimd;
				trimd =trim(ln->substr(pos+1));
				value =STRING(*trimd);
				delete	trimd;
			} else {
				key = ln->substr(1, ln->length()-1);
				STRING	*s = trim(key);
				key = *s;
				delete s;
				fprintf(stderr, "WARNING: Key line with no =, key was %s\n", key.c_str());
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
		MAPDHASH	*parent;
		if (devm)
			parent = devm;
		else
			parent = fm;

		if (key == KYINCLUDEFILE) {
			MAPDHASH	*submap,
					*plusmap = NULL;
			MAPDHASH::iterator subp;
			submap = parsefile(value.c_str(), search);
			if (submap != NULL) {
				subp = parent->find(KYPLUSDOT);
				if (subp == parent->end()) {
					MAPT	elm;
					elm.m_typ = MAPT_MAP;
					plusmap = new MAPDHASH;
					elm.u.m_m = plusmap;
					parent->insert(KEYVALUE(KYPLUSDOT, elm));
				} else if (subp->second.m_typ != MAPT_MAP) {
					fprintf(stderr, "ERR: KEY(+) EXISTS, AND ISN\'T A MAP\n");
					// exit(EXIT_FAILURE);
					gbl_err++;
				} else {
					plusmap = subp->second.u.m_m;
				} if (plusmap)
					mergemaps(*plusmap, *submap);
			}
		} else {
			addtomap(*parent, key, value);
			// mapdump(*devm);
		}
	}

	return fm;
}

FILE	*open_data_file(const char *fname) {
	struct	stat	sb;
	if ((access(fname, R_OK)!=0)||(stat(fname, &sb) != 0)) {
		return NULL;
	} else {
		if (sb.st_mode & S_IFREG)
			return fopen(fname, "r");
	} return NULL;
}

FILE	*search_and_open(const char *fname, const STRING &search) {
	extern	FILE *gbl_dump;
	FILE	*fp = open_data_file(fname);

	if (fp == NULL) {
		STRING	copy = search;
		char	*sub = (char *)copy.c_str();
		char	*dir = strtok(sub, ", \t\n:");
		while(dir != NULL) {
			char	*full = new char[strlen(fname)+2+strlen(dir)];
			strcpy(full, dir);
			strcat(full, "/");
			strcat(full, fname);
			if (NULL != (fp=open_data_file(full))) {
				if (gbl_dump)
					fprintf(gbl_dump, "Opened: %s\n", full);
				delete[] full;
				return fp;
			} delete[] full;
			dir = strtok(NULL, ", \t\n:");
		}

		fprintf(stderr, "ERR: Could not open %s\nSearched through %s\n",
			fname, search.c_str());
		gbl_err ++;
		// exit(EXIT_FAILURE);
		return NULL;
	}

	if (gbl_dump)
		fprintf(gbl_dump, "Directly opened: %s\n", fname);
	return fp;
}

MAPDHASH	*parsefile(const char *fname, const STRING &search) {
	MAPDHASH	*map;
	FILE		*fp = search_and_open(fname, search);

	if (fp == NULL) {
		fprintf(stderr, "PARSE-ERR: Could not open %s\n", fname);
		gbl_err++;
		return NULL;
		// exit(EXIT_FAILURE);
	} else {
		map = parsefile(fp, search);
		fclose(fp);

		return map;
	}
}

MAPDHASH	*parsefile(const STRING &fname, const STRING &search) {
	return parsefile((const char *)fname.c_str(), search);
}

