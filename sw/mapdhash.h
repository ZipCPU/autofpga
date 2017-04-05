////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	mapdhash.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To define the data structures used to represent a parsed input
//		file.  Of particular note is the key-value pair unordered map
//	structure, and the components of that structure.
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
#ifndef	MAPDHASH_H
#define	MAPDHASH_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <unordered_map>

#define	MAPT_INT	0
#define	MAPT_STRING	1
#define	MAPT_MAP	2
#define	MAPT_AST	3
// Contains a "soft-link" to a hash entry, in other words, this is the "name" of another hash entry--one which we will use instead
// #define	MAPT_SLINK	3

typedef	std::string *STRINGP, STRING;
typedef std::unordered_map<STRING,struct MAPT_S> MAPDHASH;

typedef	struct MAPT_S {
	int	m_typ;
	union {
		int		m_v;
		STRINGP		m_s;
		MAPDHASH	*m_m;
		class AST	*m_a;	// Abstract syntax tree (uneval expr)
		// MAPDHASH	*m_l;	// link to elsewhere in the table
	} u;
} MAPT;

typedef	std::pair<STRING,MAPT>	KEYVALUE;

extern	STRING *trim(const STRING &s);
extern	bool	splitkey(const STRING &ky, STRING &mkey, STRING &subky);
extern	void	addtomap(MAPDHASH &fm, const STRING ky, STRING vl);
extern	void	mapdump(MAPDHASH &fm);
extern	void	mergemaps(MAPDHASH &master, MAPDHASH &sub);
extern	void	trimall(MAPDHASH &mp, const STRING &sky);
extern	void	cvtint(MAPDHASH &mp, const STRING &sky);
extern	MAPDHASH::iterator	findkey(MAPDHASH &mp, const STRING &sky);
extern	MAPDHASH *getmap(MAPDHASH &mp, const STRING &ky);
extern	STRINGP	getstring(MAPDHASH &mp);
extern	STRINGP	getstring(MAPDHASH &mp, const STRING &sky);
extern	STRINGP	getstring(MAPT &m, const STRING &sky);
extern	bool	getvalue(MAPDHASH &mp, int &value);
extern	bool	getvalue(MAPDHASH &mp, const STRING &sky, int &value);
extern	void	setvalue(MAPDHASH &mp, const STRING &sky, int value);

#endif // MAPDHASH
