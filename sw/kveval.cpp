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
		kvsub = kvpair->second.u.m_m->find(KYVAL);
		if (kvsub != kvpair->second.u.m_m->end()) {
			if (kvsub->second.m_typ == MAPT_INT) {
				value = kvsub->second.u.m_v;
				return true; // Found it!
			}
		}
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
		kvsub = kvpair->second.u.m_m->find(KYVAL);
		if (kvsub != kvpair->second.u.m_m->end()) {
			// We have a value!!
			kvstr = kvsub->second.u.m_m->find(KYSTR);
			if (kvstr != kvsub->second.u.m_m->end()) {
				return kvstr->second.u.m_s; // Found it!
			}
			kvfmt = kvsub->second.u.m_m->find(KYFORMAT);
			if (kvsub->second.m_typ == MAPT_INT) {
				// The value exists, but ... needs to be
				// created into a string
				char	buffer[512];

				if (kvfmt != kvsub->second.u.m_m->end()) {
					sprintf(buffer,
						kvfmt->second.u.m_s->c_str(),
						kvsub->second.u.m_v);
				} else
					sprintf(buffer, "0x%08x",
						kvsub->second.u.m_v);

				MAPT	elm;
				elm.m_typ = MAPT_STRING;
				elm.u.m_s = new STRING(buffer);
				kvsub->second.u.m_m->insert(KEYVALUE(KYSTR, elm));
				return elm.u.m_s; // Just created it
			}
		}
	} return NULL;
}

bool	sexpr_eval(MAPDHASH &top, MAPDHASH &here, STRINGP expr, int &value) {
#warning "BUILD-ME"
	return false;
}

void	expr_eval(MAPDHASH &top, MAPDHASH &here, MAPDHASH &sub, MAPT &expr) {
	if (expr.m_typ == MAPT_STRING) {
		int	value;
		bool	valid;

		valid = sexpr_eval(top, here, expr.u.m_s, value);
		if (valid) {
			expr.m_typ = MAPT_INT;
			expr.u.m_v = value;
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

void	subresults_into(MAPDHASH &info, MAPDHASH *here, STRINGP &sval) {
	bool	changed = false;

	do {
		unsigned long	sloc = -1;

		sloc = 0;
		while(STRING::npos != (sloc = sval->find("@$", sloc))) {
			int	kylen = 0, value;
			const char *ptr = sval->c_str() + sloc + 2;
			for(; ptr[kylen]; kylen++) {
				if((!isalpha(ptr[kylen]))&&(ptr[kylen] != '.'))
					break;
			} if (kylen > 0) {
				STRING key = sval->substr(sloc+2, kylen), *vstr;
				if (NULL != (vstr = get_named_string(info,*here, key))) {
					sval->replace(sloc, kylen+2, *vstr);
				}else if(get_named_value(info,*here,key,value)){
					char	buffer[64];
					STRING	tmp;
					sprintf(buffer, "0x%08x", value);
					tmp = buffer;
					sval->replace(sloc, kylen+2, tmp);
				} else sloc++;
			} else sloc++;
		}
	} while (changed);
	
}

void	substitute_any_results_sub(MAPDHASH &info, MAPDHASH *here,
		MAPDHASH &sub) {
	MAPDHASH::iterator	pfx = sub.end(), subi;
	MAPDHASH	*component;

	pfx = sub.find(KYPREFIX);
	if (pfx != sub.end())
		component = &sub;
	else
		component = here;

	for(subi=sub.begin(); subi != sub.end(); subi++) {
		if (subi->second.m_typ == MAPT_MAP) {
			substitute_any_results_sub(info, component,
				*subi->second.u.m_m);
		} else if (subi->second.m_typ == MAPT_STRING) {
			subresults_into(info, here, subi->second.u.m_s);
		}
	}
}

void	substitute_any_results(MAPDHASH &info) {
	substitute_any_results_sub(info, &info, info);
}

void	reeval(MAPDHASH &info) {
	// First, find expressions that need evalution
	find_any_unevaluated(info);
	// Then, substitute their results back in as necessary
	substitute_any_results(info);
}
