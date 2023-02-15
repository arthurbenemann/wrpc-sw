#!/usr/bin/env python3

#
# Copyright (C) 2023 CERN
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import time
import serial
import struct
import getopt
import signal
import sys

kill_usb = False

c_BOOT_TIMEOUT = 3
c_BOOT_ENTER_ATTEMPTS = 3

def signal_handler(sig, frame):
        global kill_usb
        print('You pressed Ctrl+C!')
        kill_usb = True
        sys.exit(0)

class SerialIF:
    def __init__(self, device="/dev/ttyUSB0",baudrate=115200):
        self.ser = serial.Serial(
            port=device, baudrate=baudrate, timeout=0, rtscts=False)

    def reset_board(self):
        print ("Resetting...")
        for k in range(0,12):
            self.ser.setDTR(True)
            self.ser.setDTR(False)
        
    def send(self, x):
        if isinstance(x, int):
            self.ser.write(struct.pack("B", x))
        else:
            self.ser.write(x)

    def recv(self,timeout=None):
        t_start = time.time()
        fail = False
        while True:

            try:
                state = self.ser.read(1)
                if( state != None ):
                    return ord(state)
            except:
                fail = True
                pass

            if fail or state == None or len(state) == 0:
                #print(time.time(), t_start)
                if timeout != None and time.time() - t_start > timeout:
                    raise TimeoutError()

    def recv_nonblock(self):
        try:
            state = self.ser.read(1)
            return ord(state)
        except:
            return None

    def crc_xmodem_update(self, crc, data):
        crc = crc ^ (data << 8)

        for i in range(0, 8):
            if ((crc & 0x8000) != 0):
                crc = (crc << 1) ^ 0x1021
                crc &= 0xffff
            else:
                crc = crc << 1
                crc &= 0xffff

        return crc

    def crc_xmodem(self, data):
        crc = 0
        for c in data:
            crc = self.crc_xmodem_update(crc, c)
        return crc

    def rx_frame(self,timeout=None):
        frame = []

        t_start = time.time()

        while True:
            if timeout != None and time.time() - t_start > timeout:
                raise TimeoutError()
            b = self.recv(timeout)
            if (b != 0x55):
                continue
            b = self.recv(timeout)
            if (b != 0xaa):
                continue
            break

        frame = [0x55, 0xaa]

        rsp = self.recv(timeout)
        l = self.recv(timeout)
        l <<= 8
        l |= self.recv(timeout)

        for i in range(0, l):
            frame.append(self.recv(timeout))

        crc = self.recv(timeout)
        crc <<= 8
        crc |= self.recv(timeout)

        return (rsp, frame[2:])

    def tx_frame(self, command, data):

        frame = []
        frame.append(0x55)
        frame.append(0xaa)
        frame.append(command)
        frame.append((len(data) >> 8) & 0xff)
        frame.append(len(data) & 0xff)

        for b in data:
            frame.append(b)

        crc = self.crc_xmodem(frame)

        frame.append((crc >> 8) & 0xff)
        frame.append((crc & 0xff))

        for b in frame:
            self.send(b)


class DSIBootloader:
    def __init__(self, device="/dev/ttyUSB0",baudrate=115200,target_board=None):
        self.sock = SerialIF(device,baudrate)
        self.target_board=target_board

    CMD_WRITE_RAM = 4
    CMD_FLASH_ERASE_SECTOR = 2
    CMD_FLASH_PROGRAM_PAGE = 3
    CMD_GO = 5
    CMD_BOOT_INIT = 1
    CMD_ENTER_BOOT_MODE = 8
    CMD_RESET_PAYLOAD = 9

    RSP_OK = 1
    RSP_CRC_ERROR = 2
    RSP_VERIFY_ERROR = 3
    RSP_BAD_CRC = 4
    RSP_HELLO = 5

    def command(self, cmd, data, expect_response=True):
        while True:
            self.sock.tx_frame(cmd, data)
            if( not expect_response ):
                return 0
            status = self.sock.rx_frame(timeout = c_BOOT_TIMEOUT)[0]

            if (status != self.RSP_CRC_ERROR):
                break
            time.sleep(0.1)
        return status

    def cmd_boot_init(self):
        return self.command(self.CMD_BOOT_INIT, [])

    def cmd_reset_to_boot_mode(self):
        return self.sock.tx_frame(self.CMD_ENTER_BOOT_MODE, [])

    def cmd_reset_mmc_payload(self):
        return self.sock.tx_frame(self.CMD_RESET_PAYLOAD, [])

    def cmd_read_flash_id(self):
        data = [(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
                addr & 0xff]
        return self.command(self.CMD_FLASH_ERASE_SECTOR, data)


    def cmd_erase_sector(self, addr):
        data = [(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
                addr & 0xff]
        return self.command(self.CMD_FLASH_ERASE_SECTOR, data)

    def cmd_program_page(self, addr, data):
        buf = [(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
               addr & 0xff]
        for b in data:
            buf.append(b)
        return self.command(self.CMD_FLASH_PROGRAM_PAGE, buf)

    def cmd_write_ram(self, addr, data):
        buf = [(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
               addr & 0xff]
        for b in data:
            buf.append(b)
#	print(len(buf))
        return self.command(self.CMD_WRITE_RAM, buf)

    def cmd_jump(self, addr, expect_response=True):
        buf = [(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
               addr & 0xff]
        return self.command(self.CMD_GO, buf,expect_response=expect_response)

    def boot_enter(self):
        attempts_left = c_BOOT_ENTER_ATTEMPTS

        while True:
            self.sock.reset_board()
            self.cmd_reset_to_boot_mode()
            r = None
            try:
                r = self.sock.rx_frame( c_BOOT_TIMEOUT )
            except TimeoutError:
                print("No 'Hello' message received. Let's try again... [%d attempts left]" % attempts_left)
                if attempts_left > 0:
                    attempts_left-=1
                    pass
                else:
                    raise Exception("The bootloader is not responding (timeout exceeded)")
            if r != None:
                break

        if r[0] != self.RSP_HELLO:
            raise Exception("The bootloader responded with an incorrect 'Hello' message. Probably a broken bitstream. Bring the board to Tom.")

        board_id=""
        if len( r[1] ) < 8:
            board_id="default"
            print("Assuming default board ID. Old WRCore bootloader?")
        else:
            for i in range(0,8):
                c = r[1][i]
                if( c == 0 ):
                    break;
                board_id += chr(c)

        print("Board ID: %s" % board_id)

        if board_id != self.target_board:
            raise Exception('Board identity mismatch. Expected: "%s", got: "%s"' % (self.target_board, board_id))

        self.cmd_boot_init()
        #self.cmd_jump( 0x0,expect_response=False )

    SECTOR_SIZE_SPI = 0x10000
    SECTOR_SIZE_MMC = 0x800
    PAGE_SIZE = 0x100
    
    def program_ertm14_mmc(self, fw, target):
        if target.lower() == "mcu":
            offset = 0x8000000
            image = fw
        elif target.lower() == "fru":
            offset = 0x10000000
            image = fw
        else:
            raise Exception("Unknown flash target: %s" % target)
        self.do_program_flash(image, offset, sector_size=0x400)
        print("Flash programmed, launching the code...")
        self.cmd_jump( 0x0,expect_response=False )

    def program_ertm14_wrc(self, fw, target):
        if target.lower() == "fpga":
            offset = 0
            image = fw
        elif target.lower() == "wrc":
            offset = 0x300000
            image = struct.pack(">LLL", 0xf1dee41a, len(fw), 0xe000b800) + fw[4:]
            #print(type(fw), len(fw), len(image))
        elif target.lower() == "autoexec":
            offset = 0x610000
            image = struct.pack(">H", len(fw)) + fw
        elif target.lower() == "sdbfs":
            offset = 0x600000
            image = fw
        else:
            raise Exception("Unknown flash target: %s" % target)
        return self.do_program_flash(image, offset)


    def reset_payload(self, target):
        if self.target_board == "ertm14m0":
            print("Resetting eRTM14 payload")
            return self.cmd_reset_mmc_payload()

    def program_flash(self, fw, target):
        #print("PGM", target)
        if self.target_board == "ertm14m0" or self.target_board == "ertm14m1":
            return self.program_ertm14_mmc(fw, target)
        elif self.target_board == "ertm14fp" or self.target_board == "default":
            return self.program_ertm14_wrc(fw, target)

    def do_program_flash(self, fw, offset = 0, sector_size = 0x10000):
        remaining = len(fw)

        for i in range( offset // sector_size,
                       (offset + (remaining + sector_size - 1)) // sector_size):
            sys.stdout.write("\rErasing sector 0x%x          " %
                             (i * sector_size))
            sys.stdout.flush()
            self.cmd_erase_sector(i * sector_size)

        p = 0
        while (remaining > 0):
            n = 256 if remaining > 256 else remaining
            data = []
            for b in fw[p:p + n]:
                data.append(b)

            #print("b0 %x" % data[0])

            self.cmd_program_page(p + offset, data)
            p += n
            remaining -= n

            sys.stdout.write("\rProgramming: %.0f%%           " %
                             (float(p) / len(fw) * 100.0))
            sys.stdout.flush()

        print(
            "\nFlashing complete"
        )

        self.cmd_jump( 0x4 )

    def load_ram(self, image, addr):
        remaining = len(image)

        p = 0
        while (remaining > 0):
            n = 256 if remaining > 256 else remaining
            data = []
            for b in image[p:p + n]:
                data.append(b)


#	    print("addr %x l %d" %( p+addr, len(data)))

            self.cmd_write_ram(addr + p, data)
            p += n
            remaining -= n
            sys.stdout.write("\rLoading: %.0f%%           " %
                             (float(p) / len(image) * 100.0))
            sys.stdout.flush()

        self.cmd_jump(addr + 0x4)
        print("\nBoot done..")


def run_terminal(ser):
    '''
    Opens an interactive terminal session (similar to PuTTY/minicom) bound
    to the VUART
    '''
    import os, termios, sys
    
    def restore_terminal_state():
        import termios

        if old_settings:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    new = termios.tcgetattr(fd)

    # iflag
    new[0] = termios.IGNPAR
    # oflag
    new[1] = 0
    # cflag
    new[2] = termios.B9600 | termios.CS8 | termios.CLOCAL | termios.CREAD
    new[3] = 0

    new[6][termios.VMIN] = 0  # cc
    new[6][termios.VTIME] = 0  # cc
    termios.tcsetattr(fd, termios.TCSANOW, new)

    print('[Press Ctrl-A to terminate session.]')

    import atexit
    atexit.register(restore_terminal_state)

    while True:
        a = ser.recv_nonblock()
        if (a != None):
            sys.stderr.write(chr(a))
            if(chr(a) == '\n'):
                sys.stderr.write('\r')

        a = os.read(sys.stdin.fileno(), 1)
        if a and (ord(a) == 1 or ord(a) == 4):
            return      # exit on Ctrl-A
        else:
            ser.send(a)


def short_help_and_bugger_off():
    print('The WRCore Serial Bootloader\n-----------------------------\n')
    print('No options given. Type %s --help for help.' % sys.argv[0])
    sys.exit(2)

def main(argv):
    #signal.signal(signal.SIGINT, signal_handler)

    our_port = "/dev/ttyUSB0"
    do_flash = False
    run_term = False
    do_reset = False
    flash_target = None
    board_target = None
    ser_speed=115200
    try:
        opts, args = getopt.getopt(argv[1:], "hrb:f:s:p:t", ["uart"])
    except getopt.GetoptError:
        short_help_and_bugger_off()
    for opt, arg in opts:
        if opt == '-h':
            print('Usage: %s -b board [-f partition] filename.bin\n' % argv[0])
            print('Options:')
            print(
                '-b / --board board     - selects target board. Available choices are:')
            print('             ertm14fp  : eRTM14 FPGA')
            print('             ertm14m0  : eRTM14 MMC')
            print('             ertm14m1  : eRTM15 MMC')
            print('             default   : assume eRTM14 FPGA, use only with old LM32 bootloader\n')
            print(
                '-f / --flash partition - selects the FPGA/MMC flash partition to program:'
            )
            print('             fpga      : FPGA bitstream (MAY BRICK YOU BOARD IF FLASHED WITH WRONG BITSTREAM, IN CASE OF DOUBT CALL TOM!)')
            print('             autoexec  : FPGA WRCore autoexec script')
            print('             wrc       : FPGA WRCore CPU binary')
            print("             sdbfs     : FPGA WRCore filesystem image (YOU WILL LOSE CALIBRATION IF YOU DO IT, SO DON'T DO IT IF YOU'RE NOT TOM!)")
            print("             mcu       : MMC ARM payload")
            print("             fru       : MMC FRU info")
            print(
                '-r / --reset           - asks the MMC to reset the payload FPGA (eRTM14)'
            )
            print(
                '-p / --port:           - specifies the serial port device (default: %s)'
                % our_port)
            print(
                '-t / --term:           - runs a serial terminal on the specified port after programming')
            print(
                '-s / --speed:          - sets serial port speed (default: 921600)\n')
            
            sys.exit()
        elif opt in ("-b", "--board"):
            board_target = arg
        elif opt in ("-f", "--flash"):
            flash_target = arg
            do_flash = True
        elif opt in ("-p", "--port"):
            our_port = arg
        elif opt in ("-s", "--speed"):
            ser_speed = int(arg)
        elif opt in ("-t", "--term"):
            run_term = True
        elif opt in ("-r", "--reset"):
            do_reset = True
        else:
            print("Unrecognized option '%s'" % opt)

    if len(args) == 0 and not do_reset:
        short_help_and_bugger_off()

    if board_target == None:
        print("Please specify the target board")
        sys.exit(2)

    if (board_target == "ertm14m0" or board_target == "ertm14m1") and not do_flash:
        print("Sorry, eRTM boards cannot be booted straight to RAM. Please select the flash partition to program using the --flash switch.")
        sys.exit(2)
    

    try:
        boot = DSIBootloader(our_port, target_board=board_target,baudrate=ser_speed)
    except serial.serialutil.SerialException as e:
        print("could not open {} at speed {}, check permissions".format(
            board_target, ser_speed), file=sys.stderr)
        exit(1)

    try:
        if not do_reset:
            fw = bytearray(open(args[0], "rb").read())

        if not do_reset:
            boot.boot_enter()


        if do_reset:
            boot.reset_payload(flash_target)
        elif do_flash:
            boot.program_flash(fw, flash_target)
        else:
            boot.load_ram(fw, 0x0)

        if run_term:
            run_terminal(boot.sock)
    except Exception as e:
        print("Exception occured: %s" % str(e))
        sys.exit(-1)


if __name__ == "__main__":
    main(sys.argv)
