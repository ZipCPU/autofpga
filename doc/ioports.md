## Getting Access to External I/O Ports

Accessing external I/O has a couple parts.  First, you'll want to declare your
need for accessing external I/O in the first place.  For example, a serial
port (uart) might have an input and an output port.  Since AutoFPGA (currently)
uses the older Verilog portlist syntax, defining these ports comes in two
parts.  First, you'd define the `MAIN.PORTLIST` tag, listing all of the ports
in your design, and then the `MAIN.IODECL` tag declaring the various ports.
Further, to avoid name collision, I typically use the `@$(PREFIX)` tag as
part of any component I/O names--it's convenient, although not technically
required.

```text
@PREFIX=uart
@NADDR=4
@SLAVE.BUS=wb
@SLAVE.TYPE=DOUBLE
@MAIN.PORTLIST=
	// Serial port wires
	i_@$(PREFIX)_uart, o_@$(PREFIX)_uart
@MAIN.IODECL=
	input	wire	i_@$(PREFIX)_uart;
	output	wire	o_@$(PREFIX)_uart;
@MAIN.INSERT=
	wbuart @$(PREFIX)i (i_clk,
		@$(SLAVE.PORTLIST),
		i_@$(PREFIX)_uart, o_@$(PREFIX)_uart);
```

By default, these ports will automatically be promoted to the top level.
To keep from promoting them to the top level, or to adjust their names and
definitions at the top level, simply override the `TOP.PORTLIST` and/or
`TOP.IODECL` tags.
