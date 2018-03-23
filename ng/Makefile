# the normal SDK and the RTOS SDK
SDKBASE  := /home/flip/sandbox/esp-open-sdk
RTOSBASE := /home/flip/sandbox/esp-open-rtos

# we need the xtensa compiler and the esptool in the path
PATH        := $(SDKBASE)/xtensa-lx106-elf/bin:$(SDKBASE)/esptool:$(PATH)

# build config
PROGRAM         = tschenggins-laempli-ng
PROGRAM_SRC_DIR = ./src
PROGRAM_INC_DIR = ./src

# ESP8266 config
ESPPORT         = /dev/ttyUSB0
ESPBAUD         = 1500000
FLASH_MODE      = dout
FLASH_SIZE      = 16
FLASH_SPEED     = 40

include $(RTOSBASE)/common.mk


GREP := grep
PERL := perl
TEE  := tee

.PHONY: debug
debug:
	$(Q)$(PERL) ../tools/debug.pl $(ESPPORT):115200

$(BUILD_DIR)$(PROGRAM).sym_iram: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl iRAM 0x40100000 0x8000 > $@

$(BUILD_DIR)$(PROGRAM).sym_irom: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl iROM 0x40200000 0x5c000 > $@

$(BUILD_DIR)$(PROGRAM).sym_dram: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl dRAM 0x3ffe8000 0x14000 > $@

$(BUILD_DIR)$(PROGRAM).size: $(PROGRAM_OUT) $(BUILD_DIR)$(PROGRAM).sym_iram $(BUILD_DIR)$(PROGRAM).sym_irom  $(BUILD_DIR)$(PROGRAM).sym_dram
	$(vecho) "GEN $@"
	$(Q)$(SIZE) -Ax $< > $@
	$(Q)$(SIZE) -Bd $< >> $@
	$(Q)$(SIZE) -Bx $< >> $@
#	$(Q)$(SIZE) -Bd $(OFILES) >> $@
	@echo >> $@
	@echo >> $@
	@echo >> $@
	$(Q)$(GREP) -h ^total $(subst .size,.sym_iram,$@) $(subst .size,.sym_irom,$@) $(subst .size,.sym_dram,$@) | $(TEE) -a $@

all: $(BUILD_DIR)$(PROGRAM).size
