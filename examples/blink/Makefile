APPLICATION = blink

BOARD ?= bluepill-128kib
PORT = /dev/ttyUSB0

FBLINK ?= 10

#PROGRAMMER = wiring

RIOTBASE ?= $(CURDIR)/../..

USEMODULE += shell
USEMODULE += xtimer

DEVELHELP ?= 1

QUIET ?= 1

FEATURES_REQUIRED += periph_gpio

CFLAGS += "-DBLINK_HZ=$(FBLINK)"

include $(RIOTBASE)/Makefile.include
