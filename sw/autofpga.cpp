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

#include "parser.h"

unsigned	nextlg(unsigned vl) {
	unsigned r;

	for(r=1; (1u<<r)<vl; r+=1)
		;
	return r;
}

bool	isperipheral(MAPDHASH &phash) {
	// printf("Checking if this hash is that of a peripheral\n");
	// mapdump(phash);
	// printf("Was this a peripheral?\n");

	return (phash.end() != phash.find("PTYPE"));
}

bool	isperipheral(MAPT &pmap) {
	if (pmap.m_typ != MAPT_MAP)
		return false;
	return isperipheral(*pmap.u.m_m);
}

bool	hasscope(MAPDHASH &phash) {
	return (phash.end() != phash.find("SCOPE"));
}

bool	ismemory(MAPDHASH &phash) {
	return (phash.end() != phash.find("MEM"));
}

int count_peripherals(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	kvpair = info.find("NP");
	if ((kvpair != info.end())&&((*kvpair).second.m_typ == MAPT_INT)) {
		return (*kvpair).second.u.m_v;
	}

	int	count = 0;
	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			count++;
		}
	}

	MAPT	elm;
	elm.m_typ = MAPT_INT;
	elm.u.m_v = count;
	info.insert(KEYVALUE(STRING("NP"), elm));

	return count;
}

int count_scopes(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	kvpair = info.find("NSCOPES");
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

typedef	struct PERIPH_S {
	unsigned	p_base;
	unsigned	p_naddr;
	STRINGP		p_name;
	MAPDHASH	*p_phash;
} PERIPH, *PERIPHP;
typedef	std::vector<PERIPHP>	PLIST;

PLIST	plist;

bool	compare_naddr(PERIPHP a, PERIPHP b) {
	if (!a)
		return (b)?false:true;
	else if (!b)
		return true;
	else return (a->p_naddr <= b->p_naddr);
}

void	build_plist(MAPDHASH &info) {
	MAPDHASH::iterator	kvpair;
	STRING	straddr = STRING("NADDR");
	cvtint(info, straddr);

	int np = count_peripherals(info);

	if (np < 1) {
		printf("Only %d peripherals\n", np);
		return;
	}

	for(kvpair = info.begin(); kvpair != info.end(); kvpair++) {
		if (isperipheral(kvpair->second)) {
			MAPDHASH::iterator kvnaddr, kvname;
			MAPDHASH	*phash = kvpair->second.u.m_m;

			kvnaddr = phash->find(STRING("NADDR"));
			if (kvnaddr != phash->end()) {
				PERIPHP p = new PERIPH;
				p->p_base = 0;
				p->p_naddr = kvnaddr->second.u.m_v;
				p->p_phash = phash;
				p->p_name  = NULL;

				kvname = phash->find(STRING("PREFIX"));
				assert(kvname != phash->end());
				assert(kvname->second.m_typ == MAPT_STRING);
				if (kvname != phash->end())
					p->p_name = kvname->second.u.m_s;

				plist.push_back(p);
			}
		}
	}

	// Sort by address usage
	std::sort(plist.begin(), plist.end(), compare_naddr);
}

void assign_addresses(MAPDHASH &info, unsigned start_address = 0x400) {
	MAPDHASH::iterator	kvpair;
	STRING	straddr = STRING("NADDR");
	cvtint(info, straddr);

	int np = count_peripherals(info);

	if (np < 1) {
		printf("Only %d peripherals\n", np);
		return;
	}

	// Assign bus addresses
	for(int i=0; i<plist.size(); i++) {
		if (plist[i]->p_naddr < 1)
			continue;
		plist[i]->p_naddr = nextlg(plist[i]->p_naddr);
		// Make this address 32-bit aligned
		plist[i]->p_naddr += 2;
		plist[i]->p_base = (start_address + ((1<<plist[i]->p_naddr)-1));
		plist[i]->p_base &= (-1<<(plist[i]->p_naddr));
		printf("// assigning %8s_... to %08x\n",
			plist[i]->p_name->c_str(),
			plist[i]->p_base);
		start_address = plist[i]->p_base + (1<<(plist[i]->p_naddr));

		MAPT	elm;
		elm.m_typ = MAPT_INT;
		elm.u.m_v = plist[i]->p_base;
		plist[i]->p_phash->insert(KEYVALUE("BASE", elm));
	}
}

void	assign_interrupts(MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

void	assign_scopes(    MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

void	build_regdefs_h(  MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess;
	int	nregs;
	STRING	str;

	printf("\n\n\n// TO BE PLACED INTO regdefs.h\n");
	printf("#ifndef\tREGDEFS_H\n");
	printf("#define\tREGDEFS_H\n");
	printf("\n\n");

	int np = count_peripherals(master);
	int	longest_defname = 0;
	for(int i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.N"));
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
			STRINGP	rstr;
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
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);
		}
	}

	for(int i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.NOTE"));
		if ((kvp != plist[i]->p_phash->end())&&(kvp->second.m_typ == MAPT_STRING))
			printf("%s\n", kvp->second.u.m_s->c_str());
		kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.N"));
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
			STRINGP	rstr;
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
			printf("#define\t%-*s\t0x%08x", longest_defname,
				rname, (roff<<2)+plist[i]->p_base);

			printf("\t// wbregs names: "); 
			int	first = 1;
			// 3. Get the various user names
			while(rv = strtok(NULL, " \t\n,")) {
				if (!first)
					printf(", ");
				first = 0;
				printf("%s", rv);
			} printf("\n");
		}
	}
	printf("\n\n");

	printf("// Definitions for the bus masters\n");
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
		fputs(kvaccess->second.u.m_s->c_str(), stdout);
	}

	printf("// And then from the peripherals\n");
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
		fputs(kvaccess->second.u.m_s->c_str(), stdout);
	}printf("// End of definitions from REGS.INSERT.H\n");

	printf("\n\n");


	printf(""
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


	printf("#endif\t// REGDEFS_H\n");
}

void	build_regdefs_cpp(MAPDHASH &master) {
	int	nregs;
	STRING	str;

	printf("\n\n\n// TO BE PLACED INTO regdefs.cpp\n");
	printf("#include <stdio.h>\n");
	printf("#include <stdlib.h>\n");
	printf("#include <strings.h>\n");
	printf("#include <ctype.h>\n");
	printf("#include \"regdefs.h\"\n\n");
	printf("const\tREGNAME\traw_bregs[] = {\n");

	// First, find out how long our longest definition name is.
	// This will help to allow us to line things up later.
	int np = count_peripherals(master);
	int	longest_defname = 0;
	int	longest_uname = 0;
	for(int i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.N"));
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
			STRINGP	rstr;
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
				continue;
			}

			// 2. Get the C name
			rname = strtok(nxtp, " \t\n,");
			if (strlen(rname) > longest_defname)
				longest_defname = strlen(rname);

			while(rv = strtok(NULL, " \t\n,")) {
				if (strlen(rv) > longest_uname)
					longest_uname = strlen(rv);
			}
		}
	}

	int	first = 1;
	for(int i=0; i<plist.size(); i++) {
		MAPDHASH::iterator	kvp;

		nregs = 0;
		kvp = findkey(*plist[i]->p_phash,str=STRING("REGS.N"));
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
			STRINGP	rstr;
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
			// 3. Get the various user names
			while(rv = strtok(NULL, " \t\n,")) {
				if (!first)
					printf(",\n");
				first = 0;
				printf("\t{ %-*s,\t\"%s\"%*s\t}",
					longest_defname, rname, rv,
					(int)(longest_uname-strlen(rv)), "");
			}
		}
	}

	printf("\n};\n\n");
	printf("#define\tRAW_NREGS\t(sizeof(raw_bregs)/sizeof(bregs[0]))\n\n");
	printf("const\tREGNAME\t*bregs = raw_bregs;\n");

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
"}\n", stdout);

}

void	build_board_h(    MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

void	build_board_ld(   MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
#endif
}

void	build_latex_tbls( MAPDHASH &master) {
#ifdef	NEW_FILE_FORMAT
	printf("\n\n\n// TO BE PLACED INTO doc/src\n");

	for(int i=0; i<np; i++) {
		printf("\n\n\n// TO BE PLACED INTO doc/src/%s.tex\n",
			bus[i].b_data.);
	}
	printf("\n\n\n// TO BE PLACED INTO doc/src\n");
#endif
}

void	build_toplevel_v( MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str = "ACCESS", astr;
	int	first, np;

	printf("\n\n\n//\n// TO BE PLACED INTO toplevel.v\n//\n");
	printf("`default_nettype\tnone\n");
	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	printf("\n\n");
	
	// Define our external ports within a port list
	printf("//\n"
	"// Here we declare our toplevel.v (toplevel) design module.\n"
	"// All design logic must take place beneath this top level.\n"
	"//\n"
	"// The port declarations just copy data from the @TOP.PORTLIST\n"
	"// key, or equivalently from the @MAIN.PORTLIST key if\n"
	"// @TOP.PORTLIST is absent.  For those peripherals that don't need\n"
	"// any top level logic, the @MAIN.PORTLIST should be sufficent,\n"
	"// so the @TOP.PORTLIST key may be left undefined.\n"
	"//\n");
	printf("module\ttoplevel(i_clk,\n");
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
			printf(",\n");
		first=0;
		STRING	tmps(*kvsearch->second.u.m_s);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		printf("%s", tmps.c_str());
	} printf(");\n");

	// External declarations (input/output) for our various ports
	printf("\t//\n"
	"\t// Declaring our input and output ports.  We listed these above,\n"
	"\t// now we are declaring them here.\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.IODECLS key,\n"
	"\t// or from the @MAIN.IODECLS key if @TOP.IODECLS is absent.  For\n"
	"\t// those peripherals that don't do anything at the top level,\n"
	"\t// the @MAIN.IODECLS key should be sufficient, so the @TOP.IODECLS\n"
	"\t// key may be left undefined.\n"
	"\t//\n");
	printf("\tinput\t\t\ti_clk;\n");
	str = "TOP.IODECLS";
	astr = "MAIN.IODECLS";
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
		
		STRING	tmps(*kvsearch->second.u.m_s);
		printf("%s", kvsearch->second.u.m_s->c_str());
	}

	// Declare peripheral data
	printf("\n\n");
	printf("\t//\n"
	"\t// Declaring component data, internal wires and registers\n"
	"\t//\n"
	"\t// These declarations just copy data from the @TOP.DEFNS key\n"
	"\t// within the component data files.\n"
	"\t//\n");
	str = "TOP.DEFNS";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if (kvsearch == kvpair->second.u.m_m->end())
			continue;
		if (kvsearch->second.m_typ != MAPT_STRING)
			continue;
		printf("%s", kvsearch->second.u.m_s->c_str());
	}

	printf("\n\n");
	printf(""
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
	printf("\n\tmain(s_clk, s_reset,\n");
	str = "TOP.MAIN";
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
			printf(",\n");
		first=0;
		STRING	tmps(*kvsearch->second.u.m_s);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		printf("%s", tmps.c_str());
	} printf(");\n");

	printf("\n\n"
	"\t//\n"
	"\t// Our final section to the toplevel is used to provide all of\n"
	"\t// that special logic that couldnt fit in main.  This logic is\n"
	"\t// given by the @TOP.INSERT tag in our data files.\n"
	"\t//\n\n\n");
	str = "TOP.INSERT";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if (kvsearch == kvpair->second.u.m_m->end())
			continue;
		if (kvsearch->second.m_typ != MAPT_STRING)
			continue;
		printf("%s\n", kvsearch->second.u.m_s->c_str());
	}

	printf("\n\nendmodule; // end of toplevel.v module definition\n");

}

void	build_main_v(     MAPDHASH &master) {
	MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
	STRING	str = "ACCESS", astr;
	int	first, np;

	printf("\n\n\n//\n// TO BE PLACED INTO main.v\n//\n");
	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	printf("`default_nettype\tnone\n");
	printf(
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
	printf("// First, the access fields for any bus masters\n");
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
		printf("`define\t%s\n", kvaccess->second.u.m_s->c_str());
	}

	printf("// And then for the peripherals\n");
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
		printf("`define\t%s\n", kvaccess->second.u.m_s->c_str());
	}

	printf("//\n//\n");
	printf(
"// Finally, we define our main module itself.  We start with the list of\n"
"// I/O ports, or wires, passed into (or out of) the main function.\n"
"//\n"
"// These fields are copied verbatim from the respective I/O port lists,\n"
"// from the fields given by @MAIN.PORTLIST\n"
"//\n");
	
	// Define our external ports within a port list
	printf("module\tmain(i_clk, i_reset,\n");
	str = "MAIN.PORTLIST";
	first = 1;
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if (kvsearch == kvpair->second.u.m_m->end())
			continue;
		if (kvsearch->second.m_typ != MAPT_STRING)
			continue;
		if (!first)
			printf(",\n");
		first=0;
		STRING	tmps(*kvsearch->second.u.m_s);
		while(isspace(*tmps.rbegin()))
			*tmps.rbegin() = '\0';
		printf("%s", tmps.c_str());
	} printf(");\n");

	printf("//\n"
"////////////////////////////////////////\n"
"/// PARAMETER SUPPORT BELONGS HERE\n"
"/// (it hasn\'t been written yet\n"
"////////////////////////////////////////\n//\n");

	printf("//\n"
"// The next step is to declare all of the various ports that were just\n"
"// listed above.  \n"
"//\n"
"// The following declarations are taken from the values of the various\n"
"// @MAIN.IODECLS keys.\n"
"//\n");

// #warning "How do I know these will be in the same order?
//	They have been--because the master always reads its subfields in the
//	same order.
//
	// External declarations (input/output) for our various ports
	printf("\tinput\t\t\ti_clk, i_reset;\n");
	str = "MAIN.IODECLS";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if (kvsearch == kvpair->second.u.m_m->end())
			continue;
		if (kvsearch->second.m_typ != MAPT_STRING)
			continue;
		printf("%s", kvsearch->second.u.m_s->c_str());
	}

	// Declare Bus master data
	printf("\n\n");
	printf("\t//\n\t// Declaring wishbone master bus data\n\t//\n");
	printf("\twire\t\twb_cyc, wb_stb, wb_we, wb_stall, wb_ack, wb_err;\n");
	printf("\twire\t[31:0]\twb_data, wb_idata, wb_addr;\n");
	printf("\twire\t[3:0]\twb_sel;\n");
	printf("\n\n");

printf("\n"
"////////////////////////////////////////\n"
"/// BUS MASTER declarations belong here\n"
"////////////////////////////////////////\n");

	// Declare peripheral data
	printf("\n\n");
	printf("\t//\n\t// Declaring Peripheral data, internal wires and registers\n\t//\n"
	"\t// These declarations come from the various components values\n"
	"\t// given under the @MAIN.DEFNS key.\n\t//\n"
	"\t// wires are also declared for any interrupts defined by the\n"
	"\t// @INTERRUPT key\n\t//\n");
	str = "MAIN.DEFNS";
	astr= "INTERRUPT";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if ((kvsearch != kvpair->second.u.m_m->end())
				&&(kvsearch->second.m_typ == MAPT_STRING)) {
			printf("%s", kvsearch->second.u.m_s->c_str());
		}

		// Look for any interrupt lines that need to be declared
		kvsearch = findkey(*kvpair->second.u.m_m, astr);
		if ((kvsearch != kvpair->second.u.m_m->end())
				&&(kvsearch->second.m_typ == MAPT_STRING)) {
			char	*istr = strdup(kvsearch->second.u.m_s->c_str()),
				*tok;

			tok = strtok(istr, ", \t\n");
			while(NULL != tok) {
				printf("\twire\t%s;\n", tok);
				tok = strtok(NULL, ", \t\n");
			} free(istr);
		}
	}

	// Declare wishbone lines
	printf("\n"
	"\t// Declare those signals necessary to build the bus, and detect\n"
	"\t// bus errors upon it.\n"
	"\t//\n"
	"\twire\tnone_sel;\n"
	"\treg\tmany_sel, many_ack;\n"
	"\treg\t[31:0]\tr_bus_err;\n");
	printf("\n"
	"\t//\n"
	"\t// Wishbone slave wire declarations\n"
	"\t//\n"
	"\t// These are given for every configuration file with a @PTYPE\n"
	"\t// tag, and the names are given by the @PREFIX tag.\n"
	"\t//\n");
	printf("\n");
	str = "PREFIX";
	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		if (!isperipheral(*kvpair->second.u.m_m))
			continue;

		kvsearch = findkey(*kvpair->second.u.m_m, str);
		if (kvsearch == kvpair->second.u.m_m->end())
			continue;
		if (kvsearch->second.m_typ != MAPT_STRING)
			continue;

		const char	*pfx = kvsearch->second.u.m_s->c_str();

		printf("\twire\t%s_ack, %s_stall, %s_sel;\n", pfx, pfx, pfx);
		printf("\twire\t[31:0]\t%s_data;\n", pfx);
		printf("\n");
	}

	// Define the select lines
	printf("\n"
	"\t// Wishbone peripheral address decoding\n"
	"\t// This particular address decoder decodes addresses for all\n"
	"\t// peripherals (components with a @PTYPE tag), based upon their\n"
	"\t// NADDR (number of addresses required) tag\n"
	"\t//\n");
	printf("\n");
	np = count_peripherals(master);
	for(int i=0; i<np; i++) {
		const char	*pfx = plist[i]->p_name->c_str();

		printf("\tassign\t%6s_sel = ", pfx);
		printf("(wb_addr[29:%2d] == %d\'b", plist[i]->p_naddr-2,
			32-(plist[i]->p_naddr-2));
		int	lowbit = plist[i]->p_naddr-2;
		for(int j=0; j<30-lowbit; j++) {
			int bit = 29-j;
			printf(((plist[i]->p_base>>bit)&1)?"1":"0");
			if ((bit&3)==0)
				printf("_");
		}
		printf(");\n");
	}

	// Define none_sel
	printf("\tassign\tnone_sel = (wb_stb)&&({ ");
	for(int i=0; i<np-1; i++)
		printf("%s_sel, ", plist[i]->p_name->c_str());
	printf("%s_sel } == 0);\n", plist[np-1]->p_name->c_str());

	// Define many_sel
	printf("\talways @(*)\n\tbegin\n\t\tmany_sel <= (wb_stb);\n");
	printf("\t\tcase({");
	for(int i=0; i<np-1; i++)
		printf("%s_sel, ", plist[i]->p_name->c_str());
	printf("%s_sel })\n", plist[np-1]->p_name->c_str());
	printf("\t\t\t%d\'h0: many_sel <= 1\'b0;\n", np);
	for(int i=0; i<np; i++) {
		printf("\t\t\t%d\'b", np);
		for(int j=0; j<np; j++)
			printf((i==j)?"1":"0");
		printf(": many_sel <= 1\'b0;\n");
	} printf("\t\tendcase\n\tend\n");

	// Define many ack
	printf("\talways @(posedge i_clk)\n\tbegin\n\t\tmany_ack <= (wb_cyc);\n");
	printf("\t\tcase({");
	for(int i=0; i<np-1; i++)
		printf("%s_ack, ", plist[i]->p_name->c_str());
	printf("%s_ack })\n", plist[np-1]->p_name->c_str());
	printf("\t\t\t%d\'h0: many_ack <= 1\'b0;\n", np);
	for(int i=0; i<np; i++) {
		printf("\t\t\t%d\'b", np);
		for(int j=0; j<np; j++)
			printf((i==j)?"1":"0");
		printf(": many_ack <= 1\'b0;\n");
	} printf("\t\tendcase\n\tend\n");

	// Define the bus error
	printf("\talways @(posedge i_clk)\n\t\twb_err <= (none_sel)||(many_sel)||(many_ack);\n\n");
	printf("\talways @(posedge i_clk)\n\t\tif (wb_err)\n\t\t\tr_bus_err <= wb_addr;\n\n");

	printf("\t//Now we turn to defining all of the parts and pieces of what\n"
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
	str = "MAIN.INSERT";
	{
		STRING	straccess = "ACCESS";
		STRING	stralt = "MAIN.ALT";
		STRING	strint = "INTERRUPT";

	for(kvpair=master.begin(); kvpair != master.end(); kvpair++) {
	// MAPDHASH::iterator	kvpair, kvaccess, kvsearch;
		MAPDHASH::iterator	kvalt, kvinsert, kvint;
		bool			nomain, noaccess, noalt;

		if (kvpair->second.m_typ != MAPT_MAP)
			continue;
		kvinsert = findkey(*kvpair->second.u.m_m, str);
		kvaccess = findkey(*kvpair->second.u.m_m, straccess);
		kvalt    = findkey(*kvpair->second.u.m_m, stralt);

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
				fputs(kvinsert->second.u.m_s->c_str(), stdout);
		} else if ((!nomain)||(!noalt)) {
			if (nomain) {
				printf("`ifndef\t%s\n",
					kvaccess->second.u.m_s->c_str());
			} else {
				printf("`ifdef\t%s\n", kvaccess->second.u.m_s->c_str());
				fputs(kvinsert->second.u.m_s->c_str(), stdout);
				if (!noalt)
					printf("`else\t// %s\n", kvaccess->second.u.m_s->c_str());
			}

			if (!noalt) {
				fputs(kvalt->second.u.m_s->c_str(), stdout);
			}
			if (isperipheral(*kvpair->second.u.m_m)) {
				MAPDHASH::iterator	kvname;
				STRING		namestr = "PREFIX";
				kvname = findkey(*kvpair->second.u.m_m, namestr);
				if ((kvname == kvpair->second.u.m_m->end())
					||(kvname->second.m_typ != MAPT_STRING)) {
				} else {
					STRINGP	nm = kvname->second.u.m_s;
					printf("\n");
					printf("\treg\tr_%s_ack;\n", nm->c_str());
					printf("\talways @(posedge i_clk)\n\t\tr_%s_ack <= (wb_stb)&&(%s_sel);\n",
						nm->c_str(),
						nm->c_str());
					printf("\n");
					printf("\tassign\t%s_ack   = r_%s_ack;\n",
						nm->c_str(),
						nm->c_str());
					printf("\tassign\t%s_stall = 1\'b0;\n",
						nm->c_str());
					printf("\tassign\t%s_data  = 32\'h0;\n",
						nm->c_str());
					printf("\n");
				}
			}

			kvint    = findkey(*kvpair->second.u.m_m, strint);
			if ((kvint != kvpair->second.u.m_m->end())
				&&(kvint->second.m_typ == MAPT_STRING)) {
				char	*istr = strdup(kvint->second.u.m_s->c_str()),
					*tok;

				tok = strtok(istr, ", \t\n");
				while(NULL != tok) {
					printf("\tassign\t%s = 1\'b0;\n", tok);
					tok = strtok(NULL, ", \t\n");
				} free(istr);
			}
			printf("`endif\t// %s\n\n",
				kvaccess->second.u.m_s->c_str());
		}
	}}

	printf("\t//\n\t// Finally, determine what the response is from the\n"
	"\t// wishbone bus\n"
	"\t//\n"
	"\t// Any peripheral component with a @PTYPE value will be listed\n"
	"\t// here.\n"
	"\t//\n");
	printf("\talways @(*)\n"
		"\t\tcasez({");
	for(int i=0; i<np-1; i++)
		printf("%s_ack, ", plist[i]->p_name->c_str());
	printf("%s_ack })\n", plist[np-1]->p_name->c_str());
	for(int i=0; i<np; i++) {
		printf("\t\t\t%d\'b", np);
		for(int j=0; j<np; j++)
			printf((i==j)?"1":(i>j)?"0":"?");
		printf(": wb_data <= %s_data;\n",
			plist[i]->p_name->c_str());
	} printf("\t\tendcase\n\tend\n");
	printf("\n\nendmodule; // main.v\n");

}

int	main(int argc, char **argv) {
	int		argn, nhash = 0;
	MAPDHASH	master;

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

	STRING	str;
	trimall(master, str = STRING("PREFIX"));
	trimall(master, str = STRING("IONAME"));
	trimall(master, str = STRING("ACCESS"));
	trimall(master, str = STRING("INTERRUPTS"));
	trimall(master, str = STRING("PTYPE"));
	trimall(master, str = STRING("NADDR"));
	count_peripherals(master);
	build_plist(master);
	assign_interrupts(master);
	assign_scopes(    master);
	assign_addresses( master);

	build_regdefs_h(  master);
	build_regdefs_cpp(master);
	build_board_h(    master);
	build_board_ld(   master);
	build_latex_tbls( master);
	build_toplevel_v( master);
	build_main_v(     master);
}
