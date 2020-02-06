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
// Copyright (C) 2017-2020, Gisselquist Technology, LLC
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
#include "msgs.h"

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
	if (s.compare(0,3,STRING("@$("))==0)
		return false;
	return ((s[0] == '@')&&(s[1] != '@'));
}

MAPDHASH	*genhash(STRING &prefix) {
	MAPDHASH	*devm;
	MAPT	elm;

	devm = new MAPDHASH();
	elm.m_typ = MAPT_STRING;
	elm.u.m_s = trim(prefix);
	devm->insert(KEYVALUE(KYPREFIX,elm));

	return devm;
}

MAPDHASH	*gensubhash(MAPDHASH *top, STRING &prefix) {
	MAPDHASH	*devm = NULL;
	STRINGP	tmp = trim(prefix);

	// This isn't any old key, this is a
	// prefix key.  All keys following will
	// be placed in the hierarchy beneath
	// this key.
	MAPDHASH::iterator	kvpair;
	kvpair = top->find(*tmp);
	if (tmp->length() <= 0) {
		gbl_msg.fatal("EMPTY KEY\n");
	} else if (kvpair == top->end()) {
		STRING	kyp = STRING(*tmp);
		MAPT	elm;

		devm = genhash(kyp);

		elm.m_typ = MAPT_MAP;
		elm.u.m_m = devm;

		top->insert(KEYVALUE(kyp,elm));
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		devm = kvpair->second.u.m_m;
	} else {
		gbl_msg.fatal("NAME-CONFLICT!! (witin the same file, too!)\n");
	} delete tmp;
	return devm;
}

MAPDHASH	*parsefile(FILE *fp, const STRING &search);

void	process_keyvalue_pair(MAPDHASH *parent, const STRING &search, STRING &key, STRING &value) {

	if (key == KYINCLUDEFILE) {
		MAPDHASH	*submap,
				*plusmap;
		MAPDHASH::iterator subp;

		// Read and insert the given file here
		submap= parsefile(value.c_str(), search);

		// If the file was found, as with the data within it, then ...
		if (submap != NULL) {

			// Read this file, creating a hash
			subp = parent->find(KYPLUSDOT);

			// Declare everything within the file to be of the
			// sub-key "+" (KYPLUSDOT)
			//
			if (subp == parent->end()) {
				MAPT	elm;
				elm.m_typ = MAPT_MAP;
				plusmap = new MAPDHASH;
				elm.u.m_m = plusmap;
				parent->insert(KEYVALUE(KYPLUSDOT, elm));
			} else if (subp->second.m_typ != MAPT_MAP) {
				gbl_msg.error("KEY(+) EXISTS, AND ISN\'T A MAP\n");
			} else {
				plusmap = subp->second.u.m_m;
			} if (plusmap)
				mergemaps(*plusmap, *submap);
		} // Otherwise ... (*TRY*) to ignore the error
	} else // If its a normal key, just add it to the map
		addtomap(*parent, key, value);
}


MAPDHASH	*parsefile(FILE *fp, const STRING &search) {
	STRING		key, value, *ln, prefix;
	MAPDHASH	*fm = new MAPDHASH, *devm = NULL;
	size_t		pos;

	key = ""; value = "";
	while(NULL != (ln = getline(fp))) {
		if (iskeyline(*ln)) {

			// We may have a completed key-value pair that needs
			// to be stored.
			if (key.length() > 0) {

				// A key exists.  Let's store it.

				// If it's a PREFIX key, create a sub hash of
				// the FILE's hash
				if (key == KYPREFIX) {
					MAPDHASH	*sub;
					sub = gensubhash(fm, value);
					if (sub)
						devm = sub;
				}

				MAPDHASH	*parent;
				if (devm)
					parent = devm;
				else
					parent = fm;


				// Process the old key
				process_keyvalue_pair(parent, search, key, value);
			}

				//
			if ((pos = ln->find("="))
					&&(pos != STRING::npos)) {

				// Separate the key from its value
				key   = ln->substr(1, pos-1);

				// If we have a @KEY += line, then place the
				// plus in front of the key.  @+KEY is an
				// invalid key, so ... this should work.
				if (key.c_str()[(key.size())-1] == '+') {
					// Our key takes the + off the right,
					// and adds it to the left
					key = STRING("+")+key.substr(0,key.size()-1);
				}

				// Trim any whitespace from the key
				STRINGP	trimd;
				trimd = trim(key);
				key   = STRING(*trimd);
				delete	trimd;

				// Trim any whitespace from the value on this
				// line
				trimd = trim(ln->substr(pos+1));
				value = STRING(*trimd);
				delete	trimd;
			} else {
				// If we have a key with no "=" sign,
				// assume the whole line is the key.
				// Trim it up.
				key = ln->substr(1, ln->length()-1);
				STRINGP	trimd;
				trimd = trim(key);
				key   = STRING(*trimd);
				delete trimd;

				gbl_msg.warning("Key line with no =, key was %s\n", key.c_str());
				value = "";
			}

		} else if (ln->c_str()[0]) {
			value = value + (*ln);
		}

		delete ln;
	} if (key.length()>0) {
		if (key == KYPREFIX) {
			MAPDHASH	*sub;
			sub = gensubhash(fm, value);
			if (sub)
				devm = sub;
		}


		MAPDHASH	*parent;

		if (devm)
			parent = devm;
		else
			parent = fm;

		process_keyvalue_pair(parent, search, key, value);
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
	FILE	*fp = open_data_file(fname);

	if ((fp == NULL)&&(fname[0] != '/')&&(fname[0] != '.')) {
		STRING	copy = search;
		char	*sub = (char *)copy.c_str();
		char	*dir = strtok(sub, ", \t\n:");
		while(dir != NULL) {
			char	*full = new char[strlen(fname)+2+strlen(dir)];
			strcpy(full, dir);
			strcat(full, "/");
			strcat(full, fname);
			if (NULL != (fp=open_data_file(full))) {
				gbl_msg.info("Opened: %s\n", full);
				delete[] full;
				return fp;
			} delete[] full;
			dir = strtok(NULL, ", \t\n:");
		}

		gbl_msg.error("Could not open %s\nSearched through %s\n",
			fname, search.c_str());
		return NULL;
	}

	gbl_msg.info("Directly opened: %s\n", fname);
	return fp;
}

MAPDHASH	*parsefile(const char *fname, const STRING &search) {
	MAPDHASH	*map;
	FILE		*fp = search_and_open(fname, search);

	if (fp == NULL) {
		gbl_msg.error("PARSE-ERR: Could not open %s\n"
			"\t  Searched through: %s\n", fname, search.c_str());
		return NULL;
	} else {
		map = parsefile(fp, search);
		fclose(fp);

		return map;
	}
}

MAPDHASH	*parsefile(const STRING &fname, const STRING &search) {
	return parsefile((const char *)fname.c_str(), search);
}

