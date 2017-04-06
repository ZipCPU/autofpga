# AutoFPGA - An FPGA Design Automation routine

After now having built several FPGA designs, such as the
[xulalx25soc](https://github.com/ZipCPU/xulalx25soc),
[s6soc](https://github.com/ZipCPU/s6soc),
[openarty](https://github.com/ZipCPU/openarty),
[zbasic](https://github.com/ZipCPU/zbasic),
and even a Basys-3 design of my own that hasn't been published, I started
recognizing that all of these designs have a lot in common.  In particular, they
all have a set of bus masters, such as the UART-to-wishbone bridge that I use,
or even the [zipcpu](https://github.com/ZipCPU/zipcpu).  Many of these designs
have also started to use (and reuse) many of the peripherals I've developed,
such as
the generic [UART](https://github.com/ZipCPU/wbuart),
the [QSPI flash controller](https://github.com/ZipCPU/qspiflash),
the [SD-card controller](https://github.com/ZipCPU/sdspi),
the [block RAM controller](https://github.com/ZipCPU/openarty/blob/master/rtl/memdev.v),
the [RMII Ethernet Controller](https://github.com/ZipCPU/openarty/blob/master/rtl/enetpackets.v),
the [Real-Time Clock](https://github.com/ZipCPU/rtcclock),
the [Real-Time Date](https://github.com/ZipCPU/rtcclock/blob/master/rtl/rtcdate.v),
the [Organic LED controller](https://github.com/ZipCPU/openarty/blob/master/rtl/wboled.v),
Xilinx's [Internal Configuration Access Port](https://github.com/ZipCPU/wbicapetwo),
the [wishbone scope](https://github.com/ZipCPU/wbscope),
the [GPS controlled clock](https://github.com/ZipCPU/openarty/blob/master/rtl/gpsclock.v),
the [GPS controlled clock](https://github.com/ZipCPU/openarty/blob/master/rtl/gpsclock.v),
or even the [PWM Audio Controller](https://github.com/ZipCPU/wbpwmaudio).
All of these peripherals have a very similar format when included within a
top level design, all of these require a certain amount of care and feeding
as part of that top level design, but yet rebuilding that top level design over
and over just to repeat this information becomes a pain.

Where things really get annoying is where and when you want to assign peripheral
addresses to all of these files.  While the peripheral decoding is typically
done in some [main Verilog file](https://github.com/ZipCPU/openarty/blob/master/rtl/busmaster.v),
other files depend upon what this peripheral decoding is.  These other files
include the [host register definition file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.h),
the [register naming file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.cpp),
the [ZipCPU board definition file](https://github.com/ZipCPU/openarty/blob/master/sw/zlib/artyboard.h),
the [linker script](https://github.com/ZipCPU/openarty/blob/master/sw/board/arty.ld) for the board,
and even the [LaTeX specification](https://github.com/ZipCPU/openarty/blob/master/doc/src/spec.tex) for the board.
Creating and updating all of these files by hand anytime I create a new board
file can get tedious.

Solving this problem is the purpose of autofpga.

Unlike many of the other tools out there, such as Xilinx's schematic capture,
autofpga is not built with the beginner in mind, neither is it built to hide
the details of what is going on.  Instead, it is built to alleviate the burden
of an experienced FPGA designer who otherwise has to create and maintain
coherency between multiple design files.

# Goal

The goal of AutoFPGA is to be able to run it with a list of peripheral
definition files, given on the command line, and to thus be able to generate
(or update?) the various board definition files discussed above:

- sw/rtl/toplevel.v
- sw/rtl/main.v
- sw/rtl/scopelist.v
- sw/host/regdefs.h
- sw/host/regdefs.cpp
- sw/zlib/board.h
- sw/zlib/board.ld
- doc/src/(component name).tex

Specifically, the parser must determine:
- If any of the peripherals used in this project need to be configured, and if so, what the configuration parameters are and how they need to be set.  For example, the UART baud rate and RTC and GPS clock steps both need to be set based upon the actual clock speed of the master clock.
- If any of the peripherals have debugging wires associated with them which would need to be connected to a scope.  The AutoFPGA program should find all such peripherals, but yet still allow the user to decide which peripherals get scopes.  Even better, if by adjusting the regdefs.h file, a host program might be able to detect that its particular peripheral is not currently configured to one of the scopes.
- If peripherals have or create interrupts, those need to be found and determined, and (even better) wired up.  To do this, the user may need to specify an interrupt file (for now), specyifying which are connected and how they are connected.
- If the item it is parsing fits into one of the following classes of items:
	* Full bus masters
	* (Partial) bus masters, wanting access to one peripheral only
	* One-clock Peripherals (interrupt controller, etc.)
	* Two-clock Peripherals (RTC clock, block RAM, scopes, etc.)
	* Memory Peripherals
		* These need a line within the linker script, and they need to define if their memory region, within that linker script, has read, write, or execute permissions
	* Generic Peripherals (flash, SDRAM, MDIO, etc.)
- Peripheral files need to be able to describe more than one peripheral.  For example, the GPS file has a GPS-Clock, GPS-TB to measure the performance of the GPS clock, and a WBUART to allow us to read from the GPS and to write to it and so configure it.  Of course ... this also includes a parameter that must be set (baud rate)

Each peripheral may have 3-levels of container descriptions: top (information
to be added to the toplevel.v file), main (information to be added to a main.v
file), and sub (in case peripherals get lumped together beneath the main.v file).  Sub is only appropriate if the peripheral might be placed into a sub-container, such as a singleio.v container, or even (should I build one) a doubleio.v container.

## Classes

Some peripherals might exist at multiple locations within a design.
For example, the WBUART can be used to create multiple serial ports.
For now, we don't support classes, but rather
individual peripheral files, since it's not clear what could stay the
same.  In other words, MANY entries would need to change to avoid
variable name contention.  Without solving this problem, we can't
do classes (YET)

## Math
Some peripherals need to be able to perform math on a given value to determine
an appropriate setting value.  These peripherals need access to variables.
The classic examples are the baud rate, which depends upon the clock rate,
as well as the step size necessary for the RTC and the GPS clocks, both of which
also depend upon the master clock rate.

# Status

This project is currently in its bare infancy.  Some success has been
demonstrated to date in building a bus.  This success may be seen in the 
original [initial demo](sw/demo.txt), as well as in the updated and
[subsequent demo](../../tree/master/sw/demo-out/).

As of 20170405, the [main.v](sw/demo-out/main.v),
[regdef.h](sw/demo-out/regdefs.h),
and [regdefs.cpp](sw/demo-out/regdefs.cpp) files now pass an
initial scrub by 
[Verilator](https://www.veripool.org/wiki/verilator)
and [GCC](https://gcc.gnu.org), so I anticipate running
things via [Verilator](https://www.veripool.org/wiki/verilator)
and [wbregs](https://github.com/ZipCPU/openarty/blob/master/sw/host/wbregs.cpp)
soon to verify that the bus still works --- after having torn it apart
and put it back together via this routine.

In detail:
- Simple bus components ... just work.  (Not tested yet, though)
- Components with logic in the toplevel file appear to work.  (Since Verilator doesn't simulate this, it make take another round or two to find out what's missing there.)
- Although it shouldn't have any problems integrating memory components and cores, I have yet to try integrating any [SDRAM](https://github.com/ZipCPU/xulalx25soc/blob/master/rtl/wbsdram.v) or [DDR3 SDRAM](http://opencores.org/project,wbddr3) components.
- Only one [PC host to wishbone busmaster](sw/wbubus.txt) component has been integrated.  Driving the design from either JTAG or Digilent's DEPP interface would require a simple modification to this.
- Addresses get assigned in three groups, and processed in three groups: components having only one address, components having more addresses but only a single clock delay, and all other components and memories
- Interrupts get assigned to a named controller, and then C++ header files can be updated to reflect that
- A simple mathematical expression evaluator exists, allowing simple math expressions and print formats.  This makes it possible to set a global clock frequency value, and to then set baud rates and other clock dividers from it.
- Automatically assigning, connecting, and building [wishbone scopes](https://github.com/ZipCPU/wbscope) just ... isn't there yet
- The auto builder does nothing to create the master C++ Verilator simulation file, or any RTL based Makefiles.
- While device selection via bus decoding is built, it isn't optimized yet.  I'll probably return to this only when the bus decoding logic isn't fast enough, and that'll require it (nearly) running on actual hardware.
- It doesn't build any linker script components ... yet.
- The LaTeX specification table building isn't there ... yet.

# Sample component files

Component files now exist for many of the components I've been using regularly.
These include: 
a [Flash](sw/flash.txt),
[block RAM](sw/bkram.txt),
a [UART console](sw/wbuart.txt),
a very simple [GPIO controller](sw/gpio.txt),
[RMII ethernet controller](sw/enet.txt),
[MDIO ethernet control interface](sw/mdio.txt),
a [GPS UART and PPS-driven internal clock](sw/gps.txt), 
a [Real-Time (GPS driven) Clock](sw/rtcgps.txt),
a [PS/2 Mouse](sw/wbmouse.txt),
an [OLEDRGB component](sw/wboled.txt),
and more.
Many of these component cores exist and have their own repositories elsewhere.
For example, the wishbone UART core may be found
[here](https://github.com/ZipCPU/wbuart32).
Building the cores themselves is not a part of this project, but rather
figuring out how to create a top level design from both cores and component
descriptions.

# License

AutoFPGA is designed for release under the GPLv3 license.

# Commercial Applications

Should you find the GPLv3 license insufficient for your needs, other licenses
can be purchased from Gisselquist Technology, LLC.

Likewise, please contact us should you wish to guide, direct, or otherwise
fund the development of this project.  You can contact me at my user name,
dgisselq, at ieee.org.

