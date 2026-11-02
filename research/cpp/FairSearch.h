// Leak-free combat search for sts_lightspeed.
//
// Unlike BattleScumSearcher2 (which searches a copy of the *true* RNG and
// draw-pile order, then replays the found line), this searcher:
//   * re-determinises hidden state on EVERY simulation: draw pile is put in
//     canonical order then shuffled with the searcher's own RNG, and every
//     BattleContext RNG stream is reseeded from the searcher's RNG;
//   * never reads bc.seed;
//   * keys tree nodes by an observation hash, so a node's stored actions are
//     valid for every determinisation that reaches it (closed-loop tree);
//   * returns only ONE action; the caller re-searches after executing it.
#pragma once

#include "combat/BattleContext.h"
#include "sim/search/Action.h"

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace fair {

struct SearchParams {
    int simulations = 500;       // per decision (frozen eval budget) — used when budgetActions == 0
    std::int64_t budgetActions = 0;  // if > 0: run simulations until this many game actions were executed (compute proxy, deterministic)
    double exploration = 0.7;    // UCB c on [0,1]-normalised values
    int rolloutTurns = 99;       // stop random rollout after this many extra turns, then evaluate heuristically
    double endTurnRolloutWeight = 1.0;  // relative weight of END_TURN in random rollout (1 = uniform)
    bool allowPotions = true;  // potion actions in tree + rollouts
    int knownTop = 0;          // # of top draw-pile cards the player legitimately knows
    double rollBlockW = 1.0;   // rollout weight of skills when visible incoming damage > block
    double rollAtkW = 1.0;     // rollout weight of attacks when no damage incoming
    double rolloutOrderK = 0.0;         // bias rollout card choice toward lightspeed expert play order
    double potionRolloutWeight = 1.0;   // relative weight of potion actions in random rollout
    double hpWeight = 1.0;       // value per remaining HP on victory
    double potionValue = 4.0;    // value per potion kept on victory
    double winBonus = 35.0;      // constant for winning
    double maxHpWeight = 0.0;    // value per max hp (feed, etc.)
    double escapePenalty = 30.0; // value lost by escaping with Smoke Bomb (forfeited rewards), in HP-equivalents
    double lowHpPen = 0.0;       // extra value lost per HP below lowHpThresh at a won fight's end (risk aversion)
    double lowHpThresh = 20.0;
    bool leafVf = false;         // rollout cutoff: use the learned MLP value instead of the heuristic
    int leafSv = 0;              // Phase S: rollout cutoff valued by runtime model research/models/sv_<id>.txt (0 = off)
    bool usePrior = false;       // PUCT with the learned bilinear move prior (pf_weights.inc) instead of plain UCB
    double priorC = 1.5;         // PUCT exploration constant
    bool priorMlp = false;       // PUCT with the MLP move prior (pfm_weights.inc) at shallow depth (cost!)
    int priorMlpDepth = 1;       // apply the MLP prior at nodes with depth <= this; deeper nodes use bilinear/uniform
    double priorTemp = 1.0;      // prior^(1/temp) renormalised: <1 sharpens, >1 flattens
    double rollPrior = 0.0;      // >0: rollout actions sampled from (1-eps)*bilinear prior + eps*uniform (learned fast rollout policy)
    double rollPriorEps = 0.25;
    double selectQ = 0.0;        // final move: if > 0, pick the highest mean value among edges with n >= selectQ * max_n (robust child); 0 = most visits
    double cutWinFrac = 1.0;     // rollout-cutoff value: fraction of winBonus credited per enemy-HP fraction removed
    double cutHpFrac = 1.0;      // rollout-cutoff value: fraction of hpWeight*curHp credited
    double lossDmgW = 10.0;      // loss value: credit per enemy-HP fraction removed (the only gradient in boss fights the rollouts cannot win)
    double lossTurnW = 0.2;      // loss value: credit per turn survived
};

std::uint64_t observationHash(const sts::BattleContext &bc);
// value of a finished (or cut-off) battle state in search units; shared by the search and the data logger
double terminalValue(const sts::BattleContext &bc, const SearchParams &p);
void determinise(sts::BattleContext &bc, std::mt19937_64 &rng, int knownTop = 0);
std::vector<sts::search::Action> enumerateActions(const sts::BattleContext &bc, bool allowPotions = true);
// allocation-free variant (same action set and order); returns count written to out[0..cap)
constexpr int MAX_ACTIONS = 128;
int enumerateActionsInto(const sts::BattleContext &bc, bool allowPotions, sts::search::Action *out, int cap);

class FairSearcher {
public:
    FairSearcher(const sts::BattleContext &root, const SearchParams &p, std::uint64_t searchSeed);
    sts::search::Action search();
    void setRoot(const sts::BattleContext &bc);
    // after executing `taken` in the real game: keep matching subtree (tree reuse) or reset
    bool advance(sts::search::Action taken, const sts::BattleContext &after);

    // stats for logging
    std::int64_t rootVisits() const { return nodes.empty() ? 0 : nodes[rootIdx].visits; }
    // root visit distribution of the last search() (policy-prior training data)
    void rootStats(std::vector<std::pair<sts::search::Action, std::int64_t>> &out) const;
    struct EdgeStat { sts::search::Action action; std::int64_t n; double q; };
    void rootEdgeStats(std::vector<EdgeStat> &out, double &rootMean) const;  // q = mean backed-up value
    std::int64_t actionsExecuted = 0;  // game actions executed by the last search() (tree + rollout)
    int simulationsRun = 0;

private:
    struct Edge {
        sts::search::Action action;
        std::int64_t n = 0;
        double w = 0;
        float prior = 0.f;  // learned move prior (softmax over the node's legal actions)
        std::unordered_map<std::uint64_t, int> children;  // obs hash -> node idx
    };
    struct Node {
        std::int64_t visits = 0;
        double wsum = 0;  // sum of backed-up values (for first-play urgency under PUCT)
        std::vector<Edge> edges;
    };

    const sts::BattleContext *rootState;
    int rootIdx = 0;
    SearchParams p;
    std::mt19937_64 rng;
    std::vector<Node> nodes;
    double vmin = 1e18, vmax = -1e18;

    int newNode(const sts::BattleContext &bc, int depth = 0);
    void simulate();
    double rollout(sts::BattleContext &bc);
    double evaluate(const sts::BattleContext &bc) const;
    int select(const Node &node) const;
};

}  // namespace fair
