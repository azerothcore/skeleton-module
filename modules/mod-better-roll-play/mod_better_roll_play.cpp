#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Config.h"
#include "WorldSession.h"
#include "Language.h"
#include "SharedDefines.h"
#include "ObjectAccessor.h"
#include "Timer.h"
#include "Map.h"
#include "Random.h"

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <functional>

/*
 * AzerothCore module: mod-better-roll-play
 * Improves NPC roleplay interactions such as greetings, ambient emotes and
 * reactions to player emotes without requiring any database changes.
 */

namespace BetterRP
{
    // configuration values
    bool Enable = true;
    uint32 GreetingCooldownMs = 5000;
    bool UseExtraEmote = true;
    uint32 ExtraEmoteChance = 50;
    std::vector<uint32> ExtraEmotes;
    uint32 YellChance = 0;
    std::vector<std::string> GreetingEN;
    std::vector<std::string> GreetingDE;
    bool FollowupEnable = true;
    uint32 FollowupChance = 30;
    std::vector<std::string> FollowupEN;
    std::vector<std::string> FollowupDE;
    std::unordered_set<uint32> Whitelist;
    std::unordered_set<uint32> Blacklist;
    bool AmbientEnable = false;
    uint32 AmbientIntervalMs = 15000;
    float AmbientRangeMin = 0.0f;
    float AmbientRangeMax = 25.0f;
    std::vector<uint32> AmbientEmotes;
    bool ReactToPlayerEmotes = true;
    uint32 ReactCooldownMs = 3000;
    float ReactRangeMax = 5.0f;
    std::vector<uint32> ReactSupported;

    // cooldown maps
    std::unordered_map<uint64, std::unordered_map<uint64, uint32>> GreetingCooldown;
    std::unordered_map<uint64, std::unordered_map<uint64, uint32>> ReactCooldown;
    std::unordered_map<uint64, uint32> AmbientTimers;

    // string helpers
    static inline void Trim(std::string& s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }

    static std::vector<std::string> Split(const std::string& str, char delim)
    {
        std::vector<std::string> out;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, delim))
        {
            Trim(item);
            if (!item.empty())
                out.push_back(item);
        }
        return out;
    }

    static uint32 TokenToEmote(const std::string& token)
    {
        static const std::unordered_map<std::string, uint32> map = {
            {"WAVE", EMOTE_ONESHOT_WAVE},
            {"BOW", EMOTE_ONESHOT_BOW},
            {"DANCE", EMOTE_ONESHOT_DANCE},
            {"CHEER", EMOTE_ONESHOT_CHEER},
            {"SALUTE", EMOTE_ONESHOT_SALUTE},
            {"LOOK_AROUND", EMOTE_ONESHOT_LOOK_AROUND},
            {"TALK", EMOTE_ONESHOT_TALK},
            {"EAT", EMOTE_ONESHOT_EAT},
            {"DRINK", EMOTE_ONESHOT_DRINK},
            {"LAUGH", EMOTE_ONESHOT_LAUGH},
            {"KNEEL", EMOTE_ONESHOT_KNEEL},
            {"SIT", EMOTE_ONESHOT_SIT}
        };
        auto itr = map.find(token);
        if (itr != map.end())
            return itr->second;
        return 0;
    }

    static void ParseEmoteList(const std::string& data, std::vector<uint32>& out)
    {
        out.clear();
        for (auto const& token : Split(data, ','))
        {
            if (uint32 emote = TokenToEmote(token))
                out.push_back(emote);
        }
    }

    static void ParseIdList(const std::string& data, std::unordered_set<uint32>& out)
    {
        out.clear();
        for (auto const& token : Split(data, ','))
        {
            uint32 id = 0;
            try
            {
                id = static_cast<uint32>(std::stoul(token));
            }
            catch (...) { id = 0; }
            if (id)
                out.insert(id);
        }
    }

    static std::vector<std::string>& GetLocaleText(LocaleConstant locale, std::vector<std::string>& en, std::vector<std::string>& de)
    {
        if (locale == LOCALE_deDE && !de.empty())
            return de;
        return en;
    }

    static std::string ReplaceName(std::string text, const std::string& name)
    {
        size_t pos = 0;
        while ((pos = text.find("{name}", pos)) != std::string::npos)
        {
            text.replace(pos, 6, name);
            pos += name.length();
        }
        return text;
    }

    static bool IsAllowedCreature(Creature* creature)
    {
        uint32 entry = creature->GetEntry();
        if (!Whitelist.empty() && !Whitelist.count(entry))
            return false;
        if (!Blacklist.empty() && Blacklist.count(entry))
            return false;
        return true;
    }

    static void LoadConfig()
    {
        Enable = sConfigMgr->GetOption<bool>("BetterRP.Enable", true);

        GreetingCooldownMs = sConfigMgr->GetOption<uint32>("BetterRP.Greeting.CooldownMs", 5000);
        UseExtraEmote = sConfigMgr->GetOption<bool>("BetterRP.Greeting.UseExtraEmote", true);
        ExtraEmoteChance = sConfigMgr->GetOption<uint32>("BetterRP.Greeting.ExtraEmoteChance", 50);
        ParseEmoteList(sConfigMgr->GetOption<std::string>("BetterRP.Greeting.ExtraEmotes", "WAVE,BOW,DANCE,CHEER,SALUTE"), ExtraEmotes);
        YellChance = sConfigMgr->GetOption<uint32>("BetterRP.Greeting.YellChance", 0);

        GreetingEN = Split(sConfigMgr->GetOption<std::string>("BetterRP.Texts.EN", "Hello, {name}!|Welcome, {name}!|Greetings, {name}!"), '|');
        GreetingDE = Split(sConfigMgr->GetOption<std::string>("BetterRP.Texts.DE", "Hallo, {name}!|Willkommen, {name}!|Seid gegrüßt, {name}!"), '|');

        FollowupEnable = sConfigMgr->GetOption<bool>("BetterRP.Followup.Enable", true);
        FollowupChance = sConfigMgr->GetOption<uint32>("BetterRP.Followup.Chance", 30);
        FollowupEN = Split(sConfigMgr->GetOption<std::string>("BetterRP.Followup.Texts.EN", "Safe travels!|How can I help you?"), '|');
        FollowupDE = Split(sConfigMgr->GetOption<std::string>("BetterRP.Followup.Texts.DE", "Gute Reise!|Wie kann ich helfen?"), '|');

        ParseIdList(sConfigMgr->GetOption<std::string>("BetterRP.Filter.Whitelist", ""), Whitelist);
        ParseIdList(sConfigMgr->GetOption<std::string>("BetterRP.Filter.Blacklist", ""), Blacklist);

        AmbientEnable = sConfigMgr->GetOption<bool>("BetterRP.Ambient.Enable", false);
        AmbientIntervalMs = sConfigMgr->GetOption<uint32>("BetterRP.Ambient.IntervalMs", 15000);
        AmbientRangeMin = sConfigMgr->GetOption<float>("BetterRP.Ambient.RangeMin", 0.0f);
        AmbientRangeMax = sConfigMgr->GetOption<float>("BetterRP.Ambient.RangeMax", 25.0f);
        ParseEmoteList(sConfigMgr->GetOption<std::string>("BetterRP.Ambient.Emotes", "LOOK_AROUND,TALK,LAUGH,SIT,EAT,DRINK"), AmbientEmotes);

        ReactToPlayerEmotes = sConfigMgr->GetOption<bool>("BetterRP.ReactToPlayerEmotes", true);
        ReactCooldownMs = sConfigMgr->GetOption<uint32>("BetterRP.React.CooldownMs", 3000);
        ReactRangeMax = sConfigMgr->GetOption<float>("BetterRP.React.RangeMax", 5.0f);
        ParseEmoteList(sConfigMgr->GetOption<std::string>("BetterRP.React.Supported", "WAVE,DANCE,CHEER,SALUTE,BOW"), ReactSupported);
    }
}

class BetterRPCreatureScript : public AllCreatureScript
{
public:
    BetterRPCreatureScript() : AllCreatureScript("BetterRPCreatureScript") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!BetterRP::Enable || !player || !creature)
            return false;
        if (!creature->IsAlive() || creature->IsInCombat() || creature->IsMoving())
            return false;
        if (!creature->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
            return false;
        if (!BetterRP::IsAllowedCreature(creature))
            return false;

        uint64 cGuid = creature->GetGUID().GetRawValue();
        uint64 pGuid = player->GetGUID().GetRawValue();
        uint32 now = getMSTime();
        uint32& last = BetterRP::GreetingCooldown[cGuid][pGuid];
        if (now - last < BetterRP::GreetingCooldownMs)
            return false;
        last = now;

        creature->HandleEmoteCommand(EMOTE_ONESHOT_WAVE);

        LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
        auto& list = BetterRP::GetLocaleText(locale, BetterRP::GreetingEN, BetterRP::GreetingDE);
        if (!list.empty())
        {
            std::string msg = BetterRP::ReplaceName(list[urand(0, list.size() - 1)], player->GetName());
            if (BetterRP::YellChance && urand(1, 100) <= BetterRP::YellChance)
                creature->MonsterYell(msg.c_str(), LANG_UNIVERSAL, player);
            else
                creature->MonsterSay(msg.c_str(), LANG_UNIVERSAL, player);
        }

        if (BetterRP::UseExtraEmote && !BetterRP::ExtraEmotes.empty() && urand(1, 100) <= BetterRP::ExtraEmoteChance)
            creature->HandleEmoteCommand(BetterRP::ExtraEmotes[urand(0, BetterRP::ExtraEmotes.size() - 1)]);

        if (BetterRP::FollowupEnable && !BetterRP::FollowupEN.empty())
        {
            if (urand(1, 100) <= BetterRP::FollowupChance)
            {
                auto& followList = BetterRP::GetLocaleText(locale, BetterRP::FollowupEN, BetterRP::FollowupDE);
                if (!followList.empty())
                {
                    std::string fmsg = BetterRP::ReplaceName(followList[urand(0, followList.size() - 1)], player->GetName());
                    creature->MonsterSay(fmsg.c_str(), LANG_UNIVERSAL, player);
                }
            }
        }

        return false;
    }

    void OnCreatureUpdate(Creature* creature, uint32 diff) override
    {
        if (!BetterRP::AmbientEnable || !creature || !creature->IsInWorld())
            return;
        if (!creature->IsAlive() || creature->IsInCombat() || creature->IsMoving())
            return;
        if (!BetterRP::IsAllowedCreature(creature))
            return;

        uint64 guid = creature->GetGUID().GetRawValue();
        uint32& timer = BetterRP::AmbientTimers[guid];
        if (timer > diff)
        {
            timer -= diff;
            return;
        }

        bool hasPlayer = false;
        Map::PlayerList const& players = creature->GetMap()->GetPlayers();
        for (auto const& ref : players)
        {
            Player* p = ref.GetSource();
            if (!p || !p->IsAlive())
                continue;
            float dist = creature->GetDistance(p);
            if (dist >= BetterRP::AmbientRangeMin && dist <= BetterRP::AmbientRangeMax)
            {
                hasPlayer = true;
                break;
            }
        }

        if (!hasPlayer)
            return;

        if (!BetterRP::AmbientEmotes.empty())
            creature->HandleEmoteCommand(BetterRP::AmbientEmotes[urand(0, BetterRP::AmbientEmotes.size() - 1)]);

        timer = BetterRP::AmbientIntervalMs;
    }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        uint64 guid = creature->GetGUID().GetRawValue();
        BetterRP::GreetingCooldown.erase(guid);
        BetterRP::AmbientTimers.erase(guid);
        BetterRP::ReactCooldown.erase(guid);
    }
};

class BetterRPPlayerScript : public PlayerScript
{
public:
    BetterRPPlayerScript() : PlayerScript("BetterRPPlayerScript") { }

    void OnTextEmote(Player* player, uint32 /*textEmote*/, uint32 emote, ObjectGuid guid) override
    {
        if (!BetterRP::Enable || !BetterRP::ReactToPlayerEmotes)
            return;
        if (!player || player->IsInCombat())
            return;
        if (std::find(BetterRP::ReactSupported.begin(), BetterRP::ReactSupported.end(), emote) == BetterRP::ReactSupported.end())
            return;

        Creature* creature = nullptr;
        if (guid && guid.IsCreature())
            creature = ObjectAccessor::GetCreature(*player, guid);
        if (!creature)
            return;
        if (!creature->IsAlive() || creature->IsInCombat() || creature->IsMoving())
            return;
        if (!creature->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
            return;
        if (!BetterRP::IsAllowedCreature(creature))
            return;
        if (player->GetDistance(creature) > BetterRP::ReactRangeMax)
            return;

        uint64 cGuid = creature->GetGUID().GetRawValue();
        uint64 pGuid = player->GetGUID().GetRawValue();
        uint32 now = getMSTime();
        uint32& last = BetterRP::ReactCooldown[cGuid][pGuid];
        if (now - last < BetterRP::ReactCooldownMs)
            return;
        last = now;

        uint32 delay = urand(100, 300);
        uint32 em = emote;
        creature->AddDelayedEvent(delay, [creature, em]()
        {
            if (creature)
                creature->HandleEmoteCommand(em);
        });
    }
};

class BetterRPWorldScript : public WorldScript
{
public:
    BetterRPWorldScript() : WorldScript("BetterRPWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sConfigMgr->LoadMore("mod_better_roll_play.conf");
        BetterRP::LoadConfig();
    }
};

void Addmod_better_roll_playScripts()
{
    new BetterRPWorldScript();
    new BetterRPCreatureScript();
    new BetterRPPlayerScript();
}
