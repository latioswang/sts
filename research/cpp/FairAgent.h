// Full-run agent: leak-free combat search + out-of-combat policy.
// EDITABLE SURFACE for the autoresearch loop (see research/program.md).
#pragma once

#include "FairSearch.h"
#include "game/GameContext.h"
#include "sim/search/ScumSearchAgent2.h"

#include <cstdio>
#include <functional>
#include <limits>
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
    std::int64_t legacyCalls[10] = {};  // out-of-combat decisions left to lightspeed's legacy policy, by ScreenState
};

class FairAgent {
public:
    FairAgent(const Params &params, std::uint64_t agentSeed);
    void playout(sts::GameContext &gc);
    RunStats stats;
    // Phase F26 branch evaluation (offline measurement only; never called in play or evaluation): play to the k-th
    // single-card-reward screen, then for each option (card i, or skip) write its run features (rv::DIM floats) and the mean
    // score of M playouts by this agent from re-seeded copies (same seeds for every option); returns options written or -1
    int branchEval(sts::GameContext &gc, int k, int M, std::FILE *out);
    // Phase F33/F34 (offline measurement): kind 1 = card reward (as branchEval), kind 2 = boss relic screen, kind 3 = map path
    // choice (>= 2 next nodes), kind 4 = rest vs smith; M playouts per option,
    // rows of 666 features + 8 floats [opt, n, mean, ok, floor, own, meanA (m < M/2), meanB (m >= M/2)]
    // kind 5 = the kind-3 map decision written as [666 pre-move run features | 20 path features | 8-float tail] rows (F35)
    int branchEval2(sts::GameContext &gc, int kind, int k, int M, std::FILE *out);

    // exposed for leak test
    SearchParams searchParamsFor(const sts::BattleContext &bc) const;

    // real-game bridge (research/realgame): one out-of-combat decision on a rebuilt state, and a view of each
    // combat action right before it runs on the real battle. Unset in research runs.
    void stepOutOfCombat(sts::GameContext &gc);
    void playBattle(sts::GameContext &gc);
    std::function<void(const sts::BattleContext &, const sts::search::Action &, const FairSearcher &)> onCombatAction;

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
    int rvDim = 666;                       // row width of rvLog: rv::DIM (v1) or rv::DIM2 (Phase F14 rv_log2)
    std::vector<float> vfRows;             // buffered feature rows of the current battle
    std::uint64_t agentSeed = 0;
    // Phase F10 vr_event: V on event choices from re-seeded copies (see FairAgent.cpp vrEventChoice)
    bool inHyp = false;                    // true while a hypothetical copy is rolled forward (vr_event off inside)
    std::uint64_t evDecisions = 0;
    int evLeakChecked = 0, evLeakMism = 0, evUsed = 0; // ev_leaktest counters (printed to stderr at the end of playout)
    int vrEventChoice(const sts::GameContext &gc, std::uint64_t decisionSeed);
    double mapNodeV[7] = {0, 0, 0, 0, 0, 0, 0};  // F35: the map DP's value of each next-row node at the last chooseMapX call
    double eliteSimV = std::numeric_limits<double>::quiet_NaN();  // elite_sim: simulated elite-node value for the current map decision

    double roomValue(const sts::GameContext &gc, sts::Room r) const;
    int chooseMapX(const sts::GameContext &gc);
    // elite_sim: value of an elite node from K simulated fights against this act's elites with the current deck/HP
    double eliteSimValue(const sts::GameContext &gc);
    void cardRewards(sts::GameContext &gc);
    void shop(sts::GameContext &gc);
    void cardSelect(sts::GameContext &gc);
    int vrKeyRecall(const sts::GameContext &gc, int alt);   // Phase F18 vr_key
    long keyVrRecalls = 0;                                    // Phase F18: recalls chosen under vr_key
    int eventChoice(const sts::GameContext &gc) const;
    int bossRelicChoice(const sts::GameContext &gc);
    // bf_relic_w: value of a hypothetical run state for the boss-relic choice from K simulated fights of the next act
    float bfRelicValue(const sts::GameContext &hyp, std::uint64_t seed);
    int neowChoice(const sts::GameContext &gc) const;
    double addValue(const sts::GameContext &gc, const sts::Card &c) const;
    double kbSynergy(const sts::GameContext &gc, const sts::Card &c) const;
    // Phase C v2: interventional evaluation of a hypothetical run state by playing sampled fights
    double cfScore(const sts::GameContext &hyp, std::uint64_t cfSeed);
    // Phase C v3: play one fight from a hypothetical run state (fresh RNG streams, no potions);
    // out = {win, HP fraction left (0 on a loss), fraction of the enemies' starting HP removed}
    void bfFight(const sts::GameContext &hyp, sts::MonsterEncounter enc, std::uint64_t seed, float out[3]);
    // log the base state and each hypothetical option with boss + hallway fight outcomes (kind 0 = card
    // reward, 1 = upgrade, 2 = removal); same fight seeds for every option of the decision
    void bfLogOptions(const sts::GameContext &gc, const std::vector<sts::GameContext> &hyps, int kind);
    float bfValue(const sts::GameContext &st) const;  // learned interventional value (bf_mlp.h)
};

}  // namespace fair
