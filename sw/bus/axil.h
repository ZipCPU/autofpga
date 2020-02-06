////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	axil.h
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	AXI-lite interface--for high speed AXI-lite cores.  (Slow speed
//		ones should work too if they follow the spec)
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2019-2020, Gisselquist Technology, LLC
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
#ifndef	AXIL_H
#define	AXIL_H

#include "../genbus.h"

class	AXILBUS : public GENBUS {
protected:
	PLIST		*m_slist, *m_dlist;
	MAPDHASH	*m_interconnect;
	unsigned	m_num_total, m_num_double, m_num_single;
	bool		m_is_single, m_is_double;

	void	xbarcon_master(FILE *fp, const char *, const char *,
			const char *, bool comma=true);
	void	xbarcon_slave(FILE *fp, PLIST *pl,
			const char *, const char *, const char *, bool comma=true);
	STRINGP	master_name(int k);
	void	allocate_subbus(void);

	BUSINFO *create_sio(void);
	BUSINFO *create_dio(void);
	void	countsio(void);
public:
	AXILBUS(BUSINFO *bi);
	~AXILBUS() {};
	virtual	int	address_width(void);
	//
	virtual	void	assign_addresses(void);
	virtual	bool	get_base_address(MAPDHASH *phash, unsigned &base);

	void		write_addr_range(FILE *fp, const PERIPHP p,
				const int dalines);
	void		writeout_defn_v(FILE *fp, const char *pname,
				const char *busp, const char *btyp = "");
	virtual	void	writeout_bus_slave_defns_v(FILE *fp);
	virtual	void	writeout_bus_master_defns_v(FILE *fp);

	virtual	void	writeout_bus_logic_v(FILE *fp);

	virtual	void	writeout_no_slave_v(FILE *fp, STRINGP prefix);
	virtual	void	writeout_no_master_v(FILE *fp);
	//
	//
	virtual	STRINGP	master_portlist(BMASTERP);
	virtual	STRINGP	master_ansi_portlist(BMASTERP);
	virtual	STRINGP	slave_portlist(PERIPHP);
	virtual	STRINGP	slave_ansi_portlist(PERIPHP);

	virtual	void	integrity_check(void);
};

class	AXILBUSCLASS : public BUSCLASS {
public:
	virtual	STRINGP	name(void);	// i.e. WB
	virtual	STRINGP	longname(void);	// i.e. "Wishbone"
	virtual	bool	matchtype(STRINGP strp);
	virtual	bool	matchfail(MAPDHASH *);
	// virtual	bool	matches(BUSINFO *bi);
	GENBUS	*create(BUSINFO *bi);
};

#endif
