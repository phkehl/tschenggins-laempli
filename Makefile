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

# we want all intermediate and output files here
OUTPUT_DIR   := output/
BUILD_DIR    := $(OUTPUT_DIR)build/
FIRMWARE_DIR := $(OUTPUT_DIR)firmware/

# we need the xtensa compiler and the esptool in the path
PATH        := $(SDKBASE)/xtensa-lx106-elf/bin:$(SDKBASE)/esptool:$(PATH)

# build config
PROGRAM         = tschenggins-laempli
PROGRAM_SRC_DIR = ./src ./3rdparty
PROGRAM_INC_DIR = ./src ./3rdparty $(PROGRAM_OBJ_DIR)

EXTRA_COMPONENTS = extras/jsmn

EXTRA_CFLAGS    = -DJSMN_PARENT_LINKS -Wenum-compare

#WARNINGS_AS_ERRORS = 1

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
CP      := cp
AWK     := awk
FIND    := find
SORT    := sort
CSCOPE  := cscope
DOXYGEN := doxygen
CHMOD   := chmod
CAT     := cat

# verbosity helpers
ifeq ($(V),1)
V =
V1 =
V2 =
V12 =
OBJCOPY += -v
RM += -v
MV += -v
else
V = @
V1 = > /dev/null
V2 = 2> /dev/null
V12 = 2>&1 > /dev/null
VQ = -q
endif

###############################################################################

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
	$(Q)$(PERL) tools/debug.pl $(ESPPORT):115200

# symbol table
$(BUILD_DIR)$(PROGRAM).sym: $(PROGRAM_OUT)
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< > $@

# full listing
$(BUILD_DIR)$(PROGRAM).lst: $(PROGRAM_OUT)
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -h -S $< > $@

# print list of IRAM symbols
$(BUILD_DIR)$(PROGRAM).sym_iram: $(PROGRAM_OUT) tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl iRAM 0x40100000 0x8000 > $@

# print list of IROM symbols
$(BUILD_DIR)$(PROGRAM).sym_irom: $(PROGRAM_OUT) tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl iROM 0x40200000 0x5c000 > $@

# print list of DRAM symbols
$(BUILD_DIR)$(PROGRAM).sym_dram: $(PROGRAM_OUT) tools/symbols.pl
	$(vecho) "GEN $@"
	$(Q)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl dRAM 0x3ffe8000 0x14000 > $@

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
all: $(BUILD_DIR)$(PROGRAM).size $(BUILD_DIR)$(PROGRAM).lst $(BUILD_DIR)$(PROGRAM).sym

###############################################################################

# build version
BUILDVER    := $(shell $(PERL) tools/version.pl)
BUILDVEROLD := $(shell $(SED) -n '/FF_BUILDVER/s/.*"\(.*\)".*/\1/p' $(PROGRAM_OBJ_DIR)version_gen.h 2>/dev/null)
BUILDDATE   := $(shell $(DATE) +"%Y-%m-%d %H:%M")
GCCVERSION  := $(shell PATH=$(PATH) $(CC) --version | $(HEAD) -n1)
EORVERSION  := $(shell $(PERL) tools/version.pl $(RTOSBASE))
SDKVERSION  := $(shell $(PERL) tools/version.pl $(SDKBASE))

# trigger generation of version_gen.h if necessary
ifneq ($(BUILDVER),$(BUILDVEROLD))
$(shell $(MKDIR) -p $(PROGRAM_OBJ_DIR); $(TOUCH) $(PROGRAM_OBJ_DIR).version_gen.h)
endif

# generate version include file
$(PROGRAM_OBJ_DIR)version_gen.h: $(PROGRAM_OBJ_DIR).version_gen.h Makefile | $(PROGRAM_OBJ_DIR)
	$(vecho) "GEN $@"
	$(Q)$(RM) -f $@
	$(Q)echo "#ifndef __VERSION_GEN_H__"               >> $@.tmp
	$(Q)echo "#define __VERSION_GEN_H__"               >> $@.tmp
	$(Q)echo "#define FF_GCCVERSION \"$(GCCVERSION)\"" >> $@.tmp
	$(Q)echo "#define FF_BUILDVER   \"$(BUILDVER)\""   >> $@.tmp
	$(Q)echo "#define FF_BUILDDATE  \"$(BUILDDATE)\""  >> $@.tmp
	$(Q)echo "#define FF_PROGRAM    \"$(PROGRAM)\""    >> $@.tmp
	$(Q)echo "#define FF_EORVERSION \"$(EORVERSION)\"" >> $@.tmp
	$(Q)echo "#define FF_SDKVERSION \"$(SDKVERSION)\"" >> $@.tmp
	$(Q)echo "#endif"                                  >> $@.tmp
	$(Q)$(MV) $@.tmp $@

# all source file may need this
$(PROGRAM_OBJ_FILES): $(PROGRAM_OBJ_DIR)version_gen.h

###############################################################################

CFGFILE ?=

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

###############################################################################

FW_FILE_RBOOT_BIN  := $(OUTPUT_DIR)0x00000_rboot.bin
FW_FILE_RBOOT_CONF := $(OUTPUT_DIR)0x01000_blank.bin
FW_FILE_FIRMWARE   := $(OUTPUT_DIR)0x02000_$(PROGRAM).bin

all: $(FW_FILE_RBOOT_BIN) $(FW_FILE_RBOOT_CONF) $(FW_FILE_FIRMWARE)

$(FW_FILE_RBOOT_BIN): $(RBOOT_BIN)
	$(Q)$(CP) $^ $@

$(FW_FILE_RBOOT_CONF): $(RBOOT_CONF)
	$(Q)$(CP) $^ $@

$(FW_FILE_FIRMWARE): $(FW_FILE)
	$(Q)$(CP) $^ $@

clean: fw-files-clean
.PHONY: fw-files-clean
fw-files-clean:
	$(Q)$(RM) $(FW_FILE_RBOOT_BIN) $(FW_FILE_RBOOT_CONF) $(FW_FILE_FIRMWARE)

###############################################################################

DOC_GEN_FILES := $(PROGRAM_OBJ_DIR)cfg_gen.h $(PROGRAM_OBJ_DIR)version_gen.h
DOXY_WARNINGS_LOG := $(BUILD_DIR)doxygen_warnings.log

$(DOXY_WARNINGS_LOG): | $(BUILD_DIR)

.PHONY: doc
doc: $(OUTPUT_DIR)html/.done
$(OUTPUT_DIR)html/.done: Makefile $(CFILES) $(HFILES) Doxyfile tools/doxylogfix.pl $(DOC_GEN_FILES) $(wildcard src/*.h) $(wildcard src/*.c)
# remove previous output and create new output directory
	$(Q)$(RM) -rf $(OUTPUT_DIR)html
	$(Q)$(MKDIR) -p $(OUTPUT_DIR)html
# generate documentation (run the doxygen tool)
	@echo "$(HLG)R Doxygen $(HLR)$(OUTPUT_DIR)html$(HLO)"
	$(Q)(\
		$(CAT) Doxyfile; \
		echo "PROJECT_NUMBER = $(BUILDVER)"; \
		echo "OUTPUT_DIRECTORY = $(OUTPUT_DIR)"; \
		echo "WARN_LOGFILE = $(DOXY_WARNINGS_LOG)"; \
		echo "INPUT += $(DOC_GEN_FILES)"; \
	) | $(DOXYGEN) - | $(TEE) $(OUTPUT_DIR)doxygen.log $(V1)
# make paths in the logfiles relative
	$(Q)$(SED) -i -r -e 's@$(CURDIR)/?@./@g' -e '/^$$/d' $(DOXY_WARNINGS_LOG) $(OUTPUT_DIR)doxygen.log
# remove some known won't-fix warnings in the logfile
	$(Q)$(PERL) tools/doxylogfix.pl < $(DOXY_WARNINGS_LOG) > $(DOXY_WARNINGS_LOG)-clean
	$(Q)if [ -s $(DOXY_WARNINGS_LOG)-clean ]; then \
		$(CAT) $(DOXY_WARNINGS_LOG)-clean; \
	else \
		echo "no warnings" > $(DOXY_WARNINGS_LOG)-clean; \
	fi
# inject the warnings and doxygen logfiles into the generated HTML
	$(Q)$(SED) -i \
		-e '/%DOXYGEN_LOG/r $(OUTPUT_DIR)doxygen.log' \
		-e '/%DOXYGEN_WARNINGS/r $(DOXY_WARNINGS_LOG)-clean' \
		-e '/%DOXYGEN_WARNINGS/s/%DOXYGEN_WARNINGS%//' \
		-e '/%DOXYGEN_LOG/s/%DOXYGEN_LOG%//' \
		$(OUTPUT_DIR)html/P_DOXYGEN.html
# make docu files world-readable
	$(Q)$(CHMOD) -R a+rX $(OUTPUT_DIR)html
# done
	$(Q)$(TOUCH) $@

