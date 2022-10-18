#!/usr/bin/env -S /usr/bin/gnuplot -c

# Author: Adam Wujek, 2022

LOGFILE=ARG1
print "Using log file     : ", LOGFILE

output_scale = 4
set output LOGFILE.".t2t1t4t3.png"
if (!exists("scale_x")) scale_x=640
if (!exists("scale_y")) scale_y=480
set terminal pngcairo size scale_x*output_scale,scale_y*output_scale enhanced font "arial,10" fontscale output_scale linewidth output_scale
set title "(t2-t1)+(t4-t3)"
set ylabel "ns"
set ytics format "%.3f"
set xlabel "sample"


plot LOGFILE u ($3*1000000000) notitle
