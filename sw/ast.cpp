////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	ast.cpp
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "keys.h"

bool	AST_BRANCH::isdefined(void) {
	return ((m_left)&&(m_right)&&(m_left->isdefined())
			&&(m_right->isdefined()));
}

long AST_BRANCH::eval(void) {
	long	lft = m_left->eval(), rht = m_right->eval();
	switch(m_op) {
		case '+':	return lft + rht;	break;
		case '-':	return lft - rht;	break;
		case '*':	return lft * rht;	break;
		case '/':	return lft / rht;	break;
		case '%':	return lft % rht;	break;
		//
		case 'u':	return lft << rht;	break;
		case 'd':	return lft >> rht;	break;
		//
		case '|':	return lft | rht;	break;
		case '&':	return lft & rht;	break;
		case '^':	return lft ^ rht;	break;
		// ~ ??
		//
		case 'o':	return ((lft)||(rht))?1:0;	break;
		case 'a':	return ((lft)&&(rht))?1:0;	break;
		case 'e':	return ((lft)==(rht))?1:0;	break;
		// ! ??
		//
		default:	fprintf(stderr, "ERR: AST Unknown operation, %c", m_op);
			return lft;
	}
}
bool	AST_BRANCH::define(MAPDHASH &map, MAPDHASH &here) {
	bool	v;
	v =  m_left->define(map, here);
	v = m_right->define(map, here) || v;
	return v;
}
void	AST_BRANCH::dump(int offset) {
	printf("%*s%c\n", offset, "", m_op);
	 m_left->dump(offset+2);
	m_right->dump(offset+2);
}

bool	AST_SINGLEOP::isdefined(void) {
		return (m_val->isdefined()); }
long	AST_SINGLEOP::eval(void) {
	if (m_op == '~')
		return ~m_val->eval();
	else
		return (m_val->eval())?0:1;
}
bool	AST_SINGLEOP::define(MAPDHASH &map, MAPDHASH &here) {
	return m_val->define(map, here);
}
void	AST_SINGLEOP::dump(int offset) {
	printf("%*s%c\n", offset, "", m_op);
	m_val->dump(offset+2);
}
bool	AST_TRIOP::isdefined(void) {
	return (m_cond->isdefined())&&(m_left->isdefined())
			&&(m_right->isdefined());
}
long	AST_TRIOP::eval(void) {
	return (m_cond->eval())
		? m_left->eval() : m_right->eval();
}
bool	AST_TRIOP::define(MAPDHASH &map, MAPDHASH &here) {
	bool	v = false;
	v =  (m_cond->define(map, here)) || v;
	v =  (m_left->define(map, here)) || v;
	v = (m_right->define(map, here)) || v;
	return v;
}
void	AST_TRIOP::dump(int offset) {
	printf("%*sTRIPL:\n", offset, "");
	m_cond->dump(offset+2);
	m_left->dump(offset+2);
	m_right->dump(offset+2);
}


bool	AST_NUMBER::isdefined(void) { return true; }
long	AST_NUMBER::eval(void) { return m_val; }
bool	AST_NUMBER::define(MAPDHASH &map, MAPDHASH &here) { return false; }
void	AST_NUMBER::dump(int offset) {
	printf("%*s%ld\n", offset, "", m_val);
}


bool	AST_IDENTIFIER::isdefined(void) { return m_def; }
long	AST_IDENTIFIER::eval(void) { return m_v; }
bool	AST_IDENTIFIER::define(MAPDHASH &map, MAPDHASH &here) {
	printf("Checking for the definition of %s\n", m_id.c_str());
	if (m_def)
		return false; // No change
	printf("Looking for the definition of %s\n", m_id.c_str());
	if (strncmp(m_id.c_str(), KYTHISDOT.c_str(), KYTHISDOT.size())==0) {
		STRING	tmp = m_id.substr(KYTHISDOT.size(), m_id.size()-KYTHISDOT.size());
		if (getvalue(here, tmp, m_v)) {
			printf("FOUND!  It = %d\n", m_v);
			m_def = true;
			return true;	// We now know our value
		}
	} else if (getvalue(map, m_id, m_v)) {
		m_def = true;
		printf("FOUND!  It = %d\n", m_v);
		return true;	// We now know our value
	}
	return false;	// Nothing changed, still don't know it
}
void	AST_IDENTIFIER::dump(int offset) {
	if (m_def)
		printf("%*s%s (= %d)\n", offset, "", m_id.c_str(), m_v);
	else
		printf("%*s%s (Undefind)\n", offset, "", m_id.c_str());
}

