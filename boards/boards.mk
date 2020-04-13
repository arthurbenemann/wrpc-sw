obj-$(CONFIG_TARGET_GENERIC_PHY_8BIT) += boards/generic/board.o
obj-$(CONFIG_TARGET_GENERIC_PHY_16BIT) += boards/generic/board.o
obj-$(CONFIG_TARGET_WR_SWITCH) += boards/wr-switch/main.o boards/wr-switch/gpio-wrs.o boards/wr-switch/ad9516.o
obj-$(CONFIG_TARGET_AFCZ) += boards/afcz/board.o
obj-$(CONFIG_TARGET_ERTM14) += boards/ertm14/board.o boards/ertm14/ertm15_rf_distr.o boards/ertm14/phy_calibration.o boards/ertm14/rf_frame_transceiver.o

