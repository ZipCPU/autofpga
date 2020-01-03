## Adjusting Constraint files

One of the challenges associated with adding and removing components from a
design is that the constraint files also need to be adjusted as well.
AutoFPGA provides some support for doing this across several architectures.

The basic idea is that you would declare an original constraint file
to AutoFPGA.  This needs to be done at the top level of your design, before
any PREFIX tags in a given file.

```text
XDC.FILE=../nexysv.xdc
```

AutoFPGA will then reference that file when building a new XDC file.

## XDC file support

For example, if you give AutoFPGA an `XDC.FILE` tag, as shown above,
then AutoFPGA will create a new XDC file based upon it.  Specifically,
it will uncomment any lines in your XDC file defining ports used by your
component, and it will also insert any `XDC.INSERT` tags required by your
component into the newly produced XDC file.  All you need to do then is to
maintain a master XDC file where all of the various I/O port definitions
are commented. AutoFPGA will then uncomment only the ports defined in your
`TOP.PORTLIST` tag--or alternatively your `MAIN.PORTLIST` tag if no
`TOP.PORTLIST` tag is defined.

For example, in my ethernet component, I need to define a series of false
paths.  These are only used if the ethernet component is included into the
design.  Therefore, they are placed into the `XDC.INSERT` tag.

```
@XDC.INSERT=
set_max_delay -datapath_only -from [get_cells -hier -filter {NAME=~ *netctrl/tx*}]              -to [get_cells -hier -filter {NAME=~*netctrl/n_tx*}]               8.0
set_max_delay -datapath_only -from [get_cells -hier -filter {NAME=~ *netctrl/hw_mac*}]          -to [get_cells -hier -filter {NAME=~*netctrl/txmaci/r_hw*}]        8.0
```

## Other Constraint Files

AutoFPGA has similar support for UCF and PCF files.

