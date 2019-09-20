###############################################################################
#                                                                             #
# MAKEFILE for Rpi end of AVRWeather                                          #
#                                                                             #
# (c) Guy Wilson 2019                                                         #
#                                                                             #
###############################################################################

# Version number for WCTL
MAJOR_VERSION=1
MINOR_VERSION=3

# Source directory
SOURCE=src

# Build output directory
BUILD=build

# What is our target
WCTL=wctl
TOGGLERST=toggle
VBUILD=vbuild

# C compiler
CPP=g++
C=gcc

# Linker
LINKER=g++

# C compiler flags (Release)
CPPFLAGS=-c -Wall -std=c++11
CFLAGS=-c -Wall

# Mongoose compiler flags
MGFLAGS=

# Object files 
OBJFILES=$(BUILD)/main.o $(BUILD)/threads.o $(BUILD)/serial.o $(BUILD)/avrweather.o $(BUILD)/frame.o $(BUILD)/currenttime.o $(BUILD)/logger.o $(BUILD)/backup.o $(BUILD)/configmgr.o $(BUILD)/queuemgr.o $(BUILD)/webadmin.o $(BUILD)/rest.o $(BUILD)/views.o $(BUILD)/exception.o $(BUILD)/strutils.o $(BUILD)/mongoose.o

# Target
all: $(WCTL) $(TOGGLERST)

# Compile C source files
#
$(BUILD)/main.o: $(SOURCE)/main.cpp $(SOURCE)/serial.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h $(SOURCE)/webadmin.h $(SOURCE)/queuemgr.h $(SOURCE)/backup.h $(SOURCE)/views.h $(SOURCE)/configmgr.h $(SOURCE)/rest.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/main.o $(SOURCE)/main.cpp

$(BUILD)/threads.o: $(SOURCE)/threads.cpp $(SOURCE)/threads.h $(SOURCE)/serial.h $(SOURCE)/logger.h $(SOURCE)/queuemgr.h $(SOURCE)/frame.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h $(SOURCE)/backup.h $(SOURCE)/webadmin.h $(SOURCE)/rest.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/threads.o $(SOURCE)/threads.cpp

$(BUILD)/serial.o: $(SOURCE)/serial.cpp $(SOURCE)/serial.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/serial.o $(SOURCE)/serial.cpp

$(BUILD)/avrweather.o: $(SOURCE)/avrweather.cpp $(SOURCE)/avrweather.h $(SOURCE)/webadmin.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h $(SOURCE)/backup.h $(SOURCE)/queuemgr.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/avrweather.o $(SOURCE)/avrweather.cpp

$(BUILD)/frame.o: $(SOURCE)/frame.cpp $(SOURCE)/frame.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/frame.o $(SOURCE)/frame.cpp

$(BUILD)/currenttime.o: $(SOURCE)/currenttime.cpp $(SOURCE)/currenttime.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/currenttime.o $(SOURCE)/currenttime.cpp

$(BUILD)/logger.o: $(SOURCE)/logger.cpp $(SOURCE)/logger.h $(SOURCE)/currenttime.h $(SOURCE)/strutils.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/logger.o $(SOURCE)/logger.cpp

$(BUILD)/backup.o: $(SOURCE)/backup.cpp $(SOURCE)/backup.h $(SOURCE)/webadmin.h $(SOURCE)/logger.h $(SOURCE)/exception.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/backup.o $(SOURCE)/backup.cpp

$(BUILD)/configmgr.o: $(SOURCE)/configmgr.cpp $(SOURCE)/configmgr.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/strutils.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/configmgr.o $(SOURCE)/configmgr.cpp

$(BUILD)/queuemgr.o: $(SOURCE)/queuemgr.cpp $(SOURCE)/queuemgr.h $(SOURCE)/exception.h $(SOURCE)/frame.h $(SOURCE)/avrweather.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/queuemgr.o $(SOURCE)/queuemgr.cpp

$(BUILD)/views.o: $(SOURCE)/views.cpp $(SOURCE)/views.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/postdata.h $(SOURCE)/avrweather.h $(SOURCE)/frame.h $(SOURCE)/queuemgr.h $(SOURCE)/mongoose.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/views.o $(SOURCE)/views.cpp

$(BUILD)/webadmin.o: $(SOURCE)/webadmin.cpp $(SOURCE)/webadmin.h $(SOURCE)/avrweather.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h $(SOURCE)/queuemgr.h $(SOURCE)/backup.h $(SOURCE)/configmgr.h $(SOURCE)/mongoose.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/webadmin.o $(SOURCE)/webadmin.cpp

$(BUILD)/rest.o: $(SOURCE)/rest.cpp $(SOURCE)/rest.h $(SOURCE)/postdata.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h $(SOURCE)/queuemgr.h $(SOURCE)/backup.h $(SOURCE)/configmgr.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/rest.o $(SOURCE)/rest.cpp

$(BUILD)/exception.o: $(SOURCE)/exception.cpp $(SOURCE)/exception.h $(SOURCE)/types.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/exception.o $(SOURCE)/exception.cpp

$(BUILD)/strutils.o: $(SOURCE)/strutils.c $(SOURCE)/strutils.h
	$(C) $(CFLAGS) -o $(BUILD)/strutils.o $(SOURCE)/strutils.c

$(BUILD)/mongoose.o: $(SOURCE)/mongoose.c $(SOURCE)/mongoose.h
	$(C) $(CFLAGS) $(MGFLAGS) -o $(BUILD)/mongoose.o $(SOURCE)/mongoose.c

$(BUILD)/toggle.o: $(SOURCE)/toggle.c
	$(C) $(CFLAGS) -o $(BUILD)/toggle.o $(SOURCE)/toggle.c

$(WCTL): $(OBJFILES)
	$(VBUILD) -incfile wctl.ver -template version.c.template -out $(SOURCE)/version.c -major $(MAJOR_VERSION) -minor $(MINOR_VERSION)
	$(C) $(CFLAGS) -o $(BUILD)/version.o $(SOURCE)/version.c
	$(LINKER) -pthread -lstdc++ -o $(WCTL) $(OBJFILES) $(BUILD)/version.o -lgpioc -lpq -lcurl

$(TOGGLERST): $(BUILD)/toggle.o
	$(C) -o $(TOGGLERST) $(BUILD)/toggle.o -lgpioc

install: $(WCTL)
	cp $(WCTL) /usr/local/bin
	cp wctl.cfg /etc/weatherctl
	cp resources/css/*.css /var/www/css
	cp resources/html/avr/cmd/*.html /var/www/html/avr/cmd
