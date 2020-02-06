////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	gather.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	To gather all peripherals together, using a given bus to do so
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017-2020, Gisselquist Technology, LLC
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
#include "plist.h"
#include "bldregdefs.h"
#include "businfo.h"
#include "subbus.h"
#include "globals.h"
#include "msgs.h"
#include "predicates.h"

typedef	std::vector<PERIPHP>	APLIST;

void	gather_peripherals(APLIST *alist, BUSINFO *bus, PLIST *plist, unsigned base) {
	if (NULL == plist) {
		gbl_msg.warning("Sub-bus %s has no peripherals\n", bus->name()->c_str());
		return;
	}

	for(unsigned k=0; k<plist->size(); k++) {
		MAPDHASH	*ph = (*plist)[k]->p_phash;
		unsigned	addr;

		// unsigned base = 0;
		if (!bus->get_base_address(ph, addr))
			addr = 0;
		alist->push_back((*plist)[k]);
		if (addr+base != 0) {
			setvalue(*ph, KYREGBASE, addr+base);
			reeval(gbl_hash);
		}
		(*plist)[k]->p_regbase = addr+base;
		if (isarbiter(*ph)) {
			BUSINFO	*subbus;

			subbus = (*plist)[k]->p_master_bus;
			assert(subbus);

			gather_peripherals(alist, subbus, subbus->m_plist, addr+base);
		}
	}
}

APLIST	*gather_peripherals(BUSINFO *bus) {
	APLIST	*alist = new APLIST;

	gather_peripherals(alist, bus, bus->m_plist, 0);

	return alist;
}

APLIST *full_gather(void) {
	STRINGP	strp;
	BUSINFO	*bi;
	APLIST	*alist;

        strp = getstring(*gbl_hash, KYREGISTER_BUS_NAME);
        if (NULL == strp) {
		gbl_msg.warning("No REGISTER.BUS defined, assuming a register bus of \"wbu\".\n");
                strp = new STRING("wbu");
        } bi = find_bus(strp);
        if (NULL == bi) {
		gbl_msg.warning("Register bus %s not found, switching to default\n",
                strp->c_str());
                bi = find_bus((STRINGP)NULL);
        }
        if(!bi) {
		gbl_msg.error("No bus found for register definitions\n");
                return NULL;
        }

        // Get the list of peripherals
        alist = gather_peripherals(bi);
	sort(alist->begin(), alist->end(), compare_regaddr);
        return alist;
}
