#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Define.h"

namespace BetterRP {

extern std::unordered_map<uint64 /*creature*/,
                           std::unordered_map<uint64 /*player*/, uint32 /*ms*/>>
    GreetingCooldown;
extern std::unordered_map<uint64 /*creature*/,
                           std::unordered_map<uint64 /*player*/, uint32 /*ms*/>>
    ReactCooldown;
extern std::unordered_map<uint64 /*creature*/, uint32 /*ms left*/> AmbientTimers;
extern std::unordered_map<uint64 /*creature*/, uint32 /*ms*/> BudgetReset;
extern std::unordered_map<uint64 /*creature*/, uint32 /*count*/> GossipBudget;
extern std::unordered_map<uint64 /*creature*/, uint32 /*count*/> ReactBudget;
extern std::unordered_map<uint8 /*localeIndex*/, std::unordered_set<std::string>>
    LocaleWarnings;

bool ConsumeBudget(std::unordered_map<uint64, uint32> &budget,
                   std::unordered_map<uint64, uint32> &reset, uint64 key,
                   uint32 &limitPerMin);

} // namespace BetterRP

