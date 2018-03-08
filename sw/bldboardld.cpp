////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bldboardld.cpp
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
#include "businfo.h"
#include "globals.h"
#include "gather.h"
#include "msgs.h"

void	build_board_ld(   MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP		strp;
	int		reset_address;
	PERIPHP		fastmem = NULL, bigmem = NULL, bootmem = NULL;
	APLIST		*alist;

	legal_notice(master, fp, fname, "/*******************************************************************************", "*");
	fprintf(fp, "*/\n");

	fprintf(fp, "ENTRY(_start)\n\n");

	alist = full_gather();

	fprintf(fp, "MEMORY\n{\n");
	for(unsigned i=0; i<alist->size(); i++) {
		PERIPHP	p = (*alist)[i];
		STRINGP	name = getstring(*p->p_phash, KYLD_NAME),
			perm = getstring(*p->p_phash, KYLD_PERM);

		if (!ismemory(*p->p_phash))
			continue;

		if (NULL == name)
			name = p->p_name;
		fprintf(fp,"\t%8s(%2s) : ORIGIN = 0x%08lx, LENGTH = 0x%08x\n",
			name->c_str(), (perm)?(perm->c_str()):"r",
			p->p_regbase,
			(p->naddr()*(p->p_slave_bus->data_width()/8)));

		if (tolower(perm->c_str()[0]) != 'w') {
			// Read only memory must be flash
			if (!bootmem)
				bootmem = p;
			else if ((bootmem)&&(KYFLASH.compare(*name)==0))
				// Unless flash is explicitly named
				bootmem = p;
		} else {
			// Find our bigest (and fastest?) memories
			if (!bigmem)
				bigmem = p;
			else if ((bigmem)&&(p->naddr() > bigmem->naddr())) {
				bigmem = p;
			}
		}
	}
	fprintf(fp, "}\n\n");

	// Define pointers to these memories
	for(unsigned i=0; i<alist->size(); i++) {
		PERIPHP	p = (*alist)[i];
		STRINGP	name = getstring(*p->p_phash, KYLD_NAME);

		if (!ismemory(*p->p_phash))
			continue;

		if (NULL == name)
			name = p->p_name;

		fprintf(fp, "_%-8s = ORIGIN(%s);\n",
			name->c_str(), name->c_str());
	}

	if (NULL != (strp = getstring(master, KYLD_DEFNS)))
		fprintf(fp, "%s\n", strp->c_str());
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (NULL != (strp = getstring(kvpair->second, KYLD_DEFNS)))
			fprintf(fp, "%s\n", strp->c_str());
	}

	if (!getvalue(master, KYRESET_ADDRESS, reset_address)) {
		bool	found = false;
		for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
			if (kvpair->second.m_typ != MAPT_MAP)
				continue;
			if (getvalue(*kvpair->second.u.m_m, KYRESET_ADDRESS, reset_address)) {
				found = true;
				break;
			}
		} if (!found) {
			if (bootmem) {
				reset_address = bootmem->p_regbase;
			} else {
				for(unsigned i=0; i<alist->size(); i++) {
					PERIPHP	p = (*alist)[i];
					if (!ismemory(*p->p_phash))
						continue;
					STRINGP	name = getstring(*p->p_phash, KYLD_NAME);
					if (NULL == name) {
						bootmem = p;
						name = p->p_name;
					} if (KYFLASH.compare(*name) == 0) {
						reset_address = p->p_regbase;
						bootmem = p;
						found = true;
						break;
					}
				}
			}
		} if (!found) {
			reset_address = 0;
			gbl_msg.warning("WARNING: RESET_ADDRESS NOT FOUND\n");
		}
	}

	if ((reset_address != 0)&&(!bootmem)) {
		// We have a boot memory, but just don't know what it is.
		// Let's go find it
		for(unsigned i=0; i<alist->size(); i++) {
			PERIPHP	p = (*alist)[i];

			// If its not a memory, then we can't boot from it
			if (!ismemory(*p->p_phash))
				continue;

			// If the reset address comes before this address,
			// then its not the right peripheral
			if ((unsigned)reset_address < p->p_regbase)
				continue;
			// Likewise if the reset address comes after this
			// peripheral, this peripheral isn't it either
			else if ((unsigned)reset_address
				>= p->p_regbase + (p->naddr()
					*(p->p_slave_bus->data_width()/8)))
				continue;

			// Otherwise we just found it.
			bootmem = p;
			break;
		}
	}

	if (!bootmem) {
		gbl_msg.warning("WARNING: No boot device, abandoning board.ld\n");
	} else {

		fprintf(fp, "SECTIONS\n{\n");
		fprintf(fp, "\t.rocode 0x%08x : ALIGN(4) {\n"
			"\t\t_boot_address = .;\n"
			"\t\t*(.start) *(.boot)\n", reset_address);
		fprintf(fp, "\t} > %s\n\t_kernel_image_start = . ;\n",
			bootmem->p_name->c_str());
		if ((fastmem)&&(fastmem != bigmem)) {
			STRINGP	name = getstring(*fastmem->p_phash, KYLD_NAME);
			if (!name)
			name = fastmem->p_name;
			fprintf(fp, "\t.fastcode : ALIGN_WITH_INPUT {\n"
					"\t\t*(.kernel)\n"
					"\t\t_kernel_image_end = . ;\n"
					"\t\t*(.start) *(.boot)\n");
			fprintf(fp, "\t} > %s", name->c_str());
			if (bootmem != fastmem)
				fprintf(fp, " AT>%s", bootmem->p_name->c_str());
			fprintf(fp, "\n");
		} else {
			fprintf(fp, "\t_kernel_image_end = . ;\n");
		}

		if (bigmem) {
			STRINGP	name = getstring(*bigmem->p_phash, KYLD_NAME);
			if (!name)
				name = bigmem->p_name;
			fprintf(fp, "\t_ram_image_start = . ;\n");
			fprintf(fp, "\t.ramcode : ALIGN_WITH_INPUT {\n");
			if ((!fastmem)||(fastmem == bigmem))
				fprintf(fp, "\t\t*(.kernel)\n");
			fprintf(fp, ""
				"\t\t*(.text.startup)\n"
				"\t\t*(.text*)\n"
				"\t\t*(.rodata*) *(.strings)\n"
				"\t\t*(.data) *(COMMON)\n"
			"\t\t}> %s", bigmem->p_name->c_str());
			if (bootmem != bigmem)
				fprintf(fp, " AT> %s", bootmem->p_name->c_str());
			fprintf(fp, "\n\t_ram_image_end = . ;\n"
				"\t.bss : ALIGN_WITH_INPUT {\n"
					"\t\t*(.bss)\n"
					"\t\t_bss_image_end = . ;\n"
					"\t\t} > %s\n",
				bigmem->p_name->c_str());
		}

		fprintf(fp, "\t_top_of_heap = .;\n");
		fprintf(fp, "}\n");
	}
}

