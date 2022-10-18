#!/usr/bin/python3

# Author: Adam Wujek, 2022

import sys
from fractions import Fraction

count = 0

if len(sys.argv) < 2:
    print("Error not enough parameters: " + str(len(sys.argv)))
print("open file: " + sys.argv[1])

file_t1 = open(sys.argv[1] + '.t1', 'w')
file_t3 = open(sys.argv[1] + '.t3', 'w')
file_tx = open(sys.argv[1] + '.tx', 'w')

t21f_sum = Fraction(0)
t21f_cnt = 0
t43f_sum = Fraction(0)
t43f_cnt = 0


with open(sys.argv[1]) as fp:
    while True:
        count += 1
        line = fp.readline()

        if not line:
            break
        fields = line.split()
        if len(fields) == 0:
            continue

        if fields[0] == "t1t2:":
            t1f = Fraction(fields[1])
            t2f = Fraction(fields[2])
            t21f = t2f-t1f
            file_t1.write(fields[1]+" "+fields[2]+" "+str(float(t21f % 1))+str("\n"))

        if fields[0] == "t3t4:":
            t3f = Fraction(fields[1])
            t4f = Fraction(fields[2])
            t43f = t4f-t3f
            file_t3.write(fields[1]+" "+fields[2]+" "+str(float(t43f % 1))+str("\n"))
            try:
                # may fail if t4t3 is the first in the file
                file_tx.write(str(float(t21f % 1))+" "+str(float(t43f % 1))+" "+str(float(t43f+t21f))+str("\n"))
            except NameError:
                pass
