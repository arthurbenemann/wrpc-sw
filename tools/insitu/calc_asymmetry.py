#!/usr/bin/python

"""
calc_asymmetry.py: calculates the delayAsymmetry and delayCoefficient.

-------------------------------------------------------------------------------
Copyright (C) 2022 Nikhef, Peter Jansweijer
    
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
  calc_asymmetry.py     -l1 -l2 -lc -rtd1 -rtd2
  calc_asymmetry.py     -h | --help

Options:
  -h --help    Show this screen.
  -l1          lambda 1 [nm]
  -l2          lambda 2 [nm]
  -lc          lambda common [nm]
  -rtd1        round trip delay measured for lambda 1 [s]
  -rtd2        round trip delay measured for lambda 2 [s]

In IEEE1588-2019:
delayAsymmetry is defined in 7.4.2 (unit "TimeInterval" [s], see 8.2.15.4.8)
delayCoefficient (alpha) is defined in 7.4.3 (unit "RelativeDifference" [unit-less], see 8.2.17.3)
The delayCoefficient is reported in fractional notation, in IEEE1588 int64 notation and in
split int64H, int64L notation as is used in White Rabbit devices.

Note that unit for lambda cancels out in the equations. So lambda may be given in
nano-meter or meter. 
"""

import sys
import numpy as np
import pdb

###############################################
# Main
###############################################

if __name__ == "__main__":
  
  import argparse
  parser = argparse.ArgumentParser()
  parser.add_argument("l1", help="lambda 1 [nm]")
  parser.add_argument("l2", help="lambda 2 [nm]")
  parser.add_argument("lc", help="lambda common [nm]")
  parser.add_argument("rtd1", help="round trip delay measured for lambda 1 [s]")
  parser.add_argument("rtd2", help="round trip delay measured for lambda 2 [s]")
  args = parser.parse_args()
  l1 = np.float64(args.l1)
  l2 = np.float64(args.l2)
  lc = np.float64(args.lc)
  rtd1 = np.float64(args.rtd1)
  rtd2 = np.float64(args.rtd2)
  print("lambda 1:               ", l1)
  print("lambda 2:               ", l2)
  print("lambda common:          ", lc)
  print("round trip delay 1 [s]: ", rtd1)
  print("round trip delay 2 [s]: ", rtd2)
  print()
  
  """
  IEEE1588-2019 8.2.17.3:
  The data type of asymmetryCorrectionPortDS.scaledDelayCoefficient is
  RelativeDifference (see 5.3.11); thus, it is divided by 2+62 to obtain
  the fractional value of <delayCoefficient> (α).
  The specified range allows for storing values of <delayCoefficient>
  from (–2^+1 + 2^-62) to (2^+1 – 2^-62), inclusive.
  """

  # For a fixed wavelength lc from asymTimeReceiver to asymTimeTransmitter (tunable timeTransmitter)
  ttT_delayAsymmetry_l1 = ((l1-lc)*(rtd1-rtd2))/(2*(l1-l2))
  ttT_delayCoefficient_l1 = (2*(l1-lc)*(rtd1-rtd2))/(rtd1*(l1-l2)-(rtd1-rtd2)*(l1-lc))

  #For a fixed wavelength lc from asymTimeTransmitter to asymTimeReceiver (tunable timeReceiver)
  ttR_delayAsymmetry_l1 = (-(l1-lc)*(rtd1-rtd2))/(2*(l1-l2))
  ttR_delayCoefficient_l1 = (2*(l1-lc)*(rtd1-rtd2))/(rtd1*(l2-l1)-(rtd1-rtd2)*(l1-lc))

  print("### tunable timeTransmitter ###")
  print("ttT_delayAsymmetry_l1 [s]:      ", ttT_delayAsymmetry_l1)
  print("ttT_delayCoefficient_l1:        ", ttT_delayCoefficient_l1)

  ttT_int64_delayCoefficient_l1 = np.int64(ttT_delayCoefficient_l1 * 2**62)
  ttT_int64H_delayCoefficient_l1 = np.int64(ttT_int64_delayCoefficient_l1/1000000000)
  ttT_int64L_delayCoefficient_l1 = abs(ttT_int64_delayCoefficient_l1- ttT_int64H_delayCoefficient_l1*1000000000)
  print("ttT_int64_delayCoefficient_l1:  ", ttT_int64_delayCoefficient_l1)
  print("ttT_int64H_delayCoefficient_l1: ", ttT_int64H_delayCoefficient_l1)
  print("ttT_int64L_delayCoefficient_l1: ", ttT_int64L_delayCoefficient_l1)
  print()

  #pdb.set_trace()
  
  print("### tunable timeReceiver ###")
  print("ttR_delayAsymmetry_l1 [s]:      ", ttR_delayAsymmetry_l1)
  print("ttR_delayCoefficient_l1:        ", ttR_delayCoefficient_l1)

  ttR_int64_delayCoefficient_l1 = np.int64(ttR_delayCoefficient_l1 * 2**62)
  ttR_int64H_delayCoefficient_l1 = np.int64(ttR_int64_delayCoefficient_l1/1000000000)
  ttR_int64L_delayCoefficient_l1 = abs(ttR_int64_delayCoefficient_l1- ttR_int64H_delayCoefficient_l1*1000000000)
  print("ttR_int64_delayCoefficient_l1:  ", ttR_int64_delayCoefficient_l1)
  print("ttR_int64H_delayCoefficient_l1: ", ttR_int64H_delayCoefficient_l1)
  print("ttR_int64L_delayCoefficient_l1: ", ttR_int64L_delayCoefficient_l1)
  
  sys.exit()

