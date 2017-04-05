################################################################################
##
## Filename: 	Makefile
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
PROGRAMS := autofpga demo
OBJDIR:= obj-pc
CXX:= g++
SED:= sed
CFLAGS:= -std=c++11 -g -Wall
SOURCES := autofpga.cpp parser.cpp kveval.cpp keys.cpp mapdhash.cpp ast.cpp \
	expr.tab.cpp lex.yy.cpp
POSSHDRS:= $(subst .c,.h,$(subst .cpp,.h,$(SOURCES)))
HEADERS := $(foreach header,$(POSSHDRS),$(wildcard $(header)))
OBJECTS := $(addprefix $(OBJDIR)/,$(subst .c,.o,$(subst .cpp,.o,$(SOURCES))))

all: $(PROGRAMS)

$(OBJDIR)/%.o: %.cpp
	$(mk-objdir)
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	$(mk-objdir)
	$(CXX) $(CFLAGS) -c $< -o $@

expr.tab.cpp expr.tab.h: expr.ypp
	bison --defines=expr.tab.h expr.ypp

lex.yy.cpp lex.yy.h: expr.l
	flex --header-file=lex.yy.h -o lex.yy.cpp expr.l

autofpga: $(OBJECTS)
	echo $(OBJECTS)
	$(CXX) $(CFLAGS) $(OBJECTS) -o $@

VSRCS := bkram.txt enet.txt flash.txt gps.txt icape.txt mdio.txt wbuart.txt \
	sdspi.txt wbmouse.txt wboled.txt clkcounter.txt gpio.txt pwrcount.txt \
	version.txt buserr.txt zipmaster.txt pic.txt rtcgps.txt clock.txt \
	hdmi.txt wbubus.txt dlyarbiter.txt

.PHONY: demo
demo: autofpga $(VSRCS)
	./autofpga $(VSRCS) 

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
	rm -rf lex.yy.cpp lex.yy.h expr.tab.cpp expr.tab.h expr.output

-include $(OBJDIR)/depends.txt
