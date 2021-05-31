////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	predicates.cpp
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A predicate is a function that returns true or false.  This
//		file describes the implementation of a series of functions that
//	can be used to determine of what type a given design component is.
//	Is it a bus master?  A bus slave?  A programmable interrupt controller?
//	etc.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2021, Gisselquist Technology, LLC
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
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <ctype.h>

#include "parser.h"
#include "keys.h"
#include "kveval.h"
#include "legalnotice.h"
#include "bldtestb.h"
#include "bitlib.h"
#include "plist.h"
#include "bldregdefs.h"
#include "ifdefs.h"
#include "bldsim.h"
#include "predicates.h"

//
// Is the given location within our hash a master?  Look it up.
//
// To be a bus master, it must have a @MTYPE field.
//
bool	isbusmaster(MAPDHASH &phash) {
	STRINGP styp;

	styp = getstring(phash, KYMASTER_TYPE);
	if (NULL == styp)
		return false;
	if (KYSCRIPT.compare(*styp) == 0)
		return false;
	return true;
}
//
// Does the given location describe access to a bus lying beneath it?
//
// To be true, isbusmaster() must be true, and the MASTER.BUS.TYPE field
// must be one of: SUBBUS, XCLOCK, ARBITER, etc.
//
bool	issubbus(MAPDHASH &phash) {
	if (!isperipheral(phash))
		return false;
	if (!isbusmaster(phash))
		return false;
	STRINGP	mtype = getstring(phash, KYMASTER_TYPE);
	if (!mtype)
		return false;
	if (mtype->compare(KYBUS)==0)
		return true;
	if (mtype->compare(KYSUBBUS)==0)
		return true;
	if (mtype->compare(KYXCLOCK)==0)
		return true;
	if (mtype->compare(KYARBITER)==0)
		return true;
	return false;
}

bool	isarbiter(MAPDHASH &phash) { return issubbus(phash); }

//
// Same thing, but when given a location within the tree, rather than a hash
// value.
bool	isbusmaster(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return isbusmaster(*pmap.u.m_m);
}

//
// Is the given hash a definition of a peripheral
//
// To be a peripheral, it must have a @PTYPE field.
//
bool	isperipheral(MAPDHASH &phash) {
	return (NULL != getstring(phash, KYSLAVE_TYPE));
	// return (phash.end() != phash.find(KYSLAVE_TYPE));
}

bool	isperipheral(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return isperipheral(*pmap.u.m_m);
}

//
// Does the given hash define a programmable interrupt controller?
//
// If so, it must have a @PIC.MAX field identifying the maximum number of
// interrupts that can be assigned to it.
bool	ispic(MAPDHASH &phash) {
	return (phash.end() != findkey(phash, KYPIC_MAX));
}

bool	ispic(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return ispic(*pmap.u.m_m);
}

// Does this reference a memory peripheral?
bool	ismemory(MAPDHASH &phash) {
	STRINGP	strp;

	strp = getstring(phash, KYSLAVE_TYPE);
	if (!strp)
		return false;
	if (KYMEMORY.compare(*strp) != 0)
		return false;
	return true;
}

bool	ismemory(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return ismemory(*pmap.u.m_m);
}

//
// Does this component have a bus definition within it, or does it reference
// a bus?
bool	refbus(MAPDHASH &phash) {

	if (NULL != getstring(phash, KYBUS))
		return true;
	// else if (NULL != getmap(phash, KYBUS)) return true;
	return false;
}

bool	refbus(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return refbus(*pmap.u.m_m);
}

//
// Does this component have a clock definition within it, or does it reference
// a clock?
//
bool	refclock(MAPDHASH &phash) {
	STRINGP	strp;

	strp = getstring(phash, KYCLOCK);
	if (!strp)
		return false;
	return true;
}

bool	refclock(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return refbus(*pmap.u.m_m);
}

// Does the top level map contain a CPU as one of the peripherals?
bool	has_cpu(MAPDHASH &phash) {
	MAPDHASH::iterator	kvpair;
	STRINGP	strp;

	for(kvpair=phash.begin(); kvpair != phash.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isbusmaster(kvpair->second))
			continue;
		strp = getstring(kvpair->second.u.m_m, KYMASTER_TYPE);
		if (NULL == strp)
			continue;
		if (strp->compare(KYCPU)==0)
			return true;
	} return false;
}

bool	read_only_option(STRINGP op) {
	char 	*cstr, *tok;
	const char	DELIMITERS[] = ", \t\r\n";
	bool	read_only = false;

	cstr = strdup(op->c_str());
	tok = strtok(cstr, DELIMITERS);
	while(NULL != tok) {
		if (strcasecmp(tok, "RO")==0) {
			read_only = true;
			break;
		}
		tok = strtok(NULL, DELIMITERS);
	}

	free(cstr);
	return read_only;
}

bool	write_only_option(STRINGP op) {
	char 	*cstr, *tok;
	const char	DELIMITERS[] = ", \t\r\n";
	bool	write_only = false;

	cstr = strdup(op->c_str());
	tok = strtok(cstr, DELIMITERS);
	while(NULL != tok) {
		if (strcasecmp(tok, "WO")==0)
			write_only = true;
		tok = strtok(NULL, DELIMITERS);
	}

	free(cstr);
	return write_only;
}
