////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	mapdhash.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To handle processing our key-value pair data structure.
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
#include "ast.h"
#include "kveval.h"

STRING	*trim(STRING &s) {
	const char	*a, *b;
	STRINGP		strp;
	a = s.c_str();
	b = s.c_str() + s.length()-1;

	for(; (*a)&&(a<b); a++)
		if (!isspace(*a))
			break;
	for(; (b>a); b--)
		if (!isspace(*b))
			break;
	strp = new STRING(s.substr(a-s.c_str(), b-a+1));
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
	STRINGP	trimmed;
	bool	astnode = false;

	trimmed = trim(ky);
	if ((*trimmed)[0] == '@') {
		STRINGP	tmp = new STRING(trimmed->substr(1,trimmed->size()));
		delete trimmed;
		trimmed = tmp;
	} if ((*trimmed)[0] == '$') {
		astnode = true;
		STRINGP	tmp = new STRING(trimmed->substr(1,trimmed->size()));
		delete trimmed;
		trimmed = tmp;
	}

	if (splitkey(*trimmed, mkey, subky)) {
		MAPT	subfm;
		MAPDHASH::iterator	subloc = fm.find(mkey);
		delete	trimmed;
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
		if (astnode)
			subky = STRING("$") + subky;
		addtomap(*subfm.u.m_m, subky, vl);
		return;
	}

	MAPT	elm;
	if (astnode) {
		elm.m_typ = MAPT_AST;
		elm.u.m_a = parse_ast(vl);
		if (elm.u.m_a->isdefined()) {
			AST *ast = elm.u.m_a;
			elm.m_typ = MAPT_INT;
			long	v = ast->eval();
			elm.u.m_v = (int)v;
			delete	ast;
		}
	} else {
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = new STRING(vl);
	}
	fm.insert(KEYVALUE(*trimmed, elm ) );
	delete	trimmed;
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
		} else if ((*kvpair).second.m_typ == MAPT_AST) {
			AST	*ast = kvpair->second.u.m_a;
			printf("<AST>");
			if (ast->isdefined()) {
				printf("\tDEFINED and = %08lx\n", ast->eval());
			} else
				printf("\tNot defined\n");
			ast->dump(offset+4);
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
		} else
			master.insert(KEYVALUE(kvpair->first, kvpair->second));
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
	STRING	mkey, subky;

	if (splitkey(sky, mkey, subky)) {
		if (mp.end() != (kvpair=mp.find(mkey))) {
			if (kvpair->second.m_typ == MAPT_MAP) {
				cvtint(*kvpair->second.u.m_m, subky);
			}
		} else for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
			if (kvpair->second.m_typ == MAPT_MAP) {
				cvtint(*kvpair->second.u.m_m, sky);
			}
		}
	} else for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
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

MAPDHASH *getmap(MAPDHASH &master, const STRING &ky) {
	MAPDHASH::iterator	r;

	r = findkey(master, ky);
	if (r == master.end())
		return NULL;
	else if (r->second.m_typ != MAPT_MAP)
		return NULL;
	return r->second.u.m_m;
}

STRINGP getstring(MAPDHASH &m) {
	MAPDHASH::iterator	r;

	resolve(m);

	// Can we build this string?
	r = m.find(KYSTR);
	if (r != m.end()) {
		if (r->second.m_typ == MAPT_STRING)
			return r->second.u.m_s;
	} return NULL;
}

STRINGP getstring(MAPDHASH &master, const STRING &ky) {
	MAPDHASH::iterator	r;

	r = findkey(master, ky);
	if (r == master.end())
		return NULL;
	else if (r->second.m_typ == MAPT_MAP) {
		MAPDHASH	*m = r->second.u.m_m;

		// Check for a .STR key within this
		r = m->find(KYSTR);
		if (r == m->end()) {
			return getstring(master);
		} if (r->second.m_typ != MAPT_STRING) {
			fprintf(stderr, "ERR: STRING expression isnt a string!! (KEY=%s)\n", ky.c_str());
			return NULL;
		}
	} else if (r->second.m_typ != MAPT_STRING) {
		fprintf(stderr, "ERR: STRING expression isnt a string!! (KEY=%s)\n", ky.c_str());
		return NULL;
	}
	return r->second.u.m_s;
}

bool getvalue(MAPDHASH &map, int &value) {
	MAPDHASH::iterator	kvpair;

	kvpair = map.find(KYVAL);
	if (kvpair != map.end()) {
		value = kvpair->second.u.m_v;
		return (kvpair->second.m_typ == MAPT_INT);
	}
	kvpair = map.find(KYEXPR);
	if (kvpair == map.end())
		return false;

	if (kvpair->second.m_typ == MAPT_AST) {
		AST	*ast;
		ast = kvpair->second.u.m_a;
		if (ast->isdefined()) {
			kvpair->second.m_typ = MAPT_INT;
			kvpair->second.u.m_v = ast->eval();
			delete ast;
		} else
			return false;
	}

	if (kvpair->second.m_typ == MAPT_INT) {
		/*
		MAPT	elm;
		elm.m_typ = MAPT_INT;
		elm.u.m_v = kvpair->second.u.m_v;
		map.insert(KEYVALUE(KYVAL, elm));
		value = elm.u.m_v;
		*/
		value=kvpair->second.u.m_v;
		return true;
	} return false;
}

bool getvalue(MAPDHASH &master, const STRING &ky, int &value) {
	MAPDHASH::iterator	r;

	value = -1;

	r = findkey(master, ky);
	if (r == master.end()) {
		return false;
	} else if (r->second.m_typ == MAPT_MAP) {
		return getvalue(*r->second.u.m_m, value);
	} else if (r->second.m_typ == MAPT_AST) {
		AST	*ast = r->second.u.m_a;
		if (ast->isdefined()) {
			r->second.m_typ = MAPT_INT;
			r->second.u.m_v = ast->eval();
			delete ast;
		} else
			return false;
	} else if (r->second.m_typ == MAPT_STRING) {
		value = strtoul(r->second.u.m_s->c_str(), NULL, 0);
		return true;
	} else if (r->second.m_typ != MAPT_INT) {
		return false;
	}
	value = r->second.u.m_v;
	return true;
}

void	setvalue(MAPDHASH &master, const STRING &ky, int value) {
	MAPDHASH::iterator	kvpair, kvsub;

	kvpair = findkey(master, ky);
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
