#include "Config.h"
#include "Creature.h"
#include "Language.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "WorldSession.h"

#include "Chat.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
 * AzerothCore module: mod-better-roll-play
 * Lightweight NPC roleplay: greetings, random extra emotes, optional
 * follow-ups, ambient idle emotes and reactions to player text emotes – no DB
 * required. MUST be compatible with azerothcore/azerothcore-wotlk (3.3.5a).
 */

namespace BetterRP {

// --------------------------- Config state ---------------------------

bool Enable = true;

uint32 GreetingCooldownMs = 5000;
bool UseExtraEmote = true;
uint32 ExtraEmoteChance = 50;
std::vector<uint32> ExtraEmotes;
uint32 YellChance = 0;

std::vector<std::string> GreetingEN;
std::vector<std::string> GreetingDE;

// --- TextPool (external files) ---
bool TextPoolEnable = false;
std::string PoolFileEN;
std::string PoolFileDE;
bool PoolReloadCommand = true;

std::unordered_set<uint32> Whitelist;
std::unordered_set<uint32> Blacklist;

bool AmbientEnable = false;
uint32 AmbientIntervalMs = 15000;
float AmbientRangeMin = 0.0f;
float AmbientRangeMax = 25.0f;
std::vector<uint32> AmbientEmotes;
uint32 AmbientChance = 100;    // % Chance bei Timer-Feuer
uint32 AmbientJitterMs = 1000; // +/- Jitter pro Intervall

bool ReactToPlayerEmotes = true;
uint32 ReactCooldownMs = 3000;
float ReactRangeMax = 5.0f;
std::vector<uint32> ReactSupported;

// --- Throttle/Budget ---
uint32 GossipBudgetPerMinute = 60; // pro Creature gesprochene Ereignisse/Min.
uint32 ReactBudgetPerMinute = 60;  // pro Creature Emote-Reaktionen/Min.

// --------------------------- Runtime state --------------------------

std::unordered_map<uint64 /*creature*/,
                   std::unordered_map<uint64 /*player*/, uint32 /*ms*/>>
    GreetingCooldown;
std::unordered_map<uint64 /*creature*/,
                   std::unordered_map<uint64 /*player*/, uint32 /*ms*/>>
    ReactCooldown;
std::unordered_map<uint64 /*creature*/, uint32 /*ms left*/> AmbientTimers;
std::unordered_map<uint64 /*creature*/, uint32 /*ms*/> BudgetReset;
std::unordered_map<uint64 /*creature*/, uint32 /*count*/> GossipBudget;
std::unordered_map<uint64 /*creature*/, uint32 /*count*/> ReactBudget;

// --------------------------- Helpers --------------------------------

static inline void Trim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

static std::vector<std::string> Split(const std::string &str, char delim) {
  std::vector<std::string> out;
  std::stringstream ss(str);
  std::string item;
  while (std::getline(ss, item, delim)) {
    Trim(item);
    if (!item.empty())
      out.push_back(item);
  }
  return out;
}

static uint32 TokenToEmote(const std::string &token) {
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
#ifdef EMOTE_ONESHOT_SIT_DOWN
      {"SIT", EMOTE_ONESHOT_SIT_DOWN}
#else
      {"SIT", EMOTE_ONESHOT_SITDOWN}
#endif
  };
  auto itr = map.find(token);
  if (itr != map.end())
    return itr->second;
  return 0;
}

static void ParseEmoteList(const std::string &data, std::vector<uint32> &out) {
  out.clear();
  for (auto const &token : Split(data, ',')) {
    if (uint32 emote = TokenToEmote(token))
      out.push_back(emote);
  }
}

static void ParseIdList(const std::string &data,
                        std::unordered_set<uint32> &out) {
  out.clear();
  for (auto const &token : Split(data, ',')) {
    uint32 id = 0;
    try {
      id = static_cast<uint32>(std::stoul(token));
    } catch (...) {
      id = 0;
    }
    if (id)
      out.insert(id);
  }
}

static std::vector<std::string> &GetLocaleText(uint8 localeIndex,
                                               std::vector<std::string> &en,
                                               std::vector<std::string> &de) {
  if (localeIndex == LOCALE_deDE && !de.empty())
    return de;
  return en;
}

static std::string ReplaceName(std::string text, const std::string &name) {
  size_t pos = 0;
  while ((pos = text.find("{name}", pos)) != std::string::npos) {
    text.replace(pos, 6, name);
    pos += name.length();
  }
  return text;
}

static bool IsAllowedCreature(Creature *creature) {
  uint32 entry = creature->GetEntry();
  if (!Whitelist.empty() && !Whitelist.count(entry))
    return false;
  if (!Blacklist.empty() && Blacklist.count(entry))
    return false;
  return true;
}

static uint32 TextEmoteToEmote(uint32 textEmote) {
  return textEmote; // placeholder for now
}

// -------- TextPool structures ----------
enum class RpType { SAY, YELL, EMOTE, WHISPER };
struct RpLine {
  RpType type;
  std::string category; // "gossip", "ambient", "react_wave" ...
  uint32 weight;
  std::string text;
};

using Pool =
    std::unordered_map<std::string, std::vector<RpLine>>; // category -> lines
static std::unordered_map<uint8 /*localeIndex*/, Pool> Pools; // per locale

static bool ParsePoolLine(const std::string &ln, RpLine &out) {
  if (ln.empty() || ln[0] == '#')
    return false;
  auto parts = Split(ln, '|');
  if (parts.size() < 4)
    return false;
  auto toUpper = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return s;
  };
  const std::string typeS = toUpper(parts[0]);
  if (typeS == "SAY")
    out.type = RpType::SAY;
  else if (typeS == "YELL")
    out.type = RpType::YELL;
  else if (typeS == "EMOTE")
    out.type = RpType::EMOTE;
  else if (typeS == "WHISPER")
    out.type = RpType::WHISPER;
  else
    return false;
  out.category = parts[1];
  try {
    out.weight = std::max<uint32>(1, static_cast<uint32>(std::stoul(parts[2])));
  } catch (...) {
    out.weight = 1;
  }
  out.text = parts[3];
  return !out.text.empty();
}

static bool LoadPoolFile(const std::string &path, uint8 localeIndex) {
  std::ifstream f(path);
  if (!f.is_open())
    return false;
  std::string ln;
  Pool p;
  while (std::getline(f, ln)) {
    Trim(ln);
    RpLine line;
    if (ParsePoolLine(ln, line))
      p[line.category].push_back(line);
  }
  Pools[localeIndex] = std::move(p);
  return true;
}

static const RpLine *PickFromPool(const std::string &category,
                                  uint8 localeIndex) {
  auto pit = Pools.find(localeIndex);
  if (pit == Pools.end())
    return nullptr;
  auto cit = pit->second.find(category);
  if (cit == pit->second.end() || cit->second.empty())
    return nullptr;
  uint32 total = 0;
  for (auto const &l : cit->second)
    total += l.weight;
  uint32 r = urand(1, total);
  for (auto const &l : cit->second) {
    if (r <= l.weight)
      return &l;
    r -= l.weight;
  }
  return &cit->second.back();
}

static std::string ApplyPlaceholders(std::string text, Player *player) {
  text = ReplaceName(text, player->GetName());
  return text;
}

// --- Budget helpers ---
static bool ConsumeBudget(std::unordered_map<uint64, uint32> &budget,
                          std::unordered_map<uint64, uint32> &reset, uint64 key,
                          uint32 &limitPerMin) {
  uint32 now = getMSTime();
  uint32 &rs = reset[key];
  uint32 &used = budget[key];
  if (!rs || now - rs >= 60000) {
    rs = now;
    used = 0;
  }
  if (used >= limitPerMin)
    return false;
  ++used;
  return true;
}

static void LoadConfig() {
  Enable = sConfigMgr->GetOption<bool>("BetterRP.Enable", true);

  GreetingCooldownMs =
      sConfigMgr->GetOption<uint32>("BetterRP.Greeting.CooldownMs", 5000);
  UseExtraEmote =
      sConfigMgr->GetOption<bool>("BetterRP.Greeting.UseExtraEmote", true);
  ExtraEmoteChance =
      sConfigMgr->GetOption<uint32>("BetterRP.Greeting.ExtraEmoteChance", 50);
  ParseEmoteList(
      sConfigMgr->GetOption<std::string>("BetterRP.Greeting.ExtraEmotes",
                                         "WAVE,BOW,DANCE,CHEER,SALUTE"),
      ExtraEmotes);
  YellChance = sConfigMgr->GetOption<uint32>("BetterRP.Greeting.YellChance", 0);

  GreetingEN = Split(sConfigMgr->GetOption<std::string>(
                         "BetterRP.Texts.EN",
                         "Hello, {name}!|Welcome, {name}!|Greetings, {name}!"),
                     '|');
  GreetingDE =
      Split(sConfigMgr->GetOption<std::string>(
                "BetterRP.Texts.DE",
                "Hallo, {name}!|Willkommen, {name}!|Seid gegrüßt, {name}!"),
            '|');

  TextPoolEnable =
      sConfigMgr->GetOption<bool>("BetterRP.TextPool.Enable", false);
  PoolFileEN = sConfigMgr->GetOption<std::string>("BetterRP.TextPool.LocaleEN",
                                                  "better_rp_texts.en.txt");
  PoolFileDE = sConfigMgr->GetOption<std::string>("BetterRP.TextPool.LocaleDE",
                                                  "better_rp_texts.de.txt");
  PoolReloadCommand =
      sConfigMgr->GetOption<bool>("BetterRP.TextPool.ReloadCommand", true);

  ParseIdList(
      sConfigMgr->GetOption<std::string>("BetterRP.Filter.Whitelist", ""),
      Whitelist);
  ParseIdList(
      sConfigMgr->GetOption<std::string>("BetterRP.Filter.Blacklist", ""),
      Blacklist);

  AmbientEnable = sConfigMgr->GetOption<bool>("BetterRP.Ambient.Enable", false);
  AmbientIntervalMs =
      sConfigMgr->GetOption<uint32>("BetterRP.Ambient.IntervalMs", 15000);
  AmbientRangeMin =
      sConfigMgr->GetOption<float>("BetterRP.Ambient.RangeMin", 0.0f);
  AmbientRangeMax =
      sConfigMgr->GetOption<float>("BetterRP.Ambient.RangeMax", 25.0f);
  ParseEmoteList(
      sConfigMgr->GetOption<std::string>(
          "BetterRP.Ambient.Emotes", "LOOK_AROUND,TALK,LAUGH,SIT,EAT,DRINK"),
      AmbientEmotes);
  AmbientChance = sConfigMgr->GetOption<uint32>("BetterRP.Ambient.Chance", 100);
  AmbientJitterMs =
      sConfigMgr->GetOption<uint32>("BetterRP.Ambient.JitterMs", 1000);

  ReactToPlayerEmotes =
      sConfigMgr->GetOption<bool>("BetterRP.ReactToPlayerEmotes", true);
  ReactCooldownMs =
      sConfigMgr->GetOption<uint32>("BetterRP.React.CooldownMs", 3000);
  ReactRangeMax = sConfigMgr->GetOption<float>("BetterRP.React.RangeMax", 5.0f);
  ParseEmoteList(sConfigMgr->GetOption<std::string>(
                     "BetterRP.React.Supported", "WAVE,DANCE,CHEER,SALUTE,BOW"),
                 ReactSupported);

  GossipBudgetPerMinute = sConfigMgr->GetOption<uint32>(
      "BetterRP.Throttle.GossipBudgetPerMinute", 60);
  ReactBudgetPerMinute = sConfigMgr->GetOption<uint32>(
      "BetterRP.Throttle.ReactBudgetPerMinute", 60);

  // Load pools from etc/ (worldserver working dir)
  Pools.clear();
  if (TextPoolEnable) {
    LoadPoolFile(PoolFileEN, LOCALE_enUS);
    LoadPoolFile(PoolFileDE, LOCALE_deDE);
  }
}

} // namespace BetterRP

// --------------------------- Scripts --------------------------------

class BetterRPCreatureScript : public AllCreatureScript {
public:
  BetterRPCreatureScript() : AllCreatureScript("BetterRPCreatureScript") {}

  bool OnGossipHello(Player *player, Creature *creature) override {
    if (!BetterRP::Enable || !player || !creature)
      return false;
    if (!creature->IsAlive() || creature->IsInCombat() || creature->IsMoving())
      return false;
    if (!BetterRP::IsAllowedCreature(creature))
      return false;

    // Per-creature budget guard
    if (!BetterRP::ConsumeBudget(BetterRP::GossipBudget, BetterRP::BudgetReset,
                                 creature->GetGUID().GetRawValue(),
                                 BetterRP::GossipBudgetPerMinute))
      return false;

    const uint64 cGuid = creature->GetGUID().GetRawValue();
    const uint64 pGuid = player->GetGUID().GetRawValue();
    const uint32 now = getMSTime();

    uint32 &last = BetterRP::GreetingCooldown[cGuid][pGuid];
    if (now - last < BetterRP::GreetingCooldownMs)
      return false;
    last = now;

    creature->HandleEmoteCommand(EMOTE_ONESHOT_WAVE);

    // Choose locale list by session locale index (deDE → DE list, else EN)
    uint8 localeIndex = player->GetSession()->GetSessionDbLocaleIndex();
    bool said = false;
    if (BetterRP::TextPoolEnable) {
      if (auto *line = BetterRP::PickFromPool("gossip", localeIndex)) {
        std::string msg = BetterRP::ApplyPlaceholders(line->text, player);
        switch (line->type) {
        case BetterRP::RpType::SAY:
          creature->MonsterSay(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        case BetterRP::RpType::YELL:
          creature->MonsterYell(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        case BetterRP::RpType::EMOTE:
          creature->TextEmote(msg.c_str(), player, true);
          said = true;
          break;
        case BetterRP::RpType::WHISPER:
          creature->Whisper(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        }
      }
    }
    if (!said) {
      auto &list = BetterRP::GetLocaleText(localeIndex, BetterRP::GreetingEN,
                                           BetterRP::GreetingDE);
      if (!list.empty()) {
        std::string msg = BetterRP::ReplaceName(list[urand(0, list.size() - 1)],
                                                player->GetName());
        if (!msg.empty()) {
          if (BetterRP::YellChance && urand(1, 100) <= BetterRP::YellChance)
            creature->MonsterYell(msg.c_str(), LANG_UNIVERSAL, player);
          else
            creature->MonsterSay(msg.c_str(), LANG_UNIVERSAL, player);
        }
      }
    }

    if (BetterRP::UseExtraEmote && !BetterRP::ExtraEmotes.empty() &&
        urand(1, 100) <= BetterRP::ExtraEmoteChance)
      creature->HandleEmoteCommand(
          BetterRP::ExtraEmotes[urand(0, BetterRP::ExtraEmotes.size() - 1)]);

    return false; // keep normal gossip flow
  }

  // NOTE: AllCreatureScript tick hook name is OnAllCreatureUpdate
  void OnAllCreatureUpdate(Creature *creature, uint32 diff) override {
    if (!BetterRP::AmbientEnable || !creature || !creature->IsInWorld())
      return;
    if (!creature->IsAlive() || !creature->IsInCombat() || creature->IsMoving())
      return;
    if (!BetterRP::IsAllowedCreature(creature))
      return;

    const uint64 guid = creature->GetGUID().GetRawValue();
    uint32 &timer = BetterRP::AmbientTimers[guid];
    if (timer > diff) {
      timer -= diff;
      return;
    }

    bool anyPlayerInRange = false;
    for (auto const &ref : creature->GetMap()->GetPlayers()) {
      Player *p = ref.GetSource();
      if (!p || !p->IsAlive())
        continue;
      float d = creature->GetDistance(p);
      if (d >= BetterRP::AmbientRangeMin && d <= BetterRP::AmbientRangeMax) {
        anyPlayerInRange = true;
        break;
      }
    }
    if (!anyPlayerInRange)
      return;

    if (urand(1, 100) <= BetterRP::AmbientChance &&
        !BetterRP::AmbientEmotes.empty())
      creature->HandleEmoteCommand(BetterRP::AmbientEmotes[urand(
          0, BetterRP::AmbientEmotes.size() - 1)]);

    // add jitter to avoid sync spikes
    int32 jitter = int32(BetterRP::AmbientJitterMs) -
                   int32(urand(0, 2 * BetterRP::AmbientJitterMs));
    int32 next =
        std::max<int32>(1000, int32(BetterRP::AmbientIntervalMs) + jitter);
    timer = uint32(next);
  }

  void OnCreatureRemoveWorld(Creature *creature) override {
    const uint64 guid = creature->GetGUID().GetRawValue();
    BetterRP::GreetingCooldown.erase(guid);
    BetterRP::AmbientTimers.erase(guid);
    BetterRP::ReactCooldown.erase(guid);
    BetterRP::BudgetReset.erase(guid);
    BetterRP::GossipBudget.erase(guid);
    BetterRP::ReactBudget.erase(guid);
  }
};

class BetterRPPlayerScript : public PlayerScript {
public:
  BetterRPPlayerScript() : PlayerScript("BetterRPPlayerScript") {}

  void OnTextEmote(Player *player, uint32 /*textEmote*/, uint32 emote,
                   ObjectGuid guid) override {
    if (!BetterRP::Enable || !BetterRP::ReactToPlayerEmotes)
      return;
    if (!player || player->IsInCombat())
      return;

    // Only react to supported emotes
    if (std::find(BetterRP::ReactSupported.begin(),
                  BetterRP::ReactSupported.end(),
                  emote) == BetterRP::ReactSupported.end())
      return;

    if (!guid || !guid.IsCreature())
      return;
    Creature *creature = ObjectAccessor::GetCreature(*player, guid);
    if (!creature)
      return;
    if (!creature->IsAlive() || !creature->IsInCombat() || creature->IsMoving())
      return;
    if (!BetterRP::IsAllowedCreature(creature))
      return;
    if (player->GetDistance(creature) > BetterRP::ReactRangeMax)
      return;

    // Per-creature react budget
    if (!BetterRP::ConsumeBudget(BetterRP::ReactBudget, BetterRP::BudgetReset,
                                 creature->GetGUID().GetRawValue(),
                                 BetterRP::ReactBudgetPerMinute))
      return;

    const uint64 cGuid = creature->GetGUID().GetRawValue();
    const uint64 pGuid = player->GetGUID().GetRawValue();
    const uint32 now = getMSTime();

    uint32 &last = BetterRP::ReactCooldown[cGuid][pGuid];
    if (now - last < BetterRP::ReactCooldownMs)
      return;
    last = now;

    const uint32 delay = urand(100, 300);
    const uint32 anim = emote;
    creature->AddDelayedEvent(delay, [creature, anim]() {
      if (creature)
        creature->HandleEmoteCommand(anim);
    });
  }
};

class BetterRPWorldScript : public WorldScript {
public:
  BetterRPWorldScript() : WorldScript("BetterRPWorldScript") {}

  void OnAfterConfigLoad(bool /*reload*/) override {
    // Load our module config (placed next to other module confs)
    sConfigMgr->LoadMore("mod_better_roll_play.conf");
    BetterRP::LoadConfig();
  }
};

// --- optional command: .btrp reload ---
class BetterRPCommandScript : public CommandScript {
public:
  BetterRPCommandScript() : CommandScript("BetterRPCommandScript") {}

  std::vector<ChatCommand> GetCommands() const override {
    static std::vector<ChatCommand> reloadCmd = {
        {"reload", SEC_GAMEMASTER, Console::No,
         [](ChatHandler *handler, char const *) -> bool {
           if (!BetterRP::PoolReloadCommand) {
             handler->PSendSysMessage("BetterRP: ReloadCommand disabled.");
             return true;
           }
           BetterRP::LoadConfig();
           handler->PSendSysMessage("BetterRP: config & text pools reloaded.");
           return true;
         }}};
    static std::vector<ChatCommand> root = {
        {"btrp", SEC_GAMEMASTER, Console::No, nullptr, "", reloadCmd}};
    return root;
  }
};

// Entry point
void Addmod_better_roll_playScripts() {
  new BetterRPWorldScript();
  new BetterRPCreatureScript();
  new BetterRPPlayerScript();
  new BetterRPCommandScript();
}
