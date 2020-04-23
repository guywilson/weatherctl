#include <string>
#include <map>
#include <vector>
#include <limits.h>

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
    bool                    isConfigured = false;

    ConfigManager() {}

public:
    ~ConfigManager() {}

    void                    initialise(char * pszConfigFileName);
    void                    readConfig();

    const char *            getValue(const char * key);
    bool                    getValueAsBoolean(const char * key);
    int                     getValueAsInteger(const char * key);

    void                    dumpConfig();
};

#endif