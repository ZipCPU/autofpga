# Single Slave Definitions

AutoFPGA defines two special classes of slaves, `SINGLE` and `DOUBLE`.  These
classes are used to help AutoFPGA coalesce bus logic together, while
simplifying your design.

The `SINGLE` slave describes a slave with only one register, which register
can be accessed on every cycle without stalling.  It is the easiest slave
to create.

```text
@PREFIX=wbexample
@SLAVE.NADDR=1
@SLAVE.BUS=wbbus
@SLAVE.TYPE=SINGLE
@MAIN.INSERT=
	assign @$(SLAVE.PREFIX)_idata = value;
```

This will create your slave and connect it to the bus named `wb`.  Any
attempt to read from your slave will return the value `value`, having a
width specified by the `wb` bus width.  (You'll still need to define
`value` yourself somewhere within your design.)

Building this same example for an AXI or AXI-Lite bus is almost the
same, only that you also need to define values for `@$(SLAVE.PREFIX)_bresp`
and `@$(SLAVE.PREFIX)_rresp`.

```text
@PREFIX=axilexample
@SLAVE.NADDR=1
@SLAVE.BUS=axilbus
@SLAVE.TYPE=SINGLE
@MAIN.INSERT=
	assign @$(SLAVE.PREFIX)_bresp = 2'b00;
	assign @$(SLAVE.PREFIX)_rresp = 2'b00;
	assign @$(SLAVE.PREFIX)_rdata = value;
```

The neat thing about `SINGLE` slaves, is that the logic can be created
however within your design.  For example, you might decide to declare a
counter that could then be read from your design.

```text
@SLAVE.BUS=wbbus
@MAIN.DEFNS=
	reg	[31:0]	r_counter;
@MAIN.INSERT=
	initial	r_counter = 0;
	always @(posedge i_clk)
		r_counter <= r_counter + 1;

	assign	@$(SLAVE.PREFIX)_idata = r_counter;
```

Indeed, I often create a startup counter that I use within my designs--one
that just counts the number of clock ticks since startup, but that
saturates the top bit.  I can then use this to time things in startup sequences,
or for relative (30-bit) timing ever afterwards.

```text
@SLAVE.BUS=axilbus
@MAIN.DEFNS=
	reg	[31:0]	r_pwrcount;
@MAIN.INSERT=
	initial	r_pwrcount = 0;
	always @(posedge i_clk)
	if (r_pwrcount[31])
		r_pwrcount[30:0] <= r_pwrcount[30:0] + 1;
	else
		r_pwrcount <= r_pwrcount + 1;

	assign	@$(SLAVE.PREFIX)_idata = r_pwrcount;
```

We could also give our counter a value as well.  How you might do this
is somewhat dependent upon the bus protocol in question.  For example,
when using Wishbone, this would look like:

```text
@SLAVE.BUS=wbbus
@MAIN.DEFNS=
	reg	[31:0]	r_pwrcount;
@MAIN.INSERT=
	initial	r_pwrcount = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_stb && @$(SLAVE.PREFIX)_we)
		r_pwrcount <= @$(SLAVE.PREFIX)_data;
	else if (r_pwrcount[31])
		r_pwrcount[30:0] <= r_pwrcount[30:0] + 1;
	else
		r_pwrcount <= r_pwrcount + 1;

	assign	@$(SLAVE.PREFIX)_idata = r_pwrcount;
```

When creating an AXI or AXI-Lite slave, AutoFPGA will use one of the bus
simplifiers found in the [WB2AXIP](https://github.com/ZipCPU/wb2axip)
repository, either
[AXISINGLE](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axisingle.v) or
[AXILSINGLE](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axilsingle.v)
These drivers will guarantee for you that the respective `xREADY` lines remain
high.  All you therefore need to do is to check `WVALID` to know when to adjust
your counter.

```text
@SLAVE.BUS=axilbus
@MAIN.DEFNS=
	reg	[31:0]	r_pwrcount;
@MAIN.INSERT=
	initial	r_pwrcount = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_awvalid)
		r_pwrcount <= @$(SLAVE.PREFIX)_data;
	else if (r_pwrcount[31])
		r_pwrcount[30:0] <= r_pwrcount[30:0] + 1;
	else
		r_pwrcount <= r_pwrcount + 1;

	assign	@$(SLAVE.PREFIX)_rdata = r_pwrcount;
```

Ideally, the `@PREFIX` tag should be unique to your slave.  This makes it
possible to name wires uniquely between bus components, although doing
so isn't required.

```text
@PREFIX=renamed
@MAIN.DEFNS=
	reg	[31:0]	r_@$(PREFIX);
@MAIN.INSERT=
	// Follows as before from above, but now with a new name
```

Remember, the `@MAIN.INSERT` tag will be copied directly into your `main.v``
file.  Therefore, you can place anything into it you wish.  For example, you
might wish to reference a submodule of your own instead of placing your
logic within the global scope of `main.v`.  To make this easier, AutoFPGA
will define a `@SLAVE.PORTLIST` tag that expands into a list of all of
your design ports--just to make connections easier.

```text
@MAIN.INSERT=
	mymodule @$(PREFIX)i (i_clk
		@$(SLAVE.PORTLIST));
```

I like naming my modules as `@$(PREFIX)i` to avoid name contention, but this
is only a convention--it's not required at all.

The `@SLAVE.PORTLIST` tag doesn't include either clock or reset.  You can
either include these as I did above, or reference them from other names
within your design--such as `@SLAVE.BUS.CLOCK.WIRE`.

```text
@MAIN.INSERT=
	mymodule @$(PREFIX)i (@$(SLAVE.BUS.CLOCK.WIRE),
		@$(SLAVE.PORTLIST));
```

Since this can get tedious, and since my designs rarely use more than one
clock for the bus, I often just use `i_clk`.

