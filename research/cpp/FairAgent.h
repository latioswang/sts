// Full-run agent: leak-free combat search + out-of-combat policy.
// EDITABLE SURFACE for the autoresearch loop (see research/program.md).
#pragma once

#include "FairSearch.h"
#include "game/GameContext.h"
#include "sim/search/ScumSearchAgent2.h"

#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace fair {

using Params = std::map<std::string, double>;

double getp(const Params &p, const std::string &k, double def);

struct RunStats {
    std::int64_t combatDecisions = 0;
    std::int64_t battles = 0;
    std::int64_t actions = 0;      // total game actions executed inside search (compute proxy)
    std::int64_t simulations = 0;  // total simulations run
    bool aborted = false;
    int lastEncounter = -1;        // MonsterEncounter of the last battle (diagnostic: what killed the run)
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
    std::FILE *vfLog = nullptr;            // Phase A data logger (vf_log=1 + VF_LOG_DIR)
    std::FILE *rvLog = nullptr;            // Phase C run-state logger (rv_log=1 + VF_LOG_DIR)
    std::FILE *pfLog = nullptr;            // Phase B root-visit logger (pf_log=1 + VF_LOG_DIR)
    std::FILE *bfLog = nullptr;            // Phase C v3 interventional fight logger (bf_log=1 + VF_LOG_DIR)
    std::uint64_t bfDecisions = 0;
    std::vector<float> rvRows;
    std::vector<float> vfRows;             // buffered feature rows of the current battle
    std::uint64_t agentSeed = 0;

    void playBattle(sts::GameContext &gc);
    void stepOutOfCombat(sts::GameContext &gc);
    double roomValue(const sts::GameContext &gc, sts::Room r) const;
    int chooseMapX(const sts::GameContext &gc);
    void cardRewards(sts::GameContext &gc);
    void shop(sts::GameContext &gc);
    void cardSelect(sts::GameContext &gc);
    int eventChoice(const sts::GameContext &gc) const;
    int bossRelicChoice(const sts::GameContext &gc) const;
    int neowChoice(const sts::GameContext &gc) const;
    double addValue(const sts::GameContext &gc, const sts::Card &c) const;
    double kbSynergy(const sts::GameContext &gc, const sts::Card &c) const;
    // Phase C v2: interventional evaluation of a hypothetical run state by playing sampled fights
    double cfScore(const sts::GameContext &hyp, std::uint64_t cfSeed);
    // Phase C v3: play one fight from a hypothetical run state (fresh RNG streams, no potions);
    // out = {win, HP fraction left (0 on a loss)}
    void bfFight(const sts::GameContext &hyp, sts::MonsterEncounter enc, std::uint64_t seed, float out[2]);
    // log the base state and each hypothetical option with boss + hallway fight outcomes (kind 0 = card
    // reward, 1 = upgrade, 2 = removal); same fight seeds for every option of the decision
    void bfLogOptions(const sts::GameContext &gc, const std::vector<sts::GameContext> &hyps, int kind);
    float bfValue(const sts::GameContext &st) const;  // learned interventional value (bf_mlp.h)
};

}  // namespace fair
