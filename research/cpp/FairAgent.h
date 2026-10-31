// Full-run agent: leak-free combat search + out-of-combat policy.
// EDITABLE SURFACE for the autoresearch loop (see research/program.md).
#pragma once

#include "FairSearch.h"
#include "game/GameContext.h"
#include "sim/search/ScumSearchAgent2.h"

#include <map>
#include <random>
#include <string>

namespace fair {

using Params = std::map<std::string, double>;

double getp(const Params &p, const std::string &k, double def);

struct RunStats {
    std::int64_t combatDecisions = 0;
    std::int64_t battles = 0;
    bool aborted = false;
};

class FairAgent {
public:
    FairAgent(const Params &params, std::uint64_t agentSeed);
    void playout(sts::GameContext &gc);
    RunStats stats;

    // exposed for leak test
    SearchParams searchParamsFor(const sts::BattleContext &bc) const;

private:
    Params P;
    SearchParams sp;
    std::mt19937_64 rng;
    sts::search::ScumSearchAgent2 legacy;  // out-of-combat fallback policy

    void playBattle(sts::GameContext &gc);
    void stepOutOfCombat(sts::GameContext &gc);
    double roomValue(const sts::GameContext &gc, sts::Room r) const;
    int chooseMapX(const sts::GameContext &gc);
    void cardRewards(sts::GameContext &gc);
    void shop(sts::GameContext &gc);
    void cardSelect(sts::GameContext &gc);
    int eventChoice(const sts::GameContext &gc) const;
};

}  // namespace fair
