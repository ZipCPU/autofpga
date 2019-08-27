# AutoFPGA - An FPGA Design Automation routine

This particular branch of AutoFPGA is designed with a couple of new features
to it:

1. It supports natural multiple-master insertion, via a full bus crossbar
2. This is the first version supporting multiple bus types.  Currently, this
   is only WB (pipelined) and AXI-lite.  It should be possible to create
   an AXI implementation from this with just a little bit of cut/copy and
   paste to the [bus/axil.cpp](sw/bus/axil.cpp) file..

# License

AutoFPGA is designed for release under the GPLv3 license.
