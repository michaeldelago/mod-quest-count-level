#include "Chat.h"
#include "Configuration/Config.h"
#include "DataMap.h"
#include "DatabaseEnv.h"
#include "Hyperlinks.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "World.h"

#if AC_COMPILER == AC_COMPILER_GNU
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Acore::ChatCommands;

bool QuestCountLevelEnabled = true;
bool QuestCountLevelAnnounce = true;
bool QuestCountLevelAllowRepeatable = false;
uint32 DefaultQuestCount = 15;
uint32 LevelUpItem = 701001;
uint32 MinimumLevel = 10;
uint32 DungeonQuestCredit = 3;

void savePlayerData(Player* p);

class Quest_Count_Level_conf : public WorldScript {
 public:
  Quest_Count_Level_conf() : WorldScript("Quest_Count_Level_conf_conf") {}

  void OnBeforeConfigLoad(bool /*reload*/) override {
    QuestCountLevelAnnounce =
        sConfigMgr->GetBoolDefault("QuestCountLevel.Announce", 1);
    QuestCountLevelEnabled =
        sConfigMgr->GetBoolDefault("QuestCountLevel.Enabled", 1);
    QuestCountLevelAllowRepeatable =
        sConfigMgr->GetBoolDefault("QuestCountLevel.AllowRepeatable", 0);
    DefaultQuestCount =
        sConfigMgr->GetIntDefault("QuestCountLevel.QuestCount", 15);
    LevelUpItem = sConfigMgr->GetIntDefault("QuestCountLevel.ItemID", 701001);
    MinimumLevel =
        sConfigMgr->GetIntDefault("QuestCountLevel.MinimumLevel", 10);
    DungeonQuestCredit =
        sConfigMgr->GetIntDefault("QuestCountLevel.DungeonQuestCredit", 3);
  }
};

class Quest_Count_Level_Announce : public PlayerScript {
 public:
  Quest_Count_Level_Announce() : PlayerScript("Quest_Count_Level_Announce") {}

  void OnLogin(Player* player) {
    if (QuestCountLevelEnabled && QuestCountLevelAnnounce) {
      ChatHandler(player->GetSession())
          .SendSysMessage(
              "This server is running the |cff4CFF00QuestCountLevel |rmodule");
    }
  }
};

class PlayerQuestCount : public DataMap::Base {
 public:
  int8 QuestCount = DefaultQuestCount;
  bool PlayerQuestCountEnabled = false;
  PlayerQuestCount() {}
  PlayerQuestCount(int8 count, bool enabled) {
    QuestCount = count;
    PlayerQuestCountEnabled = enabled;
  }
};

class Quest_Count_Level : public PlayerScript {
 public:
  Quest_Count_Level() : PlayerScript("Quest_Count_Level") {}

  void OnLogin(Player* p) override {
    QueryResult result = CharacterDatabase.Query(
        "SELECT `QuestCount`,`Enabled` FROM `questcountlevel` WHERE "
        "`CharacterGUID` = '{}'",
        p->GetGUID().GetCounter());
    if (!result) {
      p->CustomData.GetDefault<PlayerQuestCount>("Quest_Count_Level")
          ->QuestCount = DefaultQuestCount;
    } else {
      Field* fields = result->Fetch();
      auto playerData =
          new PlayerQuestCount(fields[0].Get<int8>(), fields[1].Get<bool>());
      p->CustomData.Set("Quest_Count_Level", playerData);
    }
  }

  void OnLogout(Player* p) override { savePlayerData(p); }

  void OnSave(Player* p) override { savePlayerData(p); }

  void OnPlayerCompleteQuest(Player* p, Quest const* q) override {
    int32 maxQuestLevel = q->GetMaxLevel();
    uint8 playerLevel = p->GetLevel();
    uint32 suggestedPlayers = q->GetSuggestedPlayers();
    bool dungeonQuest = (q->IsDFQuest() || q->GetType() == QUEST_TYPE_DUNGEON);
    PlayerQuestCount* playerData =
        p->CustomData.Get<PlayerQuestCount>("Quest_Count_Level");

    if ((!playerData) ||                           // Failed to get playerData
        (!playerData->PlayerQuestCountEnabled) ||  // QuestCountLeveling isn't
                                                   // enabled for player
        (maxQuestLevel > 0 &&
         (playerLevel > maxQuestLevel)) ||  // Player is overlevelled
        (!QuestCountLevelAllowRepeatable &&
         q->IsRepeatable()))  // Quest is repeatable
    {
      return;
    }

    if (suggestedPlayers > 0) {
      ChatHandler(p->GetSession())
          .PSendSysMessage(
              "[QCL] Since this quest is a group quest, you have received "
              "credit for %d quests.",
              suggestedPlayers);
      playerData->QuestCount -= q->GetSuggestedPlayers();
    } else if (dungeonQuest) {
      ChatHandler(p->GetSession())
          .PSendSysMessage(
              "[QCL] Since this quest is a dungeon quest, you have received "
              "credit for %d quests",
              DungeonQuestCredit);
      playerData->QuestCount -= DungeonQuestCredit;
    } else {
      playerData->QuestCount -= 1;
    }

    if (playerData->QuestCount <= 0) {
      p->SendItemRetrievalMail(LevelUpItem, 1);
      ChatHandler(p->GetSession())
          .SendSysMessage(
              "[QCL] Congratulations, you've completed enough quests to level "
              "up. Your level up token has been mailed to you.");
      playerData->QuestCount += DefaultQuestCount;
    }

    ChatHandler(p->GetSession())
        .PSendSysMessage(
            "[QCL] You have %d quests remaining to recieve a levelup token.",
            playerData->QuestCount);
  }
};

class Quest_Count_Level_command : public CommandScript {
 public:
  Quest_Count_Level_command() : CommandScript("Quest_Count_Level_command") {}
  std::vector<ChatCommand> GetCommands() const override {
    static std::vector<ChatCommand> QuestCountLevelCommandTable = {
        {"enable", SEC_PLAYER, false, &HandleEnableCommand, ""},
        {"disable", SEC_PLAYER, false, &HandleDisableCommand, ""},
        {"view", SEC_PLAYER, false, &HandleViewCommand, ""},
        {"", SEC_PLAYER, false, &HandleQCLCommand, ""}};

    static std::vector<ChatCommand> QuestCountLevelBaseTable = {
        {"qcl", SEC_PLAYER, false, nullptr, "", QuestCountLevelCommandTable}};

    return QuestCountLevelBaseTable;
  }

  static bool HandleQCLCommand(ChatHandler* handler, char const* args) {
    if (!QuestCountLevelEnabled) {
      handler->PSendSysMessage(
          "[QCL] The Quest Count Level module is not enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    if (!*args) return false;

    return true;
  }

  static bool HandleViewCommand(ChatHandler* handler, char const* /*args*/) {
    if (!QuestCountLevelEnabled) {
      handler->PSendSysMessage(
          "[QCL] The Quest Count Level module is not enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    Player* me = handler->GetSession()->GetPlayer();
    if (!me) {
      return false;
    }

    auto playerData = me->CustomData.Get<PlayerQuestCount>("Quest_Count_Level");

    if (me->GetLevel() < MinimumLevel) {
      handler->PSendSysMessage(
          "[QCL] You have not yet reached the minimum level to use Quest Count "
          "Leveling.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    if (playerData->PlayerQuestCountEnabled) {
      handler->PSendSysMessage(
          "[QCL] The quests you need to complete until you can level is %d",
          playerData->QuestCount);
      handler->SetSentErrorMessage(true);
      return false;
    } else {
      handler->PSendSysMessage(
          "[QCL] You do not have Quest Count Leveling enabled!");
      return false;
    }
    return true;
  }

  // Disable Command
  static bool HandleDisableCommand(ChatHandler* handler, char const* /*args*/) {
    if (!QuestCountLevelEnabled) {
      handler->PSendSysMessage(
          "[QCL] The Quest Count Level module is not enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    Player* me = handler->GetSession()->GetPlayer();
    if (!me) {
      return false;
    }

    if (me->GetLevel() < MinimumLevel) {
      handler->PSendSysMessage(
          "[QCL] You have not yet reached the minimum level to use Quest Count "
          "Leveling.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    PlayerQuestCount* playerData =
        me->CustomData.Get<PlayerQuestCount>("Quest_Count_Level");
    if (!playerData->PlayerQuestCountEnabled) {
      handler->PSendSysMessage("[QCL] Quest Count Leveling is not enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    me->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);

    playerData->PlayerQuestCountEnabled = false;
    handler->PSendSysMessage(
        "[QCL] You have disabled Quest Count Leveling. You can now gain levels "
        "through the standard methods.");
    return true;
  }

  // Enable Command
  static bool HandleEnableCommand(ChatHandler* handler, char const* /*args*/) {
    if (!QuestCountLevelEnabled) {
      handler->PSendSysMessage(
          "[QCL] The Quest Count Level module is not enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    Player* me = handler->GetSession()->GetPlayer();
    if (!me) {
      return false;
    }

    if (me->GetLevel() < MinimumLevel) {
      handler->PSendSysMessage(
          "[QCL] You have not yet reached the minimum level to use Quest Count "
          "Leveling.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    PlayerQuestCount* playerData =
        me->CustomData.Get<PlayerQuestCount>("Quest_Count_Level");
    if (playerData->PlayerQuestCountEnabled) {
      handler->PSendSysMessage(
          "[QCL] Quest Count Leveling is already enabled.");
      handler->SetSentErrorMessage(true);
      return false;
    }

    me->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);

    playerData->PlayerQuestCountEnabled = true;
    handler->PSendSysMessage(
        "[QCL] You have enabled Quest Count Leveling. You can now only gain "
        "levels through completing a certain number of quests.");
    return true;
  }
};

void savePlayerData(Player* p) {
  PlayerQuestCount* playerData =
      p->CustomData.Get<PlayerQuestCount>("Quest_Count_Level");

  if (playerData && playerData->PlayerQuestCountEnabled) {
    int8 count = playerData->QuestCount;
    bool enabled = playerData->PlayerQuestCountEnabled;
    CharacterDatabase.DirectExecute(
        "REPLACE INTO `questcountlevel` (`CharacterGUID`, `QuestCount`, "
        "`Enabled`) VALUES ('{}', '{}', '{}');",
        p->GetGUID().GetCounter(), count, int(enabled));
  }
}

void AddQuest_Count_LevelScripts() {
  new Quest_Count_Level_conf();
  new Quest_Count_Level_Announce();
  new Quest_Count_Level();
  new Quest_Count_Level_command();
}
