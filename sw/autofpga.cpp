////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	autofpga.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	This is the main/master program for the autofpga project.  All
//		other components server this one.
//
//	The purpose of autofpga.cpp is to read a group of configuration files
//	(.txt currently), and to generate code from those files to connect
//	the various parts and pieces within them into a design.
//
//	Currently that design is dependent upon the Wishbone B4/Pipelined
//	bus, but that's likely to change in the future.
//
//	Design files produced include:
//
//	toplevel.v
//	main.v
//	regdefs.h
//	regdefs.cpp
//	board.h		(Built, but ... not yet tested)
//	* dev.tex	(Not yet included)
//	* kernel device tree file
//
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

//
// The ILIST, a list of interrupt lines within the design
//
//
class INTINFO {
public:
	STRINGP		i_name;
	STRINGP		i_wire;
	unsigned	i_id;
	MAPDHASH	*i_hash;
	INTINFO(void) { i_name = NULL; i_wire = NULL; i_id = 0; }
	INTINFO(STRINGP nm, STRINGP wr, unsigned id)
		: i_name(nm), i_wire(wr), i_id(id) {}
	INTINFO(STRING &nm, STRING &wr, unsigned id) : i_id(id) {
		i_name = new STRING(nm);
		i_wire = new STRING(wr);
	}
};
typedef	INTINFO	INTID, *INTP;
typedef	std::vector<INTP>	ILIST;


//
// The PICINFO structure, one that keeps track of all of the information used
// by a given Programmable Interrupt Controller (PIC)
//
//
class PICINFO {
public:
	STRINGP		i_name,	// The name of this PIC
			// i_bus is the name of a vector, that is ... a series
			// of wires, composed of the interrupts going to this
			// controller
			i_bus;
	unsigned	i_max,	// Max # of interrupts this controller can handle
			// i_nassigned is the number of interrupts that have
			// been given assignments (positions) to our interrupt
			// vector
			i_nassigned,
			// i_nallocated references the number of interrupts that
			// this controller knows about.  This is separate from
			// the number of interrupts that have positions assigned
			// to them
			i_nallocated;
	//
	// The list of all interrupts this controller can handle
	//
	ILIST		i_ilist;
	//
	// The list of assigned interrupts
	INTID		**i_alist;
	PICINFO(MAPDHASH &pic) {
		i_max = 0;
		int	mx = 0;
		i_name = getstring(pic, KYPREFIX);
		i_bus  = getstring(pic, KYPIC_BUS);
		if (getvalue( pic, KYPIC_MAX, mx)) {
			i_max = mx;
			i_alist = new INTID *[mx];
			for(int i=0; i<mx; i++)
				i_alist[i] = NULL;
		} else {
			fprintf(stderr, "ERR: Cannot find PIC.MAX within ...\n");
			gbl_err++;
			mapdump(stderr, pic);
			i_max = 0;
			i_alist = NULL;
		}
		i_nassigned  = 0;
		i_nallocated = 0;
	}

	// Add an interrupt with the given name, and hash source, to the table
	void add(MAPDHASH &psrc, STRINGP iname) {
		if ((!iname)||(i_nallocated >= i_max))
			return;

		INTP	ip = new INTID();
		i_ilist.push_back(ip);

		// Initialize the table entry with the name of this interrupt
		ip->i_name = trim(*iname);
		assert(ip->i_name->size() > 0);
		// The wire from the rest of the design to connect to this
		// interrupt bus
		ip->i_wire = getstring(psrc, KY_WIRE);
		// We'll set the ID of this interrupt to MAX, to indicate that
		// the interrupt has not been assigned yet
		ip->i_id   = i_max;
		ip->i_hash = &psrc;
		i_nallocated++;
	}

	// This is identical to the add function above, save that we are adding
	// an interrupt with a known position assignment within the controller.
	void add(unsigned id, MAPDHASH &psrc, STRINGP iname) {
		if (!iname) {
			fprintf(stderr, "WARNING: No name given for interrupt\n");
			return;
		} else if (id >= i_max) {
			fprintf(stderr, "ERR: Interrupt ID %d out of bounds [0..%d]\n",
				id, i_max);
			gbl_err++;
			return;
		} else if (i_nassigned+1 > i_max) {
			fprintf(stderr, "ERR: Too many interrupts assigned to %s\n", i_name->c_str());
			fprintf(stderr, "ERR: Assignment of %s ignored\n", iname->c_str());
			gbl_err++;
			return;
		}

		// Check to see if any other interrupt already has this
		// identifier, and write an error out if so.
		if (i_alist[id]) {
			fprintf(stderr, "ERR: %s and %s are both competing for the same interrupt ID, #%d\n",
				i_alist[id]->i_name->c_str(),
				iname->c_str(), id);
			fprintf(stderr, "ERR: interrupt %s dropped\n", iname->c_str());
			gbl_err++;
			return;
		}

		// Otherwise, add the interrupt to our list
		INTP	ip = new INTID();
		i_ilist.push_back(ip);
		ip->i_name = trim(*iname);
		assert(ip->i_name->size() > 0);
		ip->i_wire = getstring(psrc, KY_WIRE);
		ip->i_id   = id;
		ip->i_hash = &psrc;
		i_nassigned++;
		i_nallocated++;

		i_alist[id] = ip;
	}

	// Let's look through our list of interrupts for unassigned interrupt
	// values, and ... assign them
	void assignids(void) {
		for(unsigned iid=0; iid<i_max; iid++) {
			if (i_alist[iid])
				continue;
			unsigned mx = (i_ilist.size());
			unsigned	uid = mx;
			for(unsigned jid=0; jid < i_ilist.size(); jid++) {
				if (i_ilist[jid]->i_id >= i_max) {
					uid = jid;
					break;
				}
			}

			if (uid < mx) {
				// Assign this interrupt
				// 1. Give it an interrupt ID
				i_ilist[uid]->i_id = iid;
				// 2. Place it in our numbered interrupt list
				i_alist[iid] = i_ilist[uid];
				i_nassigned++;
			} else
				continue; // All interrupts assigned
		}
		if (i_max < i_ilist.size()) {
			fprintf(stderr, "WARNING: Too many interrupts assigned to PIC %s\n", i_name->c_str());
		}

		// Write the interrupt assignments back into the map
		for(unsigned iid=0; iid<i_ilist.size(); iid++) {
			if (i_ilist[iid]->i_id < i_max) {
				STRING	ky = (*i_name) + ".ID";
				setvalue(*i_ilist[iid]->i_hash, ky, i_ilist[iid]->i_id);
			}
		}
	}

	// Given an interrupt number associated with this PIC, lookup the
	// interrupt having that number, returning NULL on error or if not
	// found.
	INTP	getint(unsigned iid) {
		if ((iid < i_max)&&(NULL != i_alist[iid]))
			return i_alist[iid];
		return NULL;
	}
};
typedef	PICINFO	PICI, *PICP;
typedef	std::vector<PICP>	PICLIST;


// A list of all of our interrupt controllers
PICLIST	piclist;
unsigned	unusedmsk;


// Look up the number of bits in the address bus of the given hash
int	get_address_width(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	const int DEFAULT_BUS_ADDRESS_WIDTH = 30;	// In log_2 octets
	int	baw = DEFAULT_BUS_ADDRESS_WIDTH;

	if (getvalue(info, KYBUS_ADDRESS_WIDTH, baw))
		return baw;

	setvalue(info, KYBUS_ADDRESS_WIDTH, DEFAULT_BUS_ADDRESS_WIDTH);
	reeval(info);
	return DEFAULT_BUS_ADDRESS_WIDTH;
}

// Return the number of interrupt controllers within this design.  If no such
// field/tag exists, count the number and add it to the hash.
int count_pics(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair, kvpic;
	int	npics=0;

	if (getvalue(info, KYNPIC, npics))
		return npics;

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvpic = kvpair->second.u.m_m->find(KYPIC);
		if (kvpic != kvpair->second.u.m_m->end())
			npics++;
	}

	setvalue(info, KYNPIC, npics);
	return npics;
}


//
// assign_addresses
//
// Assign addresses to all of our peripherals, given a first address to start
// from.  The first address helps to insure that the NULL address creates a
// proper error (as it should).
//
/*
void assign_addresses(MAPDHASH &info, unsigned first_address = 0x400) {
	unsigned	start_address = first_address;
	MAPDHASH::iterator	kvpair;

	int np = count_peripherals(info);
	// int baw = count_peripherals(info);	// log_2 octets

	if (np < 1) {
		return;
	}

	if (gbl_dump)
		fprintf(gbl_dump, "// Assigning addresses to the S-LIST\n");

	// Assign bus addresses to the more generic peripherals
	if (gbl_dump) {
		fprintf(gbl_dump, "// Assigning addresses to the P-LIST (all other addresses)\n");
		fprintf(gbl_dump, "// Starting from %08x\n", start_address);
	}

	assign_addresses();
	reeval(info);
}
*/

void	assign_int_to_pics(const STRING &iname, MAPDHASH &ihash) {
	STRINGP	picname;
	int inum;

	// Now, we need to map this to a PIC
	if (NULL==(picname=getstring(ihash, KYPIC))) {
		// fprintf(stderr,
		//	"WARNING: No bus defined for INT_%s\nThis interrupt will not be connected.\n",
		//	kvpair->first.c_str());
		return;
	}

	char	*tok, *cpy;
	cpy = strdup(picname->c_str());
	tok = strtok(cpy, ", \t\n");
	while(tok) {
		unsigned pid;
		for(pid = 0; pid<piclist.size(); pid++)
			if (*piclist[pid]->i_name == tok)
				break;
		if (pid >= piclist.size()) {
			fprintf(stderr, "ERR: PIC NOT FOUND: %s\n", tok);
			gbl_err++;
		} else if (getvalue(ihash, KY_ID, inum)) {
			piclist[pid]->add((unsigned)inum, ihash, (STRINGP)&iname);
		} else {
			piclist[pid]->add(ihash, (STRINGP)&iname);
		}
		tok = strtok(NULL, ", \t\n");
	} free(cpy);
}

//
// assign_interrupts
//
// Find all the interrup controllers, and then find all the interrupts, and map
// interrupts to controllers.  Individual interrupt wires may be mapped to
// multiple interrupt controllers.
//
void	assign_interrupts(MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvint, kvline;
	MAPDHASH	*submap, *intmap;
	STRINGP		sintlist;

	// First step, gather all of our PIC's together
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (ispic(kvpair->second)) {
			PICP npic = new PICI(*kvpair->second.u.m_m);
			piclist.push_back(npic);
		}
	}

	// Okay, now we need to gather all of our interrupts together and to
	// assign them to PICs.
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;

		// submap now points to a component.  It can be any component.
		// We now need to check if it has an INT. hash.
		submap = kvpair->second.u.m_m;
		kvint = submap->find(KY_INT);
		if (kvint == submap->end()) {
			continue;
		} if (kvint->second.m_typ != MAPT_MAP) {
			continue;
		}

		// Now, let's look to see if it has a list of interrupts
		if (NULL != (sintlist = getstring(kvint->second,
					KYINTLIST))) {
			STRING	scpy = *sintlist;
			char	*tok;

			tok = strtok((char *)scpy.c_str(), " \t\n,");
			while(tok) {
				STRING	stok = STRING(tok);

				kvline = findkey(*kvint->second.u.m_m, stok);
				if (kvline != kvint->second.u.m_m->end())
					assign_int_to_pics(stok,
						*kvline->second.u.m_m);

				tok = strtok(NULL, " \t\n,");
			}
		} else {
			// NAME is now @comp.INT.

			// Yes, an INT hash exists within this component.
			// Hence the component has one (or more) interrupts.
			// Let's loop over those interrupts
			intmap = kvint->second.u.m_m;
			for(kvline=intmap->begin(); kvline != intmap->end();
						kvline++) {
				if (kvline->second.m_typ != MAPT_MAP)
					continue;
				assign_int_to_pics(kvline->first,
					*kvline->second.u.m_m);
			}
		}
	}

	// Now, let's assign everything that doesn't yet have any definitions
	for(unsigned picid=0; picid<piclist.size(); picid++)
		piclist[picid]->assignids();
	reeval(master);
	// mapdump(master);
}

void	writeout(FILE *fp, MAPDHASH &master, const STRING &ky) {
	MAPDHASH::iterator	kvpair;
	STRINGP	str;

	fprintf(fp, "// Looking for string: %s\n", ky.c_str());

	str = getstring(master, ky);
	if (NULL != str) {
		fprintf(fp, "%s", str->c_str());
	}

	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
				continue;
		str = getstring(kvpair->second, ky);
		if (str == NULL)
			continue;

		fprintf(fp, "%s", str->c_str());
	}
}

void	build_board_h(    MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRING	str, astr;
	STRINGP	defns;

	legal_notice(master, fp, fname);
	fprintf(fp, "#ifndef	BOARD_H\n#define\tBOARD_H\n");
	fprintf(fp, "\n");
	fprintf(fp, "// And, so that we can know what is and isn\'t defined\n");
	fprintf(fp, "// from within our main.v file, let\'s include:\n");
	fprintf(fp, "#include <design.h>\n\n");

	defns = getstring(master, KYBDEF_INCLUDE);
	if (defns)
		fprintf(fp, "%s\n\n", defns->c_str());
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(kvpair->second, KYBDEF_INCLUDE);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(kvpair->second, KYBDEF_DEFN);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(kvpair->second, KYBDEF_INSERT);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	osdef, osval, access;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		osdef = getstring(*kvpair->second.u.m_m, KYBDEF_OSDEF);
		osval = getstring(*kvpair->second.u.m_m, KYBDEF_OSVAL);

		if ((!osdef)&&(!osval))
			continue;
		access= getstring(kvpair->second, KYACCESS);

		if (access)
			fprintf(fp, "#ifdef\t%s\n", access->c_str());
		if (osdef)
			fprintf(fp, "#define\t%s\n", osdef->c_str());
		if (osval) {
			fputs(osval->c_str(), fp);
			if (osval->c_str()[strlen(osval->c_str())-1] != '\n')
				fputc('\n', fp);
		} if (access)
			fprintf(fp, "#endif\t// %s\n", access->c_str());
	}

	fprintf(fp, "//\n// Interrupt assignments (%ld PICs)\n//\n", piclist.size());
	for(unsigned pid = 0; pid<piclist.size(); pid++) {
		PICLIST::iterator picit = piclist.begin() + pid;
		PICP	pic = *picit;
		fprintf(fp, "// PIC: %s\n", pic->i_name->c_str());
		for(unsigned iid=0; iid< pic->i_max; iid++) {
			INTP	ip = pic->getint(iid);
			if (NULL == ip)
				continue;
			if (NULL == ip->i_name)
				continue;
			char	*buf = strdup(pic->i_name->c_str()), *ptr;
			for(ptr = buf; (*ptr); ptr++)
				*ptr = toupper(*ptr);
			fprintf(fp, "#define\t%s_%s\t%s(%d)\n",
				buf, ip->i_name->c_str(), buf, iid);
			free(buf);
		}
	}

	defns = getstring(master, KYBDEF_INSERT);
	if (defns)
		fprintf(fp, "%s\n\n", defns->c_str());
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(*kvpair->second.u.m_m, KYBDEF_INSERT);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	fprintf(fp, "#endif\t// BOARD_H\n");
}

void	build_board_ld(   MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	// STRINGP	strp;
	// int		reset_address;
	// PERIPHP		fastmem = NULL, bigmem = NULL;

	legal_notice(master, fp, fname, "/*******************************************************************************", "*");
	fprintf(fp, "*/\n");

/*
	std::sort(mlist.begin(), mlist.end(), compare_naddr);

	fprintf(fp, "ENTRY(_start)\n\n");

	fprintf(fp, "MEMORY\n{\n");
	for(unsigned i=0; i<mlist.size(); i++) {
		STRINGP	name = getstring(*mlist[i]->p_phash, KYLD_NAME),
			perm = getstring(*mlist[i]->p_phash, KYLD_PERM);

		if (NULL == name)
			name = mlist[i]->p_name;
		fprintf(fp,"\t%8s(%2s) : ORIGIN = 0x%08x, LENGTH = 0x%08x\n",
			name->c_str(), (perm)?(perm->c_str()):"r",
			mlist[i]->p_base, (mlist[i]->p_naddr<<2));

		// Find our bigest and fastest memories
		if (tolower(perm->c_str()[0]) != 'w')
			continue;
		if (!bigmem)
			bigmem = mlist[i];
		else if ((bigmem)&&(mlist[i]->p_naddr > bigmem->p_naddr)) {
			bigmem = mlist[i];
		}
	}
	fprintf(fp, "}\n\n");

	// Define pointers to these memories
	for(unsigned i=0; i<mlist.size(); i++) {
		STRINGP	name = getstring(*mlist[i]->p_phash, KYLD_NAME);
		if (NULL == name)
			name = mlist[i]->p_name;

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
			for(unsigned i=0; i<mlist.size(); i++) {
				STRINGP	name = getstring(*mlist[i]->p_phash, KYLD_NAME);
				if (NULL == name)
					name = mlist[i]->p_name;
				if (KYFLASH == *name) {
					reset_address = mlist[i]->p_base;
					found = true;
					break;
				}
			}
		} if (!found) {
			reset_address = 0;
			fprintf(stderr, "WARNING: RESET_ADDRESS NOT FOUND\n");
		}
	}

	fprintf(fp, "SECTIONS\n{\n");
	fprintf(fp, "\t.rocode 0x%08x : ALIGN(4) {\n"
			"\t\t_boot_address = .;\n"
			"\t\t*(.start) *(.boot)\n", reset_address);
	fprintf(fp, "\t} > flash\n\t_kernel_image_start = . ;\n");
	if ((fastmem)&&(fastmem != bigmem)) {
		STRINGP	name = getstring(*fastmem->p_phash, KYLD_NAME);
		if (!name)
			name = fastmem->p_name;
		fprintf(fp, "\t.fastcode : ALIGN_WITH_INPUT {\n"
				"\t\t*(.kernel)\n"
				"\t\t_kernel_image_end = . ;\n"
				"\t\t*(.start) *(.boot)\n");
		fprintf(fp, "\t} > %s AT>flash\n", name->c_str());
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
		"\t\t}> %s AT> flash\n", bigmem->p_name->c_str());
		fprintf(fp, "\t_ram_image_end = . ;\n"
			"\t.bss : ALIGN_WITH_INPUT {\n"
				"\t\t*(.bss)\n"
				"\t\t_bss_image_end = . ;\n"
				"\t\t} > %s\n",
			bigmem->p_name->c_str());
	}
*/
	fprintf(fp, "\t_top_of_heap = .;\n");
	fprintf(fp, "}\n");
}

void	build_latex_tbls( MAPDHASH &master) {
	// legal_notice(master, fp, fname);
#ifdef	NEW_FILE_FORMAT
	printf("\n\n\n// TO BE PLACED INTO doc/src\n");

	for(int i=0; i<np; i++) {
		printf("\n\n\n// TO BE PLACED INTO doc/src/%s.tex\n",
			bus[i].b_data.);
	}
	printf("\n\n\n// TO BE PLACED INTO doc/src\n");
#endif
}

//
// void	build_device_tree(MAPDHASH &master)
//
void	build_toplevel_v( MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str = "ACCESS", astr;
	int	first;

	legal_notice(master, fp, fname);
	fprintf(fp, "`default_nettype\tnone\n");
	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	fprintf(fp, "\n\n");

	// Define our external ports within a port list
	fprintf(fp, "//\n"
	"// Here we declare our toplevel.v (toplevel) design module.\n"
	"// All design logic must take place beneath this top level.\n"
	"//\n"
	"// The port declarations just copy data from the @TOP.PORTLIST\n"
	"// key, or equivalently from the @MAIN.PORTLIST key if\n"
	"// @TOP.PORTLIST is absent.  For those peripherals that don't need\n"
	"// any top level logic, the @MAIN.PORTLIST should be sufficent,\n"
	"// so the @TOP.PORTLIST key may be left undefined.\n"
	"//\n");
	fprintf(fp, "module\ttoplevel(i_clk,\n");
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp;

		strp = getstring(*kvpair->second.u.m_m, KYTOP_PORTLIST);
		if (!strp)
			strp = getstring(*kvpair->second.u.m_m, KYMAIN_PORTLIST);
		if (!strp)
			continue;

		STRING	tmps(*strp);
		STRING::iterator si;
		for(si=tmps.end()-1; si>=tmps.begin(); si--)
			if (isspace(*si))
				*si = '\0';
			else
				break;
		if (tmps.size() == 0)
			continue;
		if (!first)
			fprintf(fp, ",\n");
		first=0;
		fprintf(fp, "%s", tmps.c_str());
	} fprintf(fp, ");\n");

	// External declarations (input/output) for our various ports
	fprintf(fp, "\t//\n"
	"\t// Declaring our input and output ports.  We listed these above,\n"
	"\t// now we are declaring them here.\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.IODECLS key,\n"
	"\t// or from the @MAIN.IODECL key if @TOP.IODECL is absent.  For\n"
	"\t// those peripherals that don't do anything at the top level,\n"
	"\t// the @MAIN.IODECL key should be sufficient, so the @TOP.IODECL\n"
	"\t// key may be left undefined.\n"
	"\t//\n");
	fprintf(fp, "\tinput\twire\t\ti_clk;\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;

		STRINGP strp = getstring(*kvpair->second.u.m_m, KYTOP_IODECL);
		if (!strp)
			strp = getstring(*kvpair->second.u.m_m, KYMAIN_IODECL);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}

	// Declare peripheral data
	fprintf(fp, "\n\n");
	fprintf(fp, "\t//\n"
	"\t// Declaring component data, internal wires and registers\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.DEFNS key\n"
	"\t// within the component data files.\n"
	"\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, KYTOP_DEFNS);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}

	fprintf(fp, "\n\n");
	fprintf(fp, ""
	"\t//\n"
	"\t// Time to call the main module within main.v.  Remember, the purpose\n"
	"\t// of the main.v module is to contain all of our portable logic.\n"
	"\t// Things that are Xilinx (or even Altera) specific, or for that\n"
	"\t// matter anything that requires something other than on-off logic,\n"
	"\t// such as the high impedence states required by many wires, is\n"
	"\t// kept in this (toplevel.v) module.  Everything else goes in\n"
	"\t// main.v.\n"
	"\t//\n"
	"\t// We automatically place s_clk, and s_reset here.  You may need\n"
	"\t// to define those above.  (You did, didn't you?)  Other\n"
	"\t// component descriptions come from the keys @TOP.MAIN (if it\n"
	"\t// exists), or @MAIN.PORTLIST if it does not.\n"
	"\t//\n");
	fprintf(fp, "\n\tmain\tthedesign(s_clk, s_reset,\n");
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;

		STRINGP strp = getstring(*kvpair->second.u.m_m, KYTOP_MAIN);
		if (!strp)
			strp = getstring(*kvpair->second.u.m_m, KYMAIN_PORTLIST);
		if (!strp)
			continue;

		if (!first)
			fprintf(fp, ",\n");
		first=0;
		STRING	tmps(*strp);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		fprintf(fp, "%s", tmps.c_str());
	} fprintf(fp, ");\n");

	fprintf(fp, "\n\n"
	"\t//\n"
	"\t// Our final section to the toplevel is used to provide all of\n"
	"\t// that special logic that couldnt fit in main.  This logic is\n"
	"\t// given by the @TOP.INSERT tag in our data files.\n"
	"\t//\n\n\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP strp = getstring(*kvpair->second.u.m_m, KYTOP_INSERT);
		if (!strp)
			continue;
		fprintf(fp, "%s\n", strp->c_str());
	}

	fprintf(fp, "\n\nendmodule // end of toplevel.v module definition\n");

}


void	mkselect(FILE *fp, MAPDHASH &master, const STRING &subsel) {
/*
	const	char	*addrbus = "wb_addr";
	int	sbaw;

	BUSINFO	*bi = find_bus(new STRING("wb"));
	plist = bi->m_plist;
	sbaw  = bi->address_width();

	// use_skipaddr = false;
	sbaw = get_address_width(master) - 2;	// words

	for(unsigned i=0; i<plist.size(); i++) {
		const char	*pfx = plist[i]->p_name->c_str();

		fprintf(fp, "// \tassign\t%12s_sel = ", pfx);
		fprintf(fp, ");\n");
	}
*/
}

void	build_main_v(     MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str, astr, sellist, acklist, siosel_str, diosel_str;
	int		first;
	unsigned	nsel = 0, nacks = 0, baw;

	legal_notice(master, fp, fname);
	baw = get_address_width(master);

	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	fprintf(fp, "`default_nettype\tnone\n");

	build_access_ifdefs_v(master, fp);

	fprintf(fp, "//\n//\n");
	fprintf(fp,
"// Finally, we define our main module itself.  We start with the list of\n"
"// I/O ports, or wires, passed into (or out of) the main function.\n"
"//\n"
"// These fields are copied verbatim from the respective I/O port lists,\n"
"// from the fields given by @MAIN.PORTLIST\n"
"//\n");

	// Define our external ports within a port list
	fprintf(fp, "module\tmain(i_clk, i_reset,\n");
	str = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, str);
		if (!strp)
			continue;
		if (!first)
			fprintf(fp, ",\n");
		first=0;
		STRING	tmps(*strp);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		fprintf(fp, "%s", tmps.c_str());
	} fprintf(fp, ");\n");

	fprintf(fp, "//\n" "// Any parameter definitions\n//\n"
		"// These are drawn from anything with a MAIN.PARAM definition.\n"
		"// As they aren\'t connected to the toplevel at all, it would\n"
		"// be best to use localparam over parameter, but here we don\'t\n"
		"// check\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp = getstring(kvpair->second, KYMAIN_PARAM);
		if (!strp)
			continue;
		fprintf(fp, "%s", strp->c_str());
	}

	fprintf(fp, "//\n"
"// The next step is to declare all of the various ports that were just\n"
"// listed above.  \n"
"//\n"
"// The following declarations are taken from the values of the various\n"
"// @MAIN.IODECL keys.\n"
"//\n");

// #warning "How do I know these will be in the same order?
//	They have been--because the master always reads its subfields in the
//	same order.
//
	// External declarations (input/output) for our various ports
	fprintf(fp, "\tinput\twire\t\ti_clk, i_reset;\n");
	str = "MAIN.IODECL";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		strp = getstring(*kvpair->second.u.m_m, str);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}

	// Declare Bus master data
	fprintf(fp, "\n\n");
	fprintf(fp, "\t//\n\t// Declaring wishbone master bus data\n\t//\n");
	fprintf(fp, "\twire\t\twb_cyc, wb_stb, wb_we, wb_stall, wb_err;\n");
	if (DELAY_ACK) {
		fprintf(fp, "\treg\twb_ack;\t// ACKs delayed by extra clock\n");
	} else {
		fprintf(fp, "\twire\twb_ack;\n");
	}
	fprintf(fp, "\twire\t[(%d-1):0]\twb_addr;\n", baw);
	fprintf(fp, "\twire\t[31:0]\twb_data;\n");
	fprintf(fp, "\treg\t[31:0]\twb_idata;\n");
	fprintf(fp, "\twire\t[3:0]\twb_sel;\n");

	fprintf(fp, "\n\n");

	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring interrupt lines\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @INT.<interrupt name>.WIRE key.\n\t//\n");
	// Define any interrupt wires
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		MAPDHASH::iterator	kvint, kvsub, kvwire;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvint = kvpair->second.u.m_m->find(KY_INT);
		if (kvint == kvpair->second.u.m_m->end())
			continue;
		if (kvint->second.m_typ != MAPT_MAP)
			continue;
		for(kvsub=kvint->second.u.m_m->begin();
				kvsub != kvint->second.u.m_m->end(); kvsub++) {
			if (kvsub->second.m_typ != MAPT_MAP)
				continue;


			// Cant use getstring() here, 'cause we need the name of
			// the wire below
			kvwire = kvsub->second.u.m_m->find(KY_WIRE);
			if (kvwire == kvsub->second.u.m_m->end())
				continue;
			if (kvwire->second.m_typ != MAPT_STRING)
				continue;
			if (kvwire->second.u.m_s->size() == 0)
				continue;
			fprintf(fp, "\twire\t%s;\t// %s.%s.%s.%s\n",
				kvwire->second.u.m_s->c_str(),
				kvpair->first.c_str(),
				kvint->first.c_str(),
				kvsub->first.c_str(),
				kvwire->first.c_str());
		}
	}


	// Declare bus-master data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring Bus-Master data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, for those components with\n"
	"\t// an MTYPE flag.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (!isbusmaster(kvpair->second))
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare peripheral data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring Peripheral data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, for those components with a\n"
	"\t// PTYPE key but no MTYPE key.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (isbusmaster(kvpair->second))
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare other data
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring other data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key, but which have neither PTYPE\n"
	"\t// nor MTYPE keys.\n\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defnstr;
		if (isbusmaster(kvpair->second))
			continue;
		if (isperipheral(kvpair->second))
			continue;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defnstr = getstring(*kvpair->second.u.m_m, KYMAIN_DEFNS);
		if (defnstr)
			fprintf(fp, "%s", defnstr->c_str());
	}

	// Declare interrupt vector wires.
	fprintf(fp,
	"\n\n"
	"\t//\n\t// Declaring interrupt vector wires\n\t//\n"
	"\t// These declarations come from the various components having\n"
	"\t// PIC and PIC.MAX keys.\n\t//\n");
	for(unsigned picid=0; picid < piclist.size(); picid++) {
		STRINGP	defnstr, vecstr;
		MAPDHASH *picmap;

		if (piclist[picid]->i_max <= 0)
			continue;
		picmap = getmap(master, *piclist[picid]->i_name);
		if (!picmap)
			continue;
		vecstr = getstring(*picmap, KYPIC_BUS);
		if (vecstr) {
			fprintf(fp, "\twire\t[%d:0]\t%s;\n",
					piclist[picid]->i_max-1,
					vecstr->c_str());
		} else {
			defnstr = piclist[picid]->i_name;
			if (defnstr)
				fprintf(fp, "\twire\t[%d:0]\t%s_int_vec;\n",
					piclist[picid]->i_max,
					defnstr->c_str());
		}
	}

	// Declare wishbone lines
	fprintf(fp, "\n"
	"\t// Declare those signals necessary to build the bus, and detect\n"
	"\t// bus errors upon it.\n"
	"\t//\n"
	"\twire\tnone_sel;\n"
	"\treg\tmany_sel, many_ack;\n"
	"\treg\t[31:0]\tr_bus_err;\n");
	fprintf(fp, "\n"
	"\t//\n"
	"\t// Wishbone master wire declarations\n"
	"\t//\n"
	"\t// These are given for every configuration file with an\n"
	"\t// @SLAVE.MASTER tag, and the names are prefixed by whatever\n"
	"\t// is in the @PREFIX tag.\n"
	"\t//\n"
	"\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (!isbusmaster(kvpair->second))
			continue;

		STRINGP	strp = getstring(*kvpair->second.u.m_m, KYPREFIX);
		const char	*pfx = strp->c_str();

		fprintf(fp, "\twire\t\t%s_cyc, %s_stb, %s_we, %s_ack, %s_stall, %s_err;\n",
			pfx, pfx, pfx, pfx, pfx, pfx);
		fprintf(fp, "\twire\t[(%d-1):0]\t%s_addr;\n", baw, pfx);
		fprintf(fp, "\twire\t[31:0]\t%s_data, %s_idata;\n", pfx, pfx);
		fprintf(fp, "\twire\t[3:0]\t%s_sel;\n", pfx);
		fprintf(fp, "\n");
	}

	fprintf(fp, "\n"
	"\t//\n"
	"\t// Wishbone slave wire declarations\n"
	"\t//\n"
	"\t// These are given for every configuration file with a @PTYPE\n"
	"\t// tag, and the names are given by the @PREFIX tag.\n"
	"\t//\n"
	"\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(*kvpair->second.u.m_m))
			continue;

		STRINGP	strp = getstring(*kvpair->second.u.m_m, KYPREFIX);
		const char	*pfx = strp->c_str();

		fprintf(fp, "\twire\t%s_ack, %s_stall, %s_sel;\n", pfx, pfx, pfx);
		fprintf(fp, "\twire\t[31:0]\t%s_data;\n", pfx);
		fprintf(fp, "\n");
	}

	// Define the select lines
	fprintf(fp, "\n"
	"\t// Wishbone peripheral address decoding\n"
	"\t// This particular address decoder decodes addresses for all\n"
	"\t// peripherals (components with a @PTYPE tag), based upon their\n"
	"\t// NADDR (number of addresses required) tag\n"
	"\t//\n");
	fprintf(fp, "\n");

	// mkselect(fp, master, plist, STRING(""), "wb_skip");

	PLIST	*plist;
	plist = find_bus(new STRING("wb"))->m_plist;

	if (plist->size()>0) {
		if (sellist == "")
			sellist = (*(*plist)[0]->p_name) + "_sel";
		else
			sellist = (*(*plist)[0]->p_name) + "_sel, " + sellist;
	} for(unsigned i=1; i<plist->size(); i++)
		sellist = (*(*plist)[i]->p_name)+"_sel, "+sellist;
	nsel += plist->size();

	//
	mkselect2(fp, master);

	// Define none_sel
	fprintf(fp, "\tassign\tnone_sel = (wb_stb)&&({ ");
	fprintf(fp, "%s", sellist.c_str());
	fprintf(fp, "} == 0);\n");

	fprintf(fp, ""
	"\t//\n"
	"\t// many_sel\n"
	"\t//\n"
	"\t// This should *never* be true .... unless the address decoding logic\n"
	"\t// is somehow broken.  Given that a computer is generating the\n"
	"\t// addresses, that should never happen.  However, since it has\n"
	"\t// happened to me before (without the computer), I test/check for it\n"
	"\t// here.\n"
	"\t//\n"
	"\t// Devices are placed here as a natural result of the address\n"
	"\t// decoding logic.  Thus, any device with a sel_ line will be\n"
	"\t// tested here.\n"
	"\t//\n");
	fprintf(fp, "`ifdef\tVERILATOR\n\n");

	fprintf(fp, "\talways @(*)\n");
	fprintf(fp, "\t\tcase({%s})\n", sellist.c_str());
	fprintf(fp, "\t\t\t%d\'h0: many_sel = 1\'b0;\n", nsel);
	for(unsigned i=0; i<nsel; i++) {
		fprintf(fp, "\t\t\t%d\'b", nsel);
		for(unsigned j=0; j<nsel; j++)
			fprintf(fp, (i==j)?"1":"0");
		fprintf(fp, ": many_sel = 1\'b0;\n");
	}
	fprintf(fp, "\t\t\tdefault: many_sel = (wb_stb);\n");
	fprintf(fp, "\t\tendcase\n");

	fprintf(fp, "\n`else\t// VERILATOR\n\n");

	fprintf(fp, "\talways @(*)\n");
	fprintf(fp, "\t\tcase({%s})\n", sellist.c_str());
	fprintf(fp, "\t\t\t%d\'h0: many_sel <= 1\'b0;\n", nsel);
	for(unsigned i=0; i<nsel; i++) {
		fprintf(fp, "\t\t\t%d\'b", nsel);
		for(unsigned j=0; j<nsel; j++)
			fprintf(fp, (i==j)?"1":"0");
		fprintf(fp, ": many_sel <= 1\'b0;\n");
	}
	fprintf(fp, "\t\t\tdefault: many_sel <= (wb_stb);\n");
	fprintf(fp, "\t\tendcase\n");

	fprintf(fp, "\n`endif\t// VERILATOR\n\n");




	// Build a list of ACK signals
	acklist = ""; nacks = 0;
	if (plist->size() > 0) {
		if (nacks > 0)
			acklist = (*(*plist)[0]->p_name) + "_ack, " + acklist;
		else
			acklist = (*(*plist)[0]->p_name) + "_ack";
		nacks++;
	} for(unsigned i=1; i < plist->size(); i++) {
		acklist = (*(*plist)[i]->p_name) + "_ack, " + acklist;
		nacks++;
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// many_ack\n"
	"\t//\n"
	"\t// It is also a violation of the bus protocol to produce multiply\n"
	"\t// acks at once and on the same clock.  In that case, the bus\n"
	"\t// can\'t decide which result to return.  Worse, if someone is waiting\n"
	"\t// for a return value, that value will never come since another ack\n"
	"\t// masked it.\n"
	"\t//\n"
	"\t// The other error that isn\'t tested for here, no would I necessarily\n"
	"\t// know how to test for it, is when peripherals return values out of\n"
	"\t// order.  Instead, I propose keeping that from happening by\n"
	"\t// guaranteeing, in software, that two peripherals are not accessed\n"
	"\t// immediately one after the other.\n"
	"\t//\n");
	{
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\t\tcase({%s})\n", acklist.c_str());
		fprintf(fp, "\t\t\t%d\'h0: many_ack <= 1\'b0;\n", nacks);
		for(unsigned i=0; i<nacks; i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":"0");
			fprintf(fp, ": many_ack <= 1\'b0;\n");
		}
		fprintf(fp, "\t\tdefault: many_ack <= (wb_cyc);\n");
		fprintf(fp, "\t\tendcase\n");
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// wb_ack\n"
	"\t//\n"
	"\t// The returning wishbone ack is equal to the OR of every component that\n"
	"\t// might possibly produce an acknowledgement, gated by the CYC line.  To\n"
	"\t// add new components, OR their acknowledgements in here.\n"
	"\t//\n"
	"\t// To return an ack here, a component must have a @PTYPE.  Acks from\n"
	"\t// any @PTYPE SINGLE and DOUBLE components have been collected\n"
	"\t// together into sio_ack and dio_ack respectively, which will appear.\n"
	"\t// ahead of any other device acks.\n"
	"\t//\n");

	if (DELAY_ACK) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\twb_ack <= "
			"(wb_cyc)&&(|{%s});\n\n\n", acklist.c_str());
	} else {
		if (acklist.size() > 0) {
			fprintf(fp, "\tassign\twb_ack = (wb_cyc)&&(|{%s});\n\n\n",
				acklist.c_str());
		} else
			fprintf(fp, "\tassign\twb_ack = 1\'b0;\n\n\n");
	}


	// Define the stall line
	if (plist->size() > 0) {
		fprintf(fp,
	"\t//\n"
	"\t// wb_stall\n"
	"\t//\n"
	"\t// The returning wishbone stall line really depends upon what device\n"
	"\t// is requested.  Thus, if a particular device is selected, we return \n"
	"\t// the stall line for that device.\n"
	"\t//\n"
	"\t// Stall lines come from any component with a @PTYPE key and a\n"
	"\t// @NADDR > 0.  Since those components of @PTYPE SINGLE or DOUBLE\n"
	"\t// are not allowed to stall, they have been removed from this list\n"
	"\t// here for simplicity.\n"
	"\t//\n");

		fprintf(fp, "\tassign\twb_stall = \n"
			"\t\t  (wb_stb)&&(%6s_sel)&&(%6s_stall)",
			(*plist)[0]->p_name->c_str(), (*plist)[0]->p_name->c_str());
		for(unsigned i=1; i<plist->size(); i++)
			fprintf(fp, "\n\t\t||(wb_stb)&&(%6s_sel)&&(%6s_stall)",
				(*plist)[i]->p_name->c_str(),
				(*plist)[i]->p_name->c_str());
		fprintf(fp, ";\n\n\n");
	} else
		fprintf(fp, "\tassign\twb_stall = 1\'b0;\n\n\n");

	// Define the bus error
	fprintf(fp, ""
	"\t//\n"
	"\t// wb_err\n"
	"\t//\n"
	"\t// This is the bus error signal.  It should never be true, but practice\n"
	"\t// teaches us otherwise.  Here, we allow for three basic errors:\n"
	"\t//\n"
	"\t// 1. STB is true, but no devices are selected\n"
	"\t//\n"
	"\t//	This is the null pointer reference bug.  If you try to access\n"
	"\t//	something on the bus, at an address with no mapping, the bus\n"
	"\t//	should produce an error--such as if you try to access something\n"
	"\t//	at zero.\n"
	"\t//\n"
	"\t// 2. STB is true, and more than one device is selected\n"
	"\t//\n"
	"\t//	(This can be turned off, if you design this file well.  For\n"
	"\t//	this line to be true means you have a design flaw.)\n"
	"\t//\n"
	"\t// 3. If more than one ACK is every true at any given time.\n"
	"\t//\n"
	"\t//	This is a bug of bus usage, combined with a subtle flaw in the\n"
	"\t//	WB pipeline definition.  You can issue bus requests, one per\n"
	"\t//	clock, and if you cross device boundaries with your requests,\n"
	"\t//	you may have things come back out of order (not detected here)\n"
	"\t//	or colliding on return (detected here).  The solution to this\n"
	"\t//	problem is to make certain that any burst request does not cross\n"
	"\t//	device boundaries.  This is a requirement of whoever (or\n"
	"\t//	whatever) drives the bus.\n"
	"\t//\n"
	"\tassign	wb_err = ((wb_stb)&&(none_sel || many_sel))\n"
				"\t\t\t\t|| ((wb_cyc)&&(many_ack));\n\n");

	if (baw < 30) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { {(%d){1\'b0}}, wb_addr, 2\'b00 };\n\n", 30-baw);
	} else if (baw == 30) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { wb_addr, 2\'b00 };\n\n");
	} else if (baw == 31)
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= { wb_addr, 1\'b0 };\n\n");
	else
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= wb_addr;\n\n");

	fprintf(fp, "\t//Now we turn to defining all of the parts and pieces of what\n"
	"\t// each of the various peripherals does, and what logic it needs.\n"
	"\t//\n"
	"\t// This information comes from the @MAIN.INSERT and @MAIN.ALT tags.\n"
	"\t// If an @ACCESS tag is available, an ifdef is created to handle\n"
	"\t// having the access and not.  If the @ACCESS tag is `defined above\n"
	"\t// then the @MAIN.INSERT code is executed.  If not, the @MAIN.ALT\n"
	"\t// code is exeucted, together with any other cleanup settings that\n"
	"\t// might need to take place--such as returning zeros to the bus,\n"
	"\t// or making sure all of the various interrupt wires are set to\n"
	"\t// zero if the component is not included.\n"
	"\t//\n");

	fprintf(fp, "\t//\n\t// Declare the interrupt busses\n\t//\n");
	for(unsigned picid=0; picid < piclist.size(); picid++) {
		STRINGP	defnstr, vecstr;
		MAPDHASH *picmap;

		if (piclist[picid]->i_max <= 0)
			continue;
		picmap = getmap(master, *piclist[picid]->i_name);
		if (!picmap)
			continue;
		vecstr = getstring(*picmap, KYPIC_BUS);
		if (vecstr) {
			fprintf(fp, "\tassign\t%s = {\n",
					vecstr->c_str());
		} else {
			defnstr = piclist[picid]->i_name;
			if (defnstr)
				fprintf(fp, "\tassign\t%s_int_vec = {\n",
					defnstr->c_str());
			else {
				gbl_err++;
				fprintf(stderr, "ERR: PIC has no associated name\n");
				continue;
			}
		}

		for(int iid=piclist[picid]->i_max-1; iid>=0; iid--) {
			INTP	iip = piclist[picid]->getint(iid);
			if ((iip == NULL)||(iip->i_wire == NULL)
					||(iip->i_wire->size() == 0)) {
				fprintf(fp, "\t\t1\'b0%s\n",
					(iid != 0)?",":"");
				continue;
			} fprintf(fp, "\t\t%s%s\n",
				iip->i_wire->c_str(),
				(iid == 0)?"":",");
		}
		fprintf(fp, "\t};\n");
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
	// MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
		MAPDHASH::iterator	kvint, kvsub, kvwire;
		bool			nomain, noaccess, noalt;
		STRINGP			insert, alt, access;

		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		insert = getstring(kvpair->second, KYMAIN_INSERT);
		access = getstring(kvpair->second, KYACCESS);
		alt    = getstring(kvpair->second, KYMAIN_ALT);

		nomain   = false;
		noaccess = false;
		noalt    = false;
		if (NULL == insert)
			nomain = true;
		if (NULL == access)
			noaccess= true;
		if (NULL == alt)
			noalt = true;

		if (noaccess) {
			if (!nomain)
				fputs(insert->c_str(), fp);
		} else if ((!nomain)||(!noalt)) {
			if (nomain) {
				fprintf(fp, "`ifndef\t%s\n", access->c_str());
			} else {
				fprintf(fp, "`ifdef\t%s\n", access->c_str());
				fputs(insert->c_str(), fp);
				fprintf(fp, "`else\t// %s\n", access->c_str());
			}

			if (!noalt) {
				fputs(alt->c_str(), fp);
			}

			if (isbusmaster(kvpair->second)) {
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYPREFIX);
				if (pfx) {
					fprintf(fp, "\n");
					fprintf(fp, "\tassign\t%s_cyc = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_stb = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_we  = 1\'b0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_sel = 4\'b0000;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_addr = 0;\n", pfx->c_str());
					fprintf(fp, "\tassign\t%s_data = 0;\n", pfx->c_str());
					fprintf(fp, "\n");
				}
			}

			if (isperipheral(kvpair->second)) {
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYPREFIX);
				if (pfx) {
					fprintf(fp, "\n");
					fprintf(fp, "\treg\tr_%s_ack;\n", pfx->c_str());
					fprintf(fp, "\talways @(posedge i_clk)\n\t\tr_%s_ack <= (wb_stb)&&(%s_sel);\n",
						pfx->c_str(),
						pfx->c_str());
					fprintf(fp, "\n");
					fprintf(fp, "\tassign\t%s_ack   = r_%s_ack;\n",
						pfx->c_str(),
						pfx->c_str());
					fprintf(fp, "\tassign\t%s_stall = 1\'b0;\n",
						pfx->c_str());
					fprintf(fp, "\tassign\t%s_data  = 32\'h0;\n",
						pfx->c_str());
					fprintf(fp, "\n");
				}
			}
			kvint    = kvpair->second.u.m_m->find(KY_INT);
			if ((kvint != kvpair->second.u.m_m->end())
					&&(kvint->second.m_typ == MAPT_MAP)) {
				MAPDHASH	*imap = kvint->second.u.m_m,
						*smap;
				for(kvsub=imap->begin(); kvsub != imap->end();
						kvsub++) {
					// p.INT.SUB
					if (kvsub->second.m_typ != MAPT_MAP)
						continue;
					smap = kvsub->second.u.m_m;
					kvwire = smap->find(KY_WIRE);
					if (kvwire == smap->end())
						continue;
					if (kvwire->second.m_typ != MAPT_STRING)
						continue;
					fprintf(fp, "\tassign\t%s = 1\'b0;\t// %s.%s.%s.%s\n",
						kvwire->second.u.m_s->c_str(),
						kvpair->first.c_str(),
						kvint->first.c_str(),
						kvsub->first.c_str(),
						kvwire->first.c_str());
				}
			}
			fprintf(fp, "`endif\t// %s\n\n", access->c_str());
		}
	}

	fprintf(fp, ""
	"\t//\n"
	"\t// Finally, determine what the response is from the wishbone\n"
	"\t// bus\n"
	"\t//\n"
	"\t//\n");

	fprintf(fp, ""
	"\t//\n"
	"\t// wb_idata\n"
	"\t//\n"
	"\t// This is the data returned on the bus.  Here, we select between a\n"
	"\t// series of bus sources to select what data to return.  The basic\n"
	"\t// logic is simply this: the data we return is the data for which the\n"
	"\t// ACK line is high.\n"
	"\t//\n"
	"\t// The last item on the list is chosen by default if no other ACK's are\n"
	"\t// true.  Although we might choose to return zeros in that case, by\n"
	"\t// returning something we can skimp a touch on the logic.\n"
	"\t//\n"
	"\t// Any peripheral component with a @PTYPE value will be listed\n"
	"\t// here.\n"
	"\t//\n");

	if (DELAY_ACK) {
		fprintf(fp, "\talways @(posedge i_clk)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist->size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist->size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					(*plist)[plist->size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
	} else {
		fprintf(fp, "`ifdef\tVERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist->size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
				for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist->size())
				fprintf(fp, ": wb_idata = %s_data;\n",
					(*plist)[plist->size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata = 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata = 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "\n`else\t// VERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist->size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist->size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					(*plist)[plist->size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "`endif\t// VERILATOR\n");
	}

	fprintf(fp, "\n\nendmodule // main.v\n");

}

STRINGP	remove_comments(STRINGP s) {
	STRINGP	r;
	int	si, di;	// Source and destination indices

	r = new STRING(*s);
	for(si=0, di=0; (*s)[si]; si++) {
		if (((*s)[si] == '/')&&((*s)[si+1])&&((*s)[si+1] == '/')) {
			// Comment to the end of the line
			si += 2;
			while(((*s)[si])&&((*s)[si] != '\r')&&((*s)[si] != '\n'))
				si++;
		} else if (((*s)[si] == '/')&&((*s)[si+1])&&((*s)[si+1] == '*')) {
			si += 2;
			// Go until the end of a block comment
			while(((*s)[si])&&((*s)[si] != '*')&&((!(*s)[si+1])||((*s)[si+1]!='/')))
				si++;
			si += 1;
		} else
			(*r)[di++] = (*s)[si];
	} (*r)[di] = '\0';

	return r;
}

void	build_rtl_make_inc(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP	mkgroup, mkfiles, mksubd;
	STRING	allgroups, vdirs;

	legal_notice(master, fp, fname,
		"########################################"
		"########################################", "##");

	for(kvpair=master.begin(); kvpair!=master.end(); kvpair++) {
		const	char	DELIMITERS[] = ", \t\n";
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		mkgroup = getstring(kvpair->second, KYRTL_MAKE_GROUP);
		if (NULL == mkgroup)
			continue;
		mkfiles = getstring(kvpair->second, KYRTL_MAKE_FILES);
		mksubd  = getstring(kvpair->second, KYRTL_MAKE_SUBD);
		if (!mkfiles)
			continue;

		char	*tokstr = strdup(mkfiles->c_str()), *tok;
		STRING	filstr = "";

		tok = strtok(tokstr, DELIMITERS);
		while(NULL != tok) {
			filstr += STRING(tok) + " ";
			tok = strtok(NULL, DELIMITERS);
		} if (filstr[filstr.size()-1] == ' ')
			filstr[filstr.size()-1] = '\0';
		if (mksubd) {
			fprintf(fp, "%sD := %s\n\n", mkgroup->c_str(),
					mksubd->c_str());
			fprintf(fp, "%s  := $(addprefix $(%sD)/,%s)\n",
				mkgroup->c_str(),
				mkgroup->c_str(), filstr.c_str());

			vdirs = vdirs + STRING(" -y $(")+(*mkgroup)
					+STRING("D) ");
		} else {
			fprintf(fp, "%s:= %s\n\n", mkgroup->c_str(), filstr.c_str());
		}

		allgroups = allgroups + STRING(" $(") + (*mkgroup) + STRING(")");
		free(tokstr);
	}

	mkgroup = getstring(master, KYRTL_MAKE_GROUP);
	if (NULL == mkgroup)
		mkgroup = new STRING(KYVFLIST);
	mkfiles = getstring(master, KYRTL_MAKE_FILES);
	mksubd  = getstring(master, KYRTL_MAKE_SUBD);

	if (NULL != mkfiles) {
		if (mksubd) {
			fprintf(fp, "%sD := %s\n", mkgroup->c_str(),
				mksubd->c_str());
			fprintf(fp, "%s := $(addprefix $(%sD)/,%s) \\\t\t%s\n",
				mkgroup->c_str(), mkgroup->c_str(),
				mkfiles->c_str(), allgroups.c_str());
			vdirs = vdirs + STRING(" -y $(")+(*mkgroup)
					+STRING("D) ");
		} else
			fprintf(fp, "%s := %s \\\t\t%s\n",
				mkgroup->c_str(),
				mkfiles->c_str(), allgroups.c_str());
	} else if (allgroups.size() > 0) {
		fprintf(fp, "%s := main.v %s\n", mkgroup->c_str(), allgroups.c_str());
	}

	mksubd = getstring(master, KYRTL_MAKE_VDIRS);
	if (NULL == mksubd)
		mksubd = new STRING(KYAUTOVDIRS);
	fprintf(fp, "%s := %s\n", mksubd->c_str(), vdirs.c_str());
}

void	build_outfile_aux(MAPDHASH &info, STRINGP fname, STRINGP data) {
	STRING	str;
	STRINGP	subd = getstring(info, KYSUBD);

	if (NULL != strchr(fname->c_str(), '/')) {
		fprintf(stderr, "WARNING: Output files can only be placed in output directory\n");
		fprintf(stderr, "Output file: %s ignored\n",
			fname->c_str());
		return;
	}

	FILE	*fp;
	str = subd->c_str(); str += "/"+(*fname);
	fp = fopen(str.c_str(), "w");
	if (NULL == fp) {
		fprintf(stderr, "ERROR: Cannot write %s\n", str.c_str());
		exit(EXIT_FAILURE);
	}

	unsigned nw = fwrite(data->c_str(), 1, data->size(), fp);
	if (nw != data->size()) {
		fprintf(stderr, "ERROR: %s data not fully written\n", str.c_str());
		exit(EXIT_FAILURE);
	}
}


void	build_other_files(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	STRINGP			fname, data;

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		fname = getstring(kvpair->second, KYOUT_FILE);
		if (NULL == fname)
			continue;
		data  = getstring(kvpair->second, KYOUT_DATA);
		if (NULL == data)
			continue;

		build_outfile_aux(info, fname, data);
	}
}

FILE	*open_in(MAPDHASH &info, const STRING &fname) {
	static	const	char	delimiters[] = " \t\n:,";
	STRINGP	path = getstring(info, KYPATH);
	STRING	pathcpy;
	char	*tok;
	FILE	*fp;

	if (path) {
		pathcpy = *path;
		tok =strtok((char *)pathcpy.c_str(), delimiters);
		while(NULL != tok) {
			char	*prepath = realpath(tok, NULL);
			STRING	fpath = STRING(prepath) + "/" + fname;
			free(prepath);
			if (NULL != (fp = fopen(fpath.c_str(), "r"))) {
				return fp;
			}
			tok = strtok(NULL, delimiters);
		}
	}
	return fopen(fname.c_str(), "r");
}


typedef	std::vector<STRINGP>	PORTLIST;
void	get_portlist(MAPDHASH &master, PORTLIST &ports) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str, stripped;
	char			*pptr;

	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		const	char	*DELIMITERS = ", \t\n";
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYTOP_PORTLIST);
		if (str == NULL)
			str = getstring(kvpair->second, KYMAIN_PORTLIST);
		if (str == NULL)
			continue;
		stripped = remove_comments(str);

		pptr = strtok((char *)stripped->c_str(), DELIMITERS);
		while(pptr) {
			ports.push_back(new STRING(pptr));
			if (gbl_dump) fprintf(gbl_dump, "\t%s\n", pptr);
			pptr = strtok(NULL, DELIMITERS);
		} delete stripped;
	}
}

void	build_xdc(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;
	PORTLIST		ports;
	FILE			*fpsrc;
	char	line[512];

	if (gbl_dump) {
		fprintf(gbl_dump, "\n\nBUILD-XDC\nLooking for ports:\n");
		fflush(gbl_dump);
	}

	get_portlist(master, ports);

	str = getstring(master, KYXDC_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		fprintf(stderr, "ERR: Could not find or open %s\n", str->c_str());
		exit(EXIT_FAILURE);
	}

	while(fgets(line, sizeof(line), fpsrc)) {
		const char	*GET_PORTS_KEY = "get_ports",
				*SET_PROPERTY_KEY = "set_property";
		const	char	*cptr;

		STRINGP	tmp = trim(STRING(line));
		strcpy(line, tmp->c_str());
		delete	tmp;


		// Ignore any lines that don't begin with #,
		// Ignore any lines that start with two ##'s,
		// Ignore any lines that don't have set_property within them
		if ((line[0] != '#')||(line[1] == '#')
			||(NULL == (cptr= strstr(line, SET_PROPERTY_KEY)))) {
			fprintf(fp, "%s\n", line);
			continue;
		}

		if (NULL != (cptr = strstr(cptr, GET_PORTS_KEY))) {
			bool	found = false;

			cptr += strlen(GET_PORTS_KEY);
			while((*cptr)&&(*cptr != '{')&&(isspace(*cptr)))
				cptr++;
			if (*cptr == '{')
				cptr++;
			if (!(*cptr)) {
				fprintf(fp, "%s\n", line);
				continue;
			}

			while((*cptr)&&(isspace(*cptr)))
				cptr++;

			char		*ptr, *name;
			name = strdup(cptr);
			ptr = name;
			while((*ptr)
				&&(*ptr != '[')
				&&(*ptr != '}')
				&&(*ptr != ']')
				&&(!isspace(*ptr)))
				ptr++;
			*ptr = '\0';

			if (gbl_dump)
				fprintf(gbl_dump, "Found XDC port: %s\n", name);

			// Now, let's check to see if this is in our set
			for(unsigned k=0; k<ports.size(); k++) {
				if (strcmp(ports[k]->c_str(), name)==0) {
					found = true;
					break;
				}
			} free(name);

			if (found) {
				int start = 0;
				while((line[start])&&(
						(line[start]=='#')
						||(isspace(line[start]))))
					start++;
				fprintf(fp, "%s\n", &line[start]);
			} else
				fprintf(fp, "%s\n", line);
		} else
			fprintf(fp, "%s\n", line);
	}

	fclose(fpsrc);

	fprintf(fp, "\n## Adding in any XDC_INSERT tags\n\n");
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYXDC_INSERT);
		if (NULL == str)
			continue;

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			kvpair->second.u.m_s->c_str());
	}
	str = getstring(master, KYXDC_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			kvpair->second.u.m_s->c_str());
	}
}


void	build_ucf(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;
	FILE			*fpsrc;
	PORTLIST		ports;
	char	line[512];

	if (gbl_dump) {
		fprintf(gbl_dump, "\n\nBUILD-UCF\nLooking for ports:\n");
		fflush(gbl_dump);
	}

	get_portlist(master, ports);

	str = getstring(master, KYUCF_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		fprintf(stderr, "ERR: Could not find or open %s\n", str->c_str());
		exit(EXIT_FAILURE);
	}

	while(fgets(line, sizeof(line), fpsrc)) {
		const char	*NET_KEY = "NET";
		const char	*cptr;

		if ((line[0] == '#')&&(cptr = strstr(line, NET_KEY))) {
			bool	found = false;


			cptr += strlen(NET_KEY);
			if (!isspace(*cptr)) {
				fprintf(fp, "%s", line);
				continue;
			}

			// Skip white space
			while((*cptr)&&(isspace(*cptr)))
				cptr++;

			// On a broken file, just continue
			if (!(*cptr)) {
				fprintf(fp, "%s", line);
				continue;
			}


			char	*ptr, *name;
			name = strdup(cptr+1);

			ptr = name;
			while((*ptr)&&(!isspace(*ptr))&&(*ptr != ';'))
				ptr++;
			*ptr = '\0';

			if (gbl_dump) {
				fprintf(gbl_dump, "Found UCF port: %s", name);
				fflush(gbl_dump);
			}

			// Now, let's check to see if this is in our set
			for(unsigned k=0; k<ports.size(); k++) {
				if (strcmp(ports[k]->c_str(), name)==0) {
					found = true;
					break;
				}
			} free(name);

			if (found) {
				unsigned start = 0;
				while((line[start])&&(
						(line[start]=='#')
						||(isspace(line[start]))))
					start++;
				fprintf(fp, "%s", &line[start]);
			} else
				fprintf(fp, "%s", line);
		} else
			fprintf(fp, "%s", line);
	}

	fclose(fpsrc);


	fprintf(fp, "\n## Adding in any UCF_INSERT tags\n\n");
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYUCF_INSERT);
		if (str == NULL)
			continue;

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			kvpair->second.u.m_s->c_str());
	}
	str = getstring(master, KYUCF_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			kvpair->second.u.m_s->c_str());
	}
}

int	main(int argc, char **argv) {
	int		argn, nhash = 0;
	MAPDHASH	master;
	FILE		*fp;
	STRING		str, cmdline, searchstr = ".";
	const char	*subdir;
	gbl_dump = fopen("dump.txt", "w");

	if (argc > 0) {
		cmdline = STRING(argv[0]);
		for(argn=0; argn<argc; argn++) {
			cmdline = cmdline + " " + STRING(argv[argn]);
		}

		setstring(master, KYCMDLINE, new STRING(cmdline));
	}

	for(argn=1; argn<argc; argn++) {
		if (argv[argn][0] == '-') {
			for(int j=1; ((j<2000)&&(argv[argn][j])); j++) {
				switch(argv[argn][j]) {
				case 'o': subdir = argv[++argn];
					j+=5000;
					break;
				case 'I':
					searchstr = searchstr + ":" + argv[++argn];
					setstring(master, KYPATH, new STRING(searchstr));
					j+=5000;
					break;
				case 'V':
#ifdef	BUILDDATE
					printf("autofpga\nbuilt on %08x\n", BUILDDATE);
#else
					printf("autofpga [data-file-list]*\n");
#endif
					exit(EXIT_SUCCESS);
				default:
					fprintf(stderr, "Unknown argument, -%c\n", argv[argn][j]);
				}
			}
		} else {
			MAPDHASH	*fhash;
			STRINGP		path;

			path = getstring(master, KYPATH);
			if (NULL == path) {
				path = new STRING(".");
				setstring(master, KYPATH, path);
			}
			fhash = parsefile(argv[argn], *path);
			if (fhash) {
				mergemaps(master, *fhash);
				delete fhash;

				nhash++;
			}
		}
	}

	if (nhash == 0) {
		fprintf(stderr, "ERR: No files given, no files written\n");
		exit(EXIT_FAILURE);
	}

	gbl_hash = &master;

	STRINGP	subd;
	if (subdir) {
		subd = new STRING(subdir);
		setstring(master, KYSUBD, subd);
	} else {
		subd = getstring(master, KYSUBD);
	} if (subd == NULL) {
		// subd = new STRING("autofpga-out");
		subd = new STRING("demo-out");
		setstring(master, KYSUBD, subd);
	}
	if ((*subd) == STRING("/")) {
		fprintf(stderr, "ERR: OUTPUT SUBDIRECTORY = %s\n", subd->c_str());
		fprintf(stderr, "Cowardly refusing to place output products into the root directory, '/'\n");
		exit(EXIT_FAILURE);
	} if ((*subd)[subd->size()-1] == '/')
		(*subd)[subd->size()-1] = '\0';

	{	// Build ourselves a subdirectory for our outputs
		// First, check if the directory exists.
		// If it does not, then create it.
		struct	stat	sbuf;
		if (0 == stat(subd->c_str(), &sbuf)) {
			if (!S_ISDIR(sbuf.st_mode)) {
				fprintf(stderr, "ERR: %s exists, and is not a directory\n", subd->c_str());
				fprintf(stderr, "Cowardly refusing to erase %s and build a directory in its place\n", subd->c_str());
				exit(EXIT_FAILURE);
			}
		} else if (mkdir(subd->c_str(), 0777) != 0) {
			fprintf(stderr, "ERR: Could not create %s/ directory\n",
				subd->c_str());
			exit(EXIT_FAILURE);
		}
	}

	STRINGP	legal = getstring(master, KYLEGAL);
	if (legal == NULL) {
		legal = getstring(master, KYCOPYRIGHT);
		if (legal == NULL)
			legal = new STRING("legalgen.txt");
		setstring(master, KYLEGAL, legal);
	}

	flatten(master);

	// trimbykeylist(master, KYKEYS_TRIMLIST);
	cvtintbykeylist(master, KYKEYS_INTLIST);

	reeval(master);

	count_peripherals(master);
	build_plist(master);
	assign_interrupts(master);
	// assign_scopes(    master);
	build_bus_list(master);
	// assign_addresses( master);
	// get_address_width(master);

	reeval(master);

	str = subd->c_str(); str += "/regdefs.h";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_regdefs_h(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/regdefs.cpp";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_regdefs_cpp(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/board.h";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_board_h(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/board.ld";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_board_ld(  master, fp, str); fclose(fp); }

	build_latex_tbls( master);

	str = subd->c_str(); str += "/toplevel.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_toplevel_v(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/main.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_main_v(  master, fp, str);
		mkselect2(fp, master);
		fclose(fp); }

	str = subd->c_str(); str += "/rtl.make.inc";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_rtl_make_inc(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/testb.h";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_testb_h(  master, fp, str); fclose(fp); }

	if (NULL != getstring(master, KYXDC_FILE)) {
		str = subd->c_str(); str += "/build.xdc";
		fp = fopen(str.c_str(), "w");
		if (fp) { build_xdc(  master, fp, str); fclose(fp); }
	}

	if (NULL != getstring(master, KYUCF_FILE)) {
		str = subd->c_str(); str += "/build.ucf";
		fp = fopen(str.c_str(), "w");
		if (fp) { build_ucf(  master, fp, str); fclose(fp); }
	}

	str = subd->c_str(); str += "/main_tb.cpp";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_main_tb_cpp(  master, fp, str); fclose(fp); }

	build_other_files(master);

	if (0 != gbl_err)
		fprintf(stderr, "ERR: Errors present\n");

	if (gbl_dump)
		mapdump(gbl_dump, master);
	return gbl_err;
}
