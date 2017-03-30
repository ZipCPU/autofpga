################################################################################
##
## Filename: 	
##
## Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
##
## Purpose:	
##
## Creator:	Dan Gisselquist, Ph.D.
##		Gisselquist Technology, LLC
##
################################################################################
##
## Copyright (C) 2017, Gisselquist Technology, LLC
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
all:
PROGRAMS := autofpga demo2.txt
OBJDIR:= obj-pc
CXX:= g++
SED:= sed
CFLAGS:= -std=c++11 -g -Og
SOURCES := autofpga.cpp parser.cpp
HEADERS := $(foreach header,$(subst .cpp,.h,$(SOURCES)),$(wildcard $(header)))
OBJECTS := $(addprefix $(OBJDIR)/,$(subst .cpp,.o,$(SOURCES)))

all: $(PROGRAMS)

$(OBJDIR)/%.o: %.cpp
	$(mk-objdir)
	$(CXX) $(CFLAGS) -c $< -o $@

autofpga: $(OBJECTS)
	$(CXX) $(CFLAGS) $(OBJECTS) -o $@

VSRCS := bkram.txt enet.txt flash.txt gps.txt icape.txt mdio.txt wbuart.txt \
	sdspi.txt wbmouse.txt wboled.txt

.PHONY: demo demo2
demo: demo2.txt
demo2: demo2.txt
demo2.txt: autofpga $(VSRCS)
	./autofpga $(VSRCS) > $@

define	mk-objdir
	@bash -c "if [ ! -e $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi"
endef

define	build-depends
	@echo "Buidling dependency file"
	$(CXX) $(CFLAGS) -MM $(SOURCES) > $(OBJDIR)/xdep.txt
	$(SED) -e 's/^.*.o: /$(OBJDIR)\/&/' < $(OBJDIR)/xdep.txt > $(OBJDIR)/depends.txt
endef

.PHONY: depends
depends: tags
	$(mk-objdir)
	$(build-depends)

tags: $(SOURCES) $(HEADERS)
	@echo "Generating tags"
	@ctags $(SOURCES) $(HEADERS)

.PHONY: clean
clean:
	rm -rf $(OBJDIR)/ $(PROGRAMS)

-include $(OBJDIR)/depends.txt
