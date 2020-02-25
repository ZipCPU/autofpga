## Defining clocks

The first item you'll want to define within your design is a clock.  You may
define more than one in your design, and design elements may use them at
will, but you may only define one clock within any component.  For this
reason, I typically create a component for each of my clock definitions.

```text
@CLOCK.NAME=clk
@CLOCK.WIRE=i_clk
@CLOCK.FREQUENCY=100000000
```

This defines a clock known to AutoFPGA as `clk`.  Within your `main.v`
design file it will be known as `i_clk`.

## Associating a reset

Often clock domains also have a reset associated with them.  For this reason,
AutoFPGA allows the definition of an associated reset.

```text
@CLOCK.RESET=i_reset
```

This is primarily a convenience definition that you can use if you wish.
It may also be used by the bus composer, by any design whose bus doesn't
have an associated reset.  This will then be used as the reset for that bus,
otherwise AutoFPGA may generate an error that the bus has no associated
reset wire.

## Default clock and reset

The clock named `i_clk` is special.
It is automatically passed to the `main` module for you.  To make this work,
you'll need a clock in your top level named `s_clk`, that will then be
forwarded directly to your main module.

From the top level, AutoFPGA's generated code will look like,

```verilog
module toplevel(i_clk, /* ... */);
   input wire i_clk;

   /* Your code here */

   main themain(s_clk, s_reset, ...
```

For this reason, you'll want to define a clock named `s_clk` in your
top level.  You'll also want to define `s_reset`.  These can be as simple
as defining these two wires,

```text
@TOP.DEFNS=
    wire s_clk, s_reset;
```

and then adding the `TOP.INSERT` tag to your clock definition,

```text
@TOP.INSERT=
    assign s_clk = i_clk;
    assign s_reset = 1'b0;
```

If your external clock needs to have a top level port assigned to it, then
you'll want to use the `CLOCK.TOP` tag to specify its name.  For example,
the following defines a 50MHz input clock, transformed via logic to become the
50MHz top level clock.  (Normally I'd recommend using a PLL, but due to a
mistake during board design, the PLL wasn't available for this pin.)

```text
@PREFIX=clk
@CLOCK.TOP=i_clk
@CLOCK.NAME=clk
@CLOCK.RESET=i_reset
@TOP.DEFNS=
	reg	clk_50mhz;
	wire	s_clk, s_reset;
@TOP.INSERT=
	assign	s_reset = 1'b0;

	initial	clk_50mhz = 1'b0;
	always @(posedge i_clk)
		clk_50mhz <= !clk_50mhz;

	SB_GB global_buffer(clk_50mhz, s_clk);

@CLOCK.FREQUENCY= 50000000
```
