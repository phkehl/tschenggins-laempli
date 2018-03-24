###############################################################################
#
# flipflip's ESP8266 Tschenggins Lämpli
#
# Copyright (c) 2018 Philippe Kehl <flipflip at oinkzwurgl dot org>
#
###############################################################################

# the normal SDK and the RTOS SDK
SDKBASE  := /home/flip/sandbox/esp-open-sdk
RTOSBASE := /home/flip/sandbox/esp-open-rtos

# we need the xtensa compiler and the esptool in the path
PATH        := $(SDKBASE)/xtensa-lx106-elf/bin:$(SDKBASE)/esptool:$(PATH)

# build config
PROGRAM         = tschenggins-laempli-ng
PROGRAM_SRC_DIR = ./src
PROGRAM_INC_DIR = ./src $(PROGRAM_OBJ_DIR)

# ESP8266 config
ESPPORT         = /dev/ttyUSB0
ESPBAUD         = 1500000
FLASH_MODE      = dout
FLASH_SIZE      = 32
FLASH_SPEED     = 40

# TODO: add program_CFLAGS !!

include $(RTOSBASE)/common.mk

###############################################################################

GREP    := grep
PERL    := perl
TEE     := tee
SED     := sed
DATE    := date
HEAD    := head
TOUCH   := touch
MKDIR   := mkdir
RM      := rm
MV      := mv
AWK     := awk
FIND    := find
SORT    := sort
CSCOPE  := cscope

CSCOPEDIRS := $(PROGRAM_SRC_DIR) $(PROGRAM_INC_DIR) \
	$(RTOSBASE)/libc/xtensa-lx106-elf/include/ $(RTOSBASE)/include $(RTOSBASE)/open_esplibs \
	$(RTOSBASE)/FreeRTOS/Source $(RTOSBASE)/core $(RTOSBASE)/extras $(RTOSBASE)/lwip

cscope.files: Makefile
	$(vecho) "GEN $@"
	$(Q)$(FIND) $(CSCOPEDIRS) \
        \( -type d \( -name .git -o -name .svn -o -false \) -prune \) \
        -o \( -iname '*.[ch]' -o -false \) -and \( -type f -o -type l \) -print \
		| $(SORT) -u > $@

cscope.out: cscope.files
	$(vecho) "GEN $@"
	$(Q)$(RM) -f cscope.out*
	$(Q)$(CSCOPE) -f $@ -b -k

cscope: cscope.out
clean: cscope-clean
cscope-clean:
	$(Q)$(RM) -f cscope.*

all: cscope

###############################################################################

# show debug output
.PHONY: debug
debug:
	$(Q)$(PERL) ../tools/debug.pl $(ESPPORT):115200

# print list of IRAM symbols
$(BUILD_DIR)$(PROGRAM).sym_iram: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl iRAM 0x40100000 0x8000 > $@

# print list of IROM symbols
$(BUILD_DIR)$(PROGRAM).sym_irom: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl iROM 0x40200000 0x5c000 > $@

# print list of DRAM symbols
$(BUILD_DIR)$(PROGRAM).sym_dram: $(PROGRAM_OUT) ../tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) ../tools/symbols.pl dRAM 0x3ffe8000 0x14000 > $@

# print object size info
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

# add sizes and symbol lists to the main build target
all: $(BUILD_DIR)$(PROGRAM).size

###############################################################################

# build version
BUILDVER    := $(shell $(PERL) ../tools/version.pl)
BUILDVEROLD := $(shell $(SED) -n '/FF_BUILDVER/s/.*"\(.*\)".*/\1/p' $(PROGRAM_OBJ_DIR)version_gen.h 2>/dev/null)
BUILDDATE   := $(shell $(DATE) +"%Y-%m-%d %H:%M")
GCCVERSION  := $(shell PATH=$(PATH) $(CC) --version | $(HEAD) -n1)

# trigger generation of version_gen.h if necessary
ifneq ($(BUILDVER),$(BUILDVEROLD))
$(shell $(MKDIR) -p $(PROGRAM_OBJ_DIR); $(TOUCH) $(PROGRAM_OBJ_DIR).version_gen.h)
endif

# generate version include file
$(PROGRAM_OBJ_DIR)version_gen.h: $(PROGRAM_OBJ_DIR).version_gen.h Makefile | $(PROGRAM_OBJ_DIR)
	$(vecho) "GEN $@"
	$(Q)$(RM) -f $@
	$(Q)echo "#ifndef __VERSION_GEN_H__" >> $@.tmp
	$(Q)echo "#define __VERSION_GEN_H__" >> $@.tmp
	$(Q)echo "#define FF_GCCVERSION \"$(GCCVERSION)\"" >> $@.tmp
	$(Q)echo "#define FF_BUILDVER \"$(BUILDVER)\"" >> $@.tmp
	$(Q)echo "#define FF_BUILDDATE \"$(BUILDDATE)\"" >> $@.tmp
	$(Q)echo "#endif" >> $@.tmp
	$(Q)$(MV) $@.tmp $@

# all source file may need this
$(PROGRAM_OBJ_FILES): $(PROGRAM_OBJ_DIR)version_gen.h

###############################################################################

CFGFILE :=

ifeq ($(CFGFILE),)
$(PROGRAM_OBJ_DIR)cfg_gen.h: | $(PROGRAM_OBJ_DIR)
	$(vecho) "GEN $@ (dummy)"
	$(Q)$(TOUCH) $@
else
CFGFILEOLD := $(shell $(SED) -n '/predefined config from /s/.*predefined config from //p' $(PROGRAM_OBJ_DIR)cfg_gen.h 2>/dev/null || echo nocfg)

# trigger generation of version_gen.h if necessary
ifneq ($(CFGFILE),$(CFGFILEOLD))
$(shell $(MKDIR) -p $(PROGRAM_OBJ_DIR); $(TOUCH) $(PROGRAM_OBJ_DIR).cfg_gen.h)
endif

$(PROGRAM_OBJ_DIR)cfg_gen.h: Makefile $(PROGRAM_OBJ_DIR).cfg_gen.h $(CFGFILE) | $(PROGRAM_OBJ_DIR)
	$(vecho) "GEN $@ ($(CFGFILE))"
	$(Q)$(RM) -f $@
	$(Q)echo "#ifndef __CFG_GEN_H__" >> $@.tmp
	$(Q)echo "#define __CFG_GEN_H__" >> $@.tmp
ifneq ($(CFGFILE),)
	$(Q)echo "// predefined config from $(CFGFILE)" >> $@.tmp
	$(Q)$(AWK) '!/^\s*#/ && !/^\s*$$/ { print "#define FF_CFG_"$$1" "$$2 }' $(CFGFILE) >> $@.tmp
endif
	$(Q)echo "#endif" >> $@.tmp
	$(Q)$(MV) $@.tmp $@
endif

# all source file may need this
$(PROGRAM_OBJ_FILES): $(PROGRAM_OBJ_DIR)cfg_gen.h
