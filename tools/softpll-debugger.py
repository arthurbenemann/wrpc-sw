#!/usr/bin/python

import matplotlib.pyplot as plt
import numpy as np
import sys
import time
import serial
import struct
import getopt
import signal
import sys
import socket
kill_usb = False

def signal_handler(sig, frame):
        global kill_usb
        print('You pressed Ctrl+C!')
        kill_usb = True
        sys.exit(0)

class SPLLProxyConnection:
    def __init__(self, host="192.168.100.101",port=12345):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((host,port))

    def recv_u32(self):
        return struct.unpack("<I", self.socket.recv(4))

    def done(self):
        return False

class SPLLLogFile:
    def __init__(self, filename):
        self.f = open(filename,"rb")
        self.f.seek(-1,2)     # go to the file end.
        self.eof = self.f.tell()   # get the end of file location
        self.f.seek(0,0)     # go to the file end.


    def recv_u32(self):
        d=self.f.read(4)
        if len(d) == 0:
            return None
        return struct.unpack("<I",d)

    def done(self):
#        print("T %d eof %d" % ( self.f.tell(), self.eof ))
        return self.f.tell() + 3 >= self.eof



import threading
def sign_extend(value, bits):
    sign_bit = 1 << (bits - 1)
    return (value & (sign_bit - 1)) - (value & sign_bit)

class SPLLSample:
    EVT_START = 1
    EVT_LOCKED = 2
    EVT_GAIN_SWITCH = 3

    DBG_Y = 0
    DBG_ERR = 1
    DBG_TAG = 2
    DBG_REF = 5
    DBG_PERIOD = 3
    DBG_SAMPLE_ID = 6
    DBG_GAIN = 7

    DBG_LOOP_ID_MASK = 0x38
    DBG_LOOP_ID_SHIFT = 3
    DBG_TAG_TYPE_MASK = 0x7
    DBG_TAG_TYPE_SHIFT = 0

    DBG_EVENT = 0x40
    DBG_HELPER = (6<<3)
    DBG_EXT = (7<<3)
    DBG_MAIN = 0x0


    def __init__(self,id=0):
        self.is_event = False
        self.event_id = 0
        self.m_y = None
        self.h_y = None
        self.m_ref = None
        self.m_tag = None
        self.m_err = None
        self.h_err = None
        self.m_gain_stage = None
        self.id = id
        self.loop = None

    
    def parse(self, x):
        loop_lut=["main","aux0","aux1","aux2","aux3","aux4","helper","ext"]
        event_lut = { }
        hdr = x >> 24
        loop_id = (hdr & self.DBG_LOOP_ID_MASK) >> self.DBG_LOOP_ID_SHIFT
        #print("loopid %d" % loop_id)
        tag_type = (hdr & self.DBG_TAG_TYPE_MASK) >> self.DBG_TAG_TYPE_SHIFT
        is_last = True if hdr & 0x80 else False
        is_event = True if hdr & 0x40 else False
        self.loop = loop_lut[loop_id]
        value = x & 0xffffff;

        if self.loop in ["main", "aux0", "aux1", "aux2", "aux3", "aux4"]:
            if tag_type == self.DBG_Y:
                self.m_y = value
            elif tag_type == self.DBG_GAIN:
                self.m_gain_stage = value * 10000
            elif tag_type == self.DBG_ERR:
                self.m_err = sign_extend(value,24)
            elif tag_type == self.DBG_REF:
                self.m_ref = value
            elif tag_type == self.DBG_TAG:
                self.m_tag = value
        elif self.loop == "helper":
            if tag_type == self.DBG_Y:
                self.h_y = value
            elif tag_type == self.DBG_ERR:
                self.h_err = sign_extend(value,24)
       # elif ( what & self.DBG_EVENT ):
        #    self.is_event = True
         #   evt_str = ""
          #  if (what & 0x30) == self.DBG_HELPER:
           #     evt_str="helper"
            #elif (what & 0x30) == self.DBG_MAIN:
                #evt_str="main"
            #evt_str+="-"
            #if(value & 0xf) == self.EVT_START:
                #evt_str+="start"
            #elif(value & 0xf) == self.EVT_LOCKED:
                #evt_str+="locked"
            #elif(value & 0xf) == self.EVT_GAIN_SWITCH:
                #evt_str+="gain-switch"
            #self.event_id =evt_str
#            print("Event %x" % self.event_id)
        return is_last


class LoggerThread(threading.Thread):
    def __init__(self, port, acq_time):
        threading.Thread.__init__(self)
        self.port = port
        self.samples=[]
        self.finish = False
        self.n_samples = 0
        self.acq_time = acq_time

    def run(self):
        print("Logger started\n")
        nsamples = 0
        self.n_samples = nsamples

        nsamples = 0
        s = SPLLSample(nsamples)
        while not self.finish and not self.port.done():
                #print("Done %d" % self.port.done() )
                x = self.port.recv_u32()
                #if x & 0xff000000 == 0x20000000:
                    #sys.stdout.write("* ")
                #print("%08x" % x)
                if( x!=None and s.parse(x[0]) ):
                    self.samples.append(s)
                    s = SPLLSample(nsamples)
                    nsamples+=1
                    self.n_samples = nsamples

    def get_samples(self):
        return self.samples

    def get_samples_count(self):
        return self.n_samples

    def kill(self):
        self.finish = True

def tag_dfun(x, wrap = 1<<22):
    y=[]
    for i in range(0, len(x)-1):
        t=x[i]
        if( t > x[i+1] ):
            t-=wrap
        y += [ x[i+1]-t ]
    y+=[0]
    return y

def main(argv):
    signal.signal(signal.SIGINT, signal_handler)

    if len(argv) <= 0:
        print ("eRTM14 SoftPLL debugger - a tool for recording live SoftPLL response.\n")
        print ("Usage: %s usb_port_device [acq_time] [restart_pll]" % argv[0])
        print ("Where:")
        print (" - acq_time: optional time (in seconds) of data acquisition, default = 10s")
        print (" - restart_pll: when 1, the PLL is restarted prior to acquisition (default = 0)")
        print ("")
        print ("WARNING: This is an internal toy of WRPC developers. Don't ask for any support or documentation.")
        return

    acq_time = 30 if len(argv) <= 2 else int(argv[2])
    restart_pll = 1 if len(argv) <= 3 else int(argv[3])

#    port = SPLLProxyConnection()
    port = SPLLLogFile( "/mnt/ssh/spll.bin" )

    meas_thread = LoggerThread(port, acq_time)
    meas_thread.start()

    #while True:
        #n = len( meas_thread.get_samples() )
        #print("Acquired %d samples\n" % n )
        #if n > 100:
            #break
        #time.sleep(0.2)
    
    while meas_thread.is_alive():
        sys.stdout.write('Acquiring data (got %d samples)         \r' %( meas_thread.get_samples_count() ) )
        time.sleep(0.5)
    
    meas_thread.join()

    samples = meas_thread.get_samples()
    n = len(samples)
    print(n)
    ht=[]
    hy=[]
    herr=[]
    mt=[]
    my=[]
    merr=[]
    events=[]
    mref=[]
    mtag=[]
    mstage=[]
    aux0t=[]
    aux0y=[]
    aux0err=[]
    aux0ref=[]
    aux0tag=[]
    aux0stage=[]
    for s in samples:
        if s.loop == "helper":
            ht.append( s.id )
            hy.append( s.h_y )
            herr.append( s.h_err )
        if s.loop == "main":
            mt.append( s.id )
            my.append( s.m_y )
            mref.append( s.m_ref )
            mtag.append( s.m_tag )
            merr.append( s.m_err )
            mstage.append( s.m_gain_stage )
        if s.loop == "aux0":
            aux0t.append( s.id )
            aux0y.append( s.m_y )
            aux0ref.append( s.m_ref )
            aux0tag.append( s.m_tag )
            aux0err.append( s.m_err )
            


        if s.is_event:
            events.append(s)

    aux0ref=[]
    aux0tag=[]
    mref=[]
    mtag=[]
    for s in samples:
        if s.loop=="aux0" and s.m_ref != None:
            print("REF %d" % s.m_ref)
            aux0ref.append( s.m_ref )

        if s.loop=="aux0" and s.m_tag != None:
            print("OUT %d" % s.m_tag)
            aux0tag.append( s.m_tag )
        if s.loop=="main" and s.m_ref != None:
            print("REFM %d" % s.m_ref)
            mref.append( s.m_ref )
        if s.loop=="main" and s.m_tag != None:
            print("OUTM %d" % s.m_tag)
            mtag.append( s.m_tag )

    #print(my)
#    print (map( lambda s : s.h_y, samples ))
 #   plt.plot(t, map( lambda s : s.h_y, samples ) )
    #plt.plot(ht, hy, label = "helper[y]")
    #plt.plot(ht, herr, label = "helper[err]")
    #plt.plot(mt, my, label = "main[y]")
    #plt.plot(mt, merr, label = "main[err]")
    #plt.plot( (mref), label = "m[tag_ref]")
    #plt.plot( (mtag), label = "m[tag_out]")
    #
    plt.plot(aux0t, aux0y, label = "aux0[y]")
    plt.plot(aux0t, aux0err, label = "aux0[err]")
    plt.plot( (aux0ref), label = "aux0[tag_ref]")
    plt.plot( (aux0tag), label = "aux0[tag_out]")

    #plt.plot(mt, mstage, label = "main[stage]")
    #plt.plot(mt, mref, label = "main[int]")

    #plt.plot(mt, mref, label = "main[ref tag]")
    #plt.plot(mt, mtag, label = "main[fb tag]")
    plt.legend();

#    for e in events:
 #       print("PROCEVT %s" % e.event_id)
  #      plt.text(e.id, 0, e.event_id, rotation="vertical")



    plt.grid()
    plt.show()


if __name__ == "__main__":
    main(sys.argv)
