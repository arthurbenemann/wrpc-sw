#!/bin/bash

#development script
#update paths accordingly

RISCV_TOOLCHAIN=<path to>/riscv32-elf-
HOST_TOOLCHAIN=<path to>/aarch64-openwrt-linux-musl-
        
if [ $# -eq 1 ] 
then
    if [ $1 = "clean" ]
    then
        echo "cleaning"
        make clean
    elif [ $1 = "afcz" ]
    then	    
        echo "building for afcz"
				make wr_switch_v4_on_afcz_defconfig
        make CROSS_COMPILE=$RISCV_TOOLCHAIN CROSS_COMPILE_TARGET_HOST=$HOST_TOOLCHAIN
    elif [ $1 = "wrsv4" ]
    then
	echo "building for wrsv4"
        make wr_switch_v4_proto_defconfig
				make CROSS_COMPILE=$RISCV_TOOLCHAIN CROSS_COMPILE_TARGET_HOST=$HOST_TOOLCHAIN
    fi 
else
    echo "not enough arguments"
		echo "usage: $(basename $0) [target]"
		echo "afcz : wrsv4 development platform"
		echo "wrsv4 : wrsv4 prototype board"
    echo "clean : clean targets"
fi
