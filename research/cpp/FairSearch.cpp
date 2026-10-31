#include "FairSearch.h"

#include "sim/search/BattleScumSearcher2.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>

using namespace sts;

namespace fair {
#ifdef FAIR_TRACE
std::vector<std::string>& traceRing() { static std::vector<std::string> r; return r; }
static int _setterm = (std::set_terminate([]{ for (auto &x : traceRing()) std::cerr << "TRACE " << x << "\n"; std::abort(); }), 0);
#endif

static inline void mix(std::uint64_t &h, std::uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

std::uint64_t observationHash(const BattleContext &bc) {
    std::uint64_t h = 1469598103934665603ULL;
    mix(h, static_cast<std::uint64_t>(bc.inputState));
    mix(h, static_cast<std::uint64_t>(bc.outcome));
    mix(h, bc.turn);
    mix(h, bc.player.curHp);
    mix(h, bc.player.block);
    mix(h, bc.player.energy);
    mix(h, bc.cards.cardsInHand);
    for (int i = 0; i < bc.cards.cardsInHand; ++i) {
        const auto &c = bc.cards.hand[i];
        mix(h, static_cast<std::uint64_t>(c.id));
        mix(h, c.getUpgradeCount());
        mix(h, static_cast<std::uint64_t>(c.costForTurn + 16));
        mix(h, c.freeToPlayOnce);
        mix(h, c.specialData);
    }
    mix(h, bc.cards.drawPile.size());
    mix(h, bc.cards.discardPile.size());
    mix(h, bc.cards.exhaustPile.size());
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        const auto &m = bc.monsters.arr[i];
        mix(h, static_cast<std::uint64_t>(m.id));
        mix(h, m.curHp);
        mix(h, m.block);
        mix(h, m.isTargetable());
        mix(h, static_cast<std::uint64_t>(m.moveHistory[0]));
    }
    for (int i = 0; i < bc.potionCapacity; ++i) {
        mix(h, static_cast<std::uint64_t>(bc.potions[i]));
    }
    if (bc.inputState == InputState::CARD_SELECT) {
        mix(h, static_cast<std::uint64_t>(bc.cardSelectInfo.cardSelectTask));
        for (auto id : bc.cardSelectInfo.cards) mix(h, static_cast<std::uint64_t>(id));
        mix(h, bc.cardSelectInfo.pickCount);
    }
    return h;
}

void determinise(BattleContext &bc, std::mt19937_64 &rng) {
    auto &dp = bc.cards.drawPile;
    // canonical order first, so the result is independent of the true order
    std::sort(dp.begin(), dp.end(), [](const CardInstance &a, const CardInstance &b) {
        if (a.id != b.id) return a.id < b.id;
        if (a.upgraded != b.upgraded) return a.upgraded < b.upgraded;
        return a.uniqueId < b.uniqueId;
    });
    std::shuffle(dp.begin(), dp.end(), rng);

    bc.aiRng = Random(rng());
    bc.cardRandomRng = Random(rng());
    bc.miscRng = Random(rng());
    bc.monsterHpRng = Random(rng());
    bc.potionRng = Random(rng());
    bc.shuffleRng = Random(rng());
}

// Reuse lightspeed's action enumeration through a dummy searcher node.
std::vector<search::Action> enumerateActions(const BattleContext &bc) {
    static thread_local std::unique_ptr<search::BattleScumSearcher2> dummy;
    if (!dummy) dummy = std::make_unique<search::BattleScumSearcher2>(bc);
    search::BattleScumSearcher2::Node node;
    dummy->enumerateActionsForNode(node, bc);
    std::vector<search::Action> out;
    out.reserve(node.edges.size());
    for (auto &e : node.edges) {
        // Simulator bug workaround: Entropic Brew can create potions whose
        // effects lightspeed doesn't implement (e.g. Ambrosia) -> crash.
        if (e.action.getActionType() == search::ActionType::POTION &&
            bc.potions[e.action.getSourceIdx()] == Potion::ENTROPIC_BREW) continue;
        out.push_back(e.action);
    }
    return out;
}

FairSearcher::FairSearcher(const BattleContext &root, const SearchParams &p, std::uint64_t searchSeed)
    : rootState(&root), p(p), rng(searchSeed) {
    nodes.reserve(p.simulations * 2 + 4);
}

void FairSearcher::setRoot(const BattleContext &bc) { rootState = &bc; }

bool FairSearcher::advance(search::Action taken, const BattleContext &after) {
    // keep the subtree reached by the REAL observation (closed-loop, no leak)
    if (!nodes.empty() && after.outcome == Outcome::UNDECIDED && nodes.size() < 3000000) {
        for (auto &e : nodes[rootIdx].edges) {
            if (e.action.bits != taken.bits) continue;
            auto it = e.children.find(observationHash(after));
            if (it != e.children.end()) { rootIdx = it->second; return true; }
        }
    }
    nodes.clear();
    rootIdx = 0;
    return false;
}

int FairSearcher::newNode(const BattleContext &bc) {
    Node n;
    for (auto a : enumerateActions(bc)) {
        Edge e;
        e.action = a;
        n.edges.push_back(std::move(e));
    }
    nodes.push_back(std::move(n));
    return static_cast<int>(nodes.size()) - 1;
}

double FairSearcher::evaluate(const BattleContext &bc) const {
    if (bc.outcome == Outcome::PLAYER_VICTORY) {
        return p.winBonus + p.hpWeight * bc.player.curHp + p.potionValue * bc.potionCount
               + p.maxHpWeight * bc.player.maxHp - bc.turn * 0.01;
    }
    // loss or cutoff: partial credit for damage dealt, small for survival time
    int cur = 0, mx = 0;
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        const auto &m = bc.monsters.arr[i];
        if (!m.hasStatus<MS::MINION>() && m.id != MonsterId::INVALID) {
            cur += m.curHp;
            mx += m.maxHp;
        }
    }
    const double ratio = mx > 0 ? static_cast<double>(cur) / mx : 0.0;
    double v = (1.0 - ratio) * 10 + bc.turn * 0.2 + p.potionValue * bc.potionCount / 2;
    if (bc.outcome == Outcome::UNDECIDED) {
        // cutoff: alive with hp; scale between loss and win
        v += 0.5 * (p.winBonus * (1.0 - ratio)) + p.hpWeight * bc.player.curHp * (1.0 - ratio);
    }
    return v;
}

double FairSearcher::rollout(BattleContext &bc) {
    const int stopTurn = bc.turn + p.rolloutTurns;
    std::uniform_real_distribution<double> U(0, 1);
    while (bc.outcome == Outcome::UNDECIDED && bc.turn < stopTurn) {
        auto acts = enumerateActions(bc);
        if (acts.empty()) break;
        // weighted pick: END_TURN has weight endTurnRolloutWeight, others 1
        double total = 0;
        auto wOf = [&](const search::Action &a) {
            switch (a.getActionType()) {
                case search::ActionType::END_TURN: return p.endTurnRolloutWeight;
                case search::ActionType::POTION: return p.potionRolloutWeight;
                default: return 1.0;
            }
        };
        for (auto &a : acts) total += wOf(a);
        double r = U(rng) * total;
        search::Action chosen = acts.back();
        for (auto &a : acts) {
            r -= wOf(a);
            if (r <= 0) { chosen = a; break; }
        }
#ifdef FAIR_TRACE
        { std::ostringstream os; chosen.printDesc(os, bc); traceRing().push_back(os.str() + " turn=" + std::to_string(bc.turn)); if (traceRing().size() > 12) traceRing().erase(traceRing().begin()); }
#endif
        chosen.execute(bc);
    }
    return evaluate(bc);
}

int FairSearcher::select(const Node &node) const {
    const double range = (vmax > vmin) ? (vmax - vmin) : 1.0;
    int best = 0;
    double bestV = -1e18;
    const double logN = std::log(static_cast<double>(node.visits) + 1);
    for (int i = 0; i < static_cast<int>(node.edges.size()); ++i) {
        const auto &e = node.edges[i];
        if (e.n == 0) return i;  // try every action once
        const double q = (e.w / e.n - vmin) / range;
        const double u = q + p.exploration * std::sqrt(logN / e.n);
        if (u > bestV) { bestV = u; best = i; }
    }
    return best;
}

void FairSearcher::simulate() {
    BattleContext bc = *rootState;
    determinise(bc, rng);

    std::vector<std::pair<int,int>> path;  // (node, edge)
    int cur = rootIdx;
    double value;
    while (true) {
        if (bc.outcome != Outcome::UNDECIDED) { value = evaluate(bc); break; }
        Node &node = nodes[cur];
        if (node.edges.empty()) { value = evaluate(bc); break; }
        const int ei = select(node);
        auto action = nodes[cur].edges[ei].action;
        if (!action.isValidAction(bc)) {  // hash collision safety: treat as leaf
            value = rollout(bc);
            break;
        }
        path.emplace_back(cur, ei);
        action.execute(bc);
        const auto h = observationHash(bc);
        auto &children = nodes[cur].edges[ei].children;
        auto it = children.find(h);
        if (it == children.end()) {
            const bool terminal = bc.outcome != Outcome::UNDECIDED;
            const int child = terminal ? -1 : newNode(bc);
            if (child >= 0) nodes[cur].edges[ei].children.emplace(h, child);
            value = terminal ? evaluate(bc) : rollout(bc);
            if (child >= 0) { nodes[child].visits += 1; }
            break;
        }
        cur = it->second;
    }

    vmin = std::min(vmin, value);
    vmax = std::max(vmax, value);
    nodes[rootIdx].visits += 1;
    for (auto &[ni, ei] : path) {
        auto &e = nodes[ni].edges[ei];
        e.n += 1;
        e.w += value;
        if (ni != rootIdx) nodes[ni].visits += 1;
    }
}

search::Action FairSearcher::search() {
    if (nodes.empty()) { newNode(*rootState); rootIdx = 0; }
    if (nodes[rootIdx].edges.size() == 1) return nodes[rootIdx].edges[0].action;
    for (int i = 0; i < p.simulations; ++i) simulate();
    const auto &edges = nodes[rootIdx].edges;
    int best = 0;
    for (int i = 1; i < static_cast<int>(edges.size()); ++i) {
        if (edges[i].n > edges[best].n) best = i;
    }
    return edges[best].action;
}

}  // namespace fair
