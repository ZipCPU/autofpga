################################################################################
##
## Filename:	../demo-out/rtl.make.inc
##
## Project:	AutoFPGA, a utility for composing FPGA designs from peripherals
## {{{
## Computer Generated: This file is computer generated by AUTOFPGA. DO NOT EDIT.
## DO NOT EDIT THIS FILE!
##
## CmdLine:	./autofpga -d -o ../demo-out -I ../auto-data allclocks.txt bkram.txt buserr.txt clkcheck.txt crossbus.txt ddr3.txt edidslvscope.txt edid.txt exconsole.txt flashcfg.txt flash.txt global.txt gpio.txt gps.txt hdmi.txt i2ccpu.txt i2cdma.txt i2saudio.txt icape.txt meganet.txt mdio.txt pic.txt pwrcount.txt rtcdate.txt rtcgps.txt spio.txt sdio.txt vadj33.txt version.txt wboledbw.txt wbpmic.txt wbuarbiter.txt wbubus.txt zipcpu.txt zipmaster.txt
##
## Creator:	Dan Gisselquist, Ph.D.
##		Gisselquist Technology, LLC
##
################################################################################
## }}}
## Copyright (C) 2017-2024, Gisselquist Technology, LLC
## {{{
## This program is free software (firmware): you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published
## by the Free Software Foundation, either version 3 of the License, or (at
## your option) any later version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
## target there if the PDF file isn't present.)  If not, see
## <http://www.gnu.org/licenses/> for a copy.
## }}}
## License:	GPL, v3, as defined and found on www.gnu.org,
## {{{
##		http://www.gnu.org/licenses/gpl.html
##
##
################################################################################
##
## }}}
I2CCPUD := wbi2c
I2CCPU  := $(addprefix $(I2CCPUD)/,wbi2ccpu.v axisi2c.v)
EXBUSD := exbus
EXBUS  := $(addprefix $(EXBUSD)/,exbuswb.v excompress.v exdecompress.v exdeword.v exidle.v exmkword.v exwb.v exfifo.v)
FLASH := qflexpress.v

EDIDD := wbi2c
EDID  := $(addprefix $(EDIDD)/,wbi2ccpu.v axisi2c.v)
GPS := gpsclock_tb.v gpsclock.v bigadd.v bigsub.v bigsmpy.v

HDMID := video
HDMI  := $(addprefix $(HDMID)/,axishdmi.v axisvoverlay.v hdmi2vga.v hdmibitsync.v hdmipixelsync.v sync2stream.v synccount.v tfrstb.v tmdsdecode.v tmdsencode.v vid_empty.v vid_mux.v vidpipe.v vidstream2pix.v vid_wbframebuf.v vid_crop.v xhdmiin_deserdes.v xhdmiin.v xhdmiout.v xpxclk.v)
I2CSLVD := wbi2c
I2CSLV  := $(addprefix $(I2CSLVD)/,wbi2cslave.v)
SCOPCD := wbscope
SCOPC  := $(addprefix $(SCOPCD)/,wbscopc.v)
ICAP := wbicapetwo.v

BKRAM := memdev.v

DDR3D := ddr3
DDR3  := $(addprefix $(DDR3D)/,ddr3_controller.v ddr3_phy.v)
I2CDMAD := wbi2c
I2CDMA  := $(addprefix $(I2CDMAD)/,wbi2cdma.v)
AUDIOD := audio
AUDIO  := $(addprefix $(AUDIOD)/,axisi2s.v lli2s.v)
ENETD := ethernet
ENET  := $(addprefix $(ENETD)/,enetstream.v addecrc.v addemac.v addepad.v addepreamble.v rxecrc.v rxehwmac.v rxeipchk.v rxemin.v rxepacket.v rxepreambl.v axinwidth.v axincdc.v pktgate.v txespeed.v xiddr.v)
RTCDATED := rtc
RTCDATE  := $(addprefix $(RTCDATED)/,rtcdate.v)
RTCGPSD := rtc
RTCGPS  := $(addprefix $(RTCGPSD)/,rtcgps.v rtcbare.v rtctimer.v rtcstopwatch.v rtcalarm.v rtclight.v)
GPIO := wbgpio.v

SDIOD := sdspi
SDIO  := $(addprefix $(SDIOD)/,sdio.v sdfrontend.v sdckgen.v sdwb.v sdtxframe.v sdrxframe.v sdcmd.v sddma.v sddma_mm2s.v sddma_rxgears.v sdfifo.v sddma_txgears.v sddma_s2mm.v xsdddr.v xsdserdes8x.v)
BUSPICD := cpu
BUSPIC  := $(addprefix $(BUSPICD)/,icontrol.v)
ENETMDIOD := ethernet
ENETMDIO  := $(addprefix $(ENETMDIOD)/,enetctrl.v)
WBSPID := wbspi
WBSPI  := $(addprefix $(WBSPID)/,spicpu.v)
UPSZ := wbupsz.v

ZIPCPUD := cpu
ZIPCPU  := $(addprefix $(ZIPCPUD)/,zipsystem.v zipcore.v zipwb.v cpuops.v pfcache.v pipemem.v dblfetch.v pffifo.v pfcache.v idecode.v wbpriarbiter.v zipsystem.v zipcounter.v zipjiffies.v ziptimer.v icontrol.v wbwatchdog.v busdelay.v zipdma_ctrl.v zipdma_fsm.v zipdma_mm2s.v zipdma_rxgears.v zipdma_s2mm.v zipdma_txgears.v zipdma.v)
VFLIST := main.v  $(I2CCPU) $(EXBUS) $(FLASH) $(EDID) $(GPS) $(HDMI) $(I2CSLV) $(SCOPC) $(ICAP) $(BKRAM) $(DDR3) $(I2CDMA) $(AUDIO) $(ENET) $(RTCDATE) $(RTCGPS) $(GPIO) $(SDIO) $(BUSPIC) $(ENETMDIO) $(WBSPI) $(UPSZ) $(ZIPCPU)
AUTOVDIRS :=  -y wbi2c -y exbus -y video -y wbscope -y ddr3 -y audio -y ethernet -y rtc -y sdspi -y cpu -y wbspi
