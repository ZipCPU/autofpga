################################################################################
##
## Filename:	Makefile
##
## Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
##
## Purpose:	A master project makefile.  It tries to build all targets
##		within the project, mostly by directing subdirectory makes.
##
##
## Creator:	Dan Gisselquist, Ph.D.
##		Gisselquist Technology, LLC
##
################################################################################
##
## Copyright (C) 2017-2019, Gisselquist Technology, LLC
##
## This program is free software (firmware): you can redistribute it and/or
## modify it under the terms of  the GNU General Public License as published
## by the Free Software Foundation, either version 3 of the License, or (at
## your option) any later version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
## target there if the PDF file isn't present.)  If not, see
## <http://www.gnu.org/licenses/> for a copy.
##
## License:	GPL, v3, as defined and found on www.gnu.org,
##		http://www.gnu.org/licenses/gpl.html
##
##
################################################################################
##
##
.PHONY: all
all:	archive sw
YYMMDD:=`date +%Y%m%d`

.PHONY: archive
archive:
	tar --transform s,^,$(YYMMDD)-video/, -chjf $(YYMMDD)-autofpga.tjz sw/ auto-data/ demo-out/ doc/

.PHONY: autofpga
autofpga: sw

.PHONY: sw
sw:
	$(MAKE) --no-print-directory --directory=sw

.PHONY: clean
clean:
	$(MAKE) --no-print-directory --directory=sw clean
