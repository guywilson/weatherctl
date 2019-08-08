#include <iostream>
#include <array>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "exception.h"
#include "csvhelper.h"
#include "logger.h"

using namespace std;

CSVHelper::CSVHelper()
{
    vector<string>           header = {"TIME", "TYPE", "TEMPERATURE", "PRESSURE", "HUMIDITY"};

    log = Logger::getInstance();

    fptr = fopen("./tph.csv", "at");

    if (fptr == NULL) {
        log.logError("Failed to open CSV file: %s", strerror(errno));
        throw new Exception("Error opening CSV file");
    }

    writeHeader(header.size(), header);
}

CSVHelper::~CSVHelper()
{
    fclose(fptr);
}

void CSVHelper::writeHeader(int numColumns, vector<string> & headerArray)
{
    int         i;

    if (headerArray.size() != (size_t)numColumns) {
        log.logError("Error: numColumns %d does not match array size %d", numColumns, headerArray.size());
        throw new Exception("Array size does not match column count!");
    }

    this->numColumns = numColumns;

    for (i = 0;i < numColumns;i++) {
        fputs(headerArray.at(i).c_str(), fptr);

        if (i < (numColumns - 1)) {
            fputc(',', fptr);
        }
    }

    fputc('\n', fptr);

    fflush(fptr);
}

void CSVHelper::writeRecord(int valueCount, vector<string> & valueArray)
{
    int         i;

    if (valueCount != numColumns || valueArray.size() != (size_t)numColumns) {
        log.logError("Error: valueCount %d does not match columnCount %d previously specified", valueCount, numColumns);
        throw new Exception("Num values does not match column count!");
    }

    for (i = 0;i< valueCount;i++) {
        fputs(valueArray.at(i).c_str(), fptr);

        if (i < (numColumns - 1)) {
            fputc(',', fptr);
        }
    }

    fputc('\n', fptr);

    fflush(fptr);
}

void CSVHelper::addValue(string & szValue)
{
    static int              cellCount = 0;

    if (cellCount < numColumns) {
        fputs(szValue.c_str(), fptr);

        if (cellCount < (numColumns - 1)) {
            fputc(',', fptr);
        }
    }
    else {
        cellCount = 0;
        fputc('\n', fptr);
        fflush(fptr);
    }
}