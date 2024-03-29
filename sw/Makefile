################################################################################
##
## Filename: 	Makefile
## {{{
## Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
##
## Purpose:	
##
## Creator:	Dan Gisselquist, Ph.D.
##		Gisselquist Technology, LLC
##
################################################################################
## }}}
## Copyright (C) 2017-2024, Gisselquist Technology, LLC
## {{{
## This program is free software (firmware): you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published
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
## }}}
## License:	GPL, v3, as defined and found on www.gnu.org,
## {{{
##		http://www.gnu.org/licenses/gpl.html
##
################################################################################
##
## }}}
all:
PROGRAMS := autofpga demo
OBJDIR:= obj-pc
YYMMDD:= `date +%Y%m%d`
CXX:= g++
SED:= sed
CFLAGS:= -std=c++11 -g -Wall -I bus/
SOURCES := autofpga.cpp parser.cpp kveval.cpp keys.cpp mapdhash.cpp ast.cpp \
	legalnotice.cpp ifdefs.cpp bldregdefs.cpp expr.tab.cpp lex.yy.cpp   \
	bitlib.cpp bldtestb.cpp bldsim.cpp predicates.cpp		    \
	clockinfo.cpp subbus.cpp globals.cpp gather.cpp			    \
	bldboardld.cpp bldrtlmake.cpp msgs.cpp bldcachable.cpp		    \
	businfo.cpp plist.cpp mlist.cpp genbus.cpp $(wildcard bus/*.cpp)

POSSHDRS:= $(subst .c,.h,$(subst .cpp,.h,$(SOURCES)))
HEADERS := $(foreach header,$(POSSHDRS),$(wildcard $(header)))
OBJECTS := $(addprefix $(OBJDIR)/,$(subst bus/,,$(subst .c,.o,$(subst .cpp,.o,$(SOURCES)))))

all: $(PROGRAMS)

$(OBJDIR)/autofpga.o: autofpga.cpp
	$(mk-objdir)
	$(CXX) -DBUILDDATE=0x$(YYMMDD) $(CFLAGS) -c $< -o $@

$(OBJDIR)/lex.yy.o: lex.yy.cpp
	$(mk-objdir)
	$(CXX) $(CFLAGS) -Wno-sign-compare -Wno-unused-function -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	$(mk-objdir)
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: bus/%.cpp
	$(mk-objdir)
	$(CXX) $(CFLAGS) -I. -c $< -o $@

$(OBJDIR)/%.o: %.c
	$(mk-objdir)
	$(CXX) $(CFLAGS) -c $< -o $@

expr.tab.h: expr.ypp
	bison --defines=expr.tab.h expr.ypp
expr.tab.cpp: expr.tab.h
$(OBJDIR)/expr.tab.o: lex.yy.h

lex.yy.cpp lex.yy.h: expr.l expr.tab.h
	flex --header-file=lex.yy.h -o lex.yy.cpp expr.l

autofpga: $(OBJECTS)
	echo $(OBJECTS)
	$(CXX) $(CFLAGS) $(OBJECTS) -o $@

VDATA := bkram.txt buserr.txt clkcounter.txt clock.txt enet.txt		\
	flash.txt global.txt gpio.txt gps.txt hdmi.txt icape.txt	\
	legalgen.txt mdio.txt pic.txt pwrcount.txt rtcdate.txt		\
	rtcgps.txt sdram.txt sdspi.txt spio.txt version.txt wbmouse.txt	\
	wboledbw.txt wbpmic.txt wbscopc.txt wbscope.txt	wbubus.txt	\
	xpander.txt zipmaster.txt
# Unused:
#	wbuart.txt


.PHONY: demo
demo: runme.sh
	bash runme.sh

.PHONY: runme.sh	# Force a rebuild of this file every time
runme.sh: autofpga $(addprefix ../auto-data/,$(VDATA))
	@echo "#!/bin/bash" > $@
	@echo >> $@
	@echo >> $@
	@echo ./autofpga -d -o ../demo-out -I ../auto-data $(VDATA)  >> $@
	@echo >> $@
	@echo "echo \"exit code \" \$$?" >> $@

define	mk-objdir
	@bash -c "if [ ! -e $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi"
endef

define	build-depends
	@echo "Building dependency file"
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
