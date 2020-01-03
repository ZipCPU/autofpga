## Bus Declarations

Before creating any bus-based components, you'll want to declare the bus
that these components will connect to.  This is primarily done through
the `BUS.NAME`, `BUS.TYPE`, `BUS.CLOCK`, and `BUS.WIDTH` tags.

`BUS.NAME` gives the bus a short-hand name that you can then use to reference
it within other design components.

`BUS.TYPE` names the protocol used by the bus.  As of this writing, AutoFPGA
supports three bus types: `wb` for Wishbone (B4 pipeline), `axil` for
AXIr-Lite, and `axi` for the AXI4 bus types.

`BUS.WIDTH` defines the data width of the bus.

The bus address width will be defined internally based upon the minimum
address width required to address all of your components.  Once addresses
have been assigned, `BUS.AWID` may be used to reference the number of
addressing wires used and defined by this bus.  This is separate from the
per-component `AWID` tag which may be used to reference the number of
address bits used an individual component.

As with the clocks, you can define as many buses as you want within your
design.  However, only one bus can be defined per component, i.e. per
`@PREFIX` tag, lest the `@BUS` tags overwrite each other.

As an example, the following defines a 32-bit Wishbone bus,

```text
@PREFIX=mywb_definition
@BUS.NAME=mywb
@BUS.TYPE=wb
@BUS.CLOCK=clk
@BUS.WIDTH=32
```

