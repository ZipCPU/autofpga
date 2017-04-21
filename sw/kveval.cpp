////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	kveval.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	
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
#include "kveval.h"
#include "keys.h"
#include "ast.h"

bool	get_named_value(MAPDHASH &top, MAPDHASH &here, STRING &key, int &value){
	MAPDHASH::iterator	kvpair, kvsub;

	value = -1;

	if (strncmp(key.c_str(), KYTHISDOT.c_str(), KYTHISDOT.size())==0) {
		STRING	subkey = key.substr(KYTHISDOT.size(),
					key.size()-KYTHISDOT.size());
		kvpair = findkey(here, subkey);
		if (kvpair == here.end())
			return false;
	} else {
		kvpair = findkey(top, key);
		if (kvpair == top.end())
			return false;
	}

	if (kvpair->second.m_typ == MAPT_INT) {
		value = kvpair->second.u.m_v;
		return true; // Found it!
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		return (getvalue(*kvpair->second.u.m_m, value));
	} return false;
}

STRINGP	get_named_string(MAPDHASH &top, MAPDHASH &here, STRING &key){
	MAPDHASH::iterator	kvpair, kvsub, kvstr, kvfmt;

	if (strncmp(key.c_str(), KYTHISDOT.c_str(), KYTHISDOT.size())==0) {
		STRING	subkey = key.substr(KYTHISDOT.size(),
					key.size()-KYTHISDOT.size());
		kvpair = findkey(here, subkey);
		if (kvpair == here.end())
			return NULL;
	} else {
		kvpair = findkey(top, key);
		if (kvpair == top.end());
			return NULL;
	}

	if (kvpair->second.m_typ == MAPT_STRING) {
		return kvpair->second.u.m_s; // Found it!
	} else if (kvpair->second.m_typ == MAPT_MAP) {
		// Need to dig within the map created for this value to get this
		return getstring(*kvpair->second.u.m_m);
	} return NULL;
}

void	expr_eval(MAPDHASH &top, MAPDHASH &here, MAPDHASH &sub, MAPT &expr) {
	AST	*ast;
	if (expr.m_typ == MAPT_STRING) {
		ast = parse_ast(*expr.u.m_s);
		ast->define(top, here);
		if (ast->isdefined()) {
			expr.m_typ = MAPT_INT;
			expr.u.m_v = ast->eval();
			delete	ast;
		} else {
			expr.m_typ = MAPT_AST;
			expr.u.m_a = ast;
		}
	} else if (expr.m_typ == MAPT_AST) {
		ast = expr.u.m_a;
		ast->define(top, here);
		if (ast->isdefined()) {
			int	v = (int)ast->eval();
			expr.m_typ = MAPT_INT;
			expr.u.m_v = v;
			delete	ast;
		}
	}
}

bool	find_any_unevaluated_sub(MAPDHASH &top, MAPDHASH *here, MAPDHASH &sub) {
	bool	changed = false;
	MAPDHASH::iterator	pfx = sub.end(), subi;
	MAPDHASH	*component;

	pfx = sub.find(KYPREFIX);
	if (pfx != sub.end())
		component = &sub;
	else
		component = here;

	for(subi=sub.begin(); subi != sub.end(); subi++) {
		if (subi->second.m_typ == MAPT_MAP) {
			changed = find_any_unevaluated_sub(top, component,
				*subi->second.u.m_m) || (changed);
		} else if (subi->first == KYEXPR) {
			if (subi->second.m_typ == MAPT_STRING) {
				expr_eval(top, *component, sub, subi->second);
				if (subi->second.m_typ != MAPT_STRING)
					changed = true;
			} else if (subi->second.m_typ == MAPT_AST) {
				AST	*ast = subi->second.u.m_a;
				ast->define(top, *component);
				if (ast->isdefined()) {
					subi->second.m_typ = MAPT_INT;
					subi->second.u.m_v = ast->eval();
					delete ast;
					changed = true;
				}
			}
		} else if (subi->second.m_typ == MAPT_AST) {
			AST	*ast = subi->second.u.m_a;
			ast->define(top, *component);
			if (ast->isdefined()) {
				int	val = ast->eval();
				subi->second.m_typ = MAPT_INT;
				subi->second.u.m_v = val;
				delete ast;
				changed = true;
			}
		}
	} return changed;
}

void	find_any_unevaluated(MAPDHASH &info) {
	MAPDHASH::iterator	topi;
	bool	changed = false;

	do {
		for(topi=info.begin(); topi != info.end(); topi++) {
			if (topi->second.m_typ != MAPT_MAP)
				continue;
			changed = find_any_unevaluated_sub(info, &info,
					*topi->second.u.m_m);
		}
	} while(changed);
}

bool	subresults_into(MAPDHASH &info, MAPDHASH *here, STRINGP &sval) {
	bool	changed = false, everchanged = false;;

	do {
		unsigned long	sloc = -1;

		changed = false;
		sloc = 0;
		while(STRING::npos != (sloc = sval->find("@$", sloc))) {
			int	kylen = 0, value;
			const char *ptr = sval->c_str() + sloc + 2;
			for(; ptr[kylen]; kylen++) {
				if((!isalpha(ptr[kylen]))
						&&(!isdigit(ptr[kylen]))
						&&(ptr[kylen] != '_')
						&&(ptr[kylen] != '.'))
					break;
			} if (kylen > 0) {
				STRING key = sval->substr(sloc+2, kylen), *vstr;
				if (NULL != (vstr = get_named_string(info,*here, key))) {
					sval->replace(sloc, kylen+2, *vstr);
					changed = true;
				}else if(get_named_value(info,*here,key,value)){
					char	buffer[64];
					STRING	tmp;
					sprintf(buffer, "%d", value);
					tmp = buffer;
					sval->replace(sloc, kylen+2, tmp);
					changed = true;
				} else sloc++;
			} else sloc++;
		} everchanged = everchanged || changed;
	} while (changed);

	return everchanged;
}

STRINGP	genstr(STRING fmt, int val) {
	char	*buf = new char[fmt.size()+64];
	STRINGP	r;

	if (fmt.size() > 0) {
		sprintf(buf, fmt.c_str(), val);
	} else {
		printf("No format\n");
		sprintf(buf, "%d", val);
	}

	r = new STRING(buf);
	delete[] buf;
	return r;
}

// Return if anything changes
bool	resolve(MAPDHASH &exmap) {
	MAPDHASH::iterator	kvexpr, kvval, kvfmt, kvstr;
	MAPT	elm;
	STRINGP	strp;
	AST	*ast;

	kvexpr = exmap.find(KYEXPR);
	kvval  = exmap.find(KYVAL);
	kvfmt  = exmap.find(KYFORMAT);
	kvstr  = exmap.find(KYSTR);

	if (kvstr != exmap.end())
		return false;

	if (kvexpr == exmap.end()) {
		printf("No-Expr found\n");
		return false;
	}

	if ((kvval != exmap.end())&&(kvval->second.m_typ == MAPT_INT)) {
		if((kvfmt != exmap.end())&&(kvfmt->second.m_typ == MAPT_STRING))
			strp = genstr(*kvfmt->second.u.m_s,kvval->second.u.m_v);
		else
			strp = genstr(STRING(""),kvval->second.u.m_v);
		MAPT	elm;
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = strp;
		exmap.insert(KEYVALUE(KYSTR, elm));

		return true;
	}

	if ((kvexpr != exmap.end())&&(kvexpr->second.m_typ == MAPT_STRING)) {
		AST *ast = parse_ast(*kvexpr->second.u.m_s);
		kvexpr->second.m_typ = MAPT_AST;
		kvexpr->second.u.m_a = ast;
	}

	if ((kvexpr != exmap.end())
			&&((kvexpr->second.m_typ == MAPT_AST)
			 ||(kvexpr->second.m_typ == MAPT_INT))) {
		if (kvexpr->second.m_typ == MAPT_AST) {
			ast = kvexpr->second.u.m_a;
			if (!ast->isdefined()) {
				return false;
			} kvexpr->second.m_typ = MAPT_INT;
			kvexpr->second.u.m_v = ast->eval();
			delete ast;
		}

		elm.m_typ = MAPT_INT;
		elm.u.m_v = kvexpr->second.u.m_v;
		exmap.insert(KEYVALUE(KYVAL, elm));

		resolve(exmap);
		return true;
	}

	return false;
}

bool	substitute_any_results_sub(MAPDHASH &info, MAPDHASH *here,
		MAPDHASH &sub) {
	MAPDHASH::iterator	pfx = sub.end(), subi, kvelm, kvstr, kvfmt;
	MAPDHASH	*component;
	bool		v = false;

	pfx = sub.find(KYPREFIX);
	if (pfx != sub.end())
		component = &sub;
	else
		component = here;

	for(subi=sub.begin(); subi != sub.end(); subi++) {
		if (subi->second.m_typ == MAPT_MAP) {
			substitute_any_results_sub(info, component,
				*subi->second.u.m_m);
			kvelm = subi->second.u.m_m->find(KYEXPR);
			kvstr  = subi->second.u.m_m->find(KYSTR);
			kvfmt  = subi->second.u.m_m->find(KYFORMAT);
			if ((kvstr == subi->second.u.m_m->end())
				&&(kvelm != subi->second.u.m_m->end())) {
				v = resolve(*subi->second.u.m_m) || v;
			}
		} else if (subi->second.m_typ == MAPT_STRING) {
			v = subresults_into(info, here, subi->second.u.m_s) ||v;
		}
	} return v;
}

bool	substitute_any_results(MAPDHASH &info) {
	return substitute_any_results_sub(info, &info, info);
}

void	reeval(MAPDHASH &info) {
	do {
		// First, find expressions that need evalution
		find_any_unevaluated(info);
		// Then, substitute their results back in as necessary
	} while(substitute_any_results(info));
}
