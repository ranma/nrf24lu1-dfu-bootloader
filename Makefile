ifndef BOOT_START

all: logitech generic-16k generic-32k

logitech:
	mkdir -p bin-$@
	$(MAKE) -C bin-$@ -f $(CURDIR)/Makefile BOOT_START=0x6c00 BOOT_END=0x7400

generic-16k:
	mkdir -p bin-$@
	$(MAKE) -C bin-$@ -f $(CURDIR)/Makefile BOOT_START=0x3800 BOOT_END=0x4000 LED_GPIO=3

generic-32k:
	mkdir -p bin-$@
	$(MAKE) -C bin-$@ -f $(CURDIR)/Makefile BOOT_START=0x7800 BOOT_END=0x8000 LED_GPIO=2

clean:
	rm -f bin-*/*

else

SDCC ?= sdcc
SDLD = sdld
SDAS = sdas8051
CFLAGS = --model-large --std-c99
CFLAGS += -DBOOT_START=$(BOOT_START) -DBOOT_END=$(BOOT_END)
CFLAGS += -DFLASH_START=0x0000 -DFLASH_END=$(BOOT_START)
ifdef LED_GPIO
CFLAGS += -DLED_GPIO=$(LED_GPIO)
endif
# LDFLAGS set up for running from 2KB sram
LDFLAGS = -nmuwxMY -b CSEG=0x8000 -b XSEG=0x8780 -I 0x100 -C 0x780 -X 0x80
ASFLAGS = -ols
VPATH = ../src/
OBJS = main.rel usb.rel usb_desc.rel startup.rel

SDCC_VER := $(shell $(SDCC) -v | grep -Po "\d\.\d\.\d" | sed "s/\.//g")

all: sdcc bootloader.bin

sdcc:
	@if test $(SDCC_VER) -lt 310; then echo "Please update SDCC to 3.1.0 or newer."; exit 2; fi

bootloader.bin: $(OBJS)
	$(SDLD) $(LDFLAGS) -i bootloader.ihx $(OBJS:%=%)
	objcopy -I ihex bootloader.ihx -O binary bootloader.bin
	objcopy --pad-to 59382 --gap-fill 255 -I ihex bootloader.ihx -O binary bootloader.padded.bin
	objcopy --pad-to 35839 --gap-fill 255 -I ihex bootloader.ihx -O binary bootloader.bootimg.bin
	/bin/echo -en "\xf1" >> bootloader.bootimg.bin
	/bin/echo -en "\x02\x00\x08\x32\xff\xff\xff\xff" > bootloader.formatted.bin
	cat bootloader.padded.bin >> bootloader.formatted.bin
	objcopy -I binary bootloader.formatted.bin -O ihex bootloader.formatted.ihx

%.rel: %.s
	$(SDAS) $(ASFLAGS) $@ $<

%.rel: %.c
	$(SDCC) $(CFLAGS) -c $< -o $@

%.asm: %.c
	$(SDCC) $(CFLAGS) -S $< -o $@

endif
