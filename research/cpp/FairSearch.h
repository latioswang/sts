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
    int simulations = 500;       // per decision (frozen eval budget)
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
};

std::uint64_t observationHash(const sts::BattleContext &bc);
void determinise(sts::BattleContext &bc, std::mt19937_64 &rng, int knownTop = 0);
std::vector<sts::search::Action> enumerateActions(const sts::BattleContext &bc, bool allowPotions = true);

class FairSearcher {
public:
    FairSearcher(const sts::BattleContext &root, const SearchParams &p, std::uint64_t searchSeed);
    sts::search::Action search();
    void setRoot(const sts::BattleContext &bc);
    // after executing `taken` in the real game: keep matching subtree (tree reuse) or reset
    bool advance(sts::search::Action taken, const sts::BattleContext &after);

    // stats for logging
    std::int64_t rootVisits() const { return nodes.empty() ? 0 : nodes[rootIdx].visits; }

private:
    struct Edge {
        sts::search::Action action;
        std::int64_t n = 0;
        double w = 0;
        std::unordered_map<std::uint64_t, int> children;  // obs hash -> node idx
    };
    struct Node {
        std::int64_t visits = 0;
        std::vector<Edge> edges;
    };

    const sts::BattleContext *rootState;
    int rootIdx = 0;
    SearchParams p;
    std::mt19937_64 rng;
    std::vector<Node> nodes;
    double vmin = 1e18, vmax = -1e18;

    int newNode(const sts::BattleContext &bc);
    void simulate();
    double rollout(sts::BattleContext &bc);
    double evaluate(const sts::BattleContext &bc) const;
    int select(const Node &node) const;
};

}  // namespace fair
