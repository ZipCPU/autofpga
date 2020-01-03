## Creating a Generic Slave

Now that you know how to create `SINGLE` and `DOUBLE` slaves, generic
slaves really aren't that much more difficult.  These are typically of type
`OTHER`, and just reference your design.

```text
@PREFIX=wbslave
@SLAVE.BUS=wb
@SLAVE.TYPE=OTHER
@SLAVE.NADDR=32
@MAIN.INSERT=
	busslave @$(PREFIX)i (i_clk, i_reset,
		@$(SLAVE.PORTLIST));
```

Unlike `SINGLE` or `DOUBLE` slaves which are not allowed to stall the bus,
`OTHER` slaves can exploit the full bus protocol.
