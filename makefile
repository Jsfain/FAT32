#################################################
#                   VARIABLES
#################################################

############### compiler & options ##############
CC := avr-gcc
CFLAGS := -Wall -g -Os


################## AVR settings #################
AVRDEVICE := -mmcu=atmega1280
AVRCLOCK := -DF_CPU=16000000


###############  DIRECTORY PATHS ################

# Final Build (Hex) and Object file paths
BUILDDIR := build
OBJDIR := $(BUILDDIR)/obj

# Project source and include file paths
PROJSRCDIR := src
PROJINCDIR := includes

# Library includes and object paths
LIBINCDIR  := lib/includes
LIBOBJDIR  := lib/obj

# Test file path
TESTSRCDIR := test

################ SOUCE(.c) FILES ################

# project source files
projsrcs := $(wildcard $(PROJSRCDIR)/*.c)

# test source file(s). includes main()
testsrc := $(wildcard $(TESTSRCDIR)/*.c)

############# OBJECT & BUILD FILES ##############

# project object files
projobjs := $(patsubst $(PROJSRCDIR)/%.c, \
               $(OBJDIR)/%.o, $(projsrcs))

# test object files
testobj := $(patsubst $(TESTSRCDIR)/%.c, \
               $(OBJDIR)/%.o, $(testsrc))

# library object files.
libobjs  := $(wildcard $(LIBOBJDIR)/*.o)

# final linked object file
target := $(BUILDDIR)/test.elf

# AVR device hex file derived from .elf file
hexfile := $(BUILDDIR)/test.hex


#################################################
#                      RULES
#################################################

#
# RULE: Build, generate hex, and download to AVR
#
all: build hex download

#
# RULE: Build - Compile sources. link objects.
#
build: buildObjDir $(target)

# create build and object directories
buildObjDir:
	mkdir -p build/obj/

# link object files
$(target): $(projobjs) $(testobj)
	$(CC) $(CFLAGS) $(AVRDEVICE) \
  -o $@ $^ $(libobjs)

# compile project source files
$(OBJDIR)/%.o: $(PROJSRCDIR)/%.c
	$(CC) $(CFLAGS) $(AVRCLOCK) $(AVRDEVICE) \
	-I $(PROJINCDIR) -I $(LIBINCDIR) -c $< -o $@

# compile test file
$(testobj): $(testsrc)
	$(CC) $(CFLAGS) $(AVRCLOCK) $(AVRDEVICE) \
	-I $(PROJINCDIR) -I $(LIBINCDIR) -c $< $ -o $@

#
# RULE: Hex - Generate the AVR hex file  
#             from the linked .elf target.
#
hex: 
	avr-objcopy -j .text -j .data -O ihex \
	$(target) $(hexfile)

#
# RULE: Use AVRDUDE to download hex file 
#.      to the AVR device target. 
#
download: 
	avrdude -p atmega1280 -c dragon_jtag \
	-U flash:w:$(hexfile):i -P usb


################
# CLEAN RULES
################

clean: clean_objects clean_target

clean_objects:
	rm -f $(projobjs) $(testobj)

clean_target:
	rm -f $(hexfile) $(target)