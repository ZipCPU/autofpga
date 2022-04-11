////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	mlist.cpp
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	An MLIST is a list (i.e. std::vector<>) of bus masters.  This
//		file defines the methods associated with such a list.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2022, Gisselquist Technology, LLC
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
#include "parser.h"
#include "mlist.h"
#include "mapdhash.h"
#include "keys.h"
#include "predicates.h"

STRINGP		BMASTER::name(void) {
	return getstring(*m_hash, KYPREFIX);
}

STRINGP		BMASTER::bus_prefix(void) {
	STRINGP	pfx;

	pfx = getstring(*m_hash, KYMASTER_PREFIX);
	if (NULL == pfx) {
		STRINGP	bus = getstring(*m_hash, KYMASTER_BUS_NAME);
		if (NULL == bus)
			return NULL;
		pfx = new STRING(*bus + STRING("_") + *name());
		setstring(*m_hash, KYMASTER_PREFIX, pfx);
	}
	return pfx;
}

bool	BMASTER::read_only(void) {
	STRINGP	options;

	options = getstring(*m_hash, KYMASTER_OPTIONS);
	if (NULL != options)
		return read_only_option(options);
	return false;
}

bool	BMASTER::write_only(void) {
	STRINGP	options;

	options = getstring(*m_hash, KYMASTER_OPTIONS);
	if (NULL != options)
		return write_only_option(options);
	return false;
}
