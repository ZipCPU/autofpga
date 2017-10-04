////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	msgs.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	A class for handling messages to the user
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
#ifndef	MSGS_H
#define	MSGS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <unordered_map>
#include "mapdhash.h"

class	MSGS {
	FILE	*m_dump;
	int	m_err;
public:
	MSGS(void) { m_err = 0; }
	void	open(const char *fname);
	void	close(void) { if (m_dump) ::fclose(m_dump);  m_dump = NULL; };
	void	flush(void) { if (m_dump) fflush(m_dump); }
	//
	void	info(const char *, ...);
	void	userinfo(const char *, ...);
	void	warning(const char *, ...);
	void	error(const char *, ...);
	// void	oserr(const char *, ...);
	void	fatal(const char *, ...);
	void	dump(MAPDHASH &map, const char *msg = NULL);
	int	status(void) { return (m_err)?EXIT_FAILURE : EXIT_SUCCESS; }
};

extern	MSGS	gbl_msg;

#endif // MSGS
