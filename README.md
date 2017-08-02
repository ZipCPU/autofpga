# AutoFPGA - An FPGA Design Automation routine

After now having built several FPGA designs, such as the
[xulalx25soc](https://github.com/ZipCPU/xulalx25soc),
[s6soc](https://github.com/ZipCPU/s6soc),
[openarty](https://github.com/ZipCPU/openarty),
[zbasic](https://github.com/ZipCPU/zbasic),
[icozip](https://github.com/ZipCPU/icozip),
and even a Basys-3 design of my own that hasn't been published, I started
recognizing that all of these designs have a lot in common.  In particular, they
all have a set of bus masters, such as the
[UART-to-wishbone](https://github.com/ZipCPU/zbasic/blob/master/rtl/wbubus.v)
bridge that I use, the [hexbus](https://github.com/ZipCPU/dbgbus) debugging
bus that offers a simpler version of the same, or even the
[zipcpu](https://github.com/ZipCPU/zipcpu). 
Many of these designs have also started to use (and reuse) many of the
peripherals I've developed, such as
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

Where things were really starting to get annoying is where the C++ information
was depending upon Verilog information.  A classic example of this is the
base address of any bus components.  However, if you add clock rate into
the mix, you can then also control things such as any default UART
configuration, default clock stepping information (for the RTC clock),
or even default video clock information.

Sharing information between Verilog and C++ then became one of the primary
reasons for creating autofpga.  While peripheral decoding is typically done
in some [main Verilog
file](https://github.com/ZipCPU/openarty/blob/master/rtl/busmaster.v),
other files depend upon what this peripheral decoding is.  These other files
include the [host register definition
file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.h) (used
for debugging access), the [register naming
file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.cpp), the
[ZipCPU board definition
file](https://github.com/ZipCPU/openarty/blob/master/sw/zlib/artyboard.h) used
by newlib, the [linker
script](https://github.com/ZipCPU/openarty/blob/master/sw/board/arty.ld) used
by the compiler, and even the [LaTeX
specification](https://github.com/ZipCPU/openarty/blob/master/doc/src/spec.tex)
for the board.  Creating and updating all of these files by hand anytime I
create a new board file can get tedious.  Further, every time a board is
reconfigured, the constraints file, whether
[XDC](https://github.com/ZipCPU/openarty/blob/master/arty.xdc) or
[UCF](https://github.com/ZipCPU/xulalx25soc/blob/master/xula.ucf) file, needs
to be updated to match the current constraints.

Solving this multi-language coordination problem is the purpose of autofpga.

Unlike many of the other tools out there, such as Xilinx's schematic capture,
autofpga is not built with the clueless beginner in mind, neither is it built
to hide the details of what is going within the project it creates.  Instead,
autofpga is built with the sole purpose of alleviating any burden on the FPGA
designer who otherwise has to create and maintain coherency between multiple
design files.

That this program facilitates composing and building new designs from existing
components ... is just a bonus.

# Goal

The goal of AutoFPGA is to be able to run it with a list of peripheral
definition files, given on the command line, and to thus be able to generate
(or update?) the various board definition files discussed above:

- [rtl/toplevel.v](demo-out/toplevel.v)
- [rtl/main.v](demo-out/main.v)
- [rtl/make.inc](demo-out/rtl.make.inc)
- [sw/host/regdefs.h](demo-out/regdefs.h)
- [sw/host/regdefs.cpp](demo-out/regdefs.cpp)
- [sw/zlib/board.h](demo-out/board.h)
- [sw/zlib/board.ld](demo-out/board.ld)
- [build.xdc](demo-out/build.xdc) (Created by modifying an existing XDC file)
- [sim/verilated/testb.h](demo-out/testb.h)
- [sim/verilated/main_tb.h](demo-out/main_tb.cpp)
- doc/src/(component name).tex (Not started yet)

Specifically, the parser must determine:

- If any of the peripherals used in this project need to be configured, and if
  so, what the configuration parameters are and how they need to be set.  For
  example, the UART baud rate and RTC and GPS clock steps both need to be set
  based upon the actual clock speed of the master clock.  Placing [a
  clock module](auto-data/clock.txt) within the design that sets up a clock and
  declares its rate is the current method for accomplishing this.

- If peripherals have or create interrupts, those need to be found and
  determined, and (even better) wired up.

- If an autofpga configuration file describes one of the following classes of
  items, then the file is wired up and connected to create the necessary bus
  wiring as well.

  * Full bus masters

    Peripheral selection and decoding logic is added in

  * (Partial) bus masters, wanting access to one peripheral only

  * One-clock Peripherals (interrupt controller, etc.)

  * Two-clock Peripherals (RTC clock, block RAM, scopes, etc.)

  * Memory Peripherals

    o These need a line within the linker script, and they need to define if
      their memory region, within that linker script, has read, write, or
      execute permissions

    o Generic Peripherals ([flash](auto-data/flash.txt), SDRAM,
      [MDIO](auto-data/mdio.txt), etc.)

- Peripheral files need to be able to describe more than one peripheral.  For
  example, the [GPS peripheral file](auto-data/gps.txt) has a GPS-Clock,
  GPS-TB to measure the performance of the GPS clock, and a serial port
  ([WBUART](https://github.com/ZipCPU/wbuart32)) to allow us
  to read from the GPS and to write to it and so configure it.  Of course ...
  this also includes a parameter that must be set (baud rate) based upon the
  global clock rate.

## Classes

Some peripherals might exist at multiple locations within a design.
For example, the WBUART serial component can be used to create multiple
serial ports within a design.

To handle this case, the WBUART configuration file may be subclassed within
other component configuration files by defining a key
@INCLUDEFILE=[wbuart.txt](auto-data/wbuart.txt).  This will provide a set of
keys that the current file can then override (inherit from).

**TODO**: Subclass only named components of a configuration file

## Math

Some peripherals need to be able to perform math on a given value to determine
an appropriate setting value.  These peripherals need access to variables.
The classic examples are the baud rate, which depends upon the clock rate,
as well as the step size necessary for the RTC and the GPS clocks, both of
which also depend upon the master clock rate.

This feature is currently fully supported using integer math.

# Status

This project has moved from its bare infancy to an initial demo that is now
working on a Nexys Video board.  You can see the code this program generates
in the [demo directory](demo-out/), although you may have to collect the
actual peripheral code from elsewhere.  (Most of it already exists in the
[openarty](https://github.com/ZipCPU/openarty) project.)

In detail:

- Simple bus components ... just work.

- Components with logic in the toplevel work nicely as well.

- Although it shouldn't have any problems integrating memory components and
  cores, I have yet to try integrating any
  [SDRAM](https://github.com/ZipCPU/xulalx25soc/blob/master/rtl/wbsdram.v) or
  [DDR3 SDRAM](http://opencores.org/project,wbddr3) components.

- AutoFPGA can now support multiple, dissimilar clock rates.  Users just need
  to specify a clock to generate it.  The clock is then made available for
  configuration files to reference

  o AutoFPGA now also has support for building a [test bench](demo-out/testb.h)
    driver that would match multiple on-board clocks, and even a main Verilator
    [test bench module](demo-out/main_tb.cpp) that will declare and call any
    C++ simulators associated with the various clocks and clock rates at the
    appropriate time in your simulation.

- Addresses get assigned in three groups, and processed in three groups:
  components having only one address, components having more addresses but
  only a single clock delay, and all other components and memories

- Multiple bus support is now included, to include creating arbiters to
  transition from one bus to the next, as well as keeping track of what the
  final addresses are for devices on an upper level bus.

  This makes it possible for the SDRAM to be on one bus, supporting video
  reads/writes, and for the CPU to be able to access that bus as
  well--as a sub-bus of the main peripheral/memory bus.


- Interrupts get assigned to a named controller, and then C++ header files are
  updated to reflect the interrupt assignments

- A simple integer mathematical expression evaluator exists, allowing simple
  math expressions and print formats.  This makes it possible to set a global
  clock frequency value, and to then set baud rates and other clock dividers
  from it.

- The auto builder now creates a master C++ Verilator simulation class
  file, calls any verilator simulation support routines, and organizes these
  by whever clock rate they are placed upon.

- A [Makefile include](demo-out/rtl.make.inc) is created for the Verilog
  (RTL) directory, so as to
  facilitate the modifications necessary for integrating the new component into
  the verilator simulation build

  o Discovering the --MMD Verilator option has since rendered the dependency
    work largely unnecessary

- Only one type of address building is supported.  I'd like to be able to
  support others, but so far this has been sufficient for my first project.

  o Likewise, the project only supports WB B4/pipelined.  No support is
    provided for WB B3/classic (yet), although creating such support shoud not
    be difficult at all.

- AutoFPGA now builds a [ZipCPU Linker Script](demo-out/board.ld) for the
  project

- The LaTeX specification table building isn't there ... yet.

# Specific Test Case

A specific test case that I'm trying to support is one that will build a system
for [Digilent's](https://store.digilentinc.com) [Nexys-4
Video](https://store.digilentinc.com/nexys-video-artix-7-fpga-trainer-board-for-multimedia-applications/), with not
only a 100MHz ZipCPU on board accessing a 32-bit
wishbone bus, but also a 100MHz DDR3 SDRAM existing on its own 128-bit wide
wishbone bus, together with two HDMI components (148.5MHz for 1080p) that will
also write to and then read from this 128-bit bus.  The DDR3 wishbone bus will
then go through a bridge to become an AXI4 bus.  The video test case I am
hoping to support is 1080p at 60Hz, which should use up over 50% of the memory
bandwidth on the device.

# Sample component files

Component files now exist for many of the components I've been using regularly.
These include: 
a [Flash](auto-data/flash.txt) controller,
[block RAM](auto-data/bkram.txt),
a [UART console](auto-data/wbuart.txt),
a very simple [GPIO controller](auto-data/gpio.txt),
[RMII ethernet controller](auto-data/enet.txt),
[MDIO ethernet control interface](auto-data/mdio.txt),
a [GPS UART and PPS-driven internal clock](auto-data/gps.txt), 
a [Real-Time (GPS driven) Clock](auto-data/rtcgps.txt),
a [PS/2 Mouse](auto-data/wbmouse.txt),
an [OLED component](auto-data/wboledbw.txt),
and more.  (I'm currently working on the SDRAM ...)
Many of these component cores exist and have their own repositories elsewhere.
For example, the wishbone UART core may be found
[here](https://github.com/ZipCPU/wbuart32).
Building the cores themselves is not a part of this project, but rather
figuring out how to compose multiple cores into a top level design from
both cores and component descriptions.

# License

AutoFPGA is designed for release under the GPLv3 license.

# Commercial Applications

Should you find the GPLv3 license insufficient for your needs, other licenses
can be purchased from Gisselquist Technology, LLC.

Likewise, please contact us should you wish to guide, direct, or otherwise
fund the development of this project.  You can contact me at my user name,
dgisselq, at the wonderful ieee.org host.

