###############################################################################
#                                                                             #
# MAKEFILE for Rpi end of AVRWeather                                          #
#                                                                             #
# (c) Guy Wilson 2019                                                         #
#                                                                             #
###############################################################################

# Version number for WCTL
MAJOR_VERSION = 1
MINOR_VERSION = 3

# Source directory
SOURCE = src

# Build output directory
BUILD = build

# What is our target
TARGET = wctl
VBUILD = vbuild

# C compiler
CPP = g++
C = gcc

# C compiler flags (Release)
CPPFLAGS = -c -Wall -std=c++11
CFLAGS = -c -Wall

# Linker
LINKER = g++

# Libraries
STDLIBS = -pthread -lstdc++
EXTLIBS = -lgpioc -lpq -lcurl

# Dependency generation flags
DEPFLAGS = -MMD -MP

# Mongoose compiler flags
MGFLAGS=

COMPILE.cpp = $(CPP) $(CPPFLAGS) $(DEPFLAGS)
COMPILE.c = $(C) $(CFLAGS) $(DEPFLAGS) $(MGFLAGS)
LINK.o = $(LINKER) $(STDLIBS) 

CSRCFILES = $(wildcard $(SOURCE)/*.c)
CPPSRCFILES = $(wildcard $(SOURCE)/*.cpp)
OBJFILES = $(CSRCFILES:.c=.o) $(CPPSRCFILES:.cpp=.o)
DEPFILES = $(CSRCFILES:.c=.d) $(CPPSRCFILES:.cpp=.d)

# Compile C/C++ source files
#
$(TARGET): $(OBJFILES)
	$(VBUILD) -incfile wctl.ver -template version.c.template -out $(SOURCE)/version.c -major $(MAJOR_VERSION) -minor $(MINOR_VERSION)
	$(C) $(CFLAGS) -o $(BUILD)/version.o $(SOURCE)/version.c
	$(LINK.o) -o $@ $^ $(EXTLIBS)

-include $(DEPFILES)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin
	cp wctl.cfg /etc/weatherctl
	cp resources/css/*.css /var/www/css
	cp resources/html/avr/cmd/*.html /var/www/html/avr/cmd
