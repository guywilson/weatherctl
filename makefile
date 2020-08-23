###############################################################################
#                                                                             #
# MAKEFILE for Rpi end of AVRWeather                                          #
#                                                                             #
# (c) Guy Wilson 2019                                                         #
#                                                                             #
###############################################################################

# Version number for WCTL
MAJOR_VERSION = 1
MINOR_VERSION = 8

# Directories
SOURCE = src
BUILD = build
DEP = dep

# What is our target
TARGET = wctl

# Tools
VBUILD = vbuild
CPP = g++
C = gcc
LINKER = g++

# postcompile step
PRECOMPILE = @ mkdir -p $(BUILD) $(DEP)
# postcompile step
POSTCOMPILE = @ mv -f $(DEP)/$*.Td $(DEP)/$*.d

CPPFLAGS = -c -O1 -Wall -pedantic -std=c++11
CFLAGS = -c -O1 -Wall -pedantic
MGFLAGS=-DMG_ENABLE_SSL
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEP)/$*.Td

# Libraries
STDLIBS = -pthread -lstdc++
EXTLIBS = -lssl -lcrypto -lgpioc -lpq -lcurl -lstrutils

COMPILE.cpp = $(CPP) $(CPPFLAGS) $(DEPFLAGS) $(MGFLAGS) -o $@
COMPILE.c = $(C) $(CFLAGS) $(DEPFLAGS) $(MGFLAGS) -o $@
LINK.o = $(LINKER) $(STDLIBS) -o $@

CSRCFILES = $(wildcard $(SOURCE)/*.c)
CPPSRCFILES = $(wildcard $(SOURCE)/*.cpp)
OBJFILES = $(patsubst $(SOURCE)/%.c, $(BUILD)/%.o, $(CSRCFILES)) $(patsubst $(SOURCE)/%.cpp, $(BUILD)/%.o, $(CPPSRCFILES))
DEPFILES = $(patsubst $(SOURCE)/%.c, $(DEP)/%.d, $(CSRCFILES)) $(patsubst $(SOURCE)/%.cpp, $(DEP)/%.d, $(CPPSRCFILES))

all: $(TARGET)

# Compile C/C++ source files
#
$(TARGET): $(OBJFILES)
	$(LINK.o) $^ $(EXTLIBS)

$(BUILD)/%.o: $(SOURCE)/%.c
$(BUILD)/%.o: $(SOURCE)/%.c $(DEP)/%.d
	$(PRECOMPILE)
	$(COMPILE.c) $<
	$(POSTCOMPILE)

$(BUILD)/%.o: $(SOURCE)/%.cpp
$(BUILD)/%.o: $(SOURCE)/%.cpp $(DEP)/%.d
	$(PRECOMPILE)
	$(COMPILE.cpp) $<
	$(POSTCOMPILE)

.PRECIOUS = $(DEP)/%.d
$(DEP)/%.d: ;

-include $(DEPFILES)

install: $(TARGET)
	cp $(TARGET) /sandiskusb/bin
	cp wctl.cfg /sandiskusb/weatherctl
	chmod 600 /sandiskusb/weatherctl/wctl.cfg
	rm -r /sandiskusb/www/css
	rm -r /sandiskusb/www/html
	cp -r resources/* /sandiskusb/www

version:
	$(VBUILD) -incfile wctl.ver -template version.c.template -out $(SOURCE)/version.c -major $(MAJOR_VERSION) -minor $(MINOR_VERSION)

clean:
	rm -r $(BUILD)
	rm -r $(DEP)
	rm $(TARGET)
