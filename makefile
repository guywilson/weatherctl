###############################################################################
#                                                                             #
# MAKEFILE for Rpi end of AVRWeather                                          #
#                                                                             #
# (c) Guy Wilson 2019                                                         #
#                                                                             #
###############################################################################

# Source directory
SOURCE=src

# Build output directory
BUILD=build

# What is our target
WCTL=wctl
TOGGLERST=toggle

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
OBJFILES=$(BUILD)/main.o $(BUILD)/serial.o $(BUILD)/avrweather.o $(BUILD)/frame.o $(BUILD)/currenttime.o $(BUILD)/logger.o $(BUILD)/csvhelper.o $(BUILD)/queuemgr.o $(BUILD)/webconnect.o $(BUILD)/views.o $(BUILD)/exception.o $(BUILD)/mongoose.o

# Target
all: $(WCTL) $(TOGGLERST)

# Compile C source files
#
$(BUILD)/main.o: $(SOURCE)/main.cpp $(SOURCE)/serial.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h $(SOURCE)/currenttime.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/main.o $(SOURCE)/main.cpp

$(BUILD)/serial.o: $(SOURCE)/serial.cpp $(SOURCE)/serial.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/serial.o $(SOURCE)/serial.cpp

$(BUILD)/avrweather.o: $(SOURCE)/avrweather.cpp $(SOURCE)/avrweather.h $(SOURCE)/webconnect.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h $(SOURCE)/csvhelper.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/avrweather.o $(SOURCE)/avrweather.cpp

$(BUILD)/frame.o: $(SOURCE)/frame.cpp $(SOURCE)/frame.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/frame.o $(SOURCE)/frame.cpp

$(BUILD)/currenttime.o: $(SOURCE)/currenttime.cpp $(SOURCE)/currenttime.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/currenttime.o $(SOURCE)/currenttime.cpp

$(BUILD)/logger.o: $(SOURCE)/logger.cpp $(SOURCE)/logger.h $(SOURCE)/currenttime.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/logger.o $(SOURCE)/logger.cpp

$(BUILD)/csvhelper.o: $(SOURCE)/csvhelper.cpp $(SOURCE)/csvhelper.h $(SOURCE)/logger.h $(SOURCE)/exception.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/csvhelper.o $(SOURCE)/csvhelper.cpp

$(BUILD)/queuemgr.o: $(SOURCE)/queuemgr.cpp $(SOURCE)/queuemgr.h $(SOURCE)/exception.h $(SOURCE)/frame.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/queuemgr.o $(SOURCE)/queuemgr.cpp

$(BUILD)/views.o: $(SOURCE)/views.cpp $(SOURCE)/views.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/avrweather.h $(SOURCE)/queuemgr.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/views.o $(SOURCE)/views.cpp

$(BUILD)/webconnect.o: $(SOURCE)/webconnect.cpp $(SOURCE)/webconnect.h $(SOURCE)/logger.h $(SOURCE)/exception.h $(SOURCE)/currenttime.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/webconnect.o $(SOURCE)/webconnect.cpp

$(BUILD)/exception.o: $(SOURCE)/exception.cpp $(SOURCE)/exception.h $(SOURCE)/types.h
	$(CPP) $(CPPFLAGS) -o $(BUILD)/exception.o $(SOURCE)/exception.cpp

$(BUILD)/mongoose.o: $(SOURCE)/mongoose.c
	$(C) $(CFLAGS) $(MGFLAGS) -o $(BUILD)/mongoose.o $(SOURCE)/mongoose.c

$(WCTL): $(OBJFILES)
	$(LINKER) -lpthread -lgpioc -lpq -lcurl -lstdc++ -o $(WCTL) $(OBJFILES)

$(TOGGLERST): $(SOURCE)/toggle.c
	$(C) -Wall -lgpioc -o $(TOGGLERST) $(SOURCE)/toggle.c

test: webtest

webtest: $(BUILD)/exception.o $(BUILD)/currenttime.o $(BUILD)/webconnect.o
	$(LINKER) -lstdc++ -o webtest $(BUILD)/exception.o $(BUILD)/currenttime.o $(BUILD)/webconnect.o
