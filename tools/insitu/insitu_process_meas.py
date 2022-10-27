#!/usr/bin/python

"""
insitu_process_meas.py: calculates delayCoefficient from data outut by "insitu meas" command

-------------------------------------------------------------------------------
Copyright (C) 2022 Peter Jansweijer
    
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
-------------------------------------------------------------------------------

Usage:
  insitu_process_meas.py     -name output_file_txt.tx -l1 -l2 -lc
  insitu_process_meas.py     -h | --help

Options:
  -h --help    Show this screen.
  -l1          lambda 1 [nm]
  -l2          lambda 2 [nm]
  -lc          lambda common [nm]

"""

import os
import sys
from fractions import Fraction
import numpy as np
import matplotlib.pyplot as plt

############################################################################

def calc_delayCoefficient(l1, l2, lc, rtd1, rtd2, tunable='ttR'):
  """
  Calculates the delay coefficient.

  In IEEE1588-2019:
  delayCoefficient (alpha) is defined in 7.4.3 (unit "RelativeDifference"
  [unit-less], see 8.2.17.3)
  
  parameters:
  l1                 -- <np.float64> lambda 1
  l1                 -- <np.float64> lambda 2
  l1                 -- <np.float64> lambda common
  rtd1               -- <np.float64> round trip time 1
  rtd2               -- <np.float64> round trip time 2
  tunable            -- <str> either ttT (tunable timeTransmitter) or ttR (tunable timeReceiver)

  returns:
  delayCoefficient   -- <numpy.float64> according to array of wavelengths or ITU channels
  """

  if tunable == 'ttT':
    # For a fixed wavelength lc from asymTimeReceiver to asymTimeTransmitter (tunable timeTransmitter)
    delayCoefficient = (2*(l1-lc)*(rtd1-rtd2))/(rtd1*(l1-l2)-(rtd1-rtd2)*(l1-lc))
  else:
    #For a fixed wavelength lc from asymTimeTransmitter to asymTimeReceiver (tunable timeReceiver)
    delayCoefficient = (2*(l1-lc)*(rtd1-rtd2))/(rtd1*(l2-l1)-(rtd1-rtd2)*(l1-lc))

  return(delayCoefficient)

############################################################################

def delayCoefficient_convert(delayCoefficient):
  """
  Converts delay coefficient into IEEE1588 int64 format
  
  IEEE1588-2019 8.2.17.3: The data type of asymmetryCorrectionPortDS.scaledDelayCoefficient
  is RelativeDifference (see 5.3.11); thus, it is divided by 2+62 to obtain the fractional
  value of <delayCoefficient> (α). The specified range allows for storing values of
  <delayCoefficient> from (–2^+1 + 2^-62) to (2^+1 – 2^-62), inclusive.
  The delayCoefficient_H and L return the 9 digit + 9 digit int64 notation
  
  parameters:
  delayCoefficient   -- <numpy.float64> according to array of wavelengths or ITU channels

  returns:
  delayCoefficient_H -- <numpy.int64>   high 9 digits of delayCoefficient * 2**62
  delayCoefficient_L -- <numpy.int64>   low  9 digits of delayCoefficient * 2**62
  """

  delayCoefficient_int64 = np.int64(delayCoefficient * 2**62)
  delayCoefficient_H = np.int64(delayCoefficient_int64/1000000000)
  delayCoefficient_L = abs(delayCoefficient_int64 - delayCoefficient_H*1000000000)

  return(delayCoefficient_H, delayCoefficient_L)

############################################################################


###############################################
# Main
###############################################

"""
Usage:
  insitu_process_meas.py     -name output_file_txt.tx -l1 -l2 -lc
  insitu_process_meas.py     -h | --help

Options:
  -h --help    Show this screen.
  -l1          lambda 1 [nm]
  -l2          lambda 2 [nm]
  -lc          lambda common [nm]

"""

if __name__ == "__main__":
  
  import argparse
  parser = argparse.ArgumentParser()
  parser.add_argument("name", help="file containing round trip delay values", default="output_file.txt.tx")
  parser.add_argument("l1",       help="lambda 1 [nm]")
  parser.add_argument("l2",       help="lambda 2 [nm]")
  parser.add_argument("lc",       help="lambda common [nm]")
  args = parser.parse_args()
  name = args.name
  l1 = np.float64(args.l1)
  l2 = np.float64(args.l2)
  lc = np.float64(args.lc)
  print("Used input file: ",name)
  print("lambda 1:        ", l1)
  print("lambda 2:        ", l2)
  print("lambda common:   ", lc)

  # ref_offset can be a number for a Round Trip Delay
  # that results form a reference measurement

  #ref_offset = 445.371e-9
  ref_offset = 0
  tunable = 'ttR'

  if os.path.exists(name) == True and os.path.isfile(name) == True:
    data_file = open(name, "r")
    wrdata_file = open(name+".delayCoefficients","w")

    delayCoefficient_l1_lst = []
    delayCoefficient_l2_lst = []

    curr_meas = 0
    curr_ch = 0
    rtd_lst=[]
    ch1_mean_rtd=[]
    ch2_mean_rtd=[]
    set_index = 0

    while True:
      # Read log file and break on eof or "insitu done"
      line = data_file.readline()

      if not line:
        break
      fields = line.split()
      if len(fields) == 0:
        continue

      if fields[0] == "measurement" or (fields[0] == "insitu" and fields[1] == "done"):
        wrdata_file.write(line)
        # before continuing with a next measurement or break by "insitu done"
        # we possibly need to do a calculation over the last gathered rtd's
        if len(rtd_lst) != 0:
          rtd_arr = np.array(rtd_lst)
          print("measurement", curr_meas, "channel", curr_ch, "rtd mean", rtd_arr.mean())
          # does this measurment belong to the lambda 1 or lambda 2?
          if set_index == 0:
            ch1_mean_rtd= rtd_arr.mean()
            set_index = 1
          else:
            ch2_mean_rtd = rtd_arr.mean()
            set_index = 0
            # after lambda 2 completion we have a lambda 1, lambda 2 set of averaged round trip delays.
            # calculate the delay coefficients for lambda 1 and lambda 2
            delayCoefficient_l1 = calc_delayCoefficient(l1, l2, lc, ch1_mean_rtd, ch2_mean_rtd, tunable)
            delayCoefficient_l2 = calc_delayCoefficient(l2, l1, lc, ch2_mean_rtd, ch1_mean_rtd, tunable)
            # store them so we are able to claculate an average
            delayCoefficient_l1_lst.append(delayCoefficient_l1)
            delayCoefficient_l2_lst.append(delayCoefficient_l2)

            # convert float notation into int64 notation
            delayCoefficient_l1_H, delayCoefficient_l1_L = delayCoefficient_convert(delayCoefficient_l1)
            delayCoefficient_l2_H, delayCoefficient_l2_L = delayCoefficient_convert(delayCoefficient_l2)
            print("delayCoefficient_l1", delayCoefficient_l1,delayCoefficient_l1_H,delayCoefficient_l1_L)
            print("delayCoefficient_l2", delayCoefficient_l2,delayCoefficient_l2_H,delayCoefficient_l2_L)
      
            wrdata_file.write(str(delayCoefficient_l1) + " " + str(delayCoefficient_l1_H) + " " + str(delayCoefficient_l1_L) + "\n")

        if fields[0] == "insitu" and fields[1] == "done":
          break

        curr_meas = int(fields[1])
        curr_ch = int(fields[3])
        # (re_)start with a clean round trip delay measurement list
        rtd_lst=[]

      if fields[0] == "t1t2:":
        t1f = Fraction(fields[1])
        t2f = Fraction(fields[2])
        t21f = t2f-t1f

      if fields[0] == "t3t4:":
        t3f = Fraction(fields[1])
        t4f = Fraction(fields[2])
        t43f = t4f-t3f
        try:
          # may fail if t4t3 is the first in the file
          rtd = float((t43f+t21f))
          wrdata_file.write(str(rtd)+str("\n"))
          rtd_lst.append(rtd - ref_offset)  # correct for a reference offset
        except NameError:
          pass

    # now, all measurments are read, averaged and calculated into delayCoefficients
    delayCoefficient_l1_arr = np.array(delayCoefficient_l1_lst)
    delayCoefficient_l2_arr = np.array(delayCoefficient_l2_lst)

    wrdata_file.write("----------------------------------------------\n")
    if (tunable == 'ttR'):
      wrdata_file.write("Tunebale timeReceiver\n")
    else:
      wrdata_file.write("Tunebale timeTransmitter\n")
    
    wrdata_file.write("Lambda 1: " + str(l1) + " Lambda 2: " + str(l2) + " Lambda common: " + str(lc) +"\n")

    # report the mean and standard deviation of the calculated delayCoefficients
    # of each lamda1, 2 measurement set
    delayCoefficient_l1_mean = delayCoefficient_l1_arr.mean()
    delayCoefficient_l1_stdv = delayCoefficient_l1_arr.std(ddof=1)
    wrdata_file.write("delayCoefficient_l1 mean: " + str(delayCoefficient_l1_mean) + "\n")
    wrdata_file.write("delayCoefficient_l1 std:  " + str(delayCoefficient_l1_stdv) + "\n")

    delayCoefficient_l1_H, delayCoefficient_l1_L = delayCoefficient_convert(delayCoefficient_l1_mean)
    wrdata_file.write("delayCoefficient_l1_H: " + str(delayCoefficient_l1_H) + "\n")
    wrdata_file.write("delayCoefficient_l1_L: " + str(delayCoefficient_l1_L) + "\n")
      
    wrdata_file.write("----------------------------------------------\n")
    delayCoefficient_l2_mean = delayCoefficient_l2_arr.mean()
    delayCoefficient_l2_stdv = delayCoefficient_l2_arr.std(ddof=1)
    wrdata_file.write("delayCoefficient_l2 mean: " + str(delayCoefficient_l2_mean) + "\n")
    wrdata_file.write("delayCoefficient_l2 std:  " + str(delayCoefficient_l2_stdv) + "\n")

    delayCoefficient_l2_H, delayCoefficient_l2_L = delayCoefficient_convert(delayCoefficient_l2_mean)
    wrdata_file.write("delayCoefficient_l2_H: " + str(delayCoefficient_l2_H) + "\n")
    wrdata_file.write("delayCoefficient_l2_L: " + str(delayCoefficient_l2_L) + "\n")

    data_file.close()
    wrdata_file.close()

    # plot delayCoefficients for quick verification of outliers
    delayCoefficient_plt = plt.figure("delayCoefficients versus measurement-set number")
    ax = delayCoefficient_plt.add_subplot(111)
    ax.set_title("delayCoefficient measurements")
    ax.set_xlabel('measurement')
    ax.set_ylabel('delayCoefficient')
    ax.text(0.01, 0.95, 'min: ' + '{:1.5e}'.format(min(delayCoefficient_l1_arr)), transform=ax.transAxes)
    ax.text(0.01, 0.90, 'max: ' + '{:1.5e}'.format(max(delayCoefficient_l1_arr)), transform=ax.transAxes)
    ax.text(0.01, 0.85, 'mean:' + '{:1.5e}'.format((delayCoefficient_l1_arr).mean()), transform=ax.transAxes)
    ax.text(0.01, 0.80, 'std: ' + '{:1.5e}'.format((delayCoefficient_l1_arr).std(ddof=1)), transform=ax.transAxes)
    ax.plot(delayCoefficient_l1_arr, color='blue', label='lambda 1 delayCoefficient')
    ax.legend(loc='lower right', fontsize='medium')
    delayCoefficient_plt.subplots_adjust(left=0.17)
    plt.draw()
    plt.show()
  else:
    print("file", args.name ,"not found")

  sys.exit()

