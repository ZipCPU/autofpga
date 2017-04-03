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

STRING	*trim(STRING &s) {
	const char	*a, *b;
	STRINGP		strp;
	a = s.c_str();
	b = s.c_str() + s.length()-1;

printf("Trimming s.c_str() = %s\n", s.c_str());

	for(; (*a)&&(a<b); a++)
		if (!isspace(*a))
			break;
	for(; (b>a); b--)
		if (!isspace(*b))
			break;
	strp = new STRING(s.substr(a-s.c_str(), b-a+1));
	printf("TRIM\'d to: %s\n", strp->c_str());
	return strp;
}

bool	splitkey(const STRING &ky, STRING &mkey, STRING &subky) {
	size_t	pos;

	if (((pos=ky.find('.')) != STRING::npos)
			&&(pos != 0)
			&&(pos < ky.length()-1)) {
		mkey = ky.substr(0,pos);
		subky = ky.substr(pos+1,ky.length()-pos+1);
		assert(subky.length() > 0);
		return true;
	} return false;
}

void	addtomap(MAPDHASH &fm, STRING ky, STRING vl) {
	STRING	mkey, subky;

	if (splitkey(ky, mkey, subky)) {
		MAPT	subfm;
		MAPDHASH::iterator	subloc = fm.find(mkey);
		if (subloc == fm.end()) {
			subfm.m_typ = MAPT_MAP;
			subfm.u.m_m = new MAPDHASH;
			fm.insert(KEYVALUE(mkey, subfm ) );
		} else {
			subfm = (*subloc).second;
			if (subfm.m_typ != MAPT_MAP) {
				fprintf(stderr, "MAP[%s] isnt a map\n", mkey.c_str());
				return;
			}
		}
		addtomap(*subfm.u.m_m, subky, vl);
		return;
	}

	MAPT	elm;
	elm.m_typ = MAPT_STRING;
	elm.u.m_s = new STRING(vl);
	fm.insert(KEYVALUE(ky, elm ) );
}

MAPDHASH	*parsefile(FILE *fp) {
	STRING	key, value, *ln, prefix;
	MAPDHASH		*fm = new MAPDHASH, *devm = NULL;
	size_t		pos;

	key = ""; value = "";
	while(ln = getline(fp)) {
		if (iskeyline(*ln)) {

			// We may have a completed key-value pair that needs
			// to be stored.
			if (key.length()>0) {
				if (key == KYPREFIX) {
					MAPDHASH::iterator	kvpair;
					kvpair = fm->find(value);
					if (kvpair == fm->end()) {
						MAPT	elm;

						prefix = value;
						devm = new MAPDHASH;
						elm.m_typ = MAPT_MAP;
						elm.u.m_m = devm;

						fm->insert(KEYVALUE(value,elm));
					} else if (kvpair->second.m_typ == MAPT_MAP) {
						devm = kvpair->second.u.m_m;
					} else {
						fprintf(stderr, "NAME-CONFLICT!!\n");
						exit(EXIT_FAILURE);
					}
				} if (devm)
					addtomap(*devm, key, value);
				else
					addtomap(*fm, key, value);
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
	} if ((key.length()>0)&&(devm)) {
		addtomap(*devm, key, value);
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

void	mapdump_aux(MAPDHASH &fm, int offset) {
	MAPDHASH::iterator	kvpair;

	for(kvpair = fm.begin(); kvpair != fm.end(); kvpair++) {
		printf("%*s%s: ", offset, "", (*kvpair).first.c_str());
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			printf("\n");
			mapdump_aux(*(*kvpair).second.u.m_m, offset+1);
		} else if ((*kvpair).second.m_typ == MAPT_INT) {
			printf("%d\n", (*kvpair).second.u.m_v);
		} else { // if ((*kvpair).second.m_typ == MAPT_STRING)
			STRINGP	s = (*kvpair).second.u.m_s;
			size_t	pos;
			if ((pos=s->find("\n")) == STRING::npos) {
				printf("%s\n", s->c_str());
			} else if (pos == s->length()-1) {
				printf("%s", s->c_str());
			} else	{
				printf("<Multi-line-String>\n");
				printf("\n\n----------------\n%s\n", s->c_str());
			}
		}
	}
}

void	mapdump(MAPDHASH &fm) {
	printf("\n\nDUMPING!!\n\n");
	mapdump_aux(fm, 0);
}

//
// Merge two maps, a master and a sub
//
void	mergemaps(MAPDHASH &master, MAPDHASH &sub) {
	MAPDHASH::iterator	kvpair, kvsub;

	for(kvpair = sub.begin(); kvpair != sub.end(); kvpair++) {
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			kvsub = master.find(kvpair->first);
			if (kvsub == master.end()) {
				// Not found
				master.insert(KEYVALUE((*kvpair).first,
					(*kvpair).second ) );
			} else if (kvsub->second.m_typ == MAPT_MAP) {
				mergemaps(*kvsub->second.u.m_m,
					*kvpair->second.u.m_m);
			} else {
				fprintf(stderr,
					"NAME CONFLICT!  Files not merged\n");
			}
		}
	}
}

void	trimall(MAPDHASH &mp, const STRING &sky) {
	MAPDHASH::iterator	kvpair;
	STRING	mkey, subky;

	if (splitkey(sky, mkey, subky)) {
		if (mp.end() != (kvpair=mp.find(mkey))) {
			if (kvpair->second.m_typ == MAPT_MAP) {
				trimall(*kvpair->second.u.m_m, subky);
			}
		} else for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
			if ((*kvpair).second.m_typ == MAPT_MAP) {
				trimall(*(*kvpair).second.u.m_m, sky);
			}
		}
	} else for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			trimall(*(*kvpair).second.u.m_m, sky);
		} else if ((*kvpair).second.m_typ == MAPT_STRING) {
			if ((*kvpair).first == sky) {
				STRINGP	tmps = (*kvpair).second.u.m_s;
				(*kvpair).second.u.m_s = trim(*tmps);
				delete tmps;
			}
		}
	}
}

void	cvtint(MAPDHASH &mp, const STRING &sky) {
	MAPDHASH::iterator	kvpair;

	for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			cvtint(*(*kvpair).second.u.m_m, sky);
		} else if ((*kvpair).second.m_typ == MAPT_STRING) {
			if ((*kvpair).first == sky) {
				STRINGP	tmps = (*kvpair).second.u.m_s;
				(*kvpair).second.m_typ = MAPT_INT;
				(*kvpair).second.u.m_v=strtoul(tmps->c_str(), NULL, 0);
				delete tmps;
			}
		}
	}
}

MAPDHASH::iterator findkey_aux(MAPDHASH &master, const STRING &ky, const STRING &pre) {
	MAPDHASH::iterator	result;
	size_t	pos;

	// printf("Searching for %s (from %s)\n", ky.c_str(), pre.c_str());
	if (((pos=ky.find('.')) != STRING::npos)&&(pos != 0)
			&&(pos < ky.length()-1)) {
		STRING	mkey = ky.substr(0,pos),
			subky = ky.substr(pos+1,ky.length()-pos+1);
		assert(subky.length() > 0);

		MAPDHASH::iterator	subloc = master.find(mkey);
		if (subloc == master.end()) {
			return subloc;
		} else {
			MAPT	subfm;
			subfm = (*subloc).second;

			if (subfm.m_typ != MAPT_MAP) {
				fprintf(stderr, "ERR: MAP[%s.%s] isnt a map\n",
					pre.c_str(), mkey.c_str());
				// assert(subfm->m_typ == MAPT_MAP)
				return master.end();
			}

			// printf("Recursing on %s\n", mkey.c_str());
			STRING	new_pre_key = pre+"."+mkey;
			result = findkey_aux(*subfm.u.m_m, subky, new_pre_key);
			if (result == subfm.u.m_m->end()) {
				// printf("Subkey not found\n");
				return master.end();
			}
			return result;
		}
	}

	result = master.find(ky);
	// if (result != master.end())
	//	printf("FOUND\n");
	// else	printf("Not found\n");
	return	result;
}

MAPDHASH::iterator findkey(MAPDHASH &master, const STRING &ky) {
	STRING	empty("");
	return findkey_aux(master, ky, empty);
}

STRINGP getstring(MAPDHASH &master, const STRING &ky) {
	MAPDHASH::iterator	r;

	r = findkey(master, ky);
	if (r == master.end())
		return NULL;
	else if (r->second.m_typ != MAPT_STRING)
		return NULL;
	return r->second.u.m_s;
}

bool getvalue(MAPDHASH &master, const STRING &ky, int &value) {
	MAPDHASH::iterator	r;

	value = -1;

	r = findkey(master, ky);
	if (r == master.end())
		return false;
	else if (r->second.m_typ != MAPT_MAP) {
		MAPDHASH::iterator	kvpair;
		kvpair = r->second.u.m_m->find(STRING("VAL"));
		if (kvpair != r->second.u.m_m->end())
			r = kvpair;
		else
			return false;
	} else if (r->second.m_typ != MAPT_INT) {
		return false;
	}
	value = r->second.u.m_v;
	return true;
}

void	setvalue(MAPDHASH &master, const STRING &ky, int value) {
	MAPDHASH::iterator	kvpair, kvsub;

	kvpair = master.find(ky);
	if (kvpair == master.end()) {
		MAPT	elm;
		elm.m_typ = MAPT_INT;
		elm.u.m_v = value;
		master.insert(KEYVALUE(ky, elm));
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		MAPDHASH *subhash = kvpair->second.u.m_m;
		kvsub = subhash->find(KYVAL);
		if (kvsub == subhash->end()) {
			MAPT	elm;
			elm.m_typ = MAPT_INT;
			elm.u.m_v = value;
			kvsub->second.u.m_m->insert(KEYVALUE(KYVAL, elm));
		} else {
			kvsub->second.m_typ = MAPT_INT;
			kvsub->second.u.m_v = value;
		}
	}
}
