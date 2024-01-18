////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	eval.h
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To define the interface whereby values within the key-value hash
//		can be both computed, and substituted/replaced as necessary.
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
////////////////////////////////////////////////////////////////////////////////
//
// }}}
#ifndef	KVEVAL_H
#define	KVEVAL_H

#include <vector>
#include "mapdhash.h"
#include "globals.h"

#define	REHASH	do { gbl_msg.info("REEVAL %s:%d\n", __FILE__, __LINE__); reeval(gbl_hash); gbl_msg.dump(*gbl_hash); } while(0)
typedef	std::vector<MAPDHASH *>	MAPSTACK;

bool	get_named_kvpair(MAPSTACK &stack, MAPDHASH &here, STRING &key,
		MAPDHASH::iterator &pair);
bool	get_named_value(MAPSTACK &stack, MAPDHASH &here, STRING &key,
		int &value);
STRINGP	get_named_string(MAPSTACK &stack, MAPDHASH &here, STRING &key);

extern	bool	resolve_ast_expressions(MAPDHASH &info);
extern	void	reeval(MAPDHASH &info);
extern	void	reeval(MAPDHASH *info);

#endif
