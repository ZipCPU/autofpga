////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	autogen.cpp
//
// Project:	VideoZip, a ZipCPU SoC supporting video functionality
//
// Purpose:	
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015-2017, Gisselquist Technology, LLC
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
#include <string.h>

#include "autopdata.h"

extern	AUTOPDATA	gpsclock_data, console_data, flash_data, flctl_data,
			mem_data, mdio_data, netb_data, netp_data, gpstb_data,
			gpsu_data, mouse_data, icape_data, sdspi_data;

unsigned	nextlg(unsigned vl) {
	unsigned r;

	for(r=1; (1u<<r)<vl; r+=1)
		;
	return r;
}

// 
typedef	struct	BUSENTRY_S {
	int		b_base;
	int		b_naddr;
	AUTOPDATA	b_data;
} BUSENTRY;

int main(int argc, char **argv) {
	int		np = 13, nm = 2;
	BUSENTRY	*bus;
	int		start_address = 0x400;

	master = new BUSENTRY[nm];
	master[0] = wbubus_data;
	master[1] = zipcpu_data;

	bus = new BUSENTRY[np];
	bus[0].b_data = console_data;
	bus[1].b_data = gpsu_data;
	bus[2].b_data = mouse_data;
	bus[3].b_data = flctl_data;
	bus[4].b_data = sdspi_data;
	bus[5].b_data = gpsclock_data;
	bus[6].b_data = gpstb_data;
	bus[7].b_data = netp_data;
	bus[8].b_data = mdio_data;
	bus[9].b_data = icape_data;
	bus[10].b_data = netb_data;
	bus[11].b_data = mem_data;
	bus[12].b_data = flash_data;

	// Assign bus addresses
	for(int i=0; i<np; i++) {
		bus[i].b_naddr = nextlg(bus[i].b_data.naddr);
		if (bus[i].b_naddr < 1)
			bus[i].b_naddr = 1;
		bus[i].b_naddr += 2;
		bus[i].b_base = (start_address + ((1<<bus[i].b_naddr)-1));
		bus[i].b_base &= (-1<<(bus[i].b_naddr));
		printf("// assigning %8s_... to %08x\n", bus[i].b_data.prefix, bus[i].b_base);
		start_address = bus[i].b_base + (1<<(bus[i].b_naddr));
	}


	// Include a legal notice
	// Build a set of ifdefs to turn things on or off
	printf("\n\n");
	printf("// Here is a list of defines which may be used, post auto-design\n");
	printf("// (not post-build), to turn particular peripherals (and bus masters)\n");
	printf("// on and off.\n");
	printf("//\n");
	printf("// First for the bus masters\n");
	for(int i=0; i<nm; i++) {
		if (!master[i].access)
			continue;
		if (!master[i].access[0])
			continue;
		printf("`define\t%s\n", master[i].access);
	} printf("// And then for the peripherals\n");
	for(int i=0; i<np; i++) {
		if (!bus[i].b_data.access)
			continue;
		if (!bus[i].b_data.access[0])
			continue;
		printf("`define\t%s\n", bus[i].b_data.access);
	} printf("\n\n");
	
	// Define our external ports within a port list
	printf("module\tmain(i_clk,\n");
	{
		int first = 1;
		for(int i=0; i<np; i++) {
			if (!bus[i].b_data.ext_ports)
				continue;
			if (!bus[i].b_data.ext_ports[0])
				continue;
			if (!first) {
				printf(",\n");
			} else first = 0;

			printf("%s", bus[i].b_data.ext_ports);
		} printf(");\n");
	}

	// External declarations (input/output) for our various ports
	printf("\tinput\t\t\ti_clk;\n");
	for(int i=0; i<np; i++) {
		if (!bus[i].b_data.ext_decls)
			continue;
		if (!bus[i].b_data.ext_decls[0])
			continue;
		printf("%s\n", bus[i].b_data.ext_decls);
	}

	// Declare Bus master data
	printf("\n\n");
	printf("\t//\n\t// Declaring wishbone master bus data\n\t//\n");
	printf("\twire\t\twb_cyc, wb_stb, wb_we, wb_stall, wb_ack, wb_err;\n");
	printf("\twire\t[31:0]\twb_data, wb_idata, wb_addr;\n");
	printf("\twire\t[3:0]\twb_sel;\n");
	printf("\n\n");

	// Then, for each possible bus master
	for(int i=0; i<nm; i++) {
		
	}

	// Declare peripheral data
	printf("\n\n");
	printf("\t//\n\t// Declaring Peripheral data, internal wires and registers\n\t//\n");
	for(int i=0; i<np; i++) {
		if (!bus[i].b_data.main_defns)
			continue;
		if (!bus[i].b_data.main_defns[0])
			continue;
		printf("%s\n", bus[i].b_data.main_defns);
	}

	// Declare wishbone lines
	printf("\n\t//\n\t// Wishbone slave wire declarations\n\t//\n");
	printf("\n");
	for(int i=0; i<np; i++) {
		printf("\twire\t%s_ack, %s_stall, %s_sel;\n",
			bus[i].b_data.prefix,
			bus[i].b_data.prefix,
			bus[i].b_data.prefix);
		printf("\twire\t[31:0]\t%s_data;\n",
			bus[i].b_data.prefix);
		printf("\n");
	}

	// Define the select lines
	printf("\n\t// Wishbone peripheral address decoding\n");
	printf("\n");
	for(int i=0; i<np; i++) {
		printf("\tassign\t%6s_sel = ", bus[i].b_data.prefix);
		printf("(wb_addr[29:%2d] == %d\'b", bus[i].b_naddr-2,
			32-(bus[i].b_naddr-2));
		int	lowbit = bus[i].b_naddr-2;
		for(int j=0; j<30-lowbit; j++) {
			int bit = 29-j;
			printf(((bus[i].b_base>>bit)&1)?"1":"0");
			if ((bit&3)==0)
				printf("_");
		}
		printf(");\n");
	}

	// Define none_sel
	printf("\tassign\tnone_sel = (wb_stb)&&({ ");
	for(int i=0; i<np-1; i++)
		printf("%s_sel, ", bus[i].b_data.prefix);
	printf("%s_sel } == 0);\n", bus[np-1].b_data.prefix);

	// Define many_sel
	printf("\talways @(posedge i_clk)\n\tbegin\n\t\tmany_sel <= (wb_stb);\n");
	printf("\t\tcase({");
	for(int i=0; i<np-1; i++)
		printf("%s_sel, ", bus[i].b_data.prefix);
	printf("%s_sel })\n", bus[np-1].b_data.prefix);
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
		printf("%s_ack, ", bus[i].b_data.prefix);
	printf("%s_ack })\n", bus[np-1].b_data.prefix);
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

	// Now, define all the parts and pieces of what our various peripherals
	// need in the main.v file.
	for(int i=0; i<np; i++) {
		if (!bus[i].b_data.main_defns)
			continue;
		if (!bus[i].b_data.main_defns[0])
			continue;

		int	have_access = 1;
		if (!bus[i].b_data.access)
			have_access = 0;
		else if (!bus[i].b_data.access[0])
			have_access = 0;

		if (have_access)
			printf("`ifdef\t%s\n", bus[i].b_data.access);
		printf("%s\n", bus[i].b_data.main_insert);
		if (have_access) {
		printf("`else\t// %s\n", bus[i].b_data.access);
		printf("\n");
		printf("\treg\tr_%s_ack;\n", bus[i].b_data.prefix);
		printf("\talways @(posedge i_clk)\n\t\tr_%s_ack <= (wb_stb)&&(%s_sel);\n",
			bus[i].b_data.prefix,
			bus[i].b_data.prefix);
		printf("\n");
		printf("\tassign\t%s_ack   = r_%s_ack;\n",
			bus[i].b_data.prefix,
			bus[i].b_data.prefix);
		printf("\tassign\t%s_stall = 1\'b0;\n", bus[i].b_data.prefix);
		printf("\tassign\t%s_data  = 32\'h0;\n", bus[i].b_data.prefix);
		printf("\n");
		printf("%s\n", bus[i].b_data.alt_insert);
		printf("`endif\t// %s\n\n", bus[i].b_data.access);

		}
	}
	printf("\n\nendmodule;\n");

	// First, find out how long our longest definition name is.
	// This will help to allow us to line things up later.
	int	longest_defname = 0;
	for(int i=0; i<np; i++) {
		for(int j=0; j<bus[i].b_data.naddr; j++) {
			if (bus[i].b_data.pregs[j].offset < 0)
				break;
			if (!bus[i].b_data.pregs[j].defname)
				break;
			int ln = strlen(bus[i].b_data.pregs[j].defname);
			if (ln > longest_defname)
				longest_defname = ln;
		}
	}

	char	*defstring;
	defstring = new char[longest_defname+16];

	// Print definitions for regdefs.h
	printf("\n\n\n// TO BE PLACED INTO regdefs.h\n");
	for(int i=0; i<np; i++) {
		for(int j=0; j<bus[i].b_data.naddr; j++) {
			if (bus[i].b_data.pregs[j].offset < 0)
				break;
			printf("#define %-*s\t0x%08x\n", longest_defname,
				bus[i].b_data.pregs[j].defname,
				bus[i].b_base + bus[i].b_data.pregs[j].offset*4);
		}
	}

	printf("\n\n\n// TO BE PLACED INTO regdefs.cpp\n");
	printf("#include <stdio.h>\n");
	printf("#include <stdlib.h>\n");
	printf("#include <strings.h>\n");
	printf("#include <ctype.h>\n");
	printf("#include \"regdefs.h\"\n\n");
	printf("const\tREGNAME\traw_bregs[] = {\n");
	{
		int	first = 1;

		for(int i=0; i<np; i++) {
			for(int j=0; j<bus[i].b_data.naddr; j++) {
				if (bus[i].b_data.pregs[j].offset < 0)
					break;
				if (!bus[i].b_data.pregs[j].namelist[0])
					continue;
				if (first)
					first = 0;
				else
					printf(",\n");
				sprintf(defstring,"%s,",
					bus[i].b_data.pregs[j].defname);
				printf("\t{ %-*s\t\"%s\"\t\t}",
					longest_defname, defstring,
					bus[i].b_data.pregs[j].namelist);
			}
		}

		delete[]	defstring;
	} printf("\n};\n\n");
	printf("#define\tRAW_NREGS\t(sizeof(raw_bregs)/sizeof(bregs[0]))\n\n");
	printf("const\tREGNAME\t*bregs = raw_bregs;\n");
	printf("const\tint\tNREGS = RAW_NREGS;\n");

	printf("\n\n\n// TO BE PLACED INTO doc/src\n");

	for(int i=0; i<np; i++) {
		printf("\n\n\n// TO BE PLACED INTO doc/src/%s.tex\n",
			bus[i].b_data.);
	}
	printf("\n\n\n// TO BE PLACED INTO doc/src\n");
}
