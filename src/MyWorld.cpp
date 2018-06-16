#include "Configuration/Config.h"
#include "ScriptMgr.h"

class MyWorld : public WorldScript
{
public:
    MyWorld() : WorldScript("MyWorld") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) 
        {
            std::string conf_path = _CONF_DIR;
            std::string cfg_file = conf_path + "/my_custom.conf";            
            
#if PLATFORM == PLATFORM_WINDOWS
            cfg_file = "my_custom.conf";
#endif
            std::string cfg_def_file = cfg_file + ".dist";
            
            // Load .conf.dist config
            if (!sConfigMgr->LoadMore(cfg_def_file.c_str()))
            {
                sLog->outString();
                sLog->outError("Module config: Invalid or missing configuration dist file : %s", cfg_def_file.c_str());
                sLog->outError("Module config: Verify that the file exists and has \'[worldserver]' written in the top of the file!");
                sLog->outError("Module config: Use default settings!");
                sLog->outString();
            }
            
            // Load .conf config
            if (!sConfigMgr->LoadMore(cfg_file.c_str()))
            {
                sLog->outString();
                sLog->outError("Module config: Invalid or missing configuration file : %s", cfg_file.c_str());
                sLog->outError("Module config: Verify that the file exists and has \'[worldserver]' written in the top of the file!");
                sLog->outError("Module config: Use default settings!");
                sLog->outString();
            }
        }
    }
};

void AddMyWorldScripts()
{
    new MyWorld();
}
