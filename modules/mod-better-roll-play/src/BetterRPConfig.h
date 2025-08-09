#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Define.h"

class Creature;
class Player;

namespace BetterRP {

extern bool Enable;

extern uint32 GreetingCooldownMs;
extern bool UseExtraEmote;
extern uint32 ExtraEmoteChance;
extern std::vector<uint32> ExtraEmotes;
extern uint32 YellChance;

extern std::vector<std::string> GreetingEN;
extern std::vector<std::string> GreetingDE;

extern bool TextPoolEnable;
extern std::string PoolFileEN;
extern std::string PoolFileDE;
extern bool PoolReloadCommand;
extern std::unordered_set<uint32> Whitelist;
extern std::unordered_set<uint32> Blacklist;

extern bool AmbientEnable;
extern uint32 AmbientIntervalMs;
extern float AmbientRangeMin;
extern float AmbientRangeMax;
extern std::vector<uint32> AmbientEmotes;
extern uint32 AmbientChance;
extern uint32 AmbientJitterMs;

extern bool ReactToPlayerEmotes;
extern uint32 ReactCooldownMs;
extern float ReactRangeMax;
extern std::vector<uint32> ReactSupported;

extern uint32 GossipBudgetPerMinute;
extern uint32 ReactBudgetPerMinute;

enum class RpType { SAY, YELL, EMOTE, WHISPER };
struct RpLine {
  RpType type;
  std::string category;
  uint32 weight;
  std::string text;
};
using Pool = std::unordered_map<std::string, std::vector<RpLine>>;
extern std::unordered_map<uint8 /*localeIndex*/, Pool> Pools;

void LoadConfig();

std::vector<std::string> &GetLocaleText(uint8 localeIndex,
                                        std::vector<std::string> &en,
                                        std::vector<std::string> &de);
std::string ReplaceName(std::string text, const std::string &name);
bool IsAllowedCreature(Creature *creature);
const RpLine *PickFromPool(const std::string &category, uint8 localeIndex);
std::string ApplyPlaceholders(std::string text, Player *player);

} // namespace BetterRP

