# the normal SDK and the RTOS SDK
SDKBASE  := /home/flip/sandbox/esp-open-sdk
RTOSBASE := /home/flip/sandbox/esp-open-rtos

# we need the xtensa compiler and the esptool in the path
PATH        := $(SDKBASE)/xtensa-lx106-elf/bin:$(SDKBASE)/esptool:$(PATH)

# build config
PROGRAM         = blink
PROGRAM_SRC_DIR = ./src
PROGRAM_INC_DIR = ./src

#EXTRA_CFLAGS    = -Wall -Wpadded -Wextra -Wstrict-prototypes -Wenum-compare -Wswitch-enum -Wpointer-arith -Wundef -Wno-implicit-function-declaration

# ESP8266 config
ESPPORT         = /dev/ttyUSB0
ESPBAUD         = 1500000
FLASH_MODE      = dout
FLASH_SIZE      = 16
FLASH_SPEED     = 40

include $(RTOSBASE)/common.mk
