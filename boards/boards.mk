obj-$(CONFIG_TARGET_GENERIC_PHY_8BIT) += boards/generic/board.o
obj-$(CONFIG_TARGET_GENERIC_PHY_16BIT) += boards/generic/board.o
obj-$(CONFIG_TARGET_WR_SWITCH) += boards/wr-switch/main.o boards/wr-switch/gpio-wrs.o boards/wr-switch/ad9516.o
obj-$(CONFIG_TARGET_AFCZ) += boards/afcz/board.o
obj-$(CONFIG_TARGET_SIS8300KU) += boards/sis8300ku/board.o
obj-$(CONFIG_TARGET_ERTM14) += boards/ertm14/board.o boards/ertm14/ertm15_rf_distr.o boards/ertm14/phy_calibration.o boards/ertm14/rf_frame_transceiver.o  boards/ertm14/cmd_ertm14.o boards/ertm14/sdbfs-custom-image.o
obj-$(CONFIG_TARGET_PXIE_FMC) += boards/pxie-fmc/board.o

boards/ertm14/sdbfs-custom-image.h: boards/ertm14/sdbfs
	./tools/gensdbfs -c boards/ertm14/sdbfs-custom-image.h boards/ertm14/sdbfs boards/ertm14/sdbfs-custom-image.bin


#		echo "DUPA"
#		sleep 10
#		#
