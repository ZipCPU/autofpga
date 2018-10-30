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
#include "plist.h"
#include "bldregdefs.h"
#include "businfo.h"
#include "subbus.h"
#include "globals.h"
#include "msgs.h"

typedef	std::vector<PERIPHP>	APLIST;

void	gather_peripherals(APLIST *alist, BUSINFO *bus, PLIST *plist) {
	if (NULL == plist)
		return;

	for(unsigned k=0; k<plist->size(); k++) {
		MAPDHASH	*ph = (*plist)[k]->p_phash;
		unsigned base;
		if (!bus->get_base_address(ph, base))
			base = 0;
		alist->push_back((*plist)[k]);
		if (base != 0) {
			setvalue(*ph, KYREGBASE, base);
			reeval(gbl_hash);
		}
		(*plist)[k]->p_regbase = base;
		if ((NULL != (*plist)[k]->p_master_bus)
			&&((*plist)[k]->p_master_bus != (*plist)[k]->p_slave_bus)
			&&((*plist)[k]->p_master_bus != bus)) {
			BUSINFO	*subbus;

			subbus = (*plist)[k]->p_master_bus;
			gather_peripherals(alist, bus, subbus->m_slist);
			gather_peripherals(alist, bus, subbus->m_dlist);
			gather_peripherals(alist, bus, subbus->m_plist);
		}
	}
}

APLIST	*gather_peripherals(BUSINFO *bus) {
	APLIST	*alist = new APLIST;

	gather_peripherals(alist, bus, bus->m_plist);

	return alist;
}

APLIST *full_gather(void) {
	STRINGP	strp;
	BUSINFO	*bi;
	APLIST	*alist;

        strp = getstring(*gbl_hash, KYREGISTER_BUS);
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
