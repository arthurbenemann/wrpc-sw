#!/usr/bin/python

import PyUAL
import struct
import time
import sys
import atexit
import os


'''
The Virtual UART (VUART driver)

Emulates a serial port connected to an embedded CPU in the FPGA
'''

class VUARTDriver:
    REG_HOST_RDR   = 0x014
    HOST_RDR_READY = 0x100

    REG_BCR        = 0x004
    REG_HOST_TDR   = 0x010

    '''
    Creates a VUART instance.
    @param ual: the UAL object representing the bus the VUART is connected to
    @param base: base address of the VUART
    '''
    def __init__(self, ual, base):
        self.ual = ual
        self.base = base
        self.old_settings = None


    '''
    Reads a byte from the VUART. Non-blocking. Returns None if nothing was
    read.
    '''
    def read(self):
        rdr = self.ual.readl(self.base + self.REG_HOST_RDR);
        if (rdr & self.HOST_RDR_READY):
            return chr(rdr & 0xff)
        else:
            return None

    '''
    Writes a byte to the VUART. Non-blocking.
    '''
    def write(self, value):
        self.ual.writel(self.base + self.REG_HOST_TDR, value);

    '''
    Opens an interactive terminal session (similar to PuTTY/minicom) bound
    to the VUART
    '''
    def open_session(self):
        import termios, sys
        fd = sys.stdin.fileno()
        self.old_settings = termios.tcgetattr(fd)
        new = termios.tcgetattr(fd)

        # iflag
        new[0] = termios.IGNPAR
        # oflag
        new[1] = 0
        # cflag
        new[2] = termios.B9600 | termios.CS8 | termios.CLOCAL | termios.CREAD
        new[3] = 0

        new[6][termios.VMIN] = 0  # cc
        new[6][termios.VTIME] = 0 # cc
        termios.tcsetattr(fd, termios.TCSANOW, new)

        print ("[Press Ctrl-A to terminate session.]")

        import atexit
        atexit.register( self.restore_terminal_state )

        while True:
            a = self.read()
            if(a):
                sys.stderr.write("%c"%a)

            a = os.read(sys.stdin.fileno(), 1)
            if a != None and len(a) > 0:
                if(ord(a)==4): # ctrl-d press
                    return
                else:
                    self.write(ord(a))

    def restore_terminal_state( self ):
        import termios

        if self.old_settings:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)

class WRCoreDriver:
    BASE_MEM    = 0x00000
    BASE_SYSCON = 0x20400
    BASE_VUART  = 0x20500	    
    BASE_AUXWB  = 0x20700

    def __init__(self, ual, base):
        self.ual = ual
        self.base = base
        self.vuart = VUARTDriver(self.ual, self.base + self.BASE_VUART)

    def load_firmware( self, filename ):
        try:
            image = open(filename,"rb").read()
        except IOError:
            print ("Can't open: '%s'" % filename)
            return -1

        syscon_addr = self.base + self.BASE_SYSCON

        # reset the core CPU
        self.ual.writel(syscon_addr, 0x1deadbee)
        while self.ual.readl(syscon_addr) & (1<<28) == 0:
            pass

        offset = 0
        nwords = (len(image)+3)/4
        for i in range(0, nwords, 128):
            count = min (nwords - i, 128)
            for j in range(0, count):
                d = struct.unpack(">I", image[(i+j)*4:(i+j)*4+4])[0]
                self.ual.writel(self.base + self.BASE_MEM + offset, d)
                offset += 4

        # start the CPU
        self.ual.writel(syscon_addr, 0x0deadbee)
        return 0
		
    def vuart_session(self):		
        vuart = VUARTDriver( self.ual, self.base + 0x40 )		
        vuart.open_session()		
    def read_streamers(self, offset):		
        return self.ual.readl(self.base + self.BASE_AUXWB + offset)		
    def write_streamers(self, offset, data):		
    	self.ual.writel(self.base + self.BASE_AUXWB + offset, data)		
    	return 0

class SIS83K_WRRefDesign_Driver:
    BASE_WR_CORE         = 0x00000000

    def __init__(self, lun=0):
        self.lun = lun
        desc_regs = PyUAL.PyUALPCI(search_by_vendor_id=1, vendor_id=0x10dc, device_id=0x8301, lun=lun, bar=0, size=0x100000,offset=0)                                                                             
        self.ual  = PyUAL.PyUAL("pci", desc_regs)                       
        self.wr_core   = WRCoreDriver(self.ual, self.BASE_WR_CORE)

    def write_reg( self, addr, value ):
        return self.ual.writel(addr, value);

    def read_reg( self, addr ):
        return self.ual.readl(addr);

    def run_wr_core_terminal(self):
        self.wr_core.vuart.open_session()

    def load_wr_core_firmware(self, filename):
        self.wr_core.load_firmware(filename)
