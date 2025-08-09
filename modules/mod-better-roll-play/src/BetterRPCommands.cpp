#include "BetterRPConfig.h"

#include "Chat.h"
#include "ScriptMgr.h"

namespace BetterRP {

class BetterRPCommandScript : public CommandScript {
public:
  BetterRPCommandScript() : CommandScript("BetterRPCommandScript") {}

  std::vector<ChatCommand> GetCommands() const override {
    static std::vector<ChatCommand> reloadCmd = {
        {"reload", SEC_GAMEMASTER, Console::No,
         [](ChatHandler *handler, char const *) -> bool {
           if (!PoolReloadCommand) {
             handler->PSendSysMessage("BetterRP: ReloadCommand disabled.");
             return true;
           }
           LoadConfig();
           handler->PSendSysMessage("BetterRP: config & text pools reloaded.");
           return true;
         }}};
    static std::vector<ChatCommand> root = {
        {"btrp", SEC_GAMEMASTER, Console::No, nullptr, "", reloadCmd}};
    return root;
  }
};

} // namespace BetterRP

void AddBetterRPCommandScripts() {
  new BetterRP::BetterRPCommandScript();
}

