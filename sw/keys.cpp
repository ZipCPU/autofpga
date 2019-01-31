////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	keys.cpp
//
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Defines the strings AutoFPGA understands.  These strings are
// 		typically key names, as described by the ICD.  The actual string
// 	names are (nearly) equal to their C++ name.  However, by defining them
// 	here, I get some robustness to ICD changes, and some compiler help to
// 	make sure I only use named strings.
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017-2019, Gisselquist Technology, LLC
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
#include "keys.h"

const	STRING	KYLEGAL=	"LEGAL";
const	STRING	KYCOPYRIGHT=	"COPYRIGHT";	// Another name for LEGAL
const	STRING	KYCMDLINE=	"CMDLINE";
const	STRING	KYSUBD=		"SUBD";
const	STRING	KYPATH=		"PATH";
// const	STRING	KYBUS_ADDRESS_WIDTH="BUS_ADDRESS_WIDTH";
const	STRING	KYRESET_ADDRESS	="RESET_ADDRESS";
// For subclassing
const	STRING	KYPLUSDOT	="+";
const	STRING	KYINCLUDE	="INCLUDE";
const	STRING	KYINCLUDEFILE	="INCLUDEFILE";
// Other global keys
const	STRING	KYREGDEFS_CPP_INCLUDE	="REGDEFS.CPP.INCLUDE";
const	STRING	KYREGDEFS_CPP_INSERT	="REGDEFS.CPP.INSERT";
const	STRING	KYKEYS_TRIMLIST	="KEYS.TRIMLIST";
const	STRING	KYKEYS_INTLIST	="KEYS.INTLIST";
const	STRING	KYPROJECT	="PROJECT";
//
const	STRING	KYSIO=		"sio";
const	STRING	KYDIO=		"dio";
const	STRING	KYSIO_SEL=	"sio_sel";
const	STRING	KYDIO_SEL=	"dio_sel";
//
const	STRING	KYPREFIX=	"PREFIX";
const	STRING	KYACCESS=	"ACCESS";
const	STRING	KYDEPENDS=	"DEPENDS";
const	STRING	KYERROR_WIRE=	"ERROR.WIRE";
const	STRING	KYSLAVE=	"SLAVE",
		KYSLAVE_TYPE=	"SLAVE.TYPE",
		KYSLAVE_BUS=	"SLAVE.BUS",
		KYSLAVE_ORDER=	"SLAVE.ORDER";
const	STRING	KYMASTER=	"MASTER",
		KYMASTER_TYPE=	"MASTER.TYPE",
		KYMASTER_BUS=	"MASTER.BUS",
		KYMASTER_BUS_NAME=	"MASTER.BUS.NAME";
// Types of bus masters
// KYBUS, and ...
const	STRING	KYSUBBUS=	"SUBBUS",
		KYARBITER=	"ARBITER",
		KYXCLOCK=	"XCLOCK",
		KYHOST=		"HOST",
		KYCPU=		"CPU";
//
const	STRING	KYBASE=		"BASE";
const	STRING	KYREGBASE=	"REGBASE";
const	STRING	KYNADDR=	"NADDR";
const	STRING	KYMASK=		"MASK";
//
const	STRING	KYEXPR=		"EXPR";
const	STRING	KYVAL=		"VAL";
const	STRING	KYFORMAT=	"FORMAT";
const	STRING	KYSTR=		"STR";
//
const	STRING	KYSCOPE=	"SCOPE";
const	STRING	KYMEMORY=	"MEMORY";
const	STRING	KYSINGLE=	"SINGLE";
const	STRING	KYDOUBLE=	"DOUBLE";
const	STRING	KYOTHER=	"OTHER";
// Numbers of things
const	STRING	KYNP=		"NP";
const	STRING	KYNPIC=		"NPIC";
const	STRING	KYNPSINGLE=	"NPSINGLE";
const	STRING	KYNPDOUBLE=	"NPDOUBLE";
const	STRING	KYNPMEMORY=	"NPMEMORY";
const	STRING	KYNSCOPES=	"NSCOPES";
// Regs definitions
const	STRING	KYREGS_N=	"REGS.N";
const	STRING	KYREGS_NOTE=	"REGS.NOTE";
const	STRING	KYREGDEFS_H_INCLUDE="REGDEFS.H.INCLUDE";
const	STRING	KYREGDEFS_H_DEFNS="REGDEFS.H.DEFNS";
const	STRING	KYREGDEFS_H_INSERT="REGDEFS.H.INSERT";
// Board defintions for C/C++
const	STRING	KYBDEF_INCLUDE=	"BDEF.INCLUDE";
const	STRING	KYBDEF_DEFN=	"BDEF.DEFN";
const	STRING	KYBDEF_IOTYPE=	"BDEF.IOTYPE";
const	STRING	KYBDEF_OSDEF=	"BDEF.OSDEF";
const	STRING	KYBDEF_OSVAL=	"BDEF.OSVAL";
const	STRING	KYBDEF_INSERT=	"BDEF.INSERT";
// Top definitions
const	STRING	KYTOP_PORTLIST=	"TOP.PORTLIST";
const	STRING	KYTOP_IODECL=	"TOP.IODECL";
const	STRING	KYTOP_DEFNS=	"TOP.DEFNS";
const	STRING	KYTOP_MAIN=	"TOP.MAIN";
const	STRING	KYTOP_INSERT=	"TOP.INSERT";
// Main definitions
const	STRING	KYMAIN_PORTLIST="MAIN.PORTLIST";
const	STRING	KYMAIN_IODECL=	"MAIN.IODECL";
const	STRING	KYMAIN_PARAM=	"MAIN.PARAM";
const	STRING	KYMAIN_DEFNS=	"MAIN.DEFNS";
const	STRING	KYMAIN_INSERT=	"MAIN.INSERT";
const	STRING	KYMAIN_ALT=	"MAIN.ALT";
// LD definitions
const	STRING	KYLD_FILE=	"LD.FILE";
const	STRING	KYLD_SCRIPT=	"LD.SCRIPT";
const	STRING	KYLD_ENTRY=	"LD.ENTRY";
const	STRING	KYLD_NAME=	"LD.NAME";
const	STRING	KYLD_PERM=	"LD.PERM";
const	STRING	KYLD_DEFNS=	"LD.DEFNS";
const	STRING	KYFLASH=	"flash";
// XDC/UCF definitions
const	STRING	KYXDC_FILE=	"XDC.FILE";
const	STRING	KYXDC_INSERT=	"XDC.INSERT";
const	STRING	KYPCF_FILE=	"PCF.FILE";
const	STRING	KYPCF_INSERT=	"PCF.INSERT";
const	STRING	KYUCF_FILE=	"UCF.FILE";
const	STRING	KYUCF_INSERT=	"UCF.INSERT";
// INT definitions
const	STRING	KY_INT=		"INT";
const	STRING	KYINTLIST=	"INTLIST";
const	STRING	KY_WIRE=	"WIRE";
const	STRING	KY_DOTWIRE=	".WIRE";
const	STRING	KY_ID=		"ID";
// Arbitrary output data files
const	STRING	KYOUT_FILE=	"OUT.FILE";
const	STRING	KYOUT_DATA=	"OUT.DATA";
// RTL/Makefile definitions
const	STRING	KYRTL_MAKE_GROUP= "RTL.MAKE.GROUP";
const	STRING	KYRTL_MAKE_SUBD=  "RTL.MAKE.SUBD";
const	STRING	KYVFLIST=	  "VFLIST";
const	STRING	KYRTL_MAKE_VDIRS= "RTL.MAKE.VDIRS";
const	STRING	KYRTL_MAKE_FILES= "RTL.MAKE.FILES";
const	STRING	KYAUTOVDIRS=	  "AUTOVDIRS";
// PIC definitions
const	STRING	KYPIC=		"PIC";
const	STRING	KYPIC_BUS=	"PIC.BUS";
const	STRING	KYPIC_MAX=	"PIC.MAX";
// Cache information
const	STRING	KYCACHABLE_FILE="CACHABLE.FILE";
// SIM definitions
const	STRING	KYSIM_INCLUDE=	"SIM.INCLUDE";
const	STRING	KYSIM_DEFINES=	"SIM.DEFINES";
const	STRING	KYSIM_DEFNS=	"SIM.DEFNS";
const	STRING	KYSIM_PREINITIAL="SIM.PREINITIAL";
const	STRING	KYSIM_INIT=	"SIM.INIT";
const	STRING	KYSIM_CLOCK=	"SIM.CLOCK";
const	STRING	KYSIM_TICK=	"SIM.TICK";
const	STRING	KYSIM_SETRESET=	"SIM.SETRESET";
const	STRING	KYSIM_CLRRESET=	"SIM.CLRRESET";
const	STRING	KYSIM_DBGCONDITION=	"SIM.DBGCONDITION";
const	STRING	KYSIM_DEBUG=	"SIM.DEBUG";
const	STRING	KYSIM_LOAD=	"SIM.LOAD";
const	STRING	KYSIM_METHODS=	"SIM.METHODS";
// SIM/Makefile definitions
// const	STRING	KYSIM_MAKE_GROUP= "SIM.MAKE.GROUP";
// const	STRING	KYSIM_MAKE_FILES= "SIM.MAKE.FILES";
// const	STRING	KYSIM_MAKE_SUBD=  "SIM.MAKE.SUBD";
// const	STRING	KYSIM_MAKE_VDIRS= "SIM.MAKE.VDIRS";
//
const	STRING	KYTHIS="THIS";
const	STRING	KYTHISDOT="THIS.";
// CLOCKS
const	STRING	KYCLOCK="CLOCK";
const	STRING	KYCLOCK_NAME="CLOCK.NAME";
const	STRING	KYCLOCK_TOP="CLOCK.TOP";
const	STRING	KY_NAME="NAME";
const	STRING	KY_TOP="TOP";
const	STRING	KY_FREQUENCY="FREQUENCY";
const	STRING	KY_CLASS="CLASS";
// BUS definitions
const	STRING	KYBUS = "BUS",
		KYBUS_NAME = "BUS.NAME",
		KYBUS_PREFIX = "BUS.PREFIX",
		KYBUS_TYPE   = "BUS.TYPE",
		KYBUS_AWID   = "BUS.AWID",
		KYBUS_CLOCK  = "BUS.CLOCK",
		KYBUS_NULLSZ = "BUS.NULLSZ",
		KY_TYPE      = "TYPE",
		KY_WIDTH     = "WIDTH",
		KY_AWID      = "AWID",
		KY_CLOCK     = "CLOCK",
		KY_NULLSZ    = "NULLSZ",
		KY_NSELECT   = "NSELECT",
		KYDEFAULT_BUS= "DEFAULT.BUS",
		KYREGISTER_BUS= "REGISTER.BUS";

//
//

