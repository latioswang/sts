// Mirror mode: run the agent inside the simulator on the real game's seed, send each of its actions to the real
// game, and stop at the first state difference. Every difference is a simulator bug or a bridge bug.
#pragma once

#include "CommClient.h"
#include "Compare.h"
#include "FairAgent.h"

#include <cstdint>
#include <fstream>
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
    // completed | reached_limit | diverged | bridge_gap | error
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

class Mirror {
public:
    Mirror(CommClient &game, MirrorOptions options);
    MirrorResult run();

private:
    CommClient &game_;
    MirrorOptions options_;
    const sts::GameContext *target_ = nullptr;
    std::ofstream log_;
    int step_ = 0;
    int floor_ = 0;
    int act_ = 1;
    std::string lastAction_;
    int mapComparedForAct_ = 0;
    bool rewardsCompared_ = false;

    [[nodiscard]] const json &state() const { return game_.last(); }
    [[nodiscard]] const json &gameState() const { return game_.last().at("game_state"); }
    [[nodiscard]] std::string screenType() const;

    void send(const std::string &command);
    void logStep(const std::string &where, const std::string &action, const Diffs &diffs);
    void stopOnDifference(const std::string &where, const Diffs &diffs);

    void onGameAction(const sts::GameContext &gc, const sts::search::GameAction &a);
    void onCombatAction(const sts::BattleContext &bc, const sts::search::Action &a);

    void alignOutOfCombat(const sts::GameContext &gc);
    void compareOutOfCombat(const sts::GameContext &gc);
    void applyGameAction(const sts::GameContext &gc, const sts::search::GameAction &a);

    void applyEvent(const sts::GameContext &gc, int option);
    void applyRewards(const sts::GameContext &gc, const sts::search::GameAction &a);
    void applyShop(const sts::GameContext &gc, const sts::search::GameAction &a);
    void applyCardSelect(const sts::GameContext &gc, int index);
    void applyMap(const sts::GameContext &gc, int x);
    void applyCombatAction(const sts::BattleContext &bc, const sts::search::Action &a);
    void chooseCardOnScreen(const std::string &key);
    void confirmIfRequired();

    void finishRun(const sts::GameContext &gc);
};

// Gets the game back to the main menu from anywhere, by losing the run as quickly as possible.
void returnToMainMenu(CommClient &game);

}  // namespace realgame
