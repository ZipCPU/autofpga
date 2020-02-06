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
		case 'N':	return (lft != rht)? 1:0;	break;
		case 'L':	return (lft <= rht)? 1:0;	break;
		case 'G':	return (lft >= rht)? 1:0;	break;
		case '<':	return (lft <  rht)? 1:0;	break;
		case '>':	return (lft >  rht)? 1:0;	break;
		//
		//
		default:	fprintf(stderr, "ERR: AST Unknown operation, %c", m_op);
			return lft;
	}
}
bool	AST_BRANCH::define(MAPSTACK &stack, MAPDHASH &here) {
	bool	v;
	v =  m_left->define(stack, here);
	v = m_right->define(stack, here) || v;
	return v;
}
void	AST_BRANCH::dump(FILE *fp, int offset) {
	fprintf(fp, "%*s%c\n", offset, "", m_op);
	 m_left->dump(fp, offset+2);
	m_right->dump(fp, offset+2);
}

AST	*AST_BRANCH::copy(void) {
	return new AST_BRANCH(m_op, ::copy(m_left), ::copy(m_right));
}

bool	AST_SINGLEOP::isdefined(void) {
		return (m_val->isdefined()); }
long	AST_SINGLEOP::eval(void) {
	if (m_op == '~')
		return ~m_val->eval();
	else
		return (m_val->eval())?0:1;
}
bool	AST_SINGLEOP::define(MAPSTACK &stack, MAPDHASH &here) {
	return m_val->define(stack, here);
}
void	AST_SINGLEOP::dump(FILE *fp, int offset) {
	fprintf(fp, "%*s%c\n", offset, "", m_op);
	m_val->dump(fp, offset+2);
}

AST	*AST_SINGLEOP::copy(void) {
	return new AST_SINGLEOP(m_op, ::copy(m_val));
}

bool	AST_TRIOP::isdefined(void) {
	return (m_cond->isdefined())&&(m_left->isdefined())
			&&(m_right->isdefined());
}
long	AST_TRIOP::eval(void) {
	return (m_cond->eval())
		? m_left->eval() : m_right->eval();
}
bool	AST_TRIOP::define(MAPSTACK &stack, MAPDHASH &here) {
	bool	v = false;
	v =  (m_cond->define(stack, here)) || v;
	v =  (m_left->define(stack, here)) || v;
	v = (m_right->define(stack, here)) || v;
	return v;
}
void	AST_TRIOP::dump(FILE *fp, int offset) {
	fprintf(fp, "%*sTRIPL:\n", offset, "");
	m_cond->dump(fp, offset+2);
	m_left->dump(fp, offset+2);
	m_right->dump(fp, offset+2);
}
AST	*AST_TRIOP::copy(void) {
	return new AST_TRIOP(::copy(m_cond), ::copy(m_left), ::copy(m_right));
}


bool	AST_NUMBER::isdefined(void) { return true; }
long	AST_NUMBER::eval(void) { return m_val; }
bool	AST_NUMBER::define(MAPSTACK &stack, MAPDHASH &here) { return false; }
void	AST_NUMBER::dump(FILE *fp, int offset) {
	fprintf(fp, "%*s%ld\n", offset, "", m_val);
}
AST	*AST_NUMBER::copy(void) {
	return new AST_NUMBER(m_val);
}


bool	AST_IDENTIFIER::isdefined(void) { return m_def; }
long	AST_IDENTIFIER::eval(void) { return m_v; }
bool	AST_IDENTIFIER::define(MAPSTACK &stack, MAPDHASH &here) {
	if (m_def)
		return false; // No change
	if (get_named_value(stack, here, m_id, m_v)) {
		// GOT IT!!!!  We now know our value, so ... set it and
		// report that things have changed.
		m_def = true;
		return true;	// Things have changed!
	} return false;	// Nothing changed, we still don't know our value
}
void	AST_IDENTIFIER::dump(FILE *fp, int offset) {
	if (m_def)
		fprintf(fp, "%*s%s (= %d)\n", offset, "", m_id.c_str(), m_v);
	else
		fprintf(fp, "%*s%s (Undefined)\n", offset, "", m_id.c_str());
}
AST	*AST_IDENTIFIER::copy(void) {
	return new AST_IDENTIFIER(m_id.c_str());
}
