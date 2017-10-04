////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	msgs.cpp
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
#include <stdarg.h>
#include "msgs.h"

MSGS	gbl_msg;

void	MSGS::open(const char *fname) {
	close();
	m_dump = fopen(fname, "w");

	if (NULL == m_dump) {
		fprintf(stderr, "ERR: Could not open %s\n", fname);
		exit(EXIT_FAILURE);
	}
}

void	MSGS::info(const char *fmt, ...) {
	va_list	args;

	if (m_dump) {
		va_start(args, fmt);
		vfprintf(m_dump, fmt, args);
		va_end(args);
	}
}

void	MSGS::userinfo(const char *fmt, ...) {
	va_list	args;

	if (m_dump) {
		va_start(args, fmt);
		vfprintf(m_dump, fmt, args);
		va_end(args);
	}

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

void	MSGS::warning(const char *fmt, ...) {
	const	char	*prefix = "WARNING: ";
	va_list	args;

	if (m_dump) {
		va_start(args, fmt);
		fprintf(m_dump, "%s", prefix);
		vfprintf(m_dump, fmt, args);
		va_end(args);
	}

	va_start(args, fmt);
	fprintf(stderr, "%s", prefix);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void	MSGS::error(const char *fmt, ...) {
	const char	*prefix = "ERR: ";
	va_list	args;

	if (m_dump) {
		va_start(args, fmt);
		fprintf(m_dump, "%s", prefix);
		vfprintf(m_dump, fmt, args);
		va_end(args);
	}

	va_start(args, fmt);
	fprintf(stderr, "%s", prefix);
	vfprintf(stderr, fmt, args);
	va_end(args);

	m_err++;
}

void	MSGS::fatal(const char *fmt, ...) {
	const char	*prefix = "FATAL ERR: ";
	va_list	args;

	if (m_dump) {
		va_start(args, fmt);
		fprintf(m_dump, "%s", prefix);
		vfprintf(m_dump, fmt, args);
		va_end(args);
	}

	va_start(args, fmt);
	fprintf(stderr, "%s", prefix);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(EXIT_FAILURE);
}

void	MSGS::dump(MAPDHASH &map, const char *msg) {
	if (!m_dump)
		return;

	fprintf(m_dump, "%s\n", msg);
	mapdump(m_dump, map);
}
