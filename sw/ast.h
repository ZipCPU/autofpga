////////////////////////////////////////////////////////////////////////////////
//
// Filename:	sw/ast.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
// {{{
// Purpose:	
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2024, Gisselquist Technology, LLC
// {{{
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
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
// }}}
// License:	GPL, v3, as defined and found on www.gnu.org,
// {{{
//		http://www.gnu.org/licenses/gpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
// }}}
#ifndef	AST_H
#define	AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "kveval.h"

class AST {
public:
	virtual ~AST(void) {}
	char	m_node_type;
	virtual bool	isdefined(void) = 0;
	virtual long	eval(void) = 0;
	virtual bool	define(MAPSTACK &stack, MAPDHASH &comp) = 0;
	virtual void	dump(FILE *fp, int offset) = 0;
	virtual	AST	*copy(void) = 0;
};

class AST_BRANCH : public AST {
public:
	int	m_op;
	AST	*m_left, *m_right;
	AST_BRANCH(int op, AST *l, AST *r)
		: m_op(op), m_left(l), m_right(r) {
		m_node_type = 'B';}
	~AST_BRANCH(void) {
		if (m_left)
			delete m_left;
		if (m_right)
			delete m_right;
	}

	virtual	bool	isdefined(void);
	virtual	long	eval(void);
	virtual	bool	define(MAPSTACK &stack, MAPDHASH &here);
	virtual	void	dump(FILE *fp, int offset);
	virtual	AST	*copy(void);
};

class	AST_SINGLEOP : public AST {
  public:
	int	m_op;
	AST	*m_val;
	AST_SINGLEOP(int op, AST *v)
		: m_op(op), m_val(v) { m_node_type = 'S'; }
	~AST_SINGLEOP(void) {	delete m_val; }

	virtual	bool	isdefined(void);
	virtual	long	eval(void);
	virtual	bool	define(MAPSTACK &stack, MAPDHASH &here);
	virtual	void	dump(FILE *fp, int offset);
	virtual	AST	*copy(void);
};

class	AST_TRIOP : public AST {
public:
	AST	*m_cond, *m_left, *m_right;
	AST_TRIOP(AST *c, AST *l, AST *r)
			: m_cond(c), m_left(l), m_right(r) {
		m_node_type = 'T';
	}
	~AST_TRIOP(void) {
		delete m_cond; delete m_left; delete m_right;
	}

	virtual	bool	isdefined(void);
	virtual	long	eval(void);
	virtual	bool	define(MAPSTACK &stack, MAPDHASH &here);
	virtual	void	dump(FILE *fp, int offset);
	virtual	AST	*copy(void);
};

class	AST_NUMBER : public AST {
public:
	long	m_val;
	AST_NUMBER(long val) : m_val(val) { m_node_type = 'N'; }

	virtual	bool	isdefined(void);
	virtual	long	eval(void);
	virtual	bool	define(MAPSTACK &stack, MAPDHASH &here);
	virtual	void	dump(FILE *fp, int offset);
	virtual	AST	*copy(void);
};

class	AST_IDENTIFIER : public AST {
public:
	STRING	m_id;
	bool	m_def;
	int	m_v;
	AST_IDENTIFIER(const char *id) : m_id(id) {
		m_node_type = 'I';
		m_def=false;
		m_v = 0;
	}

	virtual	bool	isdefined(void);
	virtual	long	eval(void);
	virtual	bool	define(MAPSTACK &stack, MAPDHASH &here);
	virtual	void	dump(FILE *fp, int offset);
	virtual	AST	*copy(void);
};

extern AST	*parse_ast(const STRING &str);
inline	AST *copy(AST *a) { return a->copy(); }

#endif	// AST_H
