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

MAPT	operator+(MAPT a, MAPT b) {
	char	*sbuf;

	switch(a.m_typ) {
	case MAPT_STRING:
		switch(b.m_typ) {
		case MAPT_STRING:
			sbuf = new char[a.u.m_s->length()
					+b.u.m_s->length()+4];
			sprintf(sbuf, "%s %s", a.u.m_s->c_str(),
				b.u.m_s->c_str());
			a.u.m_s = new STRING(sbuf);
			delete sbuf;
			break;
		case	MAPT_INT:
			sbuf = new char[a.u.m_s->length()+32];
			sprintf(sbuf, "%s + %d", a.u.m_s->c_str(), b.u.m_v);
			a.u.m_s = new STRING(sbuf);
			delete sbuf;
			break;
		case	MAPT_AST:
			fprintf(stderr, "WARNING: Dont know how to add STRING to AST\n");
			break;
		default:
			fprintf(stderr, "WARNING: Dont know how to add STRING to other than INT or STRING\n");
		} break;
	case MAPT_INT:
		switch(b.m_typ) {
		case MAPT_STRING:
			a.m_typ = MAPT_AST;
			a.u.m_a = new AST_BRANCH(
				'+', new AST_NUMBER(a.u.m_v),
				parse_ast(*b.u.m_s));
			break;
		case MAPT_INT:
			// a.m_typ = MAPT_INT;
			a.u.m_v += b.u.m_v;
			break;
		case MAPT_AST:
			a.m_typ = MAPT_AST;
			a.u.m_a = new AST_BRANCH(
				'+', new AST_NUMBER(a.u.m_v),
				b.u.m_a);
			break;
		default:
			fprintf(stderr, "WARNING: Dont know how to add INT to a MAP\n");
		} break;
	case MAPT_AST:
		switch(b.m_typ) {
		case MAPT_STRING:
			a.u.m_a = new AST_BRANCH( '+', a.u.m_a,
				parse_ast(*b.u.m_s));
			break;
		case MAPT_INT:
			a.m_typ = MAPT_AST;
			a.u.m_a = new AST_BRANCH('+', a.u.m_a,
				new AST_NUMBER(b.u.m_v));
			break;
		case MAPT_AST:
			a.u.m_a = new AST_BRANCH('+', a.u.m_a, b.u.m_a);
			break;
		default:
			fprintf(stderr, "WARNING: Dont know how to add AST to a MAP\n");
		} break;
	default:
		fprintf(stderr, "1. DONT KNOW HOW TO ADD TWO MAPS TOGETHER (%d and %d)\n",
			a.m_typ, b.m_typ);
		if (a.m_typ == MAPT_MAP)
			mapdump(*a.u.m_m);
		if (b.m_typ == MAPT_MAP)
			mapdump(*b.u.m_m);
	}

	return a;
}

MAPT	operator+(MAPT a, const STRING b) {
	char	*sbuf;

	switch(a.m_typ) {
	case MAPT_STRING:
		sbuf = new char[a.u.m_s->length() +b.length()+4];
		sprintf(sbuf, "%s %s", a.u.m_s->c_str(), b.c_str());
		a.u.m_s = new STRING(sbuf);
		delete sbuf;
		break;
	case MAPT_INT:
		a.m_typ = MAPT_AST;
		a.u.m_a = new AST_BRANCH( '+', new AST_NUMBER(a.u.m_v),
			parse_ast(b));
		break;
	case MAPT_AST:
		a.u.m_a = new AST_BRANCH( '+', a.u.m_a, parse_ast(b));
		break;
	default:
		fprintf(stderr, "2. DONT KNOW HOW TO ADD A MAP TO A STRING (%d)\n", a.m_typ);
		mapdump(*a.u.m_m);
	}

	return a;
}
STRING	*trim(const STRING &s) {
	const char	*a, *b;
	STRINGP		strp;
	a = s.c_str();
	b = s.c_str() + s.length()-1;

	for(; (*a)&&(a<=b); a++)
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
	bool	astnode = false, pluskey = false;
	MAPT	subfm;
	MAPDHASH::iterator	subloc = fm.end();

	trimmed = trim(ky);
	if ((*trimmed)[0] == '@') {
		STRINGP	tmp = new STRING(trimmed->substr(1));
		delete trimmed;
		trimmed = tmp;
	} if ((*trimmed)[0] == '$') {
		astnode = true;
		STRINGP	tmp = new STRING(trimmed->substr(1));
		delete trimmed;
		trimmed = tmp;
	} if (((*trimmed)[0] == '+')&&((*trimmed)[1]!='.')) {
		pluskey = true;
		STRINGP	tmp = new STRING(trimmed->substr(1));
		delete trimmed;
		trimmed = tmp;
	}

	if (splitkey(*trimmed, mkey, subky)) {
		MAPDHASH::iterator	subloc = fm.find(mkey);
		delete	trimmed;

		if ((subloc == fm.end())&&(pluskey))
			subloc = fm.find(STRING("+")+mkey);
		else if (subloc == fm.end()) {
			subloc = fm.find(STRING("+")+mkey);
			if (subloc != fm.end()) {
				// REMOVE anything on conlict
				fm.erase(subloc);
				subloc = fm.end();
			}
		}

		if (subloc == fm.end()) {
			subfm.m_typ = MAPT_MAP;
			subfm.u.m_m = new MAPDHASH;
			fm.insert(KEYVALUE(mkey, subfm ) );
		} else {
			subfm = (*subloc).second;
		}

		if ((!pluskey)&&(subfm.m_typ != MAPT_MAP)) {
			return;
		} else if (subfm.m_typ == MAPT_MAP) {
			if (pluskey)
				subky = STRING("+") + subky;
			if (astnode)
				subky = STRING("$") + subky;
			addtomap(*subfm.u.m_m, subky, vl);
			return;
		}
	} else {
		subloc = fm.find(*trimmed);
		if ((subloc == fm.end())&&(pluskey))
			subloc = fm.find(STRING("+")+(*trimmed));
		if(subloc != fm.end()) {
			subfm = subloc->second;
		}
	}

	if ((astnode)&&(*trimmed != KYEXPR)) {
		if (subloc  == fm.end()) {
			MAPDHASH	*node;
			MAPT		elm;

			elm.m_typ = MAPT_MAP;
			elm.u.m_m = node = new MAPDHASH;
			fm.insert(KEYVALUE(*trimmed, elm));
			addtomap(*node, STRING("$")+STRING((pluskey)?"+":"")+KYEXPR, vl);
			return;
		} else if (subfm.m_typ == MAPT_MAP) {
			addtomap(*subfm.u.m_m, STRING("$")+STRING((pluskey)?"+":"")+KYEXPR, vl);
			return;
		}
	}

	MAPT	elm;
	if (pluskey) {
		if (subloc == fm.end()) {

			MAPT	elm;
			if (astnode) {
				elm.m_typ = MAPT_AST;
				elm.u.m_a = parse_ast(vl);
			} else
				elm.m_typ = MAPT_STRING;
			fm.insert(KEYVALUE(STRING("+")+(*trimmed), elm));
		} else {
			subloc->second = subfm + vl;
		}
	} else if (astnode) {
		if ((*trimmed)!=KYEXPR) {
			MAPDHASH	*node;
			elm.m_typ = MAPT_MAP;
			elm.u.m_m = node = new MAPDHASH;
			fm.insert(KEYVALUE(*trimmed, elm));
			addtomap(*node, STRING("$")+STRING((pluskey)?"+":"")+KYEXPR, vl);
		} else {
			elm.m_typ = MAPT_AST;
			elm.u.m_a = parse_ast(vl);
			if (NULL == elm.u.m_a)
				exit(EXIT_FAILURE);
			fm.insert(KEYVALUE(*trimmed, elm ) );
			if ((elm.u.m_a->isdefined())&&(*trimmed == KYEXPR)) {
				AST *ast = elm.u.m_a;
				elm.m_typ = MAPT_INT;
				elm.u.m_v = (int)ast->eval();
				fm.insert(KEYVALUE(KYVAL, elm));
			} delete trimmed;
			return;
		}
	} else if (subloc == fm.end()) {
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = new STRING(vl);
		fm.insert(KEYVALUE(*trimmed, elm ) );
	} else {
		subfm.m_typ = MAPT_STRING;
		subfm.u.m_s = new STRING(vl);
	}
	delete	trimmed;
}

void	mapdump_aux(FILE *fp, MAPDHASH &fm, int offset) {
	MAPDHASH::iterator	kvpair;

	for(kvpair = fm.begin(); kvpair != fm.end(); kvpair++) {
		fprintf(fp, "%*s%s: ", offset, "", (*kvpair).first.c_str());
		if ((*kvpair).second.m_typ == MAPT_MAP) {
			fprintf(fp, "\n");
			mapdump_aux(fp, *(*kvpair).second.u.m_m, offset+1);
		} else if ((*kvpair).second.m_typ == MAPT_INT) {
			fprintf(fp, "%d\n", (*kvpair).second.u.m_v);
		} else if ((*kvpair).second.m_typ == MAPT_AST) {
			AST	*ast = kvpair->second.u.m_a;
			fprintf(fp, "<AST>");
			if (ast->isdefined()) {
				fprintf(fp, "\tDEFINED and = %08lx\n", ast->eval());
			} else
				fprintf(fp, "\tNot defined\n");
			ast->dump(fp, offset+4);
		} else { // if ((*kvpair).second.m_typ == MAPT_STRING)
			STRINGP	s = (*kvpair).second.u.m_s;
			size_t	pos;
			if ((pos=s->find("\n")) == STRING::npos) {
				fprintf(fp, "%s\n", s->c_str());
			} else if (pos == s->length()-1) {
				fprintf(fp, "%s", s->c_str());
			} else	{
				fprintf(fp, "<Multi-line-String>\n");
				fprintf(fp, "\n\n----------------\n%s\n", s->c_str());
			}
		}
	}
}

void	mapdump(FILE *fp, MAPDHASH &fm) {
	fprintf(fp, "\n\nDUMPING!!\n\n");
	mapdump_aux(fp, fm, 0);
}

void	mapdump(MAPDHASH &fm) {
	mapdump_aux(stdout, fm, 0);
}

//
// Merge two maps, a master and a sub
//
void	mergemaps(MAPDHASH &master, MAPDHASH &sub) {
	MAPDHASH::iterator	kvmaster, kvsub;

	for(kvsub = sub.begin(); kvsub != sub.end(); kvsub++) {
		bool	pluskey;
		pluskey = (kvsub->first.c_str()[0] == '+');
		if (kvsub->second.m_typ == MAPT_MAP) {
			kvmaster = master.find(kvsub->first);
			if (kvmaster == master.end()) {
				// Not found
				master.insert(KEYVALUE((*kvsub).first,
					(*kvsub).second ) );
			} else if (kvmaster->second.m_typ == MAPT_MAP) {
				mergemaps(*kvmaster->second.u.m_m,
					*kvsub->second.u.m_m);
			} else {
				fprintf(stderr,
					"NAME CONFLICT!  Files not merged\n");
			}
		} else if (pluskey) {
			// Does the key already exist in the map?
			if(kvsub->first.length() > 1) {
				kvmaster = master.find(kvsub->first.substr(1));
				if (kvmaster == master.end())
					kvmaster = master.find(kvsub->first);
			} else
				kvmaster = master.find(kvsub->first);


			if (kvmaster == master.end())
				// No, this key doesn't exist.  Let's insert it
				master.insert(KEYVALUE(kvsub->first, kvsub->second));
			else
				kvmaster->second = kvmaster->second + kvsub->second;
		} else
			master.insert(KEYVALUE(kvsub->first, kvsub->second));
	}
}

/*
 * trimkey
 *
 * Given a key to a string within the current map level, trim all spaces from
 * that key.  Do NOT recurse if the key is not found.
 */
void	trimkey(MAPDHASH &mp, const STRING &sky) {
	MAPDHASH::iterator	kvpair, kvsecond;
	STRING	mkey, subky;

	if (splitkey(sky, mkey, subky)) {
		if (mp.end() != (kvpair=mp.find(mkey))) {
			if (kvpair->second.m_typ == MAPT_MAP) {
				trimkey(*kvpair->second.u.m_m, subky);
			}
		}
	} else for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
		if ((*kvpair).second.m_typ == MAPT_STRING) {
			if ((*kvpair).first == sky) {
				STRINGP	tmps = (*kvpair).second.u.m_s;
				(*kvpair).second.u.m_s = trim(*tmps);
				delete tmps;
			}
		}
	}
}

/*
 * trimall
 *
 * Find all keys of  the given name, then find the strings they refer to, then
 * trim any white space from those strings.
 */
void	trimall(MAPDHASH &mp, const STRING &sky) {
	MAPDHASH::iterator	kvpair, kvsecond;
	STRING	mkey, subky;

	if (splitkey(sky, mkey, subky)) {
		if (mp.end() != (kvpair=mp.find(mkey))) {
			if (kvpair->second.m_typ == MAPT_MAP) {
				// Cannot recurse further, else the key M.S
				// would cause keys M.*.S to be trimmed as well.
				// Hence we now force this to be an absolute
				// reference
				trimkey(*kvpair->second.u.m_m, subky);
			}
		}
	} for(kvpair = mp.begin(); kvpair != mp.end(); kvpair++) {
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

/*
 * trimbykeylist
 *
 * Look for all map elements with the given keylist, and apply trimall to
 * all elements within their respective maps
 */
void	trimbykeylist(MAPDHASH &mp, const STRING &kylist) {
	STRINGP	strp;

	if (NULL != (strp = getstring(mp, kylist))) {
		static const char delimiters[] = " \t\n,";
		STRING scpy = *strp;
		char	*tok = strtok((char *)scpy.c_str(), delimiters);

		while(NULL != tok) {
			trimall(mp, STRING(tok));
			tok = strtok(NULL, delimiters);
		}
	}
}

void	trimbykeylist(MAPT &elm, const STRING &kylist) {
	if (elm.m_typ == MAPT_MAP)
		trimbykeylist(*elm.u.m_m, kylist);
}

/*
 * cvtintbykeylist
 *
 * Very similar to trimbykeylist, save that in this case we use the keylist
 * to determine what to convert to integers.
 */
void	cvtintbykeylist(MAPDHASH &mp, const STRING &kylist) {
	STRINGP	strp;

	if (NULL != (strp = getstring(mp, kylist))) {
		static const char delimiters[] = " \t\n,";
		STRING scpy = *strp;
		char	*tok = strtok((char *)scpy.c_str(), delimiters);

		while(NULL != tok) {
			cvtint(mp, STRING(tok));
			tok = strtok(NULL, delimiters);
		}
	}
/*
	MAPDHASH::iterator	kvpair;
	for(kvpair=mp.begin(); kvpair != mp.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		cvtintbykeylist(kvpair->second, kylist);
	}
*/
}

void	cvtintbykeylist(MAPT &elm, const STRING &kylist) {
	if (elm.m_typ == MAPT_MAP)
		cvtintbykeylist(*elm.u.m_m, kylist);
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
	STRING	mkey, subky;
	MAPDHASH::iterator	result;

	if (splitkey(ky, mkey, subky)) {
		MAPDHASH::iterator	subloc = master.find(mkey);
		if (subloc == master.end()) {
			// Check any super classes for a definition of this key
			if ((master.end() != (subloc = master.find(KYPLUSDOT)))
					&&(subloc->second.m_typ == MAPT_MAP)) {
				result = findkey_aux(*subloc->second.u.m_m, ky, pre);
				if (result == subloc->second.u.m_m->end())
					return master.end();
				return result;
			} return subloc;
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

	resolve_ast_expressions(m);

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

STRINGP getstring(MAPT &m, const STRING &ky) {
	if (m.m_typ != MAPT_MAP)
		return NULL;
	return getstring(*m.u.m_m, ky);
}

void setstring(MAPDHASH &master, const STRING &ky, STRINGP strp) {
	MAPDHASH::iterator	kvpair, kvsub;

	kvpair = findkey(master, ky);
	if (kvpair == master.end()) {
		// The given key was not found in the hash
		STRING	mkey, subky;

		if (splitkey(ky, mkey, subky)) {
			MAPT	subfm;
			MAPDHASH::iterator	subloc = master.find(mkey);

			if (subloc == master.end()) {
				// Create a map to hold our value
				subfm.m_typ = MAPT_MAP;
				subfm.u.m_m = new MAPDHASH;
				master.insert(KEYVALUE(mkey, subfm ) );
			} else {
				// The map exists, let's reference it
				subfm = (*subloc).second;
				if (subfm.m_typ != MAPT_MAP) {
					fprintf(stderr, "ERR: MAP[%s] isnt a map\n", mkey.c_str());
					return;
				}
			} setstring(*subfm.u.m_m, subky, strp);
			return;
		}

		MAPT	elm;
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = strp;
		master.insert(KEYVALUE(ky, elm ) );
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		MAPDHASH *subhash = kvpair->second.u.m_m;
		kvsub = subhash->find(KYSTR);
		if (kvsub == subhash->end()) {
			MAPT	elm;
			elm.m_typ = MAPT_STRING;
			elm.u.m_s = strp;
			kvsub->second.u.m_m->insert(KEYVALUE(KYVAL, elm));
		} else {
			kvsub->second.m_typ = MAPT_STRING;
			kvsub->second.u.m_s = strp;
		}
	} else if (kvpair->second.m_typ == MAPT_STRING) {
		kvpair->second.u.m_s = strp;
	}
}

void	setstring(MAPT &m, const STRING &ky, STRINGP strp) {
	setstring(*m.u.m_m, ky, strp);
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
		STRING	mkey, subky;
		STRINGP	trimmed;

		trimmed = trim(ky);
		if ((*trimmed)[0] == '@') {
			STRINGP	tmp = new STRING(trimmed->substr(1,trimmed->size()));
			delete trimmed;
			trimmed = tmp;
		} if ((*trimmed)[0] == '$') {
			STRINGP	tmp = new STRING(trimmed->substr(1,trimmed->size()));
			delete trimmed;
			trimmed = tmp;
		}

		if (splitkey(*trimmed, mkey, subky)) {
			MAPT	subfm;
			MAPDHASH::iterator	subloc = master.find(mkey);
			delete	trimmed;
			if (subloc == master.end()) {
				// Create a map to hold our value
				subfm.m_typ = MAPT_MAP;
				subfm.u.m_m = new MAPDHASH;
				master.insert(KEYVALUE(mkey, subfm ) );
			} else {
				// The map exists, let's reference it
				subfm = (*subloc).second;
				if (subfm.m_typ != MAPT_MAP) {
					fprintf(stderr, "MAP[%s] isnt a map\n", mkey.c_str());
					return;
				}
			} setvalue(*subfm.u.m_m, subky, value);
			return;
		}

		MAPT	elm;
		elm.m_typ = MAPT_INT;
		elm.u.m_v = value;
		master.insert(KEYVALUE(*trimmed, elm ) );
		delete	trimmed;
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

void	flatten_maps(MAPDHASH &node, MAPDHASH &sub) {
	MAPDHASH::iterator	kvpair, nodepair;

	for(kvpair = sub.begin(); kvpair!=sub.end(); kvpair++) {
		nodepair = node.find(kvpair->first);
		if (nodepair == node.end()) {
			printf("Key not found, %s\n", kvpair->first.c_str());
			node.insert(KEYVALUE(kvpair->first, kvpair->second));
		} else if ((nodepair->second.m_typ==MAPT_MAP)&&(kvpair->second.m_typ == MAPT_MAP)) {
			flatten_maps(*nodepair->second.u.m_m, *kvpair->second.u.m_m);
		}
	}
}

void	flatten_aux(MAPDHASH &master, MAPDHASH &sub) {
	MAPDHASH::iterator	kvpair, kvsub;

	for(kvpair=sub.begin(); kvpair != sub.end(); kvpair++) {
		if ((KYPLUSDOT == kvpair->first)
				&&(kvpair->second.m_typ == MAPT_MAP)) {
			kvsub = kvpair->second.u.m_m->begin();
			if ((kvsub == kvpair->second.u.m_m->end())
					||(kvsub->second.m_typ != MAPT_MAP)) {
				flatten_maps(sub, *kvpair->second.u.m_m);
			} else
				flatten_aux(master, *kvsub->second.u.m_m);
		} else if (kvpair->first[0] == '/') {
			STRING	nkey = kvpair->first.substr(1);
			if (master.find(nkey) == master.end())
				master.insert(KEYVALUE(nkey, kvpair->second));
		} else if (kvpair->second.m_typ == MAPT_MAP)
			flatten_aux(master, *kvpair->second.u.m_m);
	}
}

void	flatten(MAPDHASH &master) {
	flatten_aux(master, master);
}

