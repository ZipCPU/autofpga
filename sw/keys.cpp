////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	keys.h
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
#include "keys.h"

const	STRING	KYLEGAL=	"LEGAL";
const	STRING	KYCMDLINE=	"CMDLINE";
const	STRING	KYSUBD=		"SUBD";
const	STRING	KYBUS_ADDRESS_WIDTH="BUS_ADDRESS_WIDTH";
const	STRING	KYSKIPADDR	="SKIPADDR";
//
const	STRING	KYSIO=		"sio";
const	STRING	KYDIO=		"dio";
const	STRING	KYSIO_SEL=	"sio_sel";
const	STRING	KYDIO_SEL=	"dio_sel";
//
const	STRING	KYPREFIX=	"PREFIX";
const	STRING	KYACCESS=	"ACCESS";
const	STRING	KYPTYPE=	"PTYPE";
const	STRING	KYMTYPE=	"MTYPE";
//
const	STRING	KYBASE=		"BASE";
const	STRING	KYNADDR=	"NADDR";
const	STRING	KYMASK=		"MASK";
// const	STRING	KYSKIPADDR=		"SKIPADDR";	// Given above
const	STRING	KYSKIPMASK=		"SKIPMASK";
const	STRING	KYSKIPNBITS=		"SKIPNBITS";
const	STRING	KYSKIPAWID=		"SKIPAWID";
const	STRING	KYSKIPDEFN=		"SKIPDEFN";
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
const	STRING	KYREGS_INSERT_H="REGS.INSERT.H";
// Board defintions for C/C++
const	STRING	KYBDEF_DEFN=	"BDEF.DEFN";
const	STRING	KYBDEF_IOTYPE=	"BDEF.IOTYPE";
const	STRING	KYBDEF_OSDEF=	"BDEF.OSDEF";
const	STRING	KYBDEF_OSVAL=	"BDEF.OSVAL";
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
// INT definitions
const	STRING	KY_INT=		"INT";
const	STRING	KY_WIRE=	"WIRE";
const	STRING	KY_DOTWIRE=	".WIRE";
const	STRING	KY_ID=		"ID";
// PIC definitions
const	STRING	KYPIC=		"PIC";
const	STRING	KYPIC_BUS=	"PIC.BUS";
const	STRING	KYPIC_MAX=	"PIC.MAX";
//
const	STRING	KYTHIS="THIS";
const	STRING	KYTHISDOT="THIS.";

//
//

