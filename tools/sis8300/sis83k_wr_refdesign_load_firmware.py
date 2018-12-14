#!/usr/bin/python

import sys
from sis83k_wr_refdesign_driver import *

#load driver with, lun = 0 is the first board detected on PCI

if len(sys.argv) < 2:
    print("usage: %s file_name.bin" % sys.argv[0])
    
drv = SIS83K_WRRefDesign_Driver(lun=0)
print("Loading '%s' into the WR Core CPU..." % sys.argv[1])
drv.load_wr_core_firmware(sys.argv[1])
