
# Those hardware-specific files should not be built for the host, even if
# most of them give no error no warning. The host has different implementations
obj-$(CONFIG_LM32) += 	dev/simple_uart.o dev/console.o

obj-$(CONFIG_EMBEDDED_NODE) += \
	dev/endpoint.o \
	dev/ep_pfilter.o \
	dev/minic.o \
	dev/syscon.o \
	dev/sfp.o \
	dev/devicelist.o \
	dev/rxts_calibrator.o \
	dev/flash.o \
	dev/fram.o \
	dev/gpio.o \
	dev/bb_spi.o \
	dev/bb_i2c.o \
	dev/24aa025.o \
	dev/74x595.o \
	dev/ad7888.o \
	dev/ad951x.o \
	dev/ad9910.o \
	dev/clock_monitor.o \
	dev/spi_flash.o \
	dev/iuart.o \
	dev/ltc6950.o \
	dev/ad9520.o \
	dev/i2c_eeprom.o \
	dev/storage.o \
	dev/fine_pulse_generator.o

obj-$(CONFIG_WR_NODE) += \
	dev/temperature.o \
	dev/pps_gen.o

obj-$(CONFIG_TARGET_WR_SWITCH) += dev/timer-wrs.o dev/gpio.o
obj-$(CONFIG_PUTS_SYSLOG) += dev/puts-syslog.o

#obj-$(CONFIG_LEGACY_EEPROM) += dev/eeprom.o
#obj-$(CONFIG_SDB_STORAGE) += dev/sdb-storage.o

obj-$(CONFIG_DAC_LOG) += dev/dac_log.o
obj-$(CONFIG_W1) +=		dev/w1.o	dev/w1-hw.o	dev/w1-shell.o
obj-$(CONFIG_W1) +=		dev/w1-temp.o
obj-$(CONFIG_W1) +=		dev/temp-w1.o

obj-$(CONFIG_FAKE_TEMPERATURES) += dev/fake-temp.o

# Filter rules are selected according to configuration, but we may
# have more than one. Note: the filename is reflected in symbol names,
# so they are hardwired in ../Makefile (and ../tools/pfilter-builder too)

dev/ep_pfilter.o: $(pfilter-y)

dev/storage.o: $(sdbfsimg-y)

$(pfilter-y): tools
	./tools/pfilter-builder include/dev/

$(sdbfsimg-y): tools
	./tools/gensdbfs -c include/dev/sdbfs-default.h tools/sdbfs tools/sdbfs-default.bin
