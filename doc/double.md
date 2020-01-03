# Double Slave Definitions

AutoFPGA defines two special classes of slaves, `SINGLE` and `DOUBLE`.  These
classes are used to help AutoFPGA coalesce bus logic together, while
simplifying your design.

The `DOUBLE` slave describes a slave that may have many registers, that
never stalls, and whose register values may be read with a single
clock cycle delay.  It's almost as simple as the `SINGLE` slave to
create.

```text
@PREFIX=wb_double_example
@SLAVE.NADDR=2
@SLAVE.BUS=wbbus
@SLAVE.TYPE=DOUBLE
@MAIN.DEFNS=
	reg	[31:0]	r_@$(PREFIX)_0, r_@$(PREFIX)_b, r_@$(PREFIX)_rdata;
@MAIN.INSERT=
	initial	r_@$(PREFIX)_0 = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_stb && !@$(SLAVE.PREFIX)_addr[0])
		r_@$(PREFIX)_0 <= @$(SLAVE.PREFIX)_data;
	else if (r_@$(PREFIX)_0 > 0)
		r_@$(PREFIX)_0 <= r_@$(PREFIX)_0 - 1;

	initial	r_@$(PREFIX)_1 = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_stb && @$(SLAVE.PREFIX)_addr[0])
		r_@$(PREFIX)_1 <= @$(SLAVE.PREFIX)_data;
	else if (r_@$(PREFIX)_1 > 0)
		r_@$(PREFIX)_1 <= r_@$(PREFIX)_1 - 1;

	always @(posedge i_clk)
	case (@$(SLAVE.PREFIX)_addr[0])
	1'b0:	r_@$(PREFIX)_data <= r_@$(PREFIX)_0;
	1'b1:	r_@$(PREFIX)_data <= r_@$(PREFIX)_1;
	endcase

	assign @$(SLAVE.PREFIX)_idata = r_@$(PREFIX)_rdata;
```

This will create your slave and connect it to the bus named `wbbus`.  Any
attempt to read from your slave will return the value `r_@$(PREFIX)_rdata`,
which was itself calculated from the bus address given to it one clock prior.

Building this same example for an AXI or AXI-Lite bus is almost the
same, only that you also need to define values for `@$(SLAVE.PREFIX)_bresp`
and `@$(SLAVE.PREFIX)_rresp`.  Further, AXI slave addresses are octet based,
whereas Wishbone addresses are word based.  Hence we'll check awaddr[2]
instead of addr[0].

```text
@PREFIX=wb_double_example
@SLAVE.NADDR=2
@SLAVE.BUS=axilbus
@SLAVE.TYPE=DOUBLE
@MAIN.INSERT=
	initial	r_@$(PREFIX)_0 = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_awvalid && !@$(SLAVE.PREFIX)_awaddr[2])
		r_@$(PREFIX)_0 <= @$(SLAVE.PREFIX)_data;
	else if (r_@$(PREFIX)_0 > 0)
		r_@$(PREFIX)_0 <= r_@$(PREFIX)_0 - 1;

	initial	r_@$(PREFIX)_1 = 0;
	always @(posedge i_clk)
	if (@$(SLAVE.PREFIX)_stb && @$(SLAVE.PREFIX)_addr[2])
		r_@$(PREFIX)_1 <= @$(SLAVE.PREFIX)_data;
	else if (r_@$(PREFIX)_1 > 0)
		r_@$(PREFIX)_1 <= r_@$(PREFIX)_1 - 1;

	always @(posedge i_clk)
	case (@$(SLAVE.PREFIX)_addr[0])
	1'b0:	r_@$(PREFIX)_data <= r_@$(PREFIX)_0;
	1'b1:	r_@$(PREFIX)_data <= r_@$(PREFIX)_1;
	endcase

	assign @$(SLAVE.PREFIX)_rdata = r_@$(PREFIX)_rdata;
```

When creating an AXI or AXI-Lite `DOUBLE` slave, AutoFPGA will use one of the
bus simplifiers found in the [WB2AXIP](https://github.com/ZipCPU/wb2axip)
repository, either
[AXIDOUBLE](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axisingle.v) or
[AXILDOUBLE](https://github.com/ZipCPU/wb2axip/blob/master/rtl/axilsingle.v).
These drivers will guarantee for you that the respective `xREADY` lines remain
high, and that `AWVALID == WVALID` (and more).  All you therefore need to do
is to check `WVALID` to know when to adjust your counter.

For more information about these bus drivers, please see their internal
documentation.

As with the `SINGLE` slaves, AutoFPGA defines a `@SLAVE.PORTLIST` tag that
expands into a list of all of your design ports.  This makes it easier to
design an AXI slave from a submodule.

```text
@MAIN.INSERT=
	mymodule @$(PREFIX)i (i_axi_aclk, i_axi_aresetn,
		@$(SLAVE.PORTLIST));
```
