////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldcachable.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Creates a verilog file, upon request, containing combinatorial
//		logic returning true if a particular address beneath this
//	location in the address/bus hierarchy is a memory.  This can be used
//	to determine if an address is a cachable address or not--something the
//	ZipCPU data cache needs to know.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2018-2020, Gisselquist Technology, LLC
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
#include <stdlib.h>
#include <string>
#include <vector>
#include <string.h>

#include "bldcachable.h"
#include "keys.h"
#include "legalnotice.h"
#include "businfo.h"
#include "bitlib.h"
#include "predicates.h"
#include "msgs.h"


static void	print_cachable(FILE *fp, BUSINFO *bi, unsigned dw,
		unsigned mask_8, unsigned addr_8) {
	unsigned	pbits;
	unsigned	submask = 0, subaddr = 0;
	unsigned	lgdw = nextlg(bi->data_width()), bbits=lgdw-3;
	PLIST		*pl;


	pl = bi->m_plist;
	fprintf(fp, "\t\t// Bus master: %s\n", bi->name()->c_str());
	submask = mask_8; // Octets
	for(unsigned i=0; i<pl->size(); i++)
		submask |= (*pl)[i]->p_mask << bbits;

	for(unsigned i=0; i<pl->size(); i++) {
		if (!(*pl)[i]->p_name) {
			continue;
		} if (((0)&&(*pl)[i]->p_mask == 0)) {
			continue;//
		}


		// Octets
		submask = ((*pl)[i]->p_mask<<bbits) | mask_8;
		subaddr = ((*pl)[i]->p_base | addr_8) & submask;
		pbits = nextlg(submask);
		if ((1u<<pbits) <= submask)
			pbits++;	// log_2 Octets

		if ((*pl)[i]->p_master_bus) {
			print_cachable(fp, (*pl)[i]->p_master_bus,
				dw, submask, subaddr);
			continue;
		} else if ((!(*pl)[i]->ismemory())
			&&(!issubbus(*(*pl)[i]->p_phash))) {
			// fprintf(fp, "\t\t// %s: Not a memory\n", (*pl)[i]->p_name->c_str());
			continue;
		}

		pbits -= dw;
		fprintf(fp, "\t\t// %s\n"
		"\t\tif ((i_addr[%d:0] & %d'h%0*x) == %d'h%0*x)\n"
				"\t\t\to_cachable = 1'b1;\n",
			(*pl)[i]->p_name->c_str(), pbits - 1,
			pbits, (pbits+3)/4, submask>>dw,
			pbits, (pbits+3)/4, subaddr>>dw);
	}
}

void build_cachable_core_v(MAPDHASH &master, MAPDHASH &busmaster,
		FILE *fp, STRING &fname) {
	// We come in here after a CACHEABLE.FILE = (*fname) key
	char	*modulename;
	const char *ptr;
	BUSINFO	*bi;
	int	dw;
	MAPDHASH	*bimap;

	bimap = getmap(busmaster, KYMASTER_BUS);
	if (bimap == NULL) {
		gbl_msg.error("Could not find MASTER.BUS key for LD script file %s\n", fname.c_str());
		return;
	}

	bi = find_bus(bimap);

	if (NULL == (ptr = strrchr(fname.c_str(),'/')))
		modulename = strdup(fname.c_str());
	else
		modulename = strdup(ptr+1);
	
	assert((strlen(modulename)>2)
		&& strcmp(&modulename[strlen(modulename)-2],".v")==0);

	modulename[strlen(modulename)-2] = '\0';

	legal_notice(master, fp, fname);
	fprintf(fp,
"`default_nettype none\n"
"//\n"
"module %s(i_addr, o_cachable);\n"
	"\tlocalparam		AW = %d;\n"
	"\tinput\twire\t[AW-1:0]\ti_addr;\n"
	"\toutput\treg\t\t\to_cachable;\n"
"\n", modulename, bi->address_width());
	free(modulename);

	fprintf(fp,
	"\talways @(*)\n"
	"\tbegin\n"
	"\t\to_cachable = 1'b0;\n");

	dw = bi->data_width();
	dw = nextlg(dw)-3;
	print_cachable(fp, bi, dw, 0, 0);
	
	fprintf(fp,
	"\tend\n"
"\n"
"endmodule\n");
}

void build_cachable_v(MAPDHASH &master, STRINGP subd) {
	FILE	*fp;
	MAPDHASH::iterator	kvpair;
	STRINGP	fnamep;

	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		fnamep = getstring(kvpair->second.u.m_m, KYCACHABLE_FILE);
		if (fnamep == NULL)
			// No cachable file tag, ignore this component
			continue;

		// A cachable file tag can only be found within a bus master
		// component
		if (fnamep->size() < 3) {
			gbl_msg.error("Cachable filename is too short\n",
				kvpair->first.c_str());
			continue;
		} else if ((*fnamep)[0] == '/') {
			gbl_msg.error("Cowardly refusing to write cachable with an absolute pathname, %s\n",
				fnamep->c_str());
			continue;
		}

		STRING	fname = (*subd) + "/" + (*fnamep);
		if (strcmp(&fname.c_str()[fname.size()-2],".v") != 0)
			fname += ".v";
		fp = fopen(fname.c_str(), "w");
		if (fp == NULL)
			gbl_msg.error("Could not write cachable file: %s\n", fname.c_str());
		else {
			build_cachable_core_v(master, *kvpair->second.u.m_m,
				fp, fname);
			fclose(fp);
		}
	}
}
