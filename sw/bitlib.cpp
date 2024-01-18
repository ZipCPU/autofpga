////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	bitlib.cpp
// {{{
// Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
//
// Purpose:	Basic bit-level processing functions.
//
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
// }}}
// Copyright (C) 2017-2024, Gisselquist Technology, LLC
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



//
// nextlg
// {{{
// This is basically ceil(log_2(vl))
//
unsigned	nextlg(const unsigned long vl) {
	unsigned r;

	for(r=0; (1ul<<r)<(unsigned long)vl; r+=1)
		;
	return r;
}
// }}}

//
// popc
// {{{
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
// }}}
