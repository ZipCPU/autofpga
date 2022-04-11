////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	autofpga.cpp
// {{{
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
//	iscachable.h
//	regdefs.cpp
//	board.h
//	board.ld
//	main_tb.cpp
//	testb.h
//	* dev.tex	(Not yet included)
//	* kernel device tree file
//
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
#include "bldboardld.h"
#include "bldrtlmake.h"
#include "bitlib.h"
#include "plist.h"
#include "bldregdefs.h"
#include "ifdefs.h"
#include "bldsim.h"
#include "predicates.h"
#include "businfo.h"
#include "globals.h"
#include "msgs.h"
#include "bldcachable.h"

// class INFINFO
// The ILIST, a list of interrupt lines within the design
// {{{
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
// }}}

// class PICINFO
// {{{
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
		assert(i_name);
		i_bus  = getstring(pic, KYPIC_BUS);
		if (getvalue( pic, KYPIC_MAX, mx)) {
			i_max = mx;
			i_alist = new INTID *[mx];
			for(int i=0; i<mx; i++)
				i_alist[i] = NULL;
		} else {
			gbl_msg.error("ERR: Cannot find PIC.MAX, the maximum number of interrupts %s can take on\n", (i_name) ? i_name->c_str() : "(No name)");
			gbl_msg.dump(pic);
			i_max = 0;
			i_alist = NULL;
		}
		i_nassigned  = 0;
		i_nallocated = 0;
	}

	// Add an interrupt with the given name, and hash source, to the table
	void add(MAPDHASH &psrc, STRINGP iname) {
		if (!iname)
			return;
		if (i_nallocated >= i_max) {
			gbl_msg.warning("Interrupt %s not assigned, PIC %s is full\n",
				iname->c_str(), i_name->c_str());
			return;
		}

		INTP	ip = new INTID();
		i_ilist.push_back(ip);

		// Initialize the table entry with the name of this interrupt
		assert(iname);
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
			gbl_msg.warning("No name given for interrupt\n");
			return;
		} else if (id >= i_max) {
			gbl_msg.error("ERR: Interrupt ID %d out of bounds [0..%d]\n",
				id, i_max);
			return;
		} else if (i_nassigned+1 > i_max) {
			gbl_msg.error("ERR: Too many interrupts assigned to %s\n", i_name->c_str());
			return;
		}

		// Check to see if any other interrupt already has this
		// identifier, and write an error out if so.
		if (i_alist[id]) {
			gbl_msg.error("%s and %s are both competing for the same interrupt ID, #%d\n",
				i_alist[id]->i_name->c_str(),
				iname->c_str(), id);
			gbl_msg.error("interrupt %s dropped\n", iname->c_str());
			return;
		}

		// Otherwise, add the interrupt to our list
		INTP	ip = new INTID();
		i_ilist.push_back(ip);
		assert(iname);
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
			gbl_msg.warning("Too many interrupts assigned to PIC %s\n", i_name->c_str());
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
// }}}

// Return the number of interrupt controllers within this design.  If no such
// field/tag exists, count the number and add it to the hash.
int count_pics(MAPDHASH &info) {
	// {{{
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
// }}}

void	assign_int_to_pics(const STRING &iname, MAPDHASH &ihash) {
	// {{{
	STRINGP	picname;
	int inum;

	// Now, we need to map this to a PIC
	if (NULL==(picname=getstring(ihash, KYPIC))) {
		return;
	}

	char	*tok, *cpy, *sr;
	cpy = strdup(picname->c_str());
	tok = strtok_r(cpy, ", \t\n", &sr);
	while(tok) {
		unsigned pid;
		for(pid = 0; pid<piclist.size(); pid++)
			if (piclist[pid]->i_name->compare(tok)==0)
				break;
		if (pid >= piclist.size()) {
			gbl_msg.error("PIC NOT FOUND: %s\n", tok);
		} else if (getvalue(ihash, KY_ID, inum)) {
			piclist[pid]->add((unsigned)inum, ihash, (STRINGP)&iname);
		} else {
			piclist[pid]->add(ihash, (STRINGP)&iname);
		}
		tok = strtok_r(NULL, ", \t\n", &sr);
	} free(cpy);
}
// }}}

//
// assign_interrupts
//
// Find all the interrup controllers, and then find all the interrupts, and map
// interrupts to controllers.  Individual interrupt wires may be mapped to
// multiple interrupt controllers.
//
void	assign_interrupts(MAPDHASH &master) {
	// {{{
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
			const char DELIMITERS[] = " \t\n,";
			char	*scpy = strdup(sintlist->c_str());
			char	*tok, *stoksv=NULL;
			tok = strtok_r(scpy, DELIMITERS, &stoksv);
			while(tok) {
				STRING	stok = STRING(tok);

				kvline = findkey(*kvint->second.u.m_m, stok);
				if (kvline != kvint->second.u.m_m->end()) {
					assign_int_to_pics(stok,
						*kvline->second.u.m_m);
				} else
					gbl_msg.warning("INT %s not found, not connected\n", tok);

				tok = strtok_r(NULL, DELIMITERS, &stoksv);
			} free(scpy);
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
}
// }}}

void	writeout(FILE *fp, MAPDHASH &master, const STRING &ky) {
	// {{{
	MAPDHASH::iterator	kvpair;
	STRINGP	str;

	// fprintf(fp, "// Looking for string: %s\n", ky.c_str());

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
// }}}

void	build_board_h(    MAPDHASH &master, FILE *fp, STRING &fname) {
	// {{{
	const	char	DELIMITERS[] = " \t\n";
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
		char	*dup, *tok;
		STRINGP	osdef, osval, access;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		osdef = getstring(*kvpair->second.u.m_m, KYBDEF_OSDEF);
		osval = getstring(*kvpair->second.u.m_m, KYBDEF_OSVAL);

		if ((!osdef)&&(!osval))
			continue;
		access= getstring(kvpair->second, KYACCESS);
		dup = NULL;
		tok = NULL;

		if (access) {
			dup = strdup(access->c_str());
			tok = strtok(dup, DELIMITERS);
			if (tok[0] == '!')
				tok++;
		} else	tok = NULL;
		if (tok)
			fprintf(fp, "#ifdef\t%s\n", tok);
		if (osdef)
			fprintf(fp, "#define\t%s\n", osdef->c_str());
		if (osval) {
			fputs(osval->c_str(), fp);
			if (osval->c_str()[strlen(osval->c_str())-1] != '\n')
				fputc('\n', fp);
		} if (tok)
			fprintf(fp, "#endif\t// %s\n", tok);
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
// }}}

void	build_latex_tbls( MAPDHASH &master) {
	// {{{
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
// }}}

//
// void	build_device_tree(MAPDHASH &master)
//
void	build_toplevel_v( MAPDHASH &master, FILE *fp, STRING &fname) {
	// {{{
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
	"//\n"
	"// The only exception is that any clocks with CLOCK.TOP tags will\n"
	"// also appear in this list\n"
	"//\n");
	fprintf(fp, "module\ttoplevel(");
	int	nclocks_at_top = 0;
	for(unsigned ck=0; ck<cklist.size(); ck++) {
		if (cklist[ck].m_top) {
			fprintf(fp, "%s%s,", (nclocks_at_top > 0)?" ":"",
				cklist[ck].m_top->c_str());
			nclocks_at_top++;
		}
	} fprintf(fp, "\n");

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
	"\t// Declaring any top level parameters.\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.PARAM key,\n"
	"\t// or from the @MAIN.PARAM key if @TOP.PARAM is absent.  For\n"
	"\t// those peripherals that don't do anything at the top level,\n"
	"\t// the @MAIN.PARAM key should be sufficient, so the @TOP.PARAM\n"
	"\t// key may be left undefined.\n"
	"\t//\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;

		STRINGP strp = getstring(*kvpair->second.u.m_m, KYTOP_PARAM);
		if (!strp)
			strp = getstring(*kvpair->second.u.m_m, KYMAIN_PARAM);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}

	fprintf(fp, "\t//\n"
	"\t// Declaring our input and output ports.  We listed these above,\n"
	"\t// now we are declaring them here.\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.IODECLS key,\n"
	"\t// or from the @MAIN.IODECL key if @TOP.IODECL is absent.  For\n"
	"\t// those peripherals that don't do anything at the top level,\n"
	"\t// the @MAIN.IODECL key should be sufficient, so the @TOP.IODECL\n"
	"\t// key may be left undefined.\n"
	"\t//\n"
	"\t// We start with any @CLOCK.TOP keys\n"
	"\t//\n");
	for(unsigned ck=0; ck<cklist.size(); ck++) {
		if (cklist[ck].m_top) {
			fprintf(fp, "\tinput\twire\t\t%s;\n",
				cklist[ck].m_top->c_str());
		}
	} for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
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

		STRING	tmps(*strp);
		STRING::iterator si;
		for(si=tmps.end()-1; si>=tmps.begin(); si--) {
			if (isspace(*si))
				*si = '\0';
			else
				break;
		} if (tmps.size() == 0)
			continue;
		if (!first)
			fprintf(fp, ",\n");
		first=0;
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
// }}}

void	build_main_v(     MAPDHASH &master, FILE *fp, STRING &fname) {
	// {{{
	char	DELIMITERS[] = ", \t\n";
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str, astr, sellist, acklist, siosel_str, diosel_str;
	int		first;

	fprintf(fp, "`timescale\t1ps / 1ps\n");
	legal_notice(master, fp, fname);

	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	fprintf(fp, "`default_nettype\tnone\n");

	fprintf(fp,
		"////////////////////////////////////////////////////////////////////////////////\n"
		"//\n" "// Macro defines\n" "// {{{\n");
	build_access_ifdefs_v(master, fp);
	fprintf(fp, "// }}}\n");

	fprintf(fp,
		"////////////////////////////////////////////////////////////////////////////////\n"
		"//\n"
		"// Any include files\n// {{{\n"
		"// These are drawn from anything with a MAIN.INCLUDE definition.\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp = getstring(kvpair->second, KYMAIN_INCLUDE);
		if (!strp)
			continue;
		fprintf(fp, "%s", strp->c_str());
	}


	fprintf(fp, "// }}}\n//\n");
	fprintf(fp,
"// Finally, we define our main module itself.  We start with the list of\n"
"// I/O ports, or wires, passed into (or out of) the main function.\n"
"//\n"
"// These fields are copied verbatim from the respective I/O port lists,\n"
"// from the fields given by @MAIN.PORTLIST\n"
"//\n");

	// Define our external ports within a port list
	fprintf(fp, "module\tmain(i_clk, i_reset,\n\t// {{{\n");
	str = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, str);
		if (!strp)
			continue;

		STRING	tmps(*strp);
		STRING::iterator si;
		for(si=tmps.end()-1; si>=tmps.begin(); si--) {
			if (isspace(*si))
				*si = '\0';
			else
				break;
		} if (tmps.size() == 0)
			continue;
		if (!first)
			fprintf(fp, ",\n");
		first=0;
		fprintf(fp, "%s", tmps.c_str());
	} fprintf(fp, "\n\t// }}}\n\t);\n");

	fprintf(fp,
		"////////////////////////////////////////////////////////////////////////////////\n"
		"//\n" "// Any parameter definitions\n// {{{\n"
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

	fprintf(fp, "// }}}\n"
"////////////////////////////////////////////////////////////////////////////////\n"
"//\n"
"// Port declarations\n"
"// {{{\n"
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
	fprintf(fp, "\tinput\twire\t\ti_clk;\n\t// verilator lint_off UNUSED\n\tinput\twire\t\ti_reset;\n\t// verilator lint_on UNUSED\n");
	str = "MAIN.IODECL";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	strp;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		strp = getstring(*kvpair->second.u.m_m, str);
		if (strp)
			fprintf(fp, "%s", strp->c_str());
	}
	fprintf(fp, "// }}}\n");

	fprintf(fp,
"\t// Make Verilator happy\n"
"\t// {{{\n"
"\t// Defining bus wires for lots of components often ends up with unused\n"
"\t// wires lying around.  We'll turn off Ver1lator\'s lint warning\n"
"\t// here that checks for unused wires.\n"
"\t// }}}\n"
"\t// verilator lint_off UNUSED\n");

	fprintf(fp,
	"\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n\t// Declaring interrupt lines\n\t// {{{\n"
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
	fprintf(fp, "\t// }}}\n");

	fprintf(fp,
	"\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n\t// Component declarations\n\t// {{{\n"
	"\t// These declarations come from the @MAIN.DEFNS keys found in the\n"
	"\t// various components comprising the design.\n\t//\n");
	writeout(fp, master, KYMAIN_DEFNS);

	// Declare interrupt vector wires.
	fprintf(fp,
	"\n// }}}\n"
	"\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n\t// Declaring interrupt vector wires\n\t// {{{\n"
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
					piclist[picid]->i_max-1,
					defnstr->c_str());
		}
	}
	fprintf(fp, "\t// }}}\n");
	writeout_bus_defns_v(fp);

	// Define the select lines
	fprintf(fp, "\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n"
	"\t// Peripheral address decoding, bus handling\n\t// {{{\n");

	writeout_bus_logic_v(fp);

	fprintf(fp, "\t// }}}\n"
	"\t////////////////////////////////////////////////////////////////////////\n"
	"\t//\n\t// Declare the interrupt busses\n\t// {{{\n"
"\t// Interrupt busses are defined by anything with a @PIC tag.\n"
"\t// The @PIC.BUS tag defines the name of the wire bus below,\n"
"\t// while the @PIC.MAX tag determines the size of the bus width.\n"
"\t//\n"
"\t// For your peripheral to be assigned to this bus, it must have an\n"
"\t// @INT.NAME.WIRE= tag to define the wire name of the interrupt line,\n"
"\t// and an @INT.NAME.PIC= tag matching the @PIC.BUS tag of the bus\n"
"\t// your interrupt will be assigned to.  If an @INT.NAME.ID tag also\n"
"\t// exists, then your interrupt will be assigned to the position given\n"
"\t// by the ID# in that tag.\n"
"\t//\n");
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
				gbl_msg.error("PIC has no associated name\n");
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
	fprintf(fp, "\t// }}}\n");

	fprintf(fp,
		"\t////////////////////////////////////////////////////////////////////////\n"
		"\t////////////////////////////////////////////////////////////////////////\n"
		"\t//\n\t// @MAIN.INSERT and @MAIN.ALT\n"
		"\t// {{{\n"
		"\t////////////////////////////////////////////////////////////////////////\n"
		"\t////////////////////////////////////////////////////////////////////////\n");
	fprintf(fp,
	"\t//\n"
	"\t//\n"
	"\t// Now we turn to defining all of the parts and pieces of what\n"
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


	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
	// MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
		MAPDHASH::iterator	kvint, kvsub, kvwire;
		bool			nomain, noaccess, noalt;
		STRINGP			insert, alt, access;
		char			*accessdup, *accesstok;

		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		insert = getstring(kvpair->second, KYMAIN_INSERT);
		access = getstring(kvpair->second, KYACCESS);
		alt    = getstring(kvpair->second, KYMAIN_ALT);

		nomain   = false;
		noaccess = false;
		noalt    = false;
		accessdup= NULL;
		if (NULL == insert)
			nomain = true;
		if (NULL == access) {
			noaccess= true;
		} else {
			accessdup = strdup(access->c_str());
			accesstok = strtok(accessdup, DELIMITERS);
			if (accesstok[0] == '!')
				accesstok++;
		}
		if (NULL == alt)
			noalt = true;

		if (noaccess) {
			if (!nomain)
				fputs(insert->c_str(), fp);
		} else if ((!nomain)||(!noalt)) {
			if (nomain) {
				fprintf(fp, "`ifndef\t%s\n", accesstok);
			} else {
				fprintf(fp, "`ifdef\t%s\n", accesstok);
				fprintf(fp, "\t// {{{\n");
				fputs(insert->c_str(), fp);
				fprintf(fp, "\t// }}}\n");
				fprintf(fp, "`else\t// %s\n", accesstok);
			}
			fprintf(fp, "\t// {{{\n");

			if (!noalt) {
				fputs(alt->c_str(), fp);
			}

			if (isbusmaster(kvpair->second)) {
				// {{{
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYPREFIX);
				if (pfx) {
					STRINGP	busname = getstring(
							*kvpair->second.u.m_m,
							KYMASTER_BUS_NAME);
					if (busname) {
						BUSINFO *bus = find_bus(busname);
						if (bus) {
							fprintf(fp, "\t// Null bus master\n\t// {{{\n");
							bus->writeout_no_master_v(fp);
							fprintf(fp, "\t// }}}\n");
						}
					}
				}
				// }}}
			}

			if (isperipheral(kvpair->second)) {
				// {{{
				BUSINFO *bi = find_bus_of_peripheral(kvpair->second.u.m_m);
				STRINGP	pfx = getstring(*kvpair->second.u.m_m,
							KYSLAVE_PREFIX);
				if ((bi)&&(pfx)) {
					fprintf(fp, "\t// Null bus slave\n\t// {{{\n");
					bi->writeout_no_slave_v(fp, pfx);
					fprintf(fp, "\t// }}}\n");
				}
				// }}}
			}
			kvint    = kvpair->second.u.m_m->find(KY_INT);
			if ((kvint != kvpair->second.u.m_m->end())
					&&(kvint->second.m_typ == MAPT_MAP)) {
				MAPDHASH	*imap = kvint->second.u.m_m,
						*smap;
				fprintf(fp, "\t// Null interrupt definitions\n"
					"\t// {{{\n");
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
					if ((NULL == kvwire->second.u.m_s)
						||(kvwire->second.u.m_s->size()==0))
						continue;
					fprintf(fp, "\tassign\t%s = 1\'b0;\t// %s.%s.%s.%s\n",
						kvwire->second.u.m_s->c_str(),
						kvpair->first.c_str(),
						kvint->first.c_str(),
						kvsub->first.c_str(),
						kvwire->first.c_str());
				}
				fprintf(fp, "\t// }}}\n");
			}
			fprintf(fp, "\t// }}}\n");
			fprintf(fp, "`endif\t// %s\n\n", accesstok);
		}

		if (accessdup)
			free(accessdup);
	}


	fprintf(fp, "\t// }}}\nendmodule // main.v\n");

}
// }}}

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

void	build_outfile_aux(MAPDHASH &info, STRINGP fname, STRINGP data) {
	STRING	str;
	STRINGP	subd = getstring(info, KYSUBD);

	if (NULL != strchr(fname->c_str(), '/')) {
		gbl_msg.warning("Output files can only be placed in output directory\n"
			"Output file: %s ignored\n", fname->c_str());
		return;
	}

	FILE	*fp;
	str = subd->c_str(); str += "/"+(*fname);
	fp = fopen(str.c_str(), "w");
	if (NULL == fp)
		gbl_msg.fatal("Cannot write %s\n", str.c_str());

	unsigned nw = fwrite(data->c_str(), 1, data->size(), fp);
	if (nw != data->size())
		gbl_msg.fatal("%s data not fully written\n", str.c_str());
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
			gbl_msg.info("\t%s\n", pptr);
			pptr = strtok(NULL, DELIMITERS);
		} delete stripped;
	}

	// Check any CLOCK.TOP keys
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		const	char	*DELIMITERS = ", \t\n";
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYCLOCK_TOP);
		if (str == NULL)
			continue;
		stripped = remove_comments(str);

		pptr = strtok((char *)stripped->c_str(), DELIMITERS);
		while(pptr) {
			ports.push_back(new STRING(pptr));
			gbl_msg.info("\t%s\n", pptr);
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

	gbl_msg.info("\n\nBUILD-XDC\nLooking for ports:\n");
	gbl_msg.flush();

	get_portlist(master, ports);

	str = getstring(master, KYXDC_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		gbl_msg.fatal("Could not find or open %s\n", str->c_str());
	}

	// Uncomment any lines referencing top-level ports
	// {{{
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

			gbl_msg.info("Found XDC port: %s\n", name);

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
	// }}}

	fclose(fpsrc);

	fprintf(fp, "\n## Adding in any XDC_INSERT tags\n\n");
	// {{{
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYXDC_INSERT);
		if (NULL == str) {
			fprintf(fp, "## No XDC.INSERT tag in %s\n",
				kvpair->first.c_str());
			continue;
		}

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			str->c_str());
	}
	str = getstring(master, KYXDC_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			str->c_str());
	}
	// }}}
}

void	build_pcf(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;
	PORTLIST		ports;
	FILE			*fpsrc;
	char	line[512];

	gbl_msg.info("\n\nBUILD-PCF\nLooking for ports:\n");
	gbl_msg.flush();

	get_portlist(master, ports);

	str = getstring(master, KYPCF_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		gbl_msg.fatal("Could not find or open %s\n", str->c_str());
	}

	// Uncomment any lines referencing top-level ports
	// {{{
	while(fgets(line, sizeof(line), fpsrc)) {
		const char	*SET_IO_KEY = "set_io";
		const	char	*cptr;

		STRINGP	tmp = trim(STRING(line));
		strcpy(line, tmp->c_str());
		delete	tmp;


		// Ignore any lines that don't begin with #,
		// Ignore any lines that start with two ##'s,
		// Ignore any lines that don't have the SET_IO_KEY within them
		if ((line[0] != '#')||(line[1] == '#')
			||(NULL == (cptr= strstr(line, SET_IO_KEY)))) {
			fprintf(fp, "%s\n", line);
			continue;
		}

		bool	found = false;
		char	*cpy = strdup(cptr + strlen(SET_IO_KEY));
		char		*ptr, *name;

		name = strtok(cpy, " \t");

		ptr = name;
		while((*ptr)
			&&(*ptr != '[')
			&&(*ptr != '}')
			&&(*ptr != ']')
			&&(!isspace(*ptr)))
			ptr++;
		*ptr = '\0';

		gbl_msg.info("Found PCF port: %s\n", name);

		// Now, let's check to see if this is in our set
		for(unsigned k=0; k<ports.size(); k++) {
			if (strcmp(ports[k]->c_str(), name)==0) {
				found = true;
				break;
			}
		} free(cpy);

		if (found) {
			int start = 0;
			while((line[start])&&(
					(line[start]=='#')
					||(isspace(line[start]))))
				start++;
			fprintf(fp, "%s\n", &line[start]);
		} else
			fprintf(fp, "%s\n", line);
	} fclose(fpsrc);
	// }}}

	fprintf(fp, "\n## Adding in any PCF_INSERT tags\n\n");
	// {{{
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYPCF_INSERT);
		if (NULL == str) {
			fprintf(fp, "## No PCF.INSERT tag in %s\n",
				kvpair->first.c_str());
			continue;
		}

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			str->c_str());
	}
	str = getstring(master, KYPCF_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			str->c_str());
	}
	// }}}
}

void	build_lpf(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;
	PORTLIST		ports;
	FILE			*fpsrc;
	char	line[512];

	gbl_msg.info("\n\nBUILD-LPF\nLooking for ports:\n");
	gbl_msg.flush();

	get_portlist(master, ports);

	// Uncomment any lines referencing top-level ports
	// {{{
	str = getstring(master, KYLPF_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		gbl_msg.fatal("Could not find or open %s\n", str->c_str());
	}

	while(fgets(line, sizeof(line), fpsrc)) {
		const char	*LOC_KEY = "LOCATE COMP",
				*BUF_KEY = "IOBUF PORT";
		const	char	*locp, *bufp, *keyp;

		STRINGP	tmp = trim(STRING(line));
		strcpy(line, tmp->c_str());
		delete	tmp;


		// Ignore any lines that don't begin with #,
		// Ignore any lines that start with two ##'s,
		// Ignore any lines that don't have either key within them
		if ((line[0] != '#')||(line[1] == '#')
			||((NULL == (locp= strcasestr(line, LOC_KEY)))
			&&(NULL == (bufp= strcasestr(line, BUF_KEY))))) {
			fprintf(fp, "%s\n", line);
			continue;
		}

		bool	found = false;
		keyp = LOC_KEY;
		if (!locp) {
			locp = bufp;
			keyp = BUF_KEY;
		}

		if (strncmp(locp+strlen(keyp), " \"", 2)==0)
			locp += 2;
		char	*cpy = strdup(locp + strlen(keyp));
		char		*ptr, *name;

		name = strtok(cpy, " \t");

		ptr = name;
		while((*ptr)
			&&(*ptr != '[')
			&&(*ptr != '}')
			&&(*ptr != ']')
			&&(*ptr != '\"')
			&&(!isspace(*ptr)))
			ptr++;
		*ptr = '\0';

		gbl_msg.info("Found LOC port: %s\n", name);

		// Now, let's check to see if this is in our set
		for(unsigned k=0; k<ports.size(); k++) {
			if (strcmp(ports[k]->c_str(), name)==0) {
				found = true;
				break;
			}
		} free(cpy);

		if (found) {
			int start = 0;
			while((line[start])&&(
					(line[start]=='#')
					||(isspace(line[start]))))
				start++;
			fprintf(fp, "%s\n", &line[start]);
		} else
			fprintf(fp, "%s\n", line);
	} fclose(fpsrc);
	// }}}

	fprintf(fp, "\n## Adding in any LPF_INSERT tags\n\n");
	// {{{
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYLPF_INSERT);
		if (NULL == str)
			continue;

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			str->c_str());
	}
	str = getstring(master, KYLPF_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			str->c_str());
	}
	// }}}
}

void	build_ucf(MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRINGP			str;
	FILE			*fpsrc;
	PORTLIST		ports;
	char	line[512];

	gbl_msg.info("\n\nBUILD-UCF\nLooking for ports:\n");
	gbl_msg.flush();

	get_portlist(master, ports);

	// Uncomment any lines referencing top-level ports
	// {{{
	str = getstring(master, KYUCF_FILE);
	fpsrc = open_in(master, *str);

	if (!fpsrc) {
		gbl_msg.fatal("Could not find or open %s\n", str->c_str());
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

			gbl_msg.info("Found UCF port: %s", name);
			gbl_msg.flush();

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
	} fclose(fpsrc);
	// }}}

	fprintf(fp, "\n## Adding in any UCF_INSERT tags\n\n");
	// {{{
	for(kvpair = master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		str = getstring(kvpair->second, KYUCF_INSERT);
		if (str == NULL)
			continue;

		fprintf(fp, "## From %s\n%s", kvpair->first.c_str(),
			str->c_str());
	} str = getstring(master, KYUCF_INSERT);
	if (NULL != str) {
		fprintf(fp, "## From the global level\n%s",
			str->c_str());
	}
	// }}}
}

int	main(int argc, char **argv) {
	int		argn, nhash = 0;
	MAPDHASH	master;
	FILE		*fp;
	STRING		str, cmdline, searchstr = ".";
	const char	*subdir = NULL;


	// gbl_msg.open("autofpga.dbg", "w");

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
				case 'd':
					if (argv[argn][j+1]) {
						gbl_msg.open("autofpga.dbg");
						gbl_msg.userinfo("Opened %s\n", "autofpga.dbg");
					} else if (argv[argn+1][0] == '-') {
						gbl_msg.open("autofpga.dbg");
						gbl_msg.userinfo("Opened %s\n", "autofpga.dbg");
					} else {
						gbl_msg.open(argv[++argn]);
						gbl_msg.userinfo("Opened %s\n", argv[argn]);
					}
					j+=5000;
					break;
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
			} fhash = parsefile(argv[argn], *path);
			if (fhash) {
				mergemaps(master, *fhash);
				delete fhash;

				nhash++;
			}
		}
	}

	if (nhash == 0)
		gbl_msg.fatal("No files given, no files written\n");

	gbl_hash = &master;

	STRINGP	subd = NULL;
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
		gbl_msg.fatal("OUTPUT SUBDIRECTORY = %s\n"
			"Cowardly refusing to place output products into the "
			"root directory, '/'\n", subd->c_str());
	} if ((*subd)[subd->size()-1] == '/')
		(*subd).erase(subd->size()-1);

	{	// Build ourselves a subdirectory for our outputs
		// First, check if the directory exists.
		// If it does not, then create it.
		struct	stat	sbuf;
		if (0 == stat(subd->c_str(), &sbuf)) {
			if (!S_ISDIR(sbuf.st_mode)) {
				gbl_msg.fatal("%s exists, and is not a directory\n"
				"Cowardly refusing to erase %s and build a directory in its place\n", subd->c_str(), subd->c_str());
			}
		} else if (mkdir(subd->c_str(), 0777) != 0) {
			gbl_msg.fatal("Could not create %s/ directory\n",
				subd->c_str());
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
	assign_interrupts(master);
	reeval(master);
	find_clocks(master);
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

	build_ld_files(master, subd);

	build_latex_tbls( master);

	str = subd->c_str(); str += "/toplevel.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_toplevel_v(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/main.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_main_v(  master, fp, str);
		// fprintf(fp, "//\n//\n//\n//\n//\n//\n");
		// writeout_bus_defns_v(fp);
		// writeout_bus_logic_v(fp);
		fclose(fp); }
	build_cachable_v(master, subd);

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
		else
			gbl_msg.error("Cannot open %s !\n", str.c_str());
	}

	if (NULL != getstring(master, KYPCF_FILE)) {
		str = subd->c_str(); str += "/build.pcf";
		fp = fopen(str.c_str(), "w");
		if (fp) { build_pcf(  master, fp, str); fclose(fp); }
		else
			gbl_msg.error("Cannot open %s !\n", str.c_str());
	}

	if (NULL != getstring(master, KYLPF_FILE)) {
		str = subd->c_str(); str += "/build.lpf";
		fp = fopen(str.c_str(), "w");
		if (fp) { build_lpf(  master, fp, str); fclose(fp); }
		else
			gbl_msg.error("Cannot open %s !\n", str.c_str());
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

	if (0 != gbl_msg.status())
		gbl_msg.error("ERR: Errors present\n");

	gbl_msg.dump(master);
	return gbl_msg.status();
}
