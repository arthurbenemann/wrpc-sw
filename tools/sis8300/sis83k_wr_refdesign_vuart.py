#!/usr/bin/python

from sis83k_wr_refdesign_driver import *

#load driver with, lun = 0 is the first board detected on PCI
drv = SIS83K_WRRefDesign_Driver(lun=0)
drv.run_wr_core_terminal()
