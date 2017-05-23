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

int	gbl_err = 0;
const bool	DELAY_ACK = true;
FILE	*gbl_dump = NULL;



//
// The PLIST, made of PERIPH(erals)
//
// A structure to contain information about lists of peripherals
//
typedef	struct PERIPH_S {
	unsigned	p_base;		// In octets
	unsigned	p_naddr;	// In words, given in file
	unsigned	p_awid;		// Log_2 (octets)
	unsigned	p_mask;		// Words.  Bit is true if relevant for address selection
	//
	unsigned	p_skipaddr;
	unsigned	p_skipmask;
	unsigned	p_sadrmask;
	unsigned	p_skipnbits;
	//
	unsigned	p_sbaw;
	STRINGP		p_name;
	MAPDHASH	*p_phash;
} PERIPH, *PERIPHP;
typedef	std::vector<PERIPHP>	PLIST;

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




//
// Define some global variables, such as a couple different lists of various
// peripheral sets
PLIST	plist, slist, dlist, mlist;
// A list of all of our interrupt controllers
PICLIST	piclist;
unsigned	unusedmsk;


//
// nextlg
//
// This is basically ceil(log_2(vl))
//
unsigned	nextlg(const unsigned vl) {
	unsigned r;

	for(r=0; (1u<<r)<vl; r+=1)
		;
	return r;
}

//
// popc
//
// Return the number of non-zero bits in a given value
unsigned	popc(const unsigned vl) {
	unsigned	r;
	r = ((vl & 0xaaaaaa)>>1)+(vl & 0x55555555);
	r = (r & 0x33333333)+((r>> 2)&0x33333333);
	r = (r +(r>> 4))&0x0f0f0f0f;
	r = (r +(r>> 8))&0x001f001f;
	r = (r +(r>>16))&0x03f;
	return r;
}

//
// Is the given location within our hash a master?  Look it up.
//
// To be a bus master, it must have a @MTYPE field.
//
bool	isbusmaster(MAPDHASH &phash) {
	return (phash.end() != phash.find(KYMTYPE));
}

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
	return (phash.end() != phash.find(KYPTYPE));
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

/*
bool	hasscope(MAPDHASH &phash) {
	return (phash.end() != phash.find(KYSCOPE));
}
*/

// Does this reference a memory peripheral?
bool	ismemory(MAPDHASH &phash) {
	STRINGP	strp;

	strp = getstring(phash, KYPTYPE);
	if (!strp)
		return false;
	if (STRING(KYMEMORY)!= *strp)
		return false;
	return true;
}

bool	ismemory(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return ismemory(*pmap.u.m_m);
}

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

//
// Count the number of peripherals that are children of this top level hash,
// and count them by type as well.
//
int count_peripherals(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair, kvptype;
	kvpair = info.find(KYNP);
	if ((kvpair != info.end())&&((*kvpair).second.m_typ == MAPT_INT)) {
		return (*kvpair).second.u.m_v;
	}

	unsigned	count = 0, np_single=0, np_double=0, np_memory=0;
	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		STRINGP	strp;
		if (isperipheral(kvpair->second)) {
			count++;

			// Let see what type of peripheral this is
			strp = getstring(kvpair->second, KYPTYPE);
			if (KYSINGLE == *strp) {
				np_single++;
			} else if (KYDOUBLE == *strp) {
				np_double++;
			} else if (KYMEMORY == *strp)
				np_memory++;
		}
	}

	setvalue(info, KYNP, count);
	setvalue(info, KYNPSINGLE, np_single);
	setvalue(info, KYNPDOUBLE, np_double);
	setvalue(info, KYNPMEMORY, np_memory);

	return count;
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

/*
int count_scopes(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	kvpair = info.find(KYNSCOPES);
	if ((kvpair != info.end())&&((*kvpair).second.m_typ == MAPT_INT)) {
		return (*kvpair).second.u.m_v;
	}

	int	count = 0;
	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			if (kvpair->second.m_typ != MAPT_MAP)
				continue;
			if (hasscope(*kvpair->second.u.m_m))
				count++;
		}
	}

	MAPT	elm;
	elm.m_typ = MAPT_INT;
	elm.u.m_v = count;
	info.insert(KEYVALUE(STRING("NSCOPES"), elm));

	return count;
}
*/

//
// Place a legal notice at the top of every file.  The legal notice is given
// to us as a filename, which we must then open and parse.
//
// We also do several substitutions here, given by the strings below.  This
// allows us to mark all of our produced files with their names within them,
// as well as marking the project that they are associated with.
//
void	legal_notice(MAPDHASH &info, FILE *fp, STRING &fname,
			const char *cline = NULL,
			const char *comment = "//") {
	char	line[512];
	FILE	*lglfp;
	STRING	str;

	STRINGP	legal_fname = getstring(info, str = KYLEGAL);
	if (!legal_fname) {
		fprintf(stderr, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license your project under the GPL\n");
		fprintf(fp, "%s WARNING: NO COPYRIGHT NOTICE\n\n%s Please be considerate and license this under the GPL\n", comment, comment);
		return;
	}

	lglfp = fopen(legal_fname->c_str(), "r");
	if (!lglfp) {
		fprintf(stderr, "ERR: Cannot open copyright notice file, %s\n", legal_fname->c_str());
		fprintf(fp, "%s WARNING: NO COPYRIGHT NOTICE\n\n"
			"%s Please be considerate and license this project under the GPL\n", comment, comment);
		perror("O/S Err:");
		gbl_err++;
		return;
	}

	while(fgets(line, sizeof(line), lglfp)) {
		static const char	kyfname[] = "// Filename:",
					kyproject[] = "// Project:",
					kycmdline[] = "// CmdLine:",
					kycomment[] = "//",
					kycline[] = "///////////////////////////////////////////////////////////////////////";
		if (strncasecmp(line, kyfname, strlen(kyfname))==0) {
			fprintf(fp, "%s %s\t%s\n", comment, &kyfname[3],
				fname.c_str());
		} else if (strncasecmp(line, kyproject, strlen(kyproject))==0) {
			STRINGP	strp;
			if (NULL != (strp = getstring(info, KYPROJECT)))
				fprintf(fp, "%s %s\t%s\n", comment,
					&kyproject[3], strp->c_str());
			else
				fputs(line, fp);
		} else if (strncasecmp(line, kycmdline, strlen(kycmdline))==0){
			STRINGP	strp = getstring(info, KYCMDLINE);
			if (strp)
				fprintf(fp, "%s %s\t%s\n", comment,
					&kycmdline[3], strp->c_str());
			else {
				fprintf(fp, "%s\t(No command line data found)\n",
					kycmdline);
			}
		}else if((cline)&&(strncmp(line, kycline, strlen(kycline))==0)){
			fprintf(fp, "%s\n", cline);
		} else if (strncmp(line, kycomment, strlen(kycomment))==0) {
			fprintf(fp, "%s%s", comment, &line[strlen(kycomment)]);
		} else
			fputs(line, fp);
	}
}

//
// compare_naddr
//
// This is part of the peripheral sorting mechanism, whereby peripherals are
// sorted by the numbers of addresses they use.  Peripherals using fewer
// addresses are placed first, with peripherals using more addresses placed
// later.
//
bool	compare_naddr(PERIPHP a, PERIPHP b) {
	if (!a)
		return (b)?false:true;
	else if (!b)
		return true;
	if (a->p_naddr != b->p_naddr)
		return (a->p_naddr < b->p_naddr);
	// Otherwise ... the two address types are equal.
	if ((a->p_name)&&(b->p_name))
		return (*a->p_name) < (*b->p_name);
	else if (!a->p_name)
		return true;
	else
		return true;
}

bool	compare_address(PERIPHP a, PERIPHP b) {
	if (!a)
		return (b)?false:true;
	else if (!b)
		return true;
	return (a->p_base < b->p_base);
}

//
// Add a peripheral to a given list of peripherals
int	addto_plist(PLIST &plist, MAPDHASH *phash) {
	STRINGP	pname;
	int	naddr;

	pname  = getstring(*phash, KYPREFIX);
	if (!pname) {
		fprintf(stderr, "ERR: Skipping unnamed peripheral\n");
		return -1;
	}
	
	if (!getvalue(*phash, KYNADDR, naddr)) {
		fprintf(stderr,
		"WARNING: Skipping peripheral %s with no addresses\n",
			pname->c_str());
		return -1;
	}
	PERIPHP p = new PERIPH;
	p->p_base = 0;
	p->p_naddr = naddr;
	p->p_awid  = nextlg(p->p_naddr)+2;
	p->p_phash = phash;
	p->p_name  = pname;

	plist.push_back(p);
	return plist.size()-1;
}
int	addto_plist(PLIST &plist, PERIPHP p) {
	plist.push_back(p);
	return plist.size()-1;
}

/*
 * build_skiplist
 *
 * Prior to any address decoding, we can build a skip list.  This collects
 * all of the relevant bits necessary for peripheral address decoding among
 * all of the peripherals within the list.  The result is that address decoding
 * can take place with fewer bits that need to be checked.  The other result,
 * though, is that a peripheral may have many locations in the memory space.
 */
void	buildskip_plist(PLIST &plist, unsigned nulladdr) {
	// Adjust the mask to find only those bits that are needed
	unsigned	masteraddr = 0;
	unsigned	adrmask = 0xffffffff,
			nullmsk = (nulladdr)?(-1<<(nextlg(nulladdr)-2)):0,
			setbits = 0,
			disbits = 0;

	// Find all the bits that our address decoder depends upon here.
	//
	// Do our computation in words, not bytes
	for(int bit=31; bit >= 0; bit--) {
		unsigned	bmsk = (1<<bit), bvl;
		bool	bset = false, onebit=true;

		for(unsigned i=0; i<plist.size(); i++) {
			if (bmsk & plist[i]->p_mask) {
				unsigned basew = plist[i]->p_base>>2;
				if (!bset) {
					// If we haven't seen this bit before,
					// let's mark it as known, and mark down
					// what it is.
					bvl = bmsk & basew;
					bset = true;
					onebit = true;
				} else if ((basew & bmsk)!=bvl) {
					// If we have seen this bit before,
					// AND it disagrees with the last time
					// we saw it, then this bit is an
					// important part of the address that
					// we cannot remove.
					onebit = false;
					break;
				}
			}
		}
		if (bset)
			setbits |= bmsk;
		if (!onebit) {
			disbits |= bmsk;
			continue;
		}
		// If its not part of anyone's mask, ... don't use it
		//
		// Not appropriate.  What if one peripheral requires 11 bit
		// decoding, but no one else does?  i.e. one peripheral has a
		// 4 word address space, but all others have an 8 word or more
		// address space?
		// if (!bset)
			// continue;

		if (bmsk & nullmsk) {
			if ((bset)&&(bvl != 0))
				continue;
		}

		// The adrmask is a mask of bits necessary for selection
		adrmask &= (~bmsk);
		// The masteraddr is a mask of what the bits are that are
		// to be set.
		masteraddr|= bvl;
	}

	// { // Just do a double check here, for consistency

	adrmask<<=2; // Convert the result from to octets
	if (gbl_dump)
		fprintf(gbl_dump, "ADRMASK = %08x (%2d bits)\n", adrmask, popc(adrmask));
	// printf("MASTRAD = %08x\n", masteraddr);
	// printf("DISBITS = %08x\n", disbits);
	// printf("SETBITS = %08x\n", setbits);

	// adrmask in octets
	int sbaw = popc(adrmask);
	for(unsigned i=0; i<plist.size(); i++) {
		// Do this work in octet space
		unsigned skipaddr = 0, skipmask = 0, skipnbits = 0,
			pmsk = plist[i]->p_mask<<2; // Mask in octets
		PERIPHP	p = plist[i];
		for(int bit=31; bit>= 0; bit--) {
			unsigned	bmsk = (1<<bit);
			if (bmsk & adrmask) {
				skipaddr <<= 1;
				skipmask = (skipmask<<1)|((pmsk&bmsk)?1:0);
				if (bmsk & pmsk)
					skipnbits ++;
				if (p->p_base & bmsk)	// Octets
					skipaddr |= 1;
			}
		}

		p->p_skipaddr = skipaddr;
		p->p_skipmask = skipmask;
		p->p_sadrmask = adrmask;
		p->p_skipnbits= sbaw;
		//
		setvalue(*plist[i]->p_phash, KYSKIPADDR,  skipaddr);
		setvalue(*plist[i]->p_phash, KYSKIPMASK,  skipmask);
		setvalue(*plist[i]->p_phash, KYSKIPDEFN,  adrmask);
		setvalue(*plist[i]->p_phash, KYSKIPNBITS, skipnbits);
		setvalue(*plist[i]->p_phash, KYSKIPAWID,  sbaw);
		// printf("// %08x %08x \n", p->p_base, pmsk);
		if (gbl_dump)
			fprintf(gbl_dump,
			"// skip-assigning %12s_... to %08x & %08x, "
				"%2d, [%08x]AWID=%d\n",
			plist[i]->p_name->c_str(), skipaddr, skipmask,
			skipnbits, adrmask, sbaw);
	}

	/* Just to be sure, let's test that this works ... */
	for(unsigned i=0; i<plist.size(); i++) {
		// printf("%2d: %2s\n", i, plist[i]->p_name->c_str());
		assert((plist[i]->p_skipaddr & (~plist[i]->p_skipmask))==0);
	}
	for(unsigned a=0; a<(1u<<sbaw); a++) {
		int nps = 0;
		for(unsigned i=0; i<plist.size(); i++) {
			if ((a&plist[i]->p_skipmask)==plist[i]->p_skipaddr)
				nps++;
		} assert(nps < 2);
	}
}

/*
 * build_plist
 *
 * Collect our peripherals into one of four lists.  This allows us to have
 * ordered access through the peripherals later.
 */
void	build_plist(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;

	int np = count_peripherals(info);

	if (np < 1) {
		if (gbl_dump)
			fprintf(gbl_dump, "Only %d peripherals\n", np);
		return;
	}

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			STRINGP	ptype;
			MAPDHASH	*phash = kvpair->second.u.m_m;
			if (NULL != (ptype = getstring(kvpair->second, KYPTYPE))) {
				if (KYSINGLE == *ptype)
					addto_plist(slist, phash);
				else if (KYDOUBLE == *ptype)
					addto_plist(dlist, phash);
				else if (KYMEMORY == *ptype) {
					addto_plist(plist, phash);
					addto_plist(mlist, plist[plist.size()-1]);
				} else
					addto_plist(plist, phash);
			} else
				addto_plist(plist, phash);
		}
	}

	// Sort by address usage
	std::sort(slist.begin(), slist.end(), compare_naddr);
	std::sort(dlist.begin(), dlist.end(), compare_naddr);
	std::sort(mlist.begin(), mlist.end(), compare_naddr);
	std::sort(plist.begin(), plist.end(), compare_naddr);
}

//
// assign_addresses
//
// Assign addresses to all of our peripherals, given a first address to start
// from.  The first address helps to insure that the NULL address creates a
// proper error (as it should).
//
void assign_addresses(MAPDHASH &info, unsigned first_address = 0x400) {
	unsigned	start_address = first_address;
	MAPDHASH::iterator	kvpair;

	int np = count_peripherals(info);
	int baw = count_peripherals(info);	// log_2 octets

	if (np < 1) {
		return;
	}

	if (gbl_dump)
		fprintf(gbl_dump, "// Assigning addresses to the S-LIST\n");
	// Find the number of slist addresses
	MAPDHASH	*sio_hash = NULL, *dio_hash = NULL;

	if (slist.size() > 0) {
		// Each has only the one address ...
		int naddr = slist.size();
		for(unsigned i=0; i<slist.size(); i++) {
			slist[i]->p_base = start_address + 4*i;
			if (gbl_dump)
				fprintf(gbl_dump, "// Assigning %12s_... to %08x\n", slist[i]->p_name->c_str(), slist[i]->p_base);

			setvalue(*slist[i]->p_phash, KYBASE, slist[i]->p_base);
		}
		start_address += (1<<(nextlg(naddr)+2));

		for(unsigned i=0; i<slist.size(); i++) {
			if (gbl_dump)
			fprintf(gbl_dump, "Setting SLIST[%d] MASK to %08x\n", i,
				((1<<baw)-1)&-4);
			slist[i]->p_mask = (1<<(baw-2))-1;	// Words
			setvalue(*slist[i]->p_phash, KYMASK, slist[i]->p_mask);
		}

		// Build an SIO peripheral
		MAPT		elm;
		elm.m_typ = MAPT_MAP;
		elm.u.m_m = sio_hash = new MAPDHASH;
		info.insert(KEYVALUE(KYSIO, elm));
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = new STRING(KYSIO);
		sio_hash->insert(KEYVALUE(KYPREFIX, elm));
		elm.m_typ = MAPT_INT;
		elm.u.m_v = (1<<(nextlg(naddr)));
		sio_hash->insert(KEYVALUE(KYNADDR, elm));
		setvalue(*sio_hash, KYREGS_N, 0);
	}

	// Assign double-peripheral bus addresses
	if (dlist.size() > 1) {
		if (gbl_dump)
			fprintf(gbl_dump, "// Assigning addresses to the D-LIST, starting from %08x\n", start_address);
		unsigned start = 0;
		for(unsigned i=0; i<dlist.size(); i++) {
			dlist[i]->p_base = 0;
			dlist[i]->p_base = (start + ((1<<dlist[i]->p_awid)-1));
			dlist[i]->p_base &= (-1<<(dlist[i]->p_awid));
			// First valid next address is ...
			start = dlist[i]->p_base + (1<<(dlist[i]->p_awid));
		}

		int dnaddr = (1<<nextlg(start));
		start_address = (start_address + (dnaddr)-1)
				& (~(dnaddr-1));

		for(unsigned i=0; i<dlist.size(); i++) {
			dlist[i]->p_base += start_address;
			if (gbl_dump)
				fprintf(gbl_dump,
				"// Assigning %12s_... to %08x/%08x\n",
				dlist[i]->p_name->c_str(), dlist[i]->p_base,
				(-1<<(dlist[i]->p_awid)));

			setvalue(*dlist[i]->p_phash, KYBASE, dlist[i]->p_base);
			dlist[i]->p_mask = (-1<<(dlist[i]->p_awid-2));
			setvalue(*dlist[i]->p_phash, KYMASK, dlist[i]->p_mask);
		}

		start_address = (start_address+dnaddr);

		// Build an DIO peripheral
		MAPT		elm;
		elm.m_typ = MAPT_MAP;
		elm.u.m_m = dio_hash = new MAPDHASH;
		info.insert(KEYVALUE(KYDIO, elm));
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = new STRING(KYDIO);
		dio_hash->insert(KEYVALUE(KYPREFIX, elm));
		elm.m_typ = MAPT_INT;
		elm.u.m_v = dnaddr;
		dio_hash->insert(KEYVALUE(KYNADDR, elm));
		setvalue(*dio_hash, KYREGS_N, 0);
	} else if (dlist.size() == 1) {
		dio_hash = dlist[0]->p_phash;
	}

	fprintf(stderr, "DLIST.SIZE = %ld\n", dlist.size());

	// Assign bus addresses to the more generic peripherals
	if (gbl_dump) {
		fprintf(gbl_dump, "// Assigning addresses to the P-LIST (all other addresses)\n");
		fprintf(gbl_dump, "// Starting from %08x\n", start_address);
	}
	for(unsigned i=0; i<plist.size(); i++) {
		if (plist[i]->p_naddr < 1)
			continue;
		// Make this address 32-bit aligned
		plist[i]->p_base = (start_address + ((1<<plist[i]->p_awid)-1));
		plist[i]->p_base &= (-1<<(plist[i]->p_awid));
		if (gbl_dump)
		fprintf(gbl_dump, "// assigning %12s_... to %08x/%08x\n",
			plist[i]->p_name->c_str(),
			plist[i]->p_base,
			(-1<<(plist[i]->p_awid)));
		start_address = plist[i]->p_base + (1<<(plist[i]->p_awid));
		plist[i]->p_mask = (-1<<(plist[i]->p_awid-2));

		setvalue(*plist[i]->p_phash, KYBASE, plist[i]->p_base);
		setvalue(*plist[i]->p_phash, KYMASK, plist[i]->p_mask);
	}

	// Adjust the mask to find only those bits that are needed
	unsigned	masteraddr = 0;
	unusedmsk = 0xffffffff;
	for(int bit=31; bit >= 0; bit--) {
		unsigned	bmsk = (1<<bit), bvl;
		bool	bset = false, onebit=true;

		for(unsigned i=0; i<slist.size(); i++) {
			if (bmsk & slist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & (slist[i]->p_base>>2);
					bset = true;
					onebit = true;
				} else if ((slist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
					break;
				}
			}
		} if (!onebit)
			continue;

		for(unsigned i=0; i<dlist.size(); i++) {
			if (bmsk & dlist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & (dlist[i]->p_base>>2);
					bset = true;
					onebit = true;
				} else if ((dlist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
					break;
				}
			}
		} if (!onebit)
			continue;

		for(unsigned i=0; i<plist.size(); i++) {
			if (bmsk & plist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & (plist[i]->p_base>>2);
					bset = true;
					onebit = true;
				} else if ((plist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
					break;
				}
			}
		} if (!onebit)
			continue;

		unusedmsk &= (~bmsk);
		masteraddr|= bvl;
	}

	// printf("// Pre Null Mask: %08x\n", unusedmsk);
	if ((1u<<nextlg(first_address))==first_address) {
		unusedmsk |= (first_address);
	}

	if (gbl_dump) {
		fprintf(gbl_dump, "// Overall  Mask: %08x\n", unusedmsk);
		fprintf(gbl_dump, "// SkipAddr Mask: %08x\n", ~unusedmsk);
		fprintf(gbl_dump, "// Mask popcount: %08x\n", popc(unusedmsk));
	}

	setvalue(info, KYSKIPADDR, unusedmsk);		// octets
	setvalue(info, KYSKIPNBITS,popc(unusedmsk));	// octets
	// Now, go back and see if anything references this address
	// or these masks
	reeval(info);

	unsigned	smask = 0, dmask = 0;
	if (dio_hash) {
		int dio_id = addto_plist(plist, dio_hash);
		int	v;

		assert(dio_id >= 0);
		plist[dio_id]->p_base = dlist[0]->p_base;
		assert(getvalue(*dio_hash, KYNADDR, v));
		plist[dio_id]->p_naddr = v;
		if (gbl_dump)
			fprintf(gbl_dump, "DIO.NADDR = %08x\n", v);
		plist[dio_id]->p_awid  = nextlg(plist[dio_id]->p_naddr);
		if (gbl_dump)
			fprintf(gbl_dump, "DIO.AWID  = %08x\n", plist[dio_id]->p_awid);
		plist[dio_id]->p_mask = (-1<<(plist[dio_id]->p_awid-2));
		dmask = plist[dio_id]->p_mask;
		if (gbl_dump)
			fprintf(gbl_dump, "DIO.MASK  = %08x\n", dmask);
		setvalue(*dio_hash, KYBASE, plist[dio_id]->p_base);
		setvalue(*dio_hash, KYMASK, plist[dio_id]->p_mask);
	} if (sio_hash) {
		int sio_id = addto_plist(plist, sio_hash);
		int	v;

		assert(sio_id >= 0);
		plist[sio_id]->p_base = slist[0]->p_base;
		getvalue(*sio_hash, KYNADDR, v);
		plist[sio_id]->p_naddr = v;
		plist[sio_id]->p_awid  = nextlg(plist[sio_id]->p_naddr);
		plist[sio_id]->p_mask = (-1<<(plist[sio_id]->p_awid));
		smask = plist[sio_id]->p_mask;
		setvalue(*sio_hash, KYBASE, plist[sio_id]->p_base);
		setvalue(*sio_hash, KYMASK, plist[sio_id]->p_mask);
	}

	// Let's build a minimal address for this component
	if (gbl_dump) {
		fprintf(gbl_dump, "SMASK = %08x\n", smask);
		fprintf(gbl_dump, "DMASK = %08x\n", dmask);
	}
	buildskip_plist(slist, 0);
	buildskip_plist(dlist, 0);
	buildskip_plist(plist, 0x400);
}

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
		if (NULL != (sintlist = getstring(*kvint->second.u.m_m,
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

void	assign_scopes(    MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

int	get_longest_defname(PLIST &plist) {
	unsigned	longest_defname = 0;
	STRING		str;

	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;
		int	nregs = 0;

		if (!getvalue(*plist[i]->p_phash, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			if (kvp == plist[i]->p_phash->end()) {
				fprintf(stderr, "%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				if (gbl_dump)
				fprintf(gbl_dump, "%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname;

			// 1. Read the number (Not used)
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				fprintf(stderr, "No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);
		}
	}

	return longest_defname;
}

//
// write_regdefs
//
// This writes out the definitions of all the registers found within the plist
// to the C++ header file given by fp.  It takes as a parameter the longest
// name definition, which we use to try to line things up in a prettier fashion
// than we could without it.
//
void write_regdefs(FILE *fp, PLIST &plist, unsigned longest_defname) {
	STRING	str;

	// Walk through this peripheral list one peripheral at a time
	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;
		int	nregs = 0;

		// If there is a note key for this peripheral, place it into
		// the output without modifications.
		kvp = findkey(*plist[i]->p_phash,KYREGS_NOTE);
		if ((kvp != plist[i]->p_phash->end())&&(kvp->second.m_typ == MAPT_STRING))
			fprintf(fp, "%s\n", kvp->second.u.m_s->c_str());


		// Walk through each of the defined registers.  There will be
		// @REGS.N registers defined.
		if (!getvalue(*plist[i]->p_phash, KYREGS_N, nregs)) {
			fprintf(stderr, "WARNING: REGS.N not found in %s\n", plist[i]->p_name->c_str());
			continue;
		}

		// Now, walk through all of the register definitions
		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			STRINGP	strp;

			// Look for a @REGS.%d tag, defining each of the named
			// registers within this set.
			sprintf(nstr, "%d", j);
			strp = getstring(*plist[i]->p_phash, str=STRING("REGS.")+nstr);
			if (!strp) {
				fprintf(stderr, "%s not found\n", str.c_str());
				continue;
			}

			STRING	scpy = *strp;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			int roff = strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				if (gbl_dump)
				fprintf(gbl_dump, "No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			fprintf(fp, "#define\t%-*s\t0x%08x", longest_defname,
				rname, (roff<<2)+plist[i]->p_base);

			fprintf(fp, "\t// %08x, wbregs names: ", plist[i]->p_base); 
			int	first = 1;
			// 3. Get the various user names
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (!first)
					fprintf(fp, ", ");
				first = 0;
				fprintf(fp, "%s", rv);
			} fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n\n");
}

//
// build_regdefs_h
//
//
// This builds a regdefs.h file, a file that can be used on a host in order
// to access our design.
//
void	build_regdefs_h(  MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess;
	STRING	str;
	STRINGP	strp;

	legal_notice(master, fp, fname);

	fprintf(fp, "#ifndef\tREGDEFS_H\n");
	fprintf(fp, "#define\tREGDEFS_H\n");
	fprintf(fp, "\n\n");

	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.INCLUDE tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.INCLUDE for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INCLUDE);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INCLUDE for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INCLUDE);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And finally any master REGDEFS.H.INCLUDE tags\n");
	strp = getstring(master, KYREGDEFS_H_INCLUDE);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.INCLUDE\n\n\n");


	fprintf(fp, "//\n// Register address definitions, from @REGS.#d\n//\n");

	unsigned	longest_defname = 0, ldef = 0;

	longest_defname = get_longest_defname(slist);
	ldef = get_longest_defname(dlist);
	longest_defname = (ldef > longest_defname) ? ldef : longest_defname;
	ldef = get_longest_defname(plist);
	longest_defname = (ldef > longest_defname) ? ldef : longest_defname;

	write_regdefs(fp, slist, longest_defname);
	write_regdefs(fp, dlist, longest_defname);
	write_regdefs(fp, plist, longest_defname);


	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.DEFNS tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.DEFNS for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_DEFNS);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.DEFNS for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_DEFNS);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.DEFNS at the top level\n");
	strp = getstring(master, KYREGDEFS_H_DEFNS);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.DEFNS\n");



	fprintf(fp, "//\n");
	fprintf(fp, "// The @REGDEFS.H.INSERT tag\n");
	fprintf(fp, "//\n");
	fprintf(fp, "// @REGDEFS.H.INSERT for masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INSERT for peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_H_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// @REGDEFS.H.INSERT from the top level\n");
	strp = getstring(master, KYREGDEFS_H_INSERT);
	if (strp)
		fputs(strp->c_str(), fp);
	fprintf(fp, "// End of definitions from REGDEFS.H.INSERT\n");

	fprintf(fp, "\n\n");

	fprintf(fp, "#endif\t// REGDEFS_H\n");
}

//
// get_longest_uname
//
// This is very similar to the get longest defname (get length of the longest
// variable definition name) above, save that this is applied to the user 
// names within regdefs.cpp
//
unsigned	get_longest_uname(PLIST &plist) {
	unsigned	longest_uname = 0;
	STRING	str;
	STRINGP	strp;

	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		int nregs = 0;
		if (!getvalue(*plist[i]->p_phash, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			strp = getstring(*plist[i]->p_phash, 
						str=STRING("REGS.")+nstr);
			STRING	scpy = *strp;

			char	*nxtp, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				continue;
			}

			// 2. Get (and ignore) the C definition name
			strtok(nxtp, " \t\n,");

			// 3. Every token that follows will be a user (string)
			// name.  Find the one of these with the longest length.
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (strlen(rv) > longest_uname)
					longest_uname = strlen(rv);
			}
		}
	}

	return longest_uname;
}

//
// write_regnames
//
// 
void write_regnames(FILE *fp, PLIST &plist,
		unsigned longest_defname, unsigned longest_uname) {
	STRING	str;
	STRINGP	strp;
	int	first = 1;

	for(unsigned i=0; i<plist.size(); i++) {
		int nregs = 0;

		if (!getvalue(*plist[i]->p_phash, KYREGS_N, nregs))
			continue;

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			strp = getstring(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			STRING	scpy = *strp;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				if (gbl_dump)
				fprintf(gbl_dump, "No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			// 3. Get the various user names and ... define them
			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (!first)
					fprintf(fp, ",\n");
				first = 0;
				fprintf(fp, "\t{ %-*s,\t\"%s\"%*s\t}",
					longest_defname, rname, rv,
					(int)(longest_uname-strlen(rv)), "");
			}
		}
	}

}


//
// build_regdefs_cpp
//
void	build_regdefs_cpp(MAPDHASH &master, FILE *fp, STRING &fname) {
	STRINGP	strp;
	STRING	str;

	legal_notice(master, fp, fname);

	if (NULL != (strp = getstring(master, KYREGDEFS_CPP_INCLUDE))) {
		fputs(strp->c_str(), fp);
	} else {
		fprintf(fp, "// No default include list found\n");
		fprintf(fp, "#include <stdio.h>\n");
		fprintf(fp, "#include <stdlib.h>\n");
		fprintf(fp, "#include <strings.h>\n");
		fprintf(fp, "#include <ctype.h>\n");
	}

	fprintf(fp, "#include \"regdefs.h\"\n\n");
	fprintf(fp, "const\tREGNAME\traw_bregs[] = {\n");

	// First, find out how long our longest definition name is.
	// This will help to allow us to line things up later.
	unsigned	longest_defname = 0, ldef = 0;
	unsigned	longest_uname = 0;

	// Find the length of the longest register name
	longest_defname = get_longest_defname(slist);
	ldef = get_longest_defname(dlist);
	longest_defname = (ldef > longest_defname) ? ldef : longest_defname;
	ldef = get_longest_defname(plist);
	longest_defname = (ldef > longest_defname) ? ldef : longest_defname;

	// Find the length of the longest user name string
	longest_uname = get_longest_uname(slist);
	ldef = get_longest_uname(dlist);
	longest_uname = (ldef > longest_defname) ? ldef : longest_uname;
	ldef = get_longest_uname(plist);
	longest_uname = (ldef > longest_defname) ? ldef : longest_uname;


	if (slist.size() > 0) {
		write_regnames(fp, slist, longest_defname, longest_uname);
		if ((dlist.size() > 0)||(plist.size() > 0))
			fprintf(fp, ",\n");
	} if (dlist.size() > 0) {
		write_regnames(fp, dlist, longest_defname, longest_uname);
		if (plist.size() > 0)
			fprintf(fp, ",\n");
	} write_regnames(fp, plist, longest_defname, longest_uname);

	fprintf(fp, "\n};\n\n");

	fprintf(fp, "// REGSDEFS.CPP.INSERT for any bus masters\n");
	for(MAPDHASH::iterator kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_CPP_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And then from the peripherals\n");
	for(MAPDHASH::iterator kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		strp = getstring(kvpair->second, KYREGDEFS_CPP_INSERT);
		if (strp)
			fputs(strp->c_str(), fp);
	}

	fprintf(fp, "// And finally any master REGS.CPP.INSERT tags\n");
	if (NULL != (strp = getstring(master, KYREGDEFS_CPP_INSERT))) {
		fputs(strp->c_str(), fp);
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
		defns = getstring(*kvpair->second.u.m_m, KYBDEF_INCLUDE);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(*kvpair->second.u.m_m, KYBDEF_DEFN);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(*kvpair->second.u.m_m, KYBDEF_INSERT);
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
	STRINGP	strp;
	int		reset_address;
	PERIPHP		fastmem = NULL, bigmem = NULL;

	legal_notice(master, fp, fname, "/*******************************************************************************", "*"); 
	fprintf(fp, "*/\n");

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

void	mkselect(FILE *fp, MAPDHASH &master, PLIST &plist, const STRING &subsel,
		const char *skipn) {
	const	char	*addrbus = "wb_addr";
	bool use_skipaddr = false;
	int	v, sbaw;
	unsigned	skipaddrw = 0, skipnbits;

	use_skipaddr = getvalue(master, KYSKIPADDR, v);	// v in octets
	// use_skipaddr = false;
	sbaw = get_address_width(master) - 2;	// words
	if (use_skipaddr) {
		getvalue(*plist[0]->p_phash, KYSKIPDEFN, v);
		skipaddrw = v>>2;
		skipnbits = popc(skipaddrw);
		sbaw = nextlg(skipaddrw);
		fprintf(fp, "\twire	[%d:0]	%s; // bits %08x, sbaw=%d\n",
			skipnbits-1, skipn, skipaddrw<<2, sbaw);
		fprintf(fp, "\tassign\t%s = {\n", skipn);
		for(int i=sbaw; i>=0; i--) {
			unsigned bmsk = (1<<i);		// Words
			if (bmsk & (skipaddrw)) {	// Words
				if ((bmsk!=1)&&((bmsk>>1)&(skipaddrw))) {
					int	j;
					for(j=i-1; j>=0; j--) {
						bmsk = (1<<j);
						if ((bmsk & (skipaddrw))==0)
							break;
					}
					fprintf(fp, "\t\t\twb_addr[%2d:%2d]%s\n", i,j+1,
						(((1<<(j+1))-1)&(skipaddrw))?",":"");
					i = j;
				} else {
					fprintf(fp, "\t\t\twb_addr[%d]%s\n", i,
						(((1<<i)-1)&(skipaddrw))?",":"");
				}
			}
		} fprintf(fp, "\t\t};\n");

		addrbus = skipn;
		sbaw = skipnbits;
	}

	for(unsigned i=0; i<plist.size(); i++) {
		const char	*pfx = plist[i]->p_name->c_str();
		int	awid = plist[i]->p_awid-2,	// words
			relevant_bits = sbaw - awid,	// Octets-octets->unless
			pmsk = plist[i]->p_mask,	// words
			base = plist[i]->p_base>>2,	// words
			offset = 2;			// bits

		if (use_skipaddr) {
			getvalue(*plist[i]->p_phash, KYSKIPADDR,  base); //alins
			getvalue(*plist[i]->p_phash, KYSKIPMASK,  pmsk); //alins
			getvalue(*plist[i]->p_phash, KYSKIPNBITS,relevant_bits);
			offset = 0;
		}

		fprintf(fp, "\tassign\t%12s_sel = ", pfx);
		if (subsel.size() > 0)
			fprintf(fp, "(%s)&&", subsel.c_str());
		fprintf(fp, "(%8s[%2d:%2d] == %2d\'b",
			addrbus, sbaw-1, sbaw-relevant_bits, relevant_bits);
		for(int j=0; j<relevant_bits; j++) {
			int bit = (sbaw-1)-j;
			fprintf(fp, ((base>>bit)&1)?"1":"0");
			if (((bit+offset)&3)==0)
				fprintf(fp, "_");
		} fprintf(fp, ");\n");
	}
}

void	build_main_v(     MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str = "ACCESS", astr, sellist, acklist, siosel_str, diosel_str;
	int		first;
	unsigned	nsel = 0, nacks = 0, baw;

	legal_notice(master, fp, fname);
	baw = get_address_width(master);

	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	fprintf(fp, "`default_nettype\tnone\n");
	fprintf(fp,
"//\n"
"//\n"
"// Here is a list of defines which may be used, post auto-design\n"
"// (not post-build), to turn particular peripherals (and bus masters)\n"
"// on and off.  In particular, to turn off support for a particular\n"
"// design component, just comment out its respective define below\n"
"//\n"
"// These lines are taken from the respective @ACCESS tags for each of our\n"
"// components.  If a component doesn\'t have an @ACCESS tag, it will not\n"
"// be listed here.\n"
"//\n");
	fprintf(fp, "// First, the access fields for any bus masters\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, str);
		if (strp)
			fprintf(fp, "`define\t%s\n", strp->c_str());
	}

	fprintf(fp, "// And then for the peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		STRINGP	strp = getstring(*kvpair->second.u.m_m, str);
		if (strp)
			fprintf(fp, "`define\t%s\n", strp->c_str());
	}

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
	if (slist.size()>0) {
		fprintf(fp, "\twire\tsio_sel, sio_stall;\n");
		fprintf(fp, "\treg\tsio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tsio_data;\n");
	}
	if (dlist.size()>0) {
		fprintf(fp, "\twire\tdio_sel, dio_stall;\n");
		fprintf(fp, "\treg\tdio_ack;\n");
		fprintf(fp, "\treg\t\tpre_dio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tdio_data;\n");
	}


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
	"\t// These are given for every configuration file with an @MTYPE\n"
	"\t// tag, and the names are prefixed by whatever is in the @PREFIX tag.\n"
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

	if (slist.size() > 1)
		mkselect(fp, master, slist, KYSIO_SEL, "sio_skip");
	if (dlist.size() > 1)
		mkselect(fp, master, dlist, KYDIO_SEL, "dio_skip");
	mkselect(fp, master, plist, STRING(""), "wb_skip");

	if (plist.size()>0) {
		if (sellist == "")
			sellist = (*plist[0]->p_name) + "_sel";
		else
			sellist = (*plist[0]->p_name) + "_sel, " + sellist;
	} for(unsigned i=1; i<plist.size(); i++)
		sellist = (*plist[i]->p_name)+"_sel, "+sellist;
	nsel += plist.size();

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
	if (plist.size() > 0) {
		if (nacks > 0)
			acklist = (*plist[0]->p_name) + "_ack, " + acklist;
		else
			acklist = (*plist[0]->p_name) + "_ack";
		nacks++;
	} for(unsigned i=1; i < plist.size(); i++) {
		acklist = (*plist[i]->p_name) + "_ack, " + acklist;
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
	if (slist.size() > 0)
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tsio_ack <= (wb_stb)&&(sio_sel);\n");
	if (dlist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tpre_dio_ack <= (wb_stb)&&(dio_sel);\n");
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tdio_ack <= pre_dio_ack;\n");
	}

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
	if (plist.size() > 0) {
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

		if (slist.size() > 0)
			fprintf(fp, "\tassign\tsio_stall = 1\'b0;\n");
		if (dlist.size() > 0)
			fprintf(fp, "\tassign\tdio_stall = 1\'b0;\n");
		fprintf(fp, "\tassign\twb_stall = \n"
			"\t\t  (wb_stb)&&(%6s_sel)&&(%6s_stall)",
			plist[0]->p_name->c_str(), plist[0]->p_name->c_str());
		for(unsigned i=1; i<plist.size(); i++)
			fprintf(fp, "\n\t\t||(wb_stb)&&(%6s_sel)&&(%6s_stall)",
				plist[i]->p_name->c_str(), plist[i]->p_name->c_str());
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

	if (slist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\tcasez({ ");
		for(unsigned i=0; i<slist.size()-1; i++)
			fprintf(fp, "%s_sel, ", slist[i]->p_name->c_str());
		fprintf(fp, "%s_sel })\n", slist[slist.size()-1]->p_name->c_str());
		for(unsigned i=0; i<slist.size(); i++) {
			fprintf(fp, "\t\t%2ld\'b", slist.size());
			for(unsigned j=0; j<slist.size(); j++) {
				if (j < i)
					fprintf(fp, "0");
				else if (j==i)
					fprintf(fp, "1");
				else
					fprintf(fp, "?");
			}
			fprintf(fp, ": sio_data <= %s_data;\n",
				slist[i]->p_name->c_str());
		} fprintf(fp, "\t\tdefault: sio_data <= 32\'h0;\n");
		fprintf(fp, "\tendcase\n\n");
	} else
		fprintf(fp, "\tinitial\tsio_data = 32\'h0;\n"
			"\talways @(posedge i_clk)\n\t\tsio_data = 32\'h0;\n");

	if (dlist.size() > 0) {
		fprintf(fp, "\talways @(posedge i_clk)\n");
		fprintf(fp, "\tcasez({ ");
		for(unsigned i=0; i<dlist.size()-1; i++)
			fprintf(fp, "%s_ack, ", dlist[i]->p_name->c_str());
		fprintf(fp, "%s_ack })\n", dlist[dlist.size()-1]->p_name->c_str());
		for(unsigned i=0; i<dlist.size(); i++) {
			fprintf(fp, "\t\t%2ld\'b", dlist.size());
			for(unsigned j=0; j<dlist.size(); j++) {
				if (j < i)
					fprintf(fp, "0");
				else if (j==i)
					fprintf(fp, "1");
				else
					fprintf(fp, "?");
			}
			fprintf(fp, ": dio_data <= %s_data;\n",
				dlist[i]->p_name->c_str());
		} fprintf(fp, "\t\tdefault: dio_data <= 32\'h0;\n\tendcase\n");
	}

	if (DELAY_ACK) {
		fprintf(fp, "\talways @(posedge i_clk)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
	} else {
		fprintf(fp, "`ifdef\tVERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
				for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata = %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata = 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata = 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "\n`else\t// VERILATOR\n\n");
		fprintf(fp, "\talways @(*)\n"
			"\tbegin\n"
			"\t\tcasez({ %s })\n", acklist.c_str());
		for(unsigned i=0; i<plist.size(); i++) {
			fprintf(fp, "\t\t\t%d\'b", nacks);
			for(unsigned j=0; j<nacks; j++)
				fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
			if (i < plist.size())
				fprintf(fp, ": wb_idata <= %s_data;\n",
					plist[plist.size()-1-i]->p_name->c_str());
			else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
		}
		fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
		fprintf(fp, "\t\tendcase\n\tend\n");
		fprintf(fp, "`endif\t// VERILATOR\n");
	}

	fprintf(fp, "\n\nendmodule // main.v\n");

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
	} return fopen(fname.c_str(), "r");
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
				default:
					fprintf(stderr, "Unknown argument, -%c\n", argv[argn][j]);
				}
			}
		} else {
			MAPDHASH	*fhash;
			STRINGP		path;

			path = getstring(master, KYPATH);
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

	STRINGP	subd;
	if (subdir)
		subd = new STRING(subdir);
	else
		subd = getstring(master, str=STRING("SUBDIR"));
	if (subd == NULL)
		// subd = new STRING("autofpga-out");
		subd = new STRING("demo-out");
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
	assign_scopes(    master);
	assign_addresses( master);
	get_address_width(master);

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
	if (fp) { build_main_v(  master, fp, str); fclose(fp); }

	if (0 != gbl_err)
		fprintf(stderr, "ERR: Errors present\n");

	if (gbl_dump)
		mapdump(gbl_dump, master);
	return gbl_err;
}
