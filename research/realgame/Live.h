// Live mode: the agent plays the real game. Every decision rebuilds a simulator state from the game's JSON, so a
// simulator difference can't derail the run. Each decision and its search statistics go to a per-run log.
#pragma once

#include "ActionSender.h"
#include "FairAgent.h"
#include "MatchGame.h"
#include "StateConverter.h"

#include <cstdint>
#include <random>
#include <string>

namespace realgame {

struct LiveOptions {
    std::uint64_t seed = 0;
    int ascension = 0;
    fair::Params params;
    std::string logDir;
    // Pick up the run already in the game instead of starting one: no START command, and the run's log is appended to.
    bool resume = false;
    // Stop on the map, sending nothing more, when the next node is the act boss (or, with pauseFloor, from that floor
    // on). A resumed run doesn't stop again on the floor it resumed on.
    bool pauseBeforeBoss = false;
    int pauseFloor = 0;
};

struct LiveResult {
    std::uint64_t seed = 0;
    std::string seedText;
    std::string status;  // finished | stalled | paused
    std::string message;
    bool victory = false;
    int score = 0;
    int floor = 0;
    int act = 0;
    int decisions = 0;
    int fallbacks = 0;  // decisions the agent could not make, resolved by the simple fallback
    double seconds = 0.0;
};

class Live : public ActionSender {
public:
    Live(CommClient &game, LiveOptions options);
    LiveResult run();

protected:
    void onCardRewardDeclined() override { ++declinedCardRewards_; }

private:
    LiveOptions options_;
    fair::FairAgent agent_;
    std::mt19937_64 rng_;
    RunMemory runMemory_;
    BattleMemory battleMemory_;
    int knownTop_ = 0;
    int lastDrawPileSize_ = -1;
    bool putOnTopPending_ = false;
    int declinedCardRewards_ = 0;
    int rewardFloor_ = -1;
    int visitedShopFloor_ = -1;
    int decisions_ = 0;
    int fallbacks_ = 0;
    bool victory_ = false;
    int score_ = 0;
    bool paused_ = false;
    int resumeFloor_ = -1;
    MatchGameMemory matchGame_;
    int matchGameFloor_ = -1;

    bool step();
    bool shouldPause() const;
    void beginResumedRun();
    bool advanceWithoutDecision();
    void decideBattle();
    void decideOutOfCombat();
    void fallback(const std::string &reason);
    bool pickMatchGameCard();
    void updateKnownTop(int drawPileSize);
};

}  // namespace realgame
