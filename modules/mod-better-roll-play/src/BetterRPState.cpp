#include "BetterRPState.h"

#include "Timer.h"

namespace BetterRP {

std::unordered_map<uint64,
                   std::unordered_map<uint64, uint32>> GreetingCooldown;
std::unordered_map<uint64,
                   std::unordered_map<uint64, uint32>> ReactCooldown;
std::unordered_map<uint64, uint32> AmbientTimers;
std::unordered_map<uint64, uint32> BudgetReset;
std::unordered_map<uint64, uint32> GossipBudget;
std::unordered_map<uint64, uint32> ReactBudget;
std::unordered_map<uint8, std::unordered_set<std::string>> LocaleWarnings;

bool ConsumeBudget(std::unordered_map<uint64, uint32> &budget,
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

} // namespace BetterRP

