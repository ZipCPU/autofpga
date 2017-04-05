////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	autofpga.cpp
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

#include "parser.h"
#include "keys.h"
#include "kveval.h"




typedef	struct PERIPH_S {
	unsigned	p_base;
	unsigned	p_naddr;
	unsigned	p_awid;
	unsigned	p_mask;
	STRINGP		p_name;
	MAPDHASH	*p_phash;
} PERIPH, *PERIPHP;
typedef	std::vector<PERIPHP>	PLIST;

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


class PICINFO {
public:
	STRINGP		i_name, i_bus;
	unsigned	i_max, i_nassigned, i_nallocated;
	ILIST		i_ilist;
	PICINFO(MAPDHASH &pic) {
		i_max = 0;
		int	mx = 0;
		i_name = getstring(pic, KYPREFIX);
		i_bus  = getstring(pic, KYPIC_BUS);
		if (getvalue( pic, KYPIC_MAX, mx)) {
			i_max = mx;
		} else {
			printf("ERR: Cannot find PIC.MAX within ...\n");
			mapdump(pic);
		}
		i_nassigned  = 0;
		i_nallocated = 0;
	} void add(MAPDHASH &psrc, STRINGP iname) {
		if ((!iname)||(i_nallocated >= i_max))
			return;

		INTP	ip = new INTID();
		i_ilist.push_back(ip);
		ip->i_name = iname;
		ip->i_wire = getstring(psrc, KY_WIRE);
		ip->i_id   = i_max;
		ip->i_hash = &psrc;
		i_nallocated++;
	} void add(unsigned id, MAPDHASH &psrc, STRINGP iname) {
		if (!iname) {
			fprintf(stderr, "WARNING: No name given for interrupt\n");
			return;
		} else if (id >= i_max) {
			fprintf(stderr, "ERR: Interrupt ID %d out of bounds [0..%d]\n",
				id, i_max);
		}
		for(unsigned i=0; i<i_nallocated; i++) {
			if (i_ilist[i]->i_id == id) {
				fprintf(stderr, "ERR: %s and %s are both competing for the same %s.interrupt ID\n",
					(i_ilist[i]->i_name)?
						i_ilist[i]->i_name->c_str()
						:"(NULL)",
					iname->c_str(), i_name->c_str());
				return;
			}
		}

		INTP	ip = new INTID();
		i_ilist.push_back(ip);
		ip->i_name = iname;
		ip->i_wire = getstring(psrc, KY_WIRE);
		ip->i_id   = id;
		ip->i_hash = &psrc;
		i_nassigned++;
		i_nallocated++;
	} void assignids(void) {
		for(unsigned iid=0; iid<i_max; iid++) {
			for(unsigned jid=0; jid < i_ilist.size(); jid++) {
				if (i_ilist[jid]->i_id == iid) {
					if (jid != iid) {
						INTP	ia, ib;
						ia = i_ilist[jid];
						ib = i_ilist[iid];
						i_ilist[iid] = ia;
						i_ilist[jid] = ib;
						continue;
					}
				}
			}
		}
		for(unsigned iid=0; iid<i_max; iid++) {
			unsigned mx = (i_ilist.size() > i_max)?i_ilist.size():i_max;
			unsigned	aid = mx, uid = mx;
			for(unsigned jid=0; jid < i_ilist.size(); jid++) {
				if (i_ilist[jid]->i_id == iid) {
					aid = jid;
					break;
				} else if ((i_ilist[jid]->i_id >= i_max)
						&&(uid >= i_max)) {
					uid = jid;
				}
			}

			if (aid < mx)
				continue; /// Interrupt has an assignment
			else if (uid < mx) {
				i_ilist[uid]->i_id = iid;
				i_nassigned++;
			} else
				continue; // All interrupts assigned
		} 
		if (i_max < i_ilist.size()) {
			fprintf(stderr, "WARNING: Too many interrupts assigned to PIC %s\n", i_name->c_str());
		}

		// Write the interrupt assignments back into the map
		for(unsigned iid=0; iid<i_ilist.size(); iid++) {
			if (i_ilist[iid]->i_id <= i_max) {
				STRING	ky = (*i_name) + ".ID";
				setvalue(*i_ilist[iid]->i_hash, ky, i_ilist[iid]->i_id);
			}
		}
	}

	INTP	getint(unsigned iid) {
		for(unsigned i=0; i<i_ilist.size(); i++) {
			if (i_ilist[i]->i_id == iid)
				return i_ilist[i];
		} return NULL;
	}
};
typedef	PICINFO	PICI, *PICP;
typedef	std::vector<PICP>	PICLIST;


PLIST	plist, slist, dlist, mlist;
PICLIST	piclist;
unsigned	unusedmsk;


unsigned	nextlg(unsigned vl) {
	unsigned r;

	for(r=0; (1u<<r)<vl; r+=1)
		;
	return r;
}

bool	isbusmaster(MAPDHASH &phash) {
	// printf("Checking if this hash is that of a peripheral\n");
	// mapdump(phash);
	// printf("Was this a peripheral?\n");

	return (phash.end() != phash.find(KYMTYPE));
}

bool	isbusmaster(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return isbusmaster(*pmap.u.m_m);
}

bool	isperipheral(MAPDHASH &phash) {
	// printf("Checking if this hash is that of a peripheral\n");
	// mapdump(phash);
	// printf("Was this a peripheral?\n");

	return (phash.end() != phash.find(KYPTYPE));
}

bool	isperipheral(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return isperipheral(*pmap.u.m_m);
}

bool	ispic(MAPDHASH &phash) {
	return (phash.end() != findkey(phash, KYPIC_MAX));
}

bool	ispic(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return ispic(*pmap.u.m_m);
}

bool	hasscope(MAPDHASH &phash) {
	return (phash.end() != phash.find(KYSCOPE));
}

bool	ismemory(MAPDHASH &phash) {
	MAPDHASH::iterator	kvpair;
	kvpair = phash.find(KYPTYPE);
	if (phash.end() == kvpair)
		return false;
	if (MAPT_STRING != kvpair->second.m_typ)
		return false;
	if (STRING(KYMEMORY)!= *kvpair->second.u.m_s)
		return false;
	return true;
}

int	get_address_width(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	STRING	bawstr = "BUS_ADDRESS_WIDTH";
	const int DEFAULT_BUS_ADDRESS_WIDTH = 30;

	kvpair = info.find(bawstr);
	if (kvpair == info.end()) {
		MAPT	elm;
		elm.m_typ = MAPT_INT;
		elm.u.m_v = DEFAULT_BUS_ADDRESS_WIDTH;
		info.insert(KEYVALUE(bawstr, elm));

		reeval(info);

		return DEFAULT_BUS_ADDRESS_WIDTH;
	} else {
		assert(kvpair->second.m_typ == MAPT_INT);
		return kvpair->second.u.m_v;
	}
}

int count_peripherals(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair, kvptype;
	kvpair = info.find(KYNP);
	if ((kvpair != info.end())&&((*kvpair).second.m_typ == MAPT_INT)) {
		return (*kvpair).second.u.m_v;
	}

	unsigned	count = 0, np_single=0, np_double=0, np_memory=0;
	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			count++;

			// Let see what type of peripheral this is
			kvptype = kvpair->second.u.m_m->find(KYPTYPE);
			if (kvptype == kvpair->second.u.m_m->end())
				continue;
			if (kvptype->second.m_typ != MAPT_STRING)
				continue;
			if (STRING("SINGLE")== *kvptype->second.u.m_s) {
				np_single++;
			} else if (STRING("DOUBLE")== *kvptype->second.u.m_s) {
				np_double++;
			} else if (STRING("MEMORY")== *kvptype->second.u.m_s)
				np_memory++;
		}
	}

	MAPT	elm;
	elm.m_typ = MAPT_INT;
	elm.u.m_v = count;
	info.insert(KEYVALUE(KYNP, elm));

	elm.u.m_v = np_single;
	info.insert(KEYVALUE(KYNPSINGLE, elm));
	elm.u.m_v = np_double;
	info.insert(KEYVALUE(KYNPDOUBLE, elm));
	elm.u.m_v = np_memory;
	info.insert(KEYVALUE(KYNPMEMORY, elm));

	return count;
}

int count_pics(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair, kvpic;
	int	npics=0;

	kvpair = info.find(KYNPIC);
	if ((kvpair != info.end())&&((*kvpair).second.m_typ == MAPT_INT)) {
		return (*kvpair).second.u.m_v;
	}

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvpic = kvpair->second.u.m_m->find(KYPIC);
		if (kvpic != kvpair->second.u.m_m->end())
			npics++;
	}

	MAPT	elm;
	elm.m_typ = MAPT_INT;
	elm.u.m_v = npics;
	info.insert(KEYVALUE(KYNPIC, elm));

	return npics;
}

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

void	legal_notice(MAPDHASH &info, FILE *fp, STRING &fname) {
	char	line[512];
	FILE	*lglfp;
	STRING	str;

	STRINGP	legal_fname = getstring(info, str = STRING("LEGAL"));
	if (!legal_fname) {
		fprintf(stderr, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license this under the GPL\n");
		fprintf(fp, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license this under the GPL\n");
		return;
	}

	lglfp = fopen(legal_fname->c_str(), "r");
	if (!lglfp) {
		fprintf(stderr, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license this project under the GPL\n");
		fprintf(fp, "WARNING: NO COPYRIGHT NOTICE\n\nPlease be considerate and license this project under the GPL\n");
		perror("O/S Err:");
		return;
	}

	while(fgets(line, sizeof(line), lglfp)) {
		static const char	nonce[] = "// Filename:";
		if (strncasecmp(line, nonce, strlen(nonce))==0)
			break;
		fputs(line, fp);
	} if (!feof(lglfp)) {
		// Must have just found the filename line
		while((line[0])&&(isspace(line[strlen(line)-1])))
			line[strlen(line)-1] = '\0';
		fprintf(fp, "%s\t%s\n", line, fname.c_str());
		while(fgets(line, sizeof(line), lglfp)) {
			static const char	nonce[] = "// CmdLine:";
			if (strncasecmp(line, nonce, strlen(nonce))==0)
				break;
			fputs(line, fp);
		} if (!feof(lglfp)) {
			// Must have just found the command line ... line
			STRINGP	cmdline= getstring(info, str=STRING("CMDLINE"));
			while((line[0])&&(isspace(line[strlen(line)-1])))
				line[strlen(line)-1] = '\0';
			if (cmdline)
				fprintf(fp, "%s\t%s\n", line, cmdline->c_str());
			else
				fprintf(fp, "%s\t(No command line found)\n", line);
			while(fgets(line, sizeof(line), lglfp))
				fputs(line, fp);
		}
		
	}
}

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

void	build_plist(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;

	int np = count_peripherals(info);

	if (np < 1) {
		printf("Only %d peripherals\n", np);
		return;
	}

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			MAPDHASH::iterator kvnaddr, kvname, kvptype;
			MAPDHASH	*phash = kvpair->second.u.m_m;

			kvnaddr = phash->find(KYNADDR);
			if (kvnaddr != phash->end()) {
				PERIPHP p = new PERIPH;
				p->p_base = 0;
				p->p_naddr = kvnaddr->second.u.m_v;
				p->p_awid  = nextlg(p->p_naddr)+2;
				p->p_phash = phash;
				p->p_name  = NULL;

				kvname = phash->find(KYPREFIX);
				assert(kvname != phash->end());
				assert(kvname->second.m_typ == MAPT_STRING);
				if (kvname != phash->end())
					p->p_name = kvname->second.u.m_s;

				kvptype = phash->find(KYPTYPE);
				if ((kvptype != phash->end())
					&&(kvptype->second.m_typ == MAPT_STRING)) {
					if (KYSINGLE== *kvptype->second.u.m_s) {
						slist.push_back(p);
					} else if (KYDOUBLE== *kvptype->second.u.m_s) {
						dlist.push_back(p);
					} else if (KYMEMORY== *kvptype->second.u.m_s) {
						mlist.push_back(p);
						plist.push_back(p);
					} else
						plist.push_back(p);
				} else plist.push_back(p);
			}
		}
	}

	// Sort by address usage
	printf("Sorting the SLIST\n");
	std::sort(slist.begin(), slist.end(), compare_naddr);
	printf("Sorting the DLIST\n");
	std::sort(dlist.begin(), dlist.end(), compare_naddr);
	printf("Sorting the MLIST\n");
	std::sort(mlist.begin(), mlist.end(), compare_naddr);
	printf("Sorting the PLIST\n");
	std::sort(plist.begin(), plist.end(), compare_naddr);
}

void assign_addresses(MAPDHASH &info, unsigned first_address = 0x400) {
	unsigned	start_address = first_address;
	MAPDHASH::iterator	kvpair;

	int np = count_peripherals(info);

	if (np < 1) {
		printf("Only %d peripherals\n", np);
		return;
	}

	printf("// Assigning addresses to the S-LIST\n");
	// Find the number of slist addresses
	if (slist.size() > 0) {
		// Each has only the one address ...
		int naddr = slist.size();
		for(unsigned i=0; i<slist.size(); i++) {
			slist[i]->p_base = start_address + 4*i;
			printf("// Assigning %12s_... to %08x\n", slist[i]->p_name->c_str(), slist[i]->p_base);

			setvalue(*slist[i]->p_phash, KYBASE, slist[i]->p_base);
		}
		naddr = nextlg(naddr)+2;
		start_address += (1<<naddr);

		for(unsigned i=0; i<slist.size(); i++) {
			slist[i]->p_mask = (-1<<naddr);
			setvalue(*slist[i]->p_phash, KYMASK, slist[i]->p_mask);
		}
	}

	// Assign double-peripheral bus addresses
	{
		printf("// Assigning addresses to the D-LIST, starting from %08x\n", start_address);
		unsigned start = 0;
		for(unsigned i=0; i<dlist.size(); i++) {
			dlist[i]->p_base = 0;
			dlist[i]->p_base = (start + ((1<<dlist[i]->p_awid)-1));
			dlist[i]->p_base &= (-1<<(dlist[i]->p_awid));
			// First valid next address is ...
			start = dlist[i]->p_base + (1<<(dlist[i]->p_awid));
		}

		int dnaddr = nextlg(start);
		start_address = (start_address + (1<<dnaddr)-1)
				& (-1<<dnaddr);
printf("// start address for d = %08x\n", start_address);

		for(unsigned i=0; i<dlist.size(); i++) {
			dlist[i]->p_base += start_address;
			printf("// Assigning %12s_... to %08x\n",
				dlist[i]->p_name->c_str(), dlist[i]->p_base);

			setvalue(*dlist[i]->p_phash, KYBASE, dlist[i]->p_base);
			dlist[i]->p_mask = (-1<<dnaddr);
			setvalue(*dlist[i]->p_phash, KYMASK, dlist[i]->p_mask);
		}

		start_address += (1<<dnaddr);
	}

	// Assign bus addresses
	printf("// Assigning addresses to the P-LIST (all other addresses)\n");
	for(unsigned i=0; i<plist.size(); i++) {
		if (plist[i]->p_naddr < 1)
			continue;
		// Make this address 32-bit aligned
		plist[i]->p_base = (start_address + ((1<<plist[i]->p_awid)-1));
		plist[i]->p_base &= (-1<<(plist[i]->p_awid));
		printf("// assigning %12s_... to %08x\n",
			plist[i]->p_name->c_str(),
			plist[i]->p_base);
		start_address = plist[i]->p_base + (1<<(plist[i]->p_awid));
		plist[i]->p_mask = (-1<<(plist[i]->p_awid));

		setvalue(*plist[i]->p_phash, KYBASE, plist[i]->p_base);
		setvalue(*plist[i]->p_phash, KYMASK, plist[i]->p_mask);
	}

	// Adjust the mask to find only those bits that are needed
	unsigned	masteraddr = 0;
	for(int bit=32; bit >= 0; bit--) {
		unsigned	bmsk = (1<<bit), bvl;
		bool	bset = false, onebit=true;

		for(unsigned i=0; i<slist.size(); i++) {
			if (bmsk & slist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & slist[i]->p_base;
					bset = true;
					onebit = true;
				} else if ((slist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
				}
			}
		} if (!onebit)
			continue;

		for(unsigned i=0; i<dlist.size(); i++) {
			if (bmsk & dlist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & dlist[i]->p_base;
					bset = true;
					onebit = true;
				} else if ((dlist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
				}
			}
		} if (!onebit)
			continue;

		for(unsigned i=0; i<plist.size(); i++) {
			if (bmsk & plist[i]->p_mask) {
				if (!bset) {
					bvl = bmsk & plist[i]->p_base;
					bset = true;
					onebit = true;
				} else if ((plist[i]->p_base & bmsk)!=bvl) {
					onebit = false;
				}
			}
		} if (!onebit)
			continue;

		if ((!bset)||(onebit)) {
			unusedmsk &= (~bmsk);
			masteraddr|= bvl;
		}
	}

	if ((1u<<nextlg(first_address))==first_address) {
		printf("// Adding in null-address\n");
		unusedmsk |= (first_address);
	}
	printf("// Overall  Mask: %08x\n", unusedmsk);
	printf("// SkipAddr Mask: %08x\n", ~unusedmsk);

	// Now, go back and see if anything references this address or these
	// masks
	reeval(info);
}


void	assign_interrupts(MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvint, kvline;
	MAPDHASH	*submap, *intmap;

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
		{
			if (kvint->second.m_typ != MAPT_MAP)
				continue;


			// NAME is now @comp.INT.

			// Yes, an INT hash exists within this component.
			// Hence the component has one (or more) interrupts.
			// Let's loop over those interrupts
			intmap = kvint->second.u.m_m;
			for(kvline=intmap->begin(); kvline != intmap->end();
						kvline++) {
				if (kvline->second.m_typ != MAPT_MAP)
					continue;

			// NAME is now @comp.INT.<name>

printf("Examining: @%s.%s.%s\n",
	kvpair->first.c_str(),
	kvint->first.c_str(),
	kvline->first.c_str());
				STRINGP	picname;
				int inum;
				trimall(*kvline->second.u.m_m, KYPIC);
				trimall(*kvline->second.u.m_m, KY_WIRE);
				// Now, we need to map this to a PIC
				if (NULL==(picname=getstring(
					*kvline->second.u.m_m, KYPIC))) {
						fprintf(stderr,
						"WARNING: No bus defined for INT_%s\nThis interrupt will not be connected.\n",
						kvpair->first.c_str());
					continue;
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
						printf("ERR: PIC NOT FOUND: %s\n", tok);
					} else if (getvalue(*kvline->second.u.m_m,
							KY_ID,inum)) {
						piclist[pid]->add((unsigned)inum,
							*kvline->second.u.m_m,
							new STRING(kvline->first));
					} else {
						piclist[pid]->add(*kvline->second.u.m_m,
							new STRING(kvline->first));
					}

					tok = strtok(NULL, ", \t\n");
				} free(cpy);
			}
		}
	}

	// Now, let's assign everything that doesn't yet have any definitions
	for(unsigned picid=0; picid<piclist.size(); picid++)
		piclist[picid]->assignids();
	reeval(master);
	mapdump(master);
}

void	assign_scopes(    MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

void	build_regdefs_h(  MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair, kvaccess;
	int	nregs;
	STRING	str;

	legal_notice(master, fp, fname);

	fprintf(fp, "#ifndef\tREGDEFS_H\n");
	fprintf(fp, "#define\tREGDEFS_H\n");
	fprintf(fp, "\n\n");

	unsigned	longest_defname = 0;
	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,KYREGS_N);
		if (kvp == plist[i]->p_phash->end()) {
			printf("REGS.N not found in %s\n", plist[i]->p_name->c_str());
			continue;
		} if (kvp->second.m_typ == MAPT_INT) {
			nregs = kvp->second.u.m_v;
		} else if (kvp->second.m_typ == MAPT_STRING) {
			nregs = strtoul(kvp->second.u.m_s->c_str(), NULL, 0);
		} else {
			printf("REGS.N is NOT AN INTEGER\n");
		}

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			if (kvp == plist[i]->p_phash->end()) {
				printf("%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				printf("%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname;

			// 1. Read the number (Not used)
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				printf("No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);
		}
	}

	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,KYREGS_NOTE);
		if ((kvp != plist[i]->p_phash->end())&&(kvp->second.m_typ == MAPT_STRING))
			fprintf(fp, "%s\n", kvp->second.u.m_s->c_str());
		kvp = findkey(*plist[i]->p_phash,KYREGS_N);
		if (kvp == plist[i]->p_phash->end()) {
			printf("REGS.N not found in %s\n", plist[i]->p_name->c_str());
			continue;
		} if (kvp->second.m_typ == MAPT_INT) {
			nregs = kvp->second.u.m_v;
		} else if (kvp->second.m_typ == MAPT_STRING) {
			nregs = strtoul(kvp->second.u.m_s->c_str(), NULL, 0);
		} else {
			printf("NOT AN INTEGER\n");
		}

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			if (kvp == plist[i]->p_phash->end()) {
				printf("%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				printf("%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			int roff = strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				printf("No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			fprintf(fp, "#define\t%-*s\t0x%08x", longest_defname,
				rname, (roff<<2)+plist[i]->p_base);

			fprintf(fp, "\t// wbregs names: "); 
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

	fprintf(fp, "// Definitions for the bus masters\n");
	str = STRING("REGS.INSERT.H");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (isperipheral(kvpair->second))
			continue;
		kvaccess = findkey(*kvpair->second.u.m_m, str);
		if (kvaccess == kvpair->second.u.m_m->end())
			continue;
		if (kvaccess->second.m_typ != MAPT_STRING)
			continue;
		fputs(kvaccess->second.u.m_s->c_str(), fp);
	}

	fprintf(fp, "// And then from the peripherals\n");
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(kvpair->second))
			continue;
		kvaccess = findkey(*kvpair->second.u.m_m, str);
		if (kvaccess == kvpair->second.u.m_m->end())
			continue;
		if (kvaccess->second.m_typ != MAPT_STRING)
			continue;
		fputs(kvaccess->second.u.m_s->c_str(), fp);
	} fprintf(fp, "// End of definitions from REGS.INSERT.H\n");

	fprintf(fp, "\n\n");


	fprintf(fp, ""
"typedef	struct {\n"
	"\tunsigned	m_addr;\n"
	"\tconst char	*m_name;\n"
"} REGNAME;\n"
"\n"
"extern	const	REGNAME	*bregs;\n"
"extern	const	int	NREGS;\n"
"// #define	NREGS	(sizeof(bregs)/sizeof(bregs[0]))\n"
"\n"
"extern	unsigned	addrdecode(const char *v);\n"
"extern	const	char *addrname(const unsigned v);\n"
"\n");


	fprintf(fp, "#endif\t// REGDEFS_H\n");
}

void	build_regdefs_cpp(MAPDHASH &master, FILE *fp, STRING &fname) {
	int	nregs;
	STRING	str;

	legal_notice(master, fp, fname);

	fprintf(fp, "#include <stdio.h>\n");
	fprintf(fp, "#include <stdlib.h>\n");
	fprintf(fp, "#include <strings.h>\n");
	fprintf(fp, "#include <ctype.h>\n");
	fprintf(fp, "#include \"regdefs.h\"\n\n");
	fprintf(fp, "const\tREGNAME\traw_bregs[] = {\n");

	// First, find out how long our longest definition name is.
	// This will help to allow us to line things up later.
	unsigned	longest_defname = 0;
	unsigned	longest_uname = 0;
	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,KYREGS_N);
		if (kvp == plist[i]->p_phash->end()) {
			printf("REGS.N not found in %s\n", plist[i]->p_name->c_str());
			continue;
		} if (kvp->second.m_typ == MAPT_INT) {
			nregs = kvp->second.u.m_v;
		} else if (kvp->second.m_typ == MAPT_STRING) {
			nregs = strtoul(kvp->second.u.m_s->c_str(), NULL, 0);
		} else {
			printf("NOT AN INTEGER\n");
		}

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			if (kvp == plist[i]->p_phash->end()) {
				printf("%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				printf("%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);

			while(NULL != (rv = strtok(NULL, " \t\n,"))) {
				if (strlen(rv) > longest_uname)
					longest_uname = strlen(rv);
			}
		}
	}

	int	first = 1;
	for(unsigned i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,KYREGS_N);
		if (kvp == plist[i]->p_phash->end()) {
			printf("REGS.N not found in %s\n", plist[i]->p_name->c_str());
			continue;
		} if (kvp->second.m_typ == MAPT_INT) {
			nregs = kvp->second.u.m_v;
		} else if (kvp->second.m_typ == MAPT_STRING) {
			nregs = strtoul(kvp->second.u.m_s->c_str(), NULL, 0);
		} else {
			printf("NOT AN INTEGER\n");
		}

		for(int j=0; j<nregs; j++) {
			char	nstr[32];
			sprintf(nstr, "%d", j);
			kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.")+nstr);
			if (kvp == plist[i]->p_phash->end()) {
				printf("%s not found\n", str.c_str());
				continue;
			}
			if (kvp->second.m_typ != MAPT_STRING) {
				printf("%s is not a string\n", str.c_str());
				continue;
			}

			STRING	scpy = *kvp->second.u.m_s;

			char	*nxtp, *rname, *rv;

			// 1. Read the number
			strtoul(scpy.c_str(), &nxtp, 0);
			if ((nxtp==NULL)||(nxtp == scpy.c_str())) {
				printf("No register name within string: %s\n", scpy.c_str());
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			// 3. Get the various user names
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

	fprintf(fp, "\n};\n\n");
	fprintf(fp, "#define\tRAW_NREGS\t(sizeof(raw_bregs)/sizeof(bregs[0]))\n\n");
	fprintf(fp, "const\tREGNAME\t*bregs = raw_bregs;\n");

	fputs(""
"unsigned	addrdecode(const char *v) {\n"
	"\tif (isalpha(v[0])) {\n"
		"\t\tfor(int i=0; i<NREGS; i++)\n"
			"\t\t\tif (strcasecmp(v, bregs[i].m_name)==0)\n"
				"\t\t\t\treturn bregs[i].m_addr;\n"
		"\t\tfprintf(stderr, \"Unknown register: %s\\n\", v);\n"
		"\t\texit(-2);\n"
	"\t} else\n"
		"\t\treturn strtoul(v, NULL, 0); \n"
"}\n"
"\n"
"const	char *addrname(const unsigned v) {\n"
	"\tfor(int i=0; i<NREGS; i++)\n"
		"\t\tif (bregs[i].m_addr == v)\n"
			"\t\t\treturn bregs[i].m_name;\n"
	"\treturn NULL;\n"
"}\n", fp);

}

void	build_board_h(    MAPDHASH &master, FILE *fp, STRING &fname) {
	MAPDHASH::iterator	kvpair;
	STRING	str, astr;

	legal_notice(master, fp, fname);
	fprintf(fp, "#ifndef	BOARD_H\n#define\tBOARD_H\n\n\n");

	str = "BDEF.DEFN";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defns;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(*kvpair->second.u.m_m, str);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	str = "REGS.INSERT.H";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	defns;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		defns = getstring(*kvpair->second.u.m_m, str);
		if (defns)
			fprintf(fp, "%s\n\n", defns->c_str());
	}

	str = "BDEF.OSDEF";
	astr = "BDEF.OSVAL";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		STRINGP	osdef, osval;
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		osdef = getstring(*kvpair->second.u.m_m, str);
		if (osdef)
			fprintf(fp, "#define\t%s\n", osdef->c_str());
		osval = getstring(*kvpair->second.u.m_m, astr);
		if (osval)
			fputs(osval->c_str(), fp);
		if ((osdef)||(osval)) {
			fprintf(fp, "\n\n");
		}
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
}

void	build_board_ld(   MAPDHASH &master) {
	// legal_notice(master, fp, fname);
#ifdef	NEW_FILE_FORMAT
#endif
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
	str = "TOP.PORTLIST";
	astr = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);

		// If there's no TOP.PORTLIST, check for a MAIN.PORTLIST
		if ((kvsearch == kvpair->second.u.m_m->end())
				||(kvsearch->second.m_typ != MAPT_STRING))
			kvsearch = findkey(*kvpair->second.u.m_m, astr);
		if ((kvsearch == kvpair->second.u.m_m->end())
				||(kvsearch->second.m_typ != MAPT_STRING))
			continue;
		
		if (!first)
			fprintf(fp, ",\n");
		first=0;
		STRING	tmps(*kvsearch->second.u.m_s);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
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
	fprintf(fp, "\tinput\t\t\ti_clk;\n");
	str = "TOP.IODECL";
	astr = "MAIN.IODECL";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);

		STRINGP strp = getstring(master, str);
		if (!strp)
			strp = getstring(master, astr);
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
	str = "TOP.DEFNS";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP	strp = getstring(master, str);
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
	fprintf(fp, "\n\tmain(s_clk, s_reset,\n");
	str = "TOP.MAIN";
	astr = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);

		STRINGP strp = getstring(master, str);
		if (!strp)
			strp = getstring(master, astr);
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
	str = "TOP.INSERT";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		STRINGP strp = getstring(master, str);
		if (!strp)
			continue;
		fprintf(fp, "%s\n", strp->c_str());
	}

	fprintf(fp, "\n\nendmodule; // end of toplevel.v module definition\n");

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
	fprintf(fp, "\tinput\t\t\ti_clk, i_reset;\n");
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
	fprintf(fp, "\twire\t\twb_cyc, wb_stb, wb_we, wb_stall, wb_ack, wb_err;\n");
	fprintf(fp, "\twire\t[(%d-1):0]\twb_addr;\n", baw);
	fprintf(fp, "\twire\t[31:0]\twb_data;\n");
	fprintf(fp, "\treg\t[31:0]\twb_idata;\n");
	fprintf(fp, "\twire\t[3:0]\twb_sel;\n");
	if ((slist.size()>0)&&(dlist.size()>0)) {
		fprintf(fp, "\twire\tsio_sel, dio_sel;\n");
		fprintf(fp, "\treg\tsio_ack, dio_ack;\n");
		fprintf(fp, "\treg\t[1:0]\tpre_dio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tsio_data, dio_data;\n");
	} else if (slist.size()>0) {
		fprintf(fp, "\twire\tsio_sel;\n");
		fprintf(fp, "\treg\tsio_ack;\n");
		fprintf(fp, "\treg\t[31:0]\tsio_data;\n");
	} else if (dlist.size()>0) {
		fprintf(fp, "\twire\tdio_sel;\n");
		fprintf(fp, "\treg\tdio_ack;\n");
		fprintf(fp, "\treg\t[1:0]\tpre_dio_ack;\n");
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
	// Coallate all of the single selects into one wire
	if (slist.size() > 0) {
		unsigned	snaddr = 0, sbase = slist[0]->p_base;
		int		lowbit;

		sellist = "sio_sel";
		snaddr = slist[slist.size()-1]->p_base + 3 - sbase;
		snaddr = (1<<nextlg(snaddr));
		snaddr >>= 2;
		unsigned snbits = nextlg(snaddr);
		nsel ++;
		lowbit = snbits;
		{
			char	sbuf[128];
			sprintf(sbuf,
			"\tassign\t%6s_sel = (wb_addr[%2d:%2d] == %2d'b",
				"sio", baw-1, snbits, baw-snbits);
			for(int j=0; j<(int)baw-lowbit; j++) {
				int bit = (baw-1)-j;
				sprintf(sbuf, "%s%d", sbuf,
					((sbase>>(bit+2))&1)?1:0);
				if (((bit+2)&3)==0)
					sprintf(sbuf, "%s_", sbuf);
			} sprintf(sbuf, "%s);\n", sbuf);
			siosel_str = STRING(sbuf);
		}
		

		for(unsigned i=0; i<slist.size(); i++) {
			const char	*pfx = slist[i]->p_name->c_str();

			fprintf(fp, "\tassign\t%12s_sel = ", pfx);
			int	lowbit = slist[i]->p_awid-2;
			fprintf(fp, "(sio_sel)&&(wb_addr[%d:0] == %2d\'b",
				snbits-1, snbits-lowbit);
			for(int j=0; j<(int)snbits-lowbit; j++) {
				int bit = snbits-1-j;
				fprintf(fp, ((slist[i]->p_base>>(bit+2))&1)?"1":"0");
				if (((bit+2)&3)==0)
					fprintf(fp, "_");
			}
			fprintf(fp, ");\n");
		}
	} else {
		fprintf(fp, "\tassign\tsio_sel = 1\'b0;\n");
	}

	if (dlist.size() > 0) {
		unsigned	dnaddr = 0, dbase = dlist[0]->p_base;
		int		lowbit;

		if (sellist.size() > 0)
			sellist = "dio_sel, " + sellist;
		else
			sellist = "dio_sel";

		dnaddr = dlist[dlist.size()-1]->p_base
				+ 4*dlist[dlist.size()-1]->p_naddr + 3 - dbase;
		dnaddr = (1<<nextlg(dnaddr));
		dnaddr >>= 2;
		int dnbits = nextlg(dnaddr);
		nsel ++;
		lowbit = dnbits;
		{
			char	sbuf[128];
			sprintf(sbuf,
			"\tassign\t%6s_sel = (wb_addr[%2d:%2d] == %2d'b",
				"dio", baw-1, dnbits, baw-dnbits);
			for(int j=0; j<(int)baw-lowbit; j++) {
				int bit = (baw-1)-j;
				sprintf(sbuf, "%s%d", sbuf,
					((dbase>>(bit+2))&1)?1:0);
				if (((bit+2)&3)==0)
					sprintf(sbuf, "%s_", sbuf);
			} sprintf(sbuf, "%s);\n", sbuf);
			diosel_str = STRING(sbuf);
		}
		
		for(unsigned i=0; i<dlist.size(); i++) {
			const char	*pfx = dlist[i]->p_name->c_str();

			fprintf(fp, "\tassign\t%6s_sel = ", pfx);
			lowbit = dlist[i]->p_awid-2;
			fprintf(fp, "(dio_sel)&&(wb_addr[%d:%d] == %2d\'b",
				dnbits-1, lowbit, dnbits-lowbit);
			for(int j=0; j<dnbits-lowbit; j++) {
				int bit = (dnbits-1)-j;
				fprintf(fp, ((dlist[i]->p_base>>(bit+2))&1)?"1":"0");
				if (((bit+2)&3)==0)
					fprintf(fp, "_");
			} fprintf(fp, ");\n");
		}
	}

	if (siosel_str.size()>0)
		fputs(siosel_str.c_str(), fp);
	if (diosel_str.size()>0)
		fputs(diosel_str.c_str(), fp);
	for(unsigned i=0; i<plist.size(); i++) {
		const char	*pfx = plist[i]->p_name->c_str();

		fprintf(fp, "\tassign\t%6s_sel = ", pfx);
		fprintf(fp, "(wb_addr[%2d:%2d] == %2d\'b", baw-1, plist[i]->p_awid-2,
			baw-(plist[i]->p_awid-2));
		int	lowbit = plist[i]->p_awid-2;
		for(int j=0; j<(int)baw-lowbit; j++) {
			int bit = (baw-1)-j;
			fprintf(fp, ((plist[i]->p_base>>(bit+2))&1)?"1":"0");
			if (((bit+2)&3)==0)
				fprintf(fp, "_");
		} fprintf(fp, ");\n");
	}

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
	if (slist.size()>0) {
		acklist = "sio_ack"; nacks++;
		if (dlist.size() > 0) {
			acklist = "dio_ack, " + acklist; nacks++;
		}
	} else if (dlist.size() > 0) {
		acklist = "dio_ack"; nacks++;
	}

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
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tpre_dio_ack[1:0] <= { pre_dio_ack[0], (wb_stb)&&(dio_sel) };\n");
		fprintf(fp, "\talways @(posedge i_clk)\n\t\tdio_ack <= pre_dio_ack[1];\n");
	}
	if (acklist.size() > 0) {
		fprintf(fp, "\tassign\twb_ack = (wb_cyc)&&(|{%s});\n\n\n",
			acklist.c_str());
	} else
		fprintf(fp, "\tassign\twb_ack = 1\'b0;\n\n\n");


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
		MAPDHASH::iterator	kvalt, kvinsert, kvint, kvsub, kvwire;
		bool			nomain, noaccess, noalt;

		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvinsert = findkey(*kvpair->second.u.m_m, KYMAIN_INSERT);
		kvaccess = findkey(*kvpair->second.u.m_m, KYACCESS);
		kvalt    = findkey(*kvpair->second.u.m_m, KYMAIN_ALT);

		nomain   = false;
		noaccess = false;
		noalt    = false;
		if ((kvinsert == kvpair->second.u.m_m->end())
			||(kvinsert->second.m_typ != MAPT_STRING))
			nomain = true;
		if ((kvaccess == kvpair->second.u.m_m->end())
			||(kvaccess->second.m_typ != MAPT_STRING))
			noaccess= true;
		if ((kvalt    == kvpair->second.u.m_m->end())
			||(kvalt->second.m_typ != MAPT_STRING))
			noalt = true;

		if (noaccess) {
			if (!nomain)
				fputs(kvinsert->second.u.m_s->c_str(), fp);
		} else if ((!nomain)||(!noalt)) {
			if (nomain) {
				fprintf(fp, "`ifndef\t%s\n",
					kvaccess->second.u.m_s->c_str());
			} else {
				fprintf(fp, "`ifdef\t%s\n", kvaccess->second.u.m_s->c_str());
				fputs(kvinsert->second.u.m_s->c_str(), fp);
				fprintf(fp, "`else\t// %s\n", kvaccess->second.u.m_s->c_str());
			}

			if (!noalt) {
				fputs(kvalt->second.u.m_s->c_str(), fp);
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

			if (isperipheral(*kvpair->second.u.m_m)) {
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
			fprintf(fp, "`endif\t// %s\n\n",
				kvaccess->second.u.m_s->c_str());
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

	fprintf(fp, "`ifdef\tVERILATOR\n\n");
	fprintf(fp, "\talways @(*)\n"
		"\tbegin\n"
		"\t\tcasez({ %s })\n", acklist.c_str());
	for(unsigned i=0; i<nacks; i++) {
		fprintf(fp, "\t\t\t%d\'b", nacks);
		for(unsigned j=0; j<nacks; j++)
			fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
		if (i < plist.size())
			fprintf(fp, ": wb_idata = %s_data;\n",
				plist[i]->p_name->c_str());
		else if ((dlist.size() > 0)&&(i == plist.size()))
			fprintf(fp, ": wb_idata = dio_data;\n");
		else if (slist.size() > 0)
			fprintf(fp, ": wb_idata = sio_data;\n");
		else	fprintf(fp, ": wb_idata = 32\'h00;\n");
	}
	fprintf(fp, "\t\t\tdefault: wb_idata = 32\'h0;\n");
	fprintf(fp, "\t\tendcase\n\tend\n");
	fprintf(fp, "\n`else\t// VERILATOR\n\n");
	fprintf(fp, "\talways @(*)\n"
		"\tbegin\n"
		"\t\tcasez({ %s })\n", acklist.c_str());
	for(unsigned i=0; i<nacks; i++) {
		fprintf(fp, "\t\t\t%d\'b", nacks);
		for(unsigned j=0; j<nacks; j++)
			fprintf(fp, (i==j)?"1":(i>j)?"0":"?");
		if (i < plist.size())
			fprintf(fp, ": wb_idata <= %s_data;\n",
				plist[i]->p_name->c_str());
		else if ((dlist.size() > 0)&&(i == plist.size()))
			fprintf(fp, ": wb_idata <= dio_data;\n");
		else if (slist.size() > 0)
			fprintf(fp, ": wb_idata <= sio_data;\n");
		else	fprintf(fp, ": wb_idata <= 32\'h00;\n");
	}
	fprintf(fp, "\t\t\tdefault: wb_idata <= 32\'h0;\n");
	fprintf(fp, "\t\tendcase\n\tend\n");
	fprintf(fp, "`endif\t// VERILATOR\n");

	fprintf(fp, "\n\nendmodule; // main.v\n");

}

int	main(int argc, char **argv) {
	int		argn, nhash = 0;
	MAPDHASH	master;
	FILE		*fp;
	STRING		str;
	STRING		cmdline;

	if (argc > 0) {
		cmdline = STRING(argv[0]);
		for(argn=0; argn<argc; argn++) {
			cmdline = cmdline + " " + STRING(argv[argn]);
		}
		MAPT	elm;
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = new STRING(cmdline);
		master.insert(KEYVALUE(str = STRING("CMDLINE"), elm));
	}

	for(argn=1; argn<argc; argn++) {
		if (0 == access(argv[argn], R_OK)) {
			MAPDHASH	*fhash;

			fhash = parsefile(argv[argn]);
			if (fhash) {
				// mapdump(*fhash);
				mergemaps(master, *fhash);
				delete fhash;

				nhash++;
			}
		} else {
			printf("Could not open %s\n", argv[argn]);
		}
	}

	if (nhash == 0) {
		printf("No files given, no files written\n");
	}

	STRINGP	subd = getstring(master, str=STRING("SUBDIR"));
	if (subd == NULL)
		// subd = new STRING("autofpga-out");
		subd = new STRING("demo-out");
	if ((*subd) == STRING("/")) {
		fprintf(stderr, "SUBD = %s\n", subd->c_str());
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
			fprintf(stderr, "Could not create %s/ directory\n",
				subd->c_str());
			exit(EXIT_FAILURE);
		}
	}

	STRINGP	legal = getstring(master, str=STRING("LEGAL"));
	if (legal == NULL) {
		legal = getstring(master, str=STRING("COPYRIGHT"));
		if (legal == NULL)
			legal = new STRING("legalgen.txt");
		MAPT	elm;
		elm.m_typ = MAPT_STRING;
		elm.u.m_s = legal;
		master.insert(KEYVALUE(str=STRING("LEGAL"), elm));
	}
	

	trimall(master, KYPREFIX);
	trimall(master, KYACCESS);
	trimall(master, KYPTYPE);
	trimall(master, KYNADDR);
	trimall(master, KYFORMAT);
	trimall(master, KYBDEF_IOTYPE);
	trimall(master, KY_WIRE);
	cvtint(master, KYBUS_ADDRESS_WIDTH);
	cvtint(master, KYNADDR);
	cvtint(master, KYNPIC);
	cvtint(master, KYNSCOPES);
	cvtint(master, KYPIC_MAX);
	cvtint(master, KYREGS_N);
	cvtint(master, KY_ID);
	reeval(master);

	// mapdump(master);

	count_peripherals(master);
	build_plist(master);
	assign_interrupts(master);
	assign_scopes(    master);
	assign_addresses( master);

	str = subd->c_str(); str += "/regdefs.h";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_regdefs_h(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/regdefs.cpp";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_regdefs_cpp(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/board.h";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_board_h(  master, fp, str); fclose(fp); }

	build_board_ld(   master);
	build_latex_tbls( master);

	str = subd->c_str(); str += "/toplevel.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_toplevel_v(  master, fp, str); fclose(fp); }

	str = subd->c_str(); str += "/main.v";
	fp = fopen(str.c_str(), "w");
	if (fp) { build_main_v(  master, fp, str); fclose(fp); }
}
