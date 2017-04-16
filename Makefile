###############################################################################
#
# flipflip's ESP8266 Tschenggins Lämpli
#
# Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>
#
# Credits: SDK examples and https://github.com/esp8266/source-code-examples
#
###############################################################################
#
# say "make help" for help..
#
###############################################################################

# verbose build?
VERBOSE     := 0

# print object sizes after compiling (0 = never, 1 = only for image, 2 = for all files)
SIZES       := 1

# debug and programming UART TX0/RX0
ESPPORT     := /dev/ttyUSB0
#ESPBAUD     := 460800
#ESPBAUD     := 800000
#ESPBAUD     := 1000000
#ESPBAUD     := 1250000
ESPBAUD     := 1500000
#ESPBAUD     := 1500000

# executable name (single word)
PROJECT     := tschenggins-laempli
COPYRIGHT   := 2017 Philippe Kehl & flipflip industries
COPY_HTML   := 2017 Philippe Kehl &amp flipflip industries
COPY_EMAIL  := flipflip at oinkzwurgl dot org
PROJTITLE   := Tschenggins Lämpli
PROJLINK    := https://oinkzwurgl.org/projaeggd/tschenggins-laempli

# model of the Lämpli
MODEL       := 1

# pre-defined configuration, see cfg-sample.txt
CFGFILE     :=

# we generate two firmware files
FWADDR1     := 0x00000
FWADDR2     := 0x10000

# configuration (sector 128)
CFGADDR     := 0x80000

# filesystem (sector 144)
FSADDR      := 0x90000
#FSADDR      := 0x100000
#FSADDR      := 0x200000

# esp8266-open-sdk (from https://github.com/pfalcon/esp-open-sdk)
SDKBASE     := /home/flip/sandbox/esp-open-sdk
SDKDIR      := $(SDKBASE)/sdk

# we need the xtensa compiler and the esptool in the path
PATH        := $(SDKBASE)/xtensa-lx106-elf/bin:$(SDKBASE)/esptool:$(PATH)


###############################################################################

# include local defaults overrides if it exists
-include config.mk

# default target (FIXME can use .DEFAULT_GOAL?)
ifeq ($(MAKECMDGOALS),)
default: what
MAKECMDGOALS = what
endif

# tools
PERL        := perl
SED         := sed
SHELL       := bash
TOUCH       := touch
RM          := rm
MV          := mv
MKDIR       := mkdir
HEAD        := head
AWK         := gawk
TEE         := tee
DOXYGEN     := doxygen
CAT         := cat
TAIL        := tail
TAR         := tar
LS          := ls
DATE        := date
FMT         := fmt
CHMOD       := chmod
FIND        := find
WC          := wc
CSCOPE      := cscope
GREP        := grep
DD          := dd

# verbosity helpers
ifeq ($(VERBOSE),1)
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

# disable fancy stuff for dumb terminals (e.g. Emacs compilation window)
fancyterm := true
ifeq ($(TERM),dumb)
fancyterm := false
endif
ifeq ($(TERM),)
fancyterm := false
endif
ifeq ($(fancyterm),true)
HLR="[31m"
HLG="[32m"
HLY="[33m"
HLV="[35m"
HLM="[1\;36m"
HLO="[m"
else
HLR=
HLG=
HLY=
HLV=
HLM=
HLO=
endif


# toolchain
XTENSAPREFIX  := xtensa-lx106-elf-
STRIP         := $(XTENSAPREFIX)strip
STRINGS       := $(XTENSAPREFIX)strings
CC            := $(XTENSAPREFIX)gcc
AS            := $(XTENSAPREFIX)as
LD            := $(XTENSAPREFIX)ld
NM            := $(XTENSAPREFIX)nm
OBJCOPY       := $(XTENSAPREFIX)objcopy
OBJDUMP       := $(XTENSAPREFIX)objdump
SIZE          := $(XTENSAPREFIX)size
GDB           := $(XTENSAPREFIX)gdb

# programmer
ESPTOOL       := esptool.py
ESPTOOLARGS   := --port $(ESPPORT) --baud $(ESPBAUD)
#ESPTOOLARGS_WRITE_FLASH := --compress


###############################################################################

# output directory
OBJDIR       := obj

# project source files
CFILES        := $(sort $(wildcard src/*.c)) $(sort $(wildcard 3rdparty/*.c))
HFILES        := $(sort $(wildcard src/*.h)) $(sort $(wildcard 3rdparty/*.h))
HTMLFILES     := $(sort $(wildcard src/*.html))

# make as far as possible, parallelise
MAKEFLAGS     := -k $(JFLAG) -j$(shell $(AWK) '/^processor/ { j = j + 1; } END { print j }' /proc/cpuinfo)

# build version
BUILDVER    := $(shell $(PERL) tools/version.pl)
BUILDVEROLD := $(shell $(SED) -n '/FF_BUILDVER/s/.*"\(.*\)".*/\1/p' $(OBJDIR)/version_gen.h 2>/dev/null)
BUILDDATE   := $(shell $(DATE) +"%Y-%m-%d %H:%M")
GCCVERSION  := $(shell PATH=$(PATH) $(CC) --version | $(HEAD) -n1)

# preprocessor defines
DEFS          += -DICACHE_FLASH
#DEFS          += -D__ets__

# compiler flags
CFLAGS        += -O2 -g3 -ggdb3 -pipe
CFLAGS        += -fdata-sections -ffunction-sections
CFLAGS        += -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals
CFLAGS        += -Wpointer-arith -Wundef # -Wpedantic -Werror
CFLAGS        += -Wall -Wpadded -Wextra -Wstrict-prototypes -Wenum-compare -Wswitch-enum
CFLAGS        += --std=gnu99 -Wno-implicit-function-declaration
CFLAGS        += -fno-common -funsigned-char -funsigned-bitfields
#CFLAGS        += -fpack-struct -fshort-enums # NO!!!!
CFLAGS        += -ffunction-sections -fdata-sections -fno-strict-aliasing -fdata-sections 
# -Wredundant-decls -Wnested-externs -Wold-style-definitions -Werror-implicit-function-declaration
CFLAGS        += -Wunused -Wno-unused-parameter -Wwrite-strings
CFLAGS        += -Wformat=2 -Wunused-result -Wjump-misses-init -Wlogical-op
CFLAGS        += -Wunused-variable -Wmissing-prototypes -Wpointer-arith -Wchar-subscripts -Wcomment
CFLAGS        += -Wimplicit-int -Wmain -Wparentheses -Wsequence-point -Wreturn-type -Wswitch -Wtrigraphs
CFLAGS        += -Wuninitialized -Wunknown-pragmas -Wfloat-equal -Wundef -Wshadow -Wsign-compare
CFLAGS        += -Waggregate-return -Wmissing-declarations -Wformat -Wmissing-format-attribute
CFLAGS        += -Wno-deprecated-declarations -Winline -Wlong-long -Wcast-align
CFLAGS        += -Wunreachable-code -Wbad-function-cast -Wpacked
#CFLAGS        += -Wa,--gstabs

# linker flags
# FIXME: what is it using? $(SDKDIR)/lib or $(SDKDIR)../xtensa-lx106-elf/xtensa-lx106-elf/sysroot/lib
LDFLAGS      += -L$(SDKDIR)/lib -T$(SDKDIR)/ld/eagle.app.v6.ld # -Teagle.app.v6.ld
LDFLAGS      += -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static
LDFLAGS      += -Wl,--gc-sections
LDFLAGS      += -Wl,-Map=$(OBJDIR)/$(PROJECT).map,--cref -Wl,-gc-sections
LDLIBS       += -Wl,--start-group
LDLIBS       +=   -lm -lc -lhal
LDLIBS       +=   $(subst .a,,$(subst $(SDKDIR)/lib/lib,-l,$(wildcard $(SDKDIR)/lib/lib*.a)))
LDLIBS       += -Wl,--end-group

# include directories and flags
#INCDIRS      += $(SDKDIR)/include # nope, esd-open-sdk has it in xtensa-lx106-elf/xtensa-lx106-elf/sysroot/usr/include
#INCDIRS      += $(SDKDIR)/driver_lib/include/driver
INCDIRS     += $(sort $(dir $(CFILES))) $(OBJDIR)
INCFLAGS    += $(strip $(foreach dir, $(INCDIRS), -I$(dir)))

# object files to generate (compile, assemble)
OFILES       +=

# makes compile rule for .c files
define makeCompileRuleC
#(info makeCompileRuleC $(1) --> $(OBJDIR)/$(subst /,__,$(patsubst %.c,%.o,$(1))))
OFILES += $(OBJDIR)/$(subst /,__,$(patsubst %.c,%.o,$(1)))
$(OBJDIR)/$(subst /,__,$(patsubst %.c,%.o,$(1))): $(1) $(MAKEFILE_LIST)
	@echo "$(HLY)C $$< $(HLR)$$@$(HLO)"
	$(V)$(CC) -c -o $$@ $$(CFLAGS) $(DEFS) $(INCFLAGS) $$< -MD -MF $$(@:%.o=%.d) -MT $$@ -Wa,-adhlns=$$(@:.o=.lst)
	$(V)if [ "$(SIZES)" -ge 2 ]; then $(SIZE) -B $$@; fi
endef

# create compile rules and populate $(OFILES) list
$(foreach cfile, $(filter %.c,$(CFILES)), $(eval $(call makeCompileRuleC,$(cfile)))) # watch the spaces!

# dependency files
DFILES := $(patsubst %.o,$(OBJDIR)/%.d,$(notdir $(OFILES)))

# firmware files
FWFILE1 := $(OBJDIR)/$(PROJECT)_$(FWADDR1).bin
FWFILE2 := $(OBJDIR)/$(PROJECT)_$(FWADDR2).bin

#FSIMG := $(OBJDIR)/fsdata.bin
FSIMG := $(OBJDIR)/$(PROJECT)_$(FSADDR).bin
FSFILES := $(shell $(AWK) '!/^\s*\#/ && !/^\s*$$/ { print "fs/"$$1 }' fs/manifest)

CFGIMG := $(OBJDIR)/$(PROJECT)_$(CFGADDR).bin


# load available dependency files
ifneq ($(MAKECMDGOALS),debugmf)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DFILES)
endif
endif
endif


###############################################################################

# make output directory
$(OBJDIR)/.dummy: $(SDKDIR)
	@echo "$(HLG)M $(OBJDIR)/$(HLO)"
	$(V)$(MKDIR) -p $(OBJDIR)
	$(V)$(TOUCH) $@


# trigger generation of version_gen.h if necessary
ifneq ($(BUILDVER),$(BUILDVEROLD))
$(shell $(MKDIR) -p $(OBJDIR); $(TOUCH) $(OBJDIR)/.version_gen.h)
endif

# generate include files
$(OBJDIR)/version_gen.h: $(OBJDIR)/.dummy $(OBJDIR)/.version_gen.h Makefile
	@echo "$(HLV)G $@ $(HLO)"
	@echo "#ifndef __VERSION_GEN_H__" > $@.tmp
	@echo "#define __VERSION_GEN_H__" >> $@.tmp
	@echo "#define FF_GCCVERSION \"$(GCCVERSION)\"" >> $@.tmp
	@echo "#define FF_BUILDVER \"$(BUILDVER)\"" >> $@.tmp
	@echo "#define FF_BUILDDATE \"$(BUILDDATE)\"" >> $@.tmp
	@echo "#define FF_PROJECT \"$(PROJECT)\"" >> $@.tmp
	@echo "#define FF_PROJTITLE \"$(PROJTITLE)\"" >> $@.tmp
	@echo "#define FF_PROJLINK \"$(PROJLINK)\"" >> $@.tmp
	@echo "#define FF_COPYRIGHT \"$(COPYRIGHT)\"" >> $@.tmp
	@echo "#define FF_COPYRIGHT_HTML \"$(COPY_HTML)\"" >> $@.tmp
	@echo "#define FF_COPYRIGHT_EMAIL \"$(COPY_EMAIL)\"" >> $@.tmp
	@echo "#define FF_CFGADDR $(CFGADDR)" >> $@.tmp
	@echo "#define FF_FSADDR $(FSADDR)" >> $@.tmp
	@echo "#define FF_MODEL $(MODEL)" >> $@.tmp
	@echo "#define FF_MODEL_STR \"$(MODEL)\"" >> $@.tmp
	@echo "#endif" >> $@.tmp
	$(V)$(MV) $@.tmp $@

$(OBJDIR)/cfg_gen.h: $(OBJDIR)/.dummy Makefile $(CFGFILE)
	@echo "$(HLV)G $@ $(HLO)"
	@echo "#ifndef __CFG_GEN_H__" > $@
	@echo "#define __CFG_GEN_H__" >> $@
ifneq ($(CFGFILE),)
	@echo "// predefined config from $(CFGFILE)" >> $@
	$(V)$(AWK) '!/^\s*#/ && !/^\s*$$/ { print "#define DEF_CFG_"$$1" "$$2 }' $(CFGFILE) >> $@
endif
	@echo "#endif" >> $@


$(OBJDIR)/html_gen.h: $(HTMLFILES) Makefile $(OBJDIR)/.dummy tools/html2c.pl $(OBJDIR)/version_gen.h
	@echo "$(HLV)G $@ $(HLO)"
	$(V)$(RM) -f $@
	$(V)$(PERL) tools/html2c.pl $(HTMLFILES) > $@.tmp
	$(V)$(MV) $@.tmp $@

$(OFILES): $(OBJDIR)/html_gen.h $(OBJDIR)/version_gen.h $(OBJDIR)/cfg_gen.h

# we generate .o files from .c files
$(OFILES): $(OBJDIR)/.dummy Makefile

# link into .elf image file
$(OBJDIR)/$(PROJECT).elf: $(OFILES)
	@echo "$(HLM)L $(@)$(HLO)"
	$(V)$(CC) -o $@ $(LDFLAGS) $(OFILES) $(LDLIBS)


# .size from .elf (section sizes)
%.size: %.elf %.sym_iram %.sym_irom %.sym_dram
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(SIZE) -Ax $< > $@
	$(V)$(SIZE) -Bd $< >> $@
	$(V)$(SIZE) -Bx $< >> $@
	$(V)$(SIZE) -Bd $(OFILES) >> $@
	$(V)if [ "$(SIZES)" -gt 1 ]; then $(SIZE) -Ad $<; fi
	@echo >> $@
	@echo >> $@
	@echo >> $@
	$(V)$(GREP) -h ^total $(subst .size,.sym_iram,$@) $(subst .size,.sym_irom,$@) $(subst .size,.sym_dram,$@) | $(TEE) -a $@

#.lss .lst .sym .hex .srec .ramsym .bin
# .lss from .elf (extended listing)
%.lss: %.elf
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -t -h $< > $@

%.dump: %.elf
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -s -x $< > $@

# .lst from .elf (disassembly)
%.lst: %.elf
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -h -S $< > $@

# .sym from .elf (symbol table)
%.sym: %.elf
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(NM) -n $< > $@

# generate firmware files
$(FWFILE1) $(FWFILE2): $(OBJDIR)/.fw-dummy
$(OBJDIR)/.fw-dummy: $(OBJDIR)/$(PROJECT).elf
	@echo "$(HLV)G $(FWFILE1) $(FWFILE2)$(HLO)"
	$(V)$(ESPTOOL) elf2image -o $(OBJDIR)/$(PROJECT)_ $< $(V2)
	$(V)$(TOUCH) $@

%.sym_iram: %.elf tools/symbols.pl
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl iRAM 0x40100000 0x8000 > $@

%.sym_irom: %.elf tools/symbols.pl
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl iROM 0x40200000 0x5c000 > $@

%.sym_dram: %.elf tools/symbols.pl
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(OBJDUMP) -t $< | $(PERL) tools/symbols.pl dRAM 0x3ffe8000 0x14000 > $@

# filesystem
$(FSIMG): $(FSFILES) tools/mkfs.pl Makefile fs/manifest
	@echo "$(HLV)G $@ $(HLY)$(FSFILES)$(HLO)"
	$(V)$(PERL) tools/mkfs.pl $(VQ) $@ $(FSFILES)

# empty config image
$(CFGIMG): Makefile
	@echo "$(HLV)G $@$(HLO)"
	$(V)$(DD) if=/dev/urandom of=$@ bs=4k count=3 $(V2)


###############################################################################

# compile all sources
.PHONY: compile
compile: $(OBJDIR)/.dummy $(OFILES)

# link sources to elf image
.PHONY: link
link: $(OBJDIR)/$(PROJECT).elf

# generate derivatives
.PHONY: deriv
deriv: $(addprefix $(OBJDIR)/$(PROJECT), .size .lss .lst .sym .sym_iram .sym_dram .sym_irom .dump)

# firmware image
.PHONY: firmware
firmware: $(OBJDIR)/.fwfiles
$(OBJDIR)/.fwfiles: $(FWFILE1) $(FWFILE2) $(FSIMG) $(CFGIMG)
	$(V)$(TOUCH) $@
	$(V)$(LS) -l -h $(FWFILE1) $(FWFILE2) $(FSIMG) $(CFGIMG)

# all(most all)
.PHONY: all
all: compile link deriv firmware doc

# clean
.PHONY: clean
clean:
	@if [ -d "$(OBJDIR)" ]; then echo "$(HLG)M removing $(OBJDIR)$(HLO)"; fi
ifneq ($(OBJDIR),)
	$(V)if [ -d "$(OBJDIR)" ]; then $(RM) -rf $(OBJDIR); fi
else
	@echo "$(HLR)ERROR: no OBJDIR!!!$(HLO)"
endif

# clean more
.PHONY: distclean
distclean:
	$(V)$(RM) -rf $(OBJDIR) src/*~ src/\#* *~ cscope.*

# flash firmware image, filesystem and config
.PHONY: prog
prog: firmware deriv $(ESPPORT)
	$(V)$(ESPTOOL) $(ESPTOOLARGS) write_flash $(ESPTOOLARGS_WRITE_FLASH) \
		$(FWADDR1) $(FWFILE1) \
		$(FWADDR2) $(FWFILE2) \
		$(FSADDR) $(FSIMG) \
		$(CFGADDR) $(CFGIMG)

# flash firmware image only
.PHONY: progfw
progfw: firmware deriv $(ESPPORT)
	$(V)$(ESPTOOL) $(ESPTOOLARGS) write_flash $(ESPTOOLARGS_WRITE_FLASH) \
		$(FWADDR1) $(FWFILE1) \
		$(FWADDR2) $(FWFILE2)

# flash filesystem
.PHONY: progfs
progfs: $(FSIMG) $(ESPPORT)
	$(V)$(ESPTOOL) $(ESPTOOLARGS) write_flash $(ESPTOOLARGS_WRITE_FLASH) \
		$(FSADDR) $(FSIMG)

# factory reset
.PHONY: progall
progall: fw $(ESPPORT) $(FWFILE1) $(FWFILE2) deriv
	$(V)$(ESPTOOL) $(ESPTOOLARGS) erase_flash
	$(V)$(ESPTOOL) $(ESPTOOLARGS) write_flash $(ESPTOOLARGS_WRITE_FLASH) \
	    0x3fc000 $(SDKDIR)/bin/esp_init_data_default.bin \
        0x03e000 $(SDKDIR)/bin/blank.bin \
        0x07e000 $(SDKDIR)/bin/blank.bin \
		$(FWADDR1) $(FWFILE1) \
		$(FWADDR2) $(FWFILE2) \
		$(FSADDR) $(FSIMG)


# clear configuration
.PHONY: clearcfg
clearcfg:
	$(V)$(DD) if=/dev/urandom of=$(OBJDIR)/blankcfg.bin bs=4k count=3 $(V2)
	$(V)$(ESPTOOL) $(ESPTOOLARGS) write_flash $(CFGADDR) $(OBJDIR)/blankcfg.bin

# pretty-print debug output to terminal
.PHONY: debug
debug:
	$(V)$(PERL) tools/debug.pl $(ESPPORT):115200


###############################################################################

.PHONE: what
what:
	@echo "Make what? Try 'make help'..."

# help
.PHONY: help
help:
	@echo
	@echo "Usage: make ... [VERBOSE=0|1] [MODEL=1|...] [...=...]"
	@echo
	@echo "Possible targets are:"
	@echo
	@echo "    all -- compile, link, generate firmare image and other files, which makes:"
	@echo "        compile   -- compile sources"
	@echo "        link      -- link objects into executable (.elf) file"
	@echo "        deriv     -- create derivatives (listing file, symbol tables, etc.)"
	@echo "        firmware  -- make firmware imagee files"
	@echo "        doc       -- generate documentation (using Doxygen)"
	@echo
	@echo "    prog -- program (flash) firmware files, or individual files:"
	@echo "        progfw -- program only firmware code files"
	@echo "        progfs -- program only filesystem"
	@echo
	@echo "    progall -- erase and re-program entire flash incl. SDK files (a.k.a. 'un-brick' it)"
	@echo
	@echo "    clearcfg -- erase configuration in flash"
	@echo
	@echo
	@echo "    clean -- remove output files"
	@echo "    distclean -- remove output and editor backup files (*~ etc.)"
	@echo
	@echo "    debug -- show colourised debug console (i.e. run tools/debug.pl)"
	@echo
	@echo "    help -- print this help"
	@echo "    debugmf -- makefile debugging"
	@echo
	@echo "Variables can be passed on the command line or set in a local config.mk file:"
	@echo
	@echo "    VERBOSE -- set to 1 for more a verbose build output"
	@echo "    ESPPORT -- set to the programming/console port (default: $(ESPPORT))"
	@echo "    ESPBAUD -- set programming baudrate (default: $(ESPBAUD))"
	@echo "    SDKBASE -- set to SDK base directory (default: $(SDKBASE))"
	@echo "    CFGFILE -- pre-defined default configuration (see cfg-sample.txt)"
	@echo "    MODEL   -- the model of the Lämpli to configure the software for"
	@echo
	@echo "Note: Be sure the 'make clean' when changing the CFGFILE or MODEL parameters."
	@echo
	@echo "Typical usage:"
	@echo
	@echo "    make clean && make all && make prog && make debug"
	@echo "    make clean && make all CFGFILE=cfg-xxx.txt MODEL=1 && make prog && make debug"
	@echo


###############################################################################

# create cscope files
.PHONY: cscope
cscope:
#	$(V)$(FIND) src $(SDKDIR)/../xtensa-lx106-elf/xtensa-lx106-elf/sysroot/usr/include \( -type f -o -type l \) -and \( -iname *.[ch] \) -print > cscope.files
	$(V)$(FIND) src $(SDKDIR)/include \( -type f -o -type l \) -and \( -iname *.[ch] \) -print > cscope.files
	@echo "Indexing $$($(WC) -l cscope.files | $(AWK) '{ print $$1 }') files..."
	$(V)$(CSCOPE) -f cscope.out -b -q -k

###############################################################################
# doxygen

.PHONY: doc
doc: $(OBJDIR)/html/.done
$(OBJDIR)/html/.done: Makefile $(CFILES) $(HFILES) Doxyfile tools/doxylogfix.pl
# remove previous output and create new output directory
	$(V)$(RM) -rf $(OBJDIR)/html
	$(V)$(MKDIR) -p $(OBJDIR)/html
# generate documentation (run the doxygen tool)
	@echo "$(HLG)R Doxygen $(HLR)$(OBJDIR)/html$(HLO)"
	$(V)(\
		$(CAT) Doxyfile; \
		echo "PROJECT_NUMBER = $(BUILDVER)"; \
		echo "OUTPUT_DIRECTORY = $(OBJDIR)"; \
		echo "WARN_LOGFILE = $(OBJDIR)/doxygen_warnings.log"; \
	) | $(DOXYGEN) - | $(TEE) $(OBJDIR)/doxygen.log $(V1)
# make paths in the logfiles relative
	$(V)$(SED) -i -r -e 's@$(CURDIR)/@@g' -e '/^$$/d' $(OBJDIR)/doxygen_warnings.log $(OBJDIR)/doxygen.log
# remove some known won't-fix warnings in the logfile
	$(V)$(PERL) tools/doxylogfix.pl < $(OBJDIR)/doxygen_warnings.log > $(OBJDIR)/doxygen_warnings_clean.log
	$(V)if [ -s $(OBJDIR)/doxygen_warnings_clean.log ]; then \
		$(CAT) $(OBJDIR)/doxygen_warnings_clean.log; \
	else \
		echo "no warnings" > $(OBJDIR)/doxygen_warnings_clean.log; \
	fi
# inject the warnings and doxygen logfiles into the generated HTML
	$(V)$(SED) -i \
		-e '/%DOXYGEN_LOG/r $(OBJDIR)/doxygen.log' \
		-e '/%DOXYGEN_WARNINGS/r $(OBJDIR)/doxygen_warnings_clean.log' \
		-e '/%DOXYGEN_WARNINGS/s/%DOXYGEN_WARNINGS%//' \
		-e '/%DOXYGEN_LOG/s/%DOXYGEN_LOG%//' \
		$(OBJDIR)/html/P_DOXYGEN.html
# make docu files world-readable
	$(V)$(CHMOD) -R a+rX $(OBJDIR)/html
# done
	$(V)$(TOUCH) $@


###############################################################################

# makefile debugging
.PHONY: debugmf
debugmf:
	@echo "----------------------------------------------------------------------"
	@echo "PROJECT='$(PROJECT)'"
	@echo "PROJTITLE='$(PROJTITLE)'"
	@echo "COPYRIGHT='$(COPYRIGHT)'"
	@echo "COPY_EMAIL='$(COPY_EMAIL)'"
	@echo "COPY_HTML='$(COPY_HTML)'"
	@echo "PROJLINK='$(PROJLINK)'"
	@echo "----------------------------------------------------------------------"
	@echo "BUILDVER='$(BUILDVER)'"
	@echo "BUILDVEROLD='$(BUILDVEROLD)'"
	@echo "BUILDDATE='$(BUILDDATE)'"
	@echo "----------------------------------------------------------------------"
	@echo "SDKDIR='$(SDKDIR)'"
	@echo "GCCVERSION=$(GCCVERSION)"
	@echo "----------------------------------------------------------------------"
	@echo "FSIMG='$(FSIMG)'"
	@echo "FSFILES='$(FSFILES)'"
	@echo "----------------------------------------------------------------------"
	@echo "PWD=$(PWD)"
	@echo "CURDIR=$(CURDIR)"
	@echo "TERM=$(TERM)"
	@echo "----------------------------------------------------------------------"
	@echo "DEFS="$(value DEFS) | $(FMT) -t -w 120
	@echo "CFLAGS=$(CFLAGS)" | $(FMT) -t -w 120
	@echo "LDFLAGS=$(LDFLAGS)" | $(FMT) -t -w 120
	@echo "LDLIBS=$(LDLIBS)" | $(FMT) -t -w 120
	@echo "INCFLAGS=$(INCFLAGS)" | $(FMT) -t -w 120
	@echo "----------------------------------------------------------------------"
	@echo "CFILES=$(CFILES)" | $(FMT) -t -w 120
	@echo "HFILES=$(HFILES)" | $(FMT) -t -w 120
	@echo "OFILES=$(OFILES)" | $(FMT) -t -w 120
	@echo "DFILES=$(DFILES)" | $(FMT) -t -w 120
	@echo "----------------------------------------------------------------------"

# eof
