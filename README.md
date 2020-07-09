# AutoFPGA - An FPGA Design Automation routine

After now having built several FPGA designs, such as the
[xulalx25soc](https://github.com/ZipCPU/xulalx25soc),
[s6soc](https://github.com/ZipCPU/s6soc),
[openarty](https://github.com/ZipCPU/openarty),
[zbasic](https://github.com/ZipCPU/zbasic),
[icozip](https://github.com/ZipCPU/icozip),
and even a Basys-3 design of my own that hasn't been published, I started
recognizing that all of these designs have a lot in common.  In particular,
they all have a set of bus masters, such as the
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
the [Real-Time Clock](https://github.com/ZipCPU/rtcclock), the [Real-Time
Date](https://github.com/ZipCPU/rtcclock/blob/master/rtl/rtcdate.v), the
[Organic LED controller](https://github.com/ZipCPU/openarty/blob/master/rtl/wboled.v),
Xilinx's [Internal Configuration Access
Port](https://github.com/ZipCPU/wbicapetwo), the [wishbone
scope](https://github.com/ZipCPU/wbscope), the [GPS controlled
clock](https://github.com/ZipCPU/openarty/blob/master/rtl/gpsclock.v),
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
or even default video clock information--just by knowing the FPGA's clock rate
within your C++ environment.

Sharing information between Verilog and C++ then became one of the primary
reasons for creating AutoFPGA.  While peripheral address decoding is typically
done in some [main Verilog
file](https://github.com/ZipCPU/openarty/blob/master/rtl/main.v),
other files depend upon what this peripheral decoding is.  These other files
include the [host register definition
file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.h) (used
for debugging access), the [register naming
file](https://github.com/ZipCPU/openarty/blob/master/sw/host/regdefs.cpp), the
[software board definition
file](https://github.com/ZipCPU/openarty/blob/master/sw/zlib/board.h) used
by newlib, the [linker
script](https://github.com/ZipCPU/openarty/blob/master/sw/board/board.ld) used
by the compiler, and even the [LaTeX
specification](https://github.com/ZipCPU/openarty/blob/master/doc/src/spec.tex)
for the board.  Creating and updating all of these files by hand anytime I
create a new board file can get tedious.  Further, every time a board is
reconfigured, the constraints file, whether
[XDC](https://github.com/ZipCPU/openarty/blob/master/arty.xdc) or
[UCF](https://github.com/ZipCPU/xulalx25soc/blob/master/xula.ucf) file, needs
to be updated to match the current constraints.

Solving this multi-language coordination problem is the purpose of AutoFPGA.

Unlike many of the other tools out there, such as Xilinx's board design flow,
AutoFPGA is not built with the clueless beginner in mind, neither is it built
to hide the details of what is going within the project it creates.  Instead,
AutoFPGA is built with the sole purpose of alleviating any burden on the FPGA
designer who otherwise has to create and maintain coherency between multiple
design files.

That this program facilitates composing and building new designs from existing
components ... is just a bonus.

# Goal

The goal of AutoFPGA is to be able to take a series of bus component
configuration files and to compose a design consisting of the various bus
components, linked together in logic, having an appropriate bus interconnect
and more.

From a user's point of view, one would run AutoFPGA with a list of component
definition files, given on the command line, and to thus be able to generate
(or update?) the various design files discussed above:

- [rtl/toplevel.v](demo-out/toplevel.v)
- [rtl/main.v](demo-out/main.v)
- [rtl/make.inc](demo-out/rtl.make.inc)
- [rtl/iscachable.v](demo-out/iscachable.v) -- a function of bus address determining what addresses are cachable and which are not
- [sw/host/regdefs.h](demo-out/regdefs.h)
- [sw/host/regdefs.cpp](demo-out/regdefs.cpp)
- [sw/zlib/board.h](demo-out/board.h)
- [sw/zlib/board.ld](demo-out/board.ld)
- [build.xdc](demo-out/build.xdc) (Created by modifying an existing XDC file.  LPF, PCF, and UCF files are also supported)
- [sim/verilated/testb.h](demo-out/testb.h)
- [sim/verilated/main_tb.h](demo-out/main_tb.cpp)
- doc/src/(component name).tex (Not started yet)

Specifically, the parser must determine:

- If any of the components used in the project need to be configured, and if
  so, what the configuration parameters are and how they need to be set.  For
  example, the UART baud rate and RTC and GPS clock steps both need to be set
  based upon the actual clock speed of the master clock.  Placing [a
  clock module](auto-data/clock.txt) within the design that sets up a clock and
  declares its rate is the current method for accomplishing this.  Designs using
  more than one clock often have an
  [allclocks.txt](https://github.com/ZipCPU/openarty/autodata/allclocks.txt)
  file to define all of the various clocks used within a design.
- If peripherals have or create interrupts, those need to be found and
  determined, and (even better) wired up.

- If an AutoFPGA configuration file describes one of the following classes of
  items, then the file is wired up and connected to create the necessary bus
  wiring as well.

  * Bus masters

    Are automatically connected to a crossbar with full access to all of the
    slaves on the bus

  * One-clock Peripherals (interrupt controller, etc.)

  * Two-clock Peripherals (RTC clock, block RAM, scopes, etc.)

  * Memory Peripherals

    o These need a line within the linker script, and they need to define if
      their memory region, within that linker script, has read, write, or

    o Generic Peripherals ([flash](auto-data/flash.txt), SDRAM,
      [MDIO](auto-data/mdio.txt), etc.)

- Peripheral files need to be able to describe more than one peripheral.  For
  example, the [GPS peripheral file](auto-data/gps.txt) has a GPS-Clock,
  a companion test bench, GPS-TB, to measure the performance of the GPS clock,
  and a serial port ([WBUART](https://github.com/ZipCPU/wbuart32)) to allow us
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

Unfortunately, this only works if the included file has only one component
defined within it.

## Math

Some peripherals need to be able to perform basic integer math on a given
value to determine an appropriate setting value.  These peripherals need
access to variables.  The classic examples are the baud rate, which depends
upon the clock rate, as well as the step size necessary for the RTC and the
GPS clocks, both of which also depend upon the master clock rate.  Other
examples might include determining the size of the address space to assign
to a core based upon the memory size of the component and so forth.

This feature is currently fully supported using integer math.

## Legacy Updates

The original version of AutoFPGA supported only one bus master, one bus type,
and an interconnect with a known bug in it.

Specifically, the broken interconnect would allow a master to make requests
of one peripheral and then another before the first peripheral had responded,
while not preventing the requests from returning out of order.

Fixing this bug introduced several incompatible changes, therefore there is
an AutoFPGA `legacy` git tag defined to get back to the older version.

This newer version, however, now supports:

- Multiple bus types: [Wishbone (pipelined)](sw/bus/wb.cpp),
  [AXI-Lite](sw/bus/axil.cpp), and [AXI](sw/bus/axi.cpp)

  Additional busses may be supported by simply creating a C++ bus component
  definition class for them.

- Full crossbar support, using bus helper files from my [WB2AXIP](https://github.com/ZipCPU/wb2axip) repository.

Much to my surprise, the full crossbar support has proved to be simpler, in
terms of logic elements used, than the legacy interconnect I had been using.

# Status

This project now has several designs built around it.  These include the
[basic AutoFPGA-demo](https://github.com/ZipCPU/autofpga-demo) project,
[OpenArty](https://github.com/ZipCPU/openarty),
[ArrowZip](https://github.com/ZipCPU/arrowzip) (legacy AutoFPGA only),
[AXI DMA test bench](https://github.com/ZipCPU/axidmacheck),
[ICOZip](https://github.com/ZipCPU/icozip),
[SDR](https://github.com/ZipCPU/sdr) (a gateware defined radio),
[ZBasic](https://github.com/ZipCPU/zbasic),
[ZipStorm-mx](https://github.com/ZipCPU/zipstormmx) (legacy AutoFGPA only), and
[ZipVersa](https://github.com/ZipCPU/zipversa).  There's also a rather nice
[Nexys Video project](https://github.com/ZipCPU/videozip) that I've used for
modifying and delivering to customers, although the current version on github
is currently a touch out of date.  You can see the autogenerated logic generated
for this project in the [demo directory](demo-out/).

In sum:

- Simple bus components ... just work.  This includes both bus masters and bus
  slaves.  Not only that, the bus simplifier logic also "just works", with
  the caveat below.

  Note that the AXI SINGLE simplifier itself hasn't (yet) been built.  (It's
  waiting on a funded need.)  For now, the [AXI DOUBLE bus
  simplifier](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axidouble.v)
  should work quite well.  To use it, just declare a bus slave to be a slave
  of an AXI type bus, with SLAVE.TYPE set to SINGLE, then follow the rule
  listed in the
  [simplifier](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axidouble.v).
  The same applies to the AXI-lite simplifiers.  Wishbone simplifiers, both
  SINGLE and DOUBLE, are handled by logic inserted into `main.v`, rather
  than referenced by `main.v`.

- Components with logic in the toplevel work nicely as well.

- AutoFPGA can now support multiple, dissimilar clock rates.  Users just need
  to specify a clock to generate it.  The clock is then made available for
  configuration files to reference.  This includes creating a test bench
  wrapper for Verilator that will drive a multi-clock simulation.

- Addresses get assigned in three groups, and processed in three groups:
  simple `SINGLE` components having only one address, simple `DOUBLE`
  components having more addresses but only a single clock delay, and all
  other components and memories.

- Multiple bus support is now included, allowing you to create and attach
  components through bus adapters.  This will allow a request to transition
  from one component to the next, while also keeping track of what the final
  addresses are for reference from the top level bus.

  This makes it possible for the SDRAM to be on one bus, supporting video
  reads/writes, and for the CPU to be able to access that bus as
  well--as a sub-bus of the main peripheral/memory bus.

- Interrupts get assigned to a named controller, and then C++ header files are
  updated to reflect the interrupt assignments

- A simple integer mathematical expression evaluator exists, allowing simple
  math expressions and print formats.  This makes it possible to set a global
  clock frequency value, and to then set baud rates and other clock dividers
  from it.

- Only one type of address building is supported.  I'd like to be able to
  support others, but so far this has been sufficient for my first project.

  o Likewise, the project only supports WB B4/pipelined.  No support is
    provided for WB B3/classic (yet), although creating such support shoud not
    be difficult at all.

- AutoFPGA now builds a [ZipCPU Linker Script](demo-out/board.ld) for the
  project.  This script is highly configurable, and many of my projects contain
  configurations for multiple linker scripts--depending upon which memories
  I decide to include in the design, or which ones I want a particular piece of
  software to use.

- The LaTeX specification table building isn't there ... yet.

# Sample component files

Component files now exist for many of the components I've been using regularly.
These include: a [Flash](auto-data/flash.txt) controller,
[block RAM](auto-data/bkram.txt), a [UART console](auto-data/wbuart.txt),
a very simple [GPIO controller](auto-data/gpio.txt),
[RMII ethernet controller](auto-data/enet.txt),
[MDIO ethernet control interface](auto-data/mdio.txt),
a [GPS UART and PPS-driven internal clock](auto-data/gps.txt),
a [Real-Time (GPS driven) Clock](auto-data/rtcgps.txt),
a [PS/2 Mouse](auto-data/wbmouse.txt),
an [OLED component](auto-data/wboledbw.txt), and more.
Many of these component cores exist and have their own repositories elsewhere.
For example, the wishbone UART core may be found
[here](https://github.com/ZipCPU/wbuart32), and you can find a [MIG-based,
Wishbone controlled SDRAM component
here](https://github.com/ZipCPU/openarty/blob/master/autodata/sdram.txt).
You can also find a AXI examples, such as [AXI S2MM stream-to-memory data
mover](https://github.com/ZipCPU/axidmacheck/blob/master/autodata/axis2mm.txt),
an [AXI MM2S memory-to-stream data
mover](https://github.com/ZipCPU/axidmacheck/blob/master/autodata/axis2mm.txt),
or an [AXM block RAM
component](https://github.com/ZipCPU/axidmacheck/blob/master/autodata/axiram.txt)
in the [AXI DMA test repository](https://github.com/ZipCPU/axidmacheck).
Building the cores themselves is not a part of this project, but rather
figuring out how to compose multiple cores into a top level design from
both cores and component descriptions.

# The ZipCPU blog

Several articles have now been written to date about AutoFPGA on the ZipCPU
blog.  These includes:

1. [A brief introduction to AutoFPGA](https://zipcpu.com/zipcpu/2017/10/05/autofpga-intro.html)

2. [Using AutoFPGA to connect simple registers to a debugging bus](https://zipcpu.com/zipcpu/2017/10/06/autofpga-dataword.html)

   This article is really out of date, in that it describes only the legacy
   mode (one master, one bus type, etc.)

3. [AutoFPGA's linker script support gets an update](https://zipcpu.com/zipcpu/2018/12/22/autofpga-ld.html)

4. [Technology debt and AutoFPGA, the bill just came due](https://zipcpu.com/zipcpu/2019/08/22/tech-debt.html)

5. [Understanding AutoFPGA's address assignment algorithm](https://zipcpu.com/zipcpu/2019/09/03/address-assignment.html)

# Getting Started

The current best reference for AutoFPGA is the [icd.txt](doc/icd.txt) file,
which describes all of the various tags AutoFPGA understands and how they
can be used.  I've also started working on an [intermediate
design](https://zipcpu.com/tutorial/intermediate.html)
tutorial based around AutoFPGA, so you might find that a useful place to start
as well.

# License

AutoFPGA is designed for release under the GPLv3 license.  The AutoFPGA
generated code is yours, and free to be relicensed as you see fit.

# Commercial Applications

Should you find the GPLv3 license insufficient for your needs, other licenses
can be purchased from Gisselquist Technology, LLC.  Given that the AutoFPGA
generated code is not encumbered by any license requirements, I really don't
expect any such requests.

Likewise, please contact us should you wish to guide, direct, or otherwise
fund the development of this project.  You can contact me at my user name,
dgisselq, at the wonderful ieee.org host.

