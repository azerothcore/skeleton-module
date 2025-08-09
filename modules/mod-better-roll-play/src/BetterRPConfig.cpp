#include "BetterRPConfig.h"

#include "Config.h"
#include "Creature.h"
#include "Language.h"
#include "Player.h"
#include "World.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace BetterRP {

bool Enable = true;

uint32 GreetingCooldownMs = 5000;
bool UseExtraEmote = true;
uint32 ExtraEmoteChance = 50;
std::vector<uint32> ExtraEmotes;
uint32 YellChance = 0;

std::vector<std::string> GreetingEN;
std::vector<std::string> GreetingDE;

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
uint32 AmbientChance = 100;
uint32 AmbientJitterMs = 1000;

bool ReactToPlayerEmotes = true;
uint32 ReactCooldownMs = 3000;
float ReactRangeMax = 5.0f;
std::vector<uint32> ReactSupported;

uint32 GossipBudgetPerMinute = 60;
uint32 ReactBudgetPerMinute = 60;

std::unordered_map<uint8 /*localeIndex*/, Pool> Pools;

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

std::vector<std::string> &GetLocaleText(uint8 localeIndex,
                                        std::vector<std::string> &en,
                                        std::vector<std::string> &de) {
  if (localeIndex == LOCALE_deDE && !de.empty())
    return de;
  return en;
}

std::string ReplaceName(std::string text, const std::string &name) {
  size_t pos = 0;
  while ((pos = text.find("{name}", pos)) != std::string::npos) {
    text.replace(pos, 6, name);
    pos += name.length();
  }
  return text;
}

bool IsAllowedCreature(Creature *creature) {
  uint32 entry = creature->GetEntry();
  if (!Whitelist.empty() && !Whitelist.count(entry))
    return false;
  if (!Blacklist.empty() && Blacklist.count(entry))
    return false;
  return true;
}

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

const RpLine *PickFromPool(const std::string &category, uint8 localeIndex) {
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

std::string ApplyPlaceholders(std::string text, Player *player) {
  text = ReplaceName(text, player->GetName());
  return text;
}

void LoadConfig() {
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
                     "BetterRP.React.Supported",
                     "WAVE,DANCE,CHEER,SALUTE,BOW"),
                 ReactSupported);

  GossipBudgetPerMinute = sConfigMgr->GetOption<uint32>(
      "BetterRP.Throttle.GossipBudgetPerMinute", 60);
  ReactBudgetPerMinute = sConfigMgr->GetOption<uint32>(
      "BetterRP.Throttle.ReactBudgetPerMinute", 60);

  Pools.clear();
  if (TextPoolEnable) {
    LoadPoolFile(PoolFileEN, LOCALE_enUS);
    LoadPoolFile(PoolFileDE, LOCALE_deDE);
  }
}

} // namespace BetterRP

