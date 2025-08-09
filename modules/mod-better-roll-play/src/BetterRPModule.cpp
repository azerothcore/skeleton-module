#include "BetterRPConfig.h"
#include "BetterRPState.h"

#include "Chat.h"
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
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <list>
#include <functional>

namespace BetterRP {

class BetterRPCreatureScript : public AllCreatureScript {
public:
  BetterRPCreatureScript() : AllCreatureScript("BetterRPCreatureScript") {}

  bool OnGossipHello(Player *player, Creature *creature) override {
    if (!Enable || !player || !creature)
      return false;
    if (!creature->IsAlive() || creature->IsInCombat() || creature->IsMoving())
      return false;
    if (!IsAllowedCreature(creature))
      return false;

    if (!ConsumeBudget(GossipBudget, BudgetReset,
                       creature->GetGUID().GetRawValue(),
                       GossipBudgetPerMinute))
      return false;

    const uint64 cGuid = creature->GetGUID().GetRawValue();
    const uint64 pGuid = player->GetGUID().GetRawValue();
    const uint32 now = getMSTime();

    uint32 &last = GreetingCooldown[cGuid][pGuid];
    if (now - last < GreetingCooldownMs)
      return false;
    last = now;

    creature->HandleEmoteCommand(EMOTE_ONESHOT_WAVE);

    uint8 localeIndex = player->GetSession()->GetSessionDbLocaleIndex();
    bool said = false;
    if (TextPoolEnable) {
      if (auto *line = PickFromPool("gossip", localeIndex)) {
        std::string msg = ApplyPlaceholders(line->text, player);
        switch (line->type) {
        case RpType::SAY:
          creature->MonsterSay(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        case RpType::YELL:
          creature->MonsterYell(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        case RpType::EMOTE:
          creature->TextEmote(msg.c_str(), player, true);
          said = true;
          break;
        case RpType::WHISPER:
          creature->Whisper(msg.c_str(), LANG_UNIVERSAL, player);
          said = true;
          break;
        }
      }
    }
    if (!said) {
      auto &list = GetLocaleText(localeIndex, GreetingEN, GreetingDE);
      if (!list.empty()) {
        std::string msg = ReplaceName(list[urand(0, list.size() - 1)],
                                      player->GetName());
        if (!msg.empty()) {
          if (YellChance && urand(1, 100) <= YellChance)
            creature->MonsterYell(msg.c_str(), LANG_UNIVERSAL, player);
          else
            creature->MonsterSay(msg.c_str(), LANG_UNIVERSAL, player);
        }
      }
    }

    if (UseExtraEmote && !ExtraEmotes.empty() &&
        urand(1, 100) <= ExtraEmoteChance)
      creature->HandleEmoteCommand(ExtraEmotes[urand(0, ExtraEmotes.size() - 1)]);

    return false;
  }

  void OnCreatureAddWorld(Creature *creature) override {
    if (!Enable || !AmbientEnable || !creature || !IsAllowedCreature(creature))
      return;
    AmbientTimers[creature->GetGUID().GetRawValue()] = 1;
  }

  void OnCreatureUpdate(Creature *creature, uint32 diff) override {
    if (!Enable || !AmbientEnable || !creature)
      return;

    auto itr = AmbientTimers.find(creature->GetGUID().GetRawValue());
    if (itr == AmbientTimers.end())
      return;
    uint32 &timer = itr->second;
    if (timer > diff) {
      timer -= diff;
      return;
    }

    timer = AmbientIntervalMs;
    bool anyPlayerInRange = false;
    Map *map = creature->GetMap();
    std::list<Player *> players;
    Trinity::AnyPlayerInObjectRangeCheck checker(
        creature, AmbientRangeMax, false);
    Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(
        creature, players, checker);
    creature->VisitNearbyWorldObject(AmbientRangeMax, searcher);
    for (auto *p : players) {
      if (p->IsInWorld() && p->IsAlive() &&
          p->GetDistance(creature) >= AmbientRangeMin) {
        anyPlayerInRange = true;
        break;
      }
    }
    if (!anyPlayerInRange)
      return;

    if (urand(1, 100) <= AmbientChance && !AmbientEmotes.empty())
      creature->HandleEmoteCommand(
          AmbientEmotes[urand(0, AmbientEmotes.size() - 1)]);

    int32 jitter = int32(AmbientJitterMs) -
                   int32(urand(0, 2 * AmbientJitterMs));
    int32 next =
        std::max<int32>(1000, int32(AmbientIntervalMs) + jitter);
    timer = uint32(next);
  }

  void OnCreatureRemoveWorld(Creature *creature) override {
    const uint64 guid = creature->GetGUID().GetRawValue();
    GreetingCooldown.erase(guid);
    AmbientTimers.erase(guid);
    ReactCooldown.erase(guid);
    BudgetReset.erase(guid);
    GossipBudget.erase(guid);
    ReactBudget.erase(guid);
  }
};

class BetterRPPlayerScript : public PlayerScript {
public:
  BetterRPPlayerScript() : PlayerScript("BetterRPPlayerScript") {}

  void OnTextEmote(Player *player, uint32 /*textEmote*/, uint32 emote,
                   ObjectGuid guid) override {
    if (!Enable || !ReactToPlayerEmotes)
      return;
    if (!player || player->IsInCombat())
      return;

    if (std::find(ReactSupported.begin(), ReactSupported.end(), emote) ==
        ReactSupported.end())
      return;

    if (!guid || !guid.IsCreature())
      return;
    Creature *creature = ObjectAccessor::GetCreature(*player, guid);
    if (!creature)
      return;
    if (!creature->IsAlive() || !creature->IsInCombat() || creature->IsMoving())
      return;
    if (!IsAllowedCreature(creature))
      return;
    if (player->GetDistance(creature) > ReactRangeMax)
      return;

    if (!ConsumeBudget(ReactBudget, BudgetReset,
                       creature->GetGUID().GetRawValue(),
                       ReactBudgetPerMinute))
      return;

    const uint64 cGuid = creature->GetGUID().GetRawValue();
    const uint64 pGuid = player->GetGUID().GetRawValue();
    const uint32 now = getMSTime();

    uint32 &last = ReactCooldown[cGuid][pGuid];
    if (now - last < ReactCooldownMs)
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
    sConfigMgr->LoadMore("mod_better_roll_play.conf");
    LoadConfig();
  }
};

} // namespace BetterRP

void AddBetterRPCommandScripts();

void Addmod_better_roll_playScripts() {
  new BetterRP::BetterRPWorldScript();
  new BetterRP::BetterRPCreatureScript();
  new BetterRP::BetterRPPlayerScript();
  AddBetterRPCommandScripts();
}

