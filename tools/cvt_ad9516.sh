#!/bin/sh
##############################
# 
# Tools to convert from/to STP format given by AD9516 software
#
# header format:
# 	{0x0198, 0x00},
# 	{0x0199, 0x22},
# stp format:
#	"0000","00011000","18"
#	"0001","00000000","00"
#
# Copyright (c) 2013, Benoit RAT
# Authors: Benoit RAT <benoit(AT)sevensols.com>
# Licence: GPL v2 or later.
#
######################################################


help()
{
cat << EOF
Usage: $(basename $0)  <cmd> <in> <out>

cmd:
   h2stp <file>.h <file>.stp
   stp2h <file>.stp <file>.h
       
EOF
exit 1
}

### Convert to STP
h2stp()
{
	hfile=$1
	stpfile=$2

echo '"Rev.","1.1.0"
""
"Addr(Hex)","Value(Bin)","Value(Hex)"' > ${stpfile}


cat ${hfile} | grep }, |\
while read line; do
	addr=$(echo ${line} | awk -F', ' '{ print $1}' | sed 's/{0x//')
	val=$(echo ${line} | awk -F', ' '{ print $2}' | sed 's/0x\([0-9A-Fa-f]*\)},/\1/')
	bin=$(echo "ibase=16\n obase=2 \n "${val} | bc -q)
	bin=$(printf %08s $bin | sed 's/ /0/g')	
	echo "\"$addr\",\"${bin}\",\"${val}\""
	echo "\"$addr\",\"${bin}\",\"${val}\"" >> ${stpfile}
done
echo '"","",""
"Other Settings..."
"REF 1:",25
"REF 2:",0
"VCO:",1500
"CLK:",0
"CPRSet:",5100
"Auto Update:",1
"Load All Regs:",0' >> ${stpfile}

return 0
}


### Convert from STP
stp2h()
{
        stpfile=$1
        hfile=$2
if [ "x$(file $stpfile | grep CRLF)" != "x" ]; then
        echo "ERROR: Windows line ending (Must be linux)"
        return 1
fi

echo 'const struct {int reg; uint8_t val} ad9516_regs[] = {' > ${hfile}


cat ${stpfile} |\
while read line; do
        addr=$(echo ${line} | awk -F',' '{ print $1}' | sed 's/"//g')
        val=$(echo ${line} | awk -F',' '{ print $3}' | sed 's/"//g')
        check=$(echo ${val} | sed 's/\([0-9a-fA-F][0-9a-fA-F]\)/OK/')
        if [ "x${check}" = "xOK" ]; then         
                echo "{0x${addr}, 0x${val}}," >> ${hfile}
        fi
done
        echo "{-1, 0}};" >> ${hfile}
return 0
}

### Main

if [ $# -eq 3 ]; then 
	if [ "x$1" = "xh2stp" ]; then
		h2stp $2 $3
		exit $?
	elif [ "x$1" = "xstp2h" ]; then
		stp2h $2 $3
		exit $?
	fi
fi

echo "Wrong args: $@"
help
