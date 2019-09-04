#include <map>
#include <vector>

#include "logger.h"

using namespace std;

#ifndef _INCL_CONFIGMGR
#define _INCL_CONFIGMGR

class ConfigManager
{
public:
    static ConfigManager & getInstance() {
        static ConfigManager instance;
        return instance;
    }

private:
    char                    szConfigFileName[PATH_MAX];
    map<string, string>     values;
	Logger & log =          Logger::getInstance();
    bool                    isConfigured = false;

    ConfigManager() {}
    void getCFG();

public:
    ~ConfigManager() {}

    void                    initialise(char * pszConfigFileName);
    void                    readConfig();

    string &                getValue(string key);
    string &                getValue(const char * key);
    const char *            getValueAsCstr(const char * key);
};

#endif