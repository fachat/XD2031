HELP="\n\
XD-2031 Firmware Makefile                                            \n\
                                                                     \n\
                                                                     \n\
make [action ... ]  [ <option> ... ]                                 \n\
                                                                     \n\
Options:                                                             \n\
                                                                     \n\
        DEVICE=<yourdevice>   --> overrides devices                  \n\
        V=1                   --> verbose compilation                \n\
        VERSION=x.y.z         --> version number for a release       \n\
        MCU=<cpu>             --> override MCU                       \n\
                                                                     \n\
For AVR specific options, have a look at avr/Makefile                \n\
Options specific for your device are in <yourdevice>/Makefile        \n\
                                                                     \n\
DO NOT EDIT Makefiles to set your options.                           \n\
Your changes would prevent updating with 'git pull'.                 \n\
Store your personal settings here:                                   \n\
                                                                     \n\
        ~/.xd2031/firmware.conf                                      \n\
                                                                     \n\
Some actions:                                                        \n\
                                                                     \n\
make           --> compile                                           \n\
make flash     --> compile and flash (ISP-programming)               \n\
make load      --> compile and update via serial bootloader          \n\
make size      --> show info about memory usage (per file + total)   \n\
make relsize   --> show memory usage expressed as a percentage       \n\
make dist      --> compile for every device and zip binaries         \n\
make clean     --> cleans object and dependencies for current device \n\
make veryclean --> forces cleaning of all generated files            \n\
make tests     --> perform some local tests                          \n\
make update    --> fetch updates and compile the new code            \n\
make doc       --> build doxygen code documentation for the firmware \n\
make help      --> gives this help text                              \n\
\n\n"

BAR="==================================================================="

# Get home directory, called with sudo or not
REALHOME=~$(shell who -m | awk '{print $$1;}')

# make zoo|dist makes no sense with settings for a specific device
# otherwise read user settings for his device
ifndef IGNORE_USER_SETTINGS
   -include $(REALHOME)/.xd2031/firmware.conf
endif

# If no DEVICE=<yourdevice> is given, default to xs1541
DEVICE?=xs1541
DEVICES=xs1541 petSD petSD2
# sockserv contains Linux-specific code, so try that on Linux only
ifeq ($(shell uname -s),Linux)
  DEVICES+=sockserv
endif

# If no VERSION=<x.y.z.> is given, default to today's date
VERSION?=$(shell date +"%Y-%m-%d")

# Enable verbose compile with make V=1
ifeq ($(V),1)
  V1=
  V2=@\#
else
  V1=@
  V2=@echo
endif

.PHONY: clean firmware elf size zoo dumppar doc
.SUFFIXES:
.SUFFIXES: .c .S
all: dumppar firmware

SWNAME=xd2031

# Create automatic dependency files (*.d) on the fly during compilation
AUTODEP=-MMD -MP -MF $(DEPDIR)$(@F).d

# Sorting include paths and defines simplifies checking them
INCPATHS=. ../common $(ARCH) $(DEVICE)
INCLUDE=$(sort $(addprefix -I,$(INCPATHS)))
SDEFS=$(sort $(DEFS))

# CFLAGS/ASFLAGS common to all devices/platforms
CFLAGS+=-Wall -Wextra -Wno-unused-parameter
CFLAGS+=-std=gnu99 $(MCU_CC) $(INCLUDE) $(SDEFS)

ASFLAGS=$(MCU_CC) $(sort -I$(ARCH) -I$(DEVICE))

# OBJDIR contains temporary object and dependency files
OBJDIR=obj/obj-$(DEVICE)-$(MCU)/
DEPDIR=$(OBJDIR).dep/
# ZOOBINDIR contains all binaries
ZOOBINDIR=$(SWNAME)-firmware-$(VERSION)
# THISBINDIR contains distributable binary files for the current device
THISBINDIR=$(ZOOBINDIR)/$(SWNAME)-$(VERSION)-$(DEVICE)-$(MCU)
# BINNAME prefixes binary files which may co-exist in various file formats
BINNAME=$(SWNAME)-$(VERSION)-$(DEVICE)-$(MCU)

# Common source files
SRC=$(wildcard *.c) $(wildcard ../common/*.c)
# The device makefile automatically includes the platform Makefile
include $(DEVICE)/Makefile

# Optional modules (enabled by USE_* in device/Makefile)
ifeq ($(strip $(USE_FAT)),y)
  INCPATHS+=fatfs
  DEFS+=-DUSE_FAT
  SRC+=fatfs/fat_provider.c fatfs/ff.c fatfs/dir.c fatfs/errcompat.c
  SRC+=fatfs/option/ccsbcs.c
endif

# locations where make searches C source files
vpath %.c . ../common $(DEVICE) $(ARCH) fatfs fatfs/option rtc i2c
# locations where make searches Assembler source files
vpath %.S $(DEVICE) $(ARCH)

# Create object names from .c and .S
_OBJ=$(notdir $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(SRC))))
OBJ=$(addprefix $(OBJDIR),$(_OBJ))
# Create dependency names from .o
DEP=$(addprefix $(DEPDIR),$(_OBJ:.o=.o.d))

# Include automatic dependencies, continue if they don't exist yet
-include $(DEP)

# Compile C files
$(OBJDIR)%.o: %.c
	$(V2) CC $<
	$(V1) $(CC) $(CFLAGS) $(AUTODEP) -c $< -o $@

# Assemble S files
$(OBJDIR)%.o: %.S
	$(V2) AS $<
	$(V1) $(AS) $(ASFLAGS) $(AUTODEP) -c $< -o $@

# Link object files
$(TARGET): $(OBJDIR) $(DEPDIR) $(THISBINDIR) $(OBJ)
	$(V2) LD $(notdir $@)
	$(V1) $(LD) -o $@ $(LDFLAGS) $(OBJ)

# Make sure, the arch/Makefile has properly set a MCU switch
MCU_CC?=$(error Undefined or unsupported MCU $(MCU))

# Clean directories
clean: objclean depclean

objclean:
	$(V2) Cleaning object files
	$(V1) rm -f $(OBJ)
	@# The object directory gets removed by make depclean

depclean:
	$(V2) Cleaning dependencies
	$(V1) rm -f $(DEP)
	$(V1) if [ -d $(DEPDIR) ]; then rmdir $(DEPDIR); fi

veryclean:
	rm -rf obj
	rm -rf xd2031-firmware*

# Create build directories
$(OBJDIR):
	$(V1) mkdir -p $@
$(DEPDIR):
	$(V1) mkdir -p $@
$(THISBINDIR):
	$(V1) mkdir -p $@

tests:
	@make --no-print-directory -C pctest tests

dumppar:
	@printf "Target: $(DEVICE)   MCU: $(MCU)   Version: $(VERSION)\n"

# Build for all devices
zoo:
	make --no-print-directory IGNORE_USER_SETTINGS=y _zoo
_zoo: veryclean
	# Build devices with default values
	@for d in $(DEVICES) ; do                                   \
	   printf "\n$(BAR)\n\tTarget: $$d\n$(BAR)\n"              ;\
	   make --no-print-directory DEVICE=$$d VERSION=$(VERSION) ;\
	done
	# Build devices with extra settings
	make --no-print-directory DEVICE=xs1541 VERSION=$(VERSION) MCU=m1284p USE_FAT=y

# Create a zip file with all binaries
dist: zoo
	zip -r $(ZOOBINDIR).zip $(ZOOBINDIR)

# Fetch updates and compile
update:
	git pull
	@make --no-print-directory
	@printf "$(BAR)\n\nNew firmware is ready.\n\n"
	@printf "To upload with a ISP programmer:\n\n\tmake flash\n\n"
	@printf "To upload using USB cable/bootloader:\n\n\tmake load\n\n"

doc:
	mkdir -p ../docsrc/doxygen
	doxygen Doxyfile

help:
	printf $(HELP)
