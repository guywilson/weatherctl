#include <array>
#include <vector>
#include <stdio.h>

#include "logger.h"

using namespace std;

#ifndef _INCL_CSVHELPER
#define _INCL_CSVHELPER

class CSVHelper
{
public:
	static CSVHelper & getInstance()
	{
		static CSVHelper instance;
		return instance;
	}

private:
    FILE *          fptr = NULL;
    int             numColumns = 0;
    Logger &        log = Logger::getInstance();

    CSVHelper();

public:
    ~CSVHelper();

    void            writeHeader(int numColumns, vector<string> & headerArray);
    void            writeRecord(int valueCount, vector<string> & valueArray);

    void            addValue(string & szValue);
};

#endif