/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "Config.h"
#include "ConfigValueCache.h"
#include "Player.h"
#include "ScriptMgr.h"

enum MyPlayerAcoreString
{
    HELLO_WORLD = 35410
};

enum class MyConfig
{
    ENABLED,

    NUM_CONFIGS,
};

class MyConfigData : public ConfigValueCache<MyConfig>
{
public:
    MyConfigData() : ConfigValueCache(MyConfig::NUM_CONFIGS) { };

    void BuildConfigCache() override
    {
        SetConfigValue<bool>(MyConfig::ENABLED, "MyModule.Enable", true);
    }
};

static MyConfigData myConfigData;

// Add player scripts
class MyPlayer : public PlayerScript
{
public:
    MyPlayer() : PlayerScript("MyPlayer") { }

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("MyModule.Enable", false))
            ChatHandler(player->GetSession()).PSendSysMessage(HELLO_WORLD);
    }
};

class MyWorldScript : public WorldScript
{
public:
MyWorldScript() : WorldScript("MyWorldScript", {
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD
    }) { }

    void OnBeforeConfigLoad(bool reload) override
    {
        myConfigData.Initialize(reload);
    }
};

// Add all scripts in one
void AddMyPlayerScripts()
{
    new MyPlayer();
    new MyWorldScript();
}
