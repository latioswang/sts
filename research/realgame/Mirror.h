// Mirror mode: run the agent inside the simulator on the real game's seed, send each of its actions to the real
// game, and stop at the first state difference. Every difference is a simulator bug or a bridge bug.
#pragma once

#include "ActionSender.h"
#include "FairAgent.h"

#include <cstdint>
#include <string>
#include <vector>

namespace realgame {

struct MirrorOptions {
    std::uint64_t seed = 0;
    int ascension = 0;
    int maxFloor = 0;  // stop cleanly after this floor (0 = play the whole run)
    fair::Params params;
    std::string logDir;
};

struct MirrorResult {
    std::uint64_t seed = 0;
    std::string seedText;
    // completed | reached_limit | diverged | bridge_gap
    std::string status;
    int floor = 0;
    int act = 0;
    int step = 0;
    int commands = 0;
    std::string where;    // screen or situation at the stop
    std::string lastAction;
    std::string message;
    Diffs diffs;
    double seconds = 0.0;
};

class Mirror : public ActionSender {
public:
    Mirror(CommClient &game, MirrorOptions options);
    MirrorResult run();

protected:
    void onCardRewardOpened(const sts::CardReward &cards) override;

private:
    MirrorOptions options_;
    const sts::GameContext *target_ = nullptr;
    int mapComparedForAct_ = 0;
    bool rewardsCompared_ = false;

    [[nodiscard]] int choiceCount() const;
    void prepareNeow(sts::GameContext &gc);
    void logStep(const std::string &where, const std::string &action, const Diffs &diffs);
    void stopOnDifference(const std::string &where, const Diffs &diffs);

    void onGameAction(const sts::GameContext &gc, const sts::search::GameAction &a);
    void onCombatAction(const sts::BattleContext &bc, const sts::search::Action &a);

    void alignOutOfCombat(const sts::GameContext &gc);
    void compareOutOfCombat(const sts::GameContext &gc);
    void finishRun(const sts::GameContext &gc);
};

// Gets the game back to the main menu from anywhere, by losing the run as quickly as possible.
void returnToMainMenu(CommClient &game);

}  // namespace realgame
