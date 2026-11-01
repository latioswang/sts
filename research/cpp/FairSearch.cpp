#include "FairSearch.h"

#include "sim/search/BattleScumSearcher2.h"
#include "sim/search/ExpertKnowledge.h"
#include "mlp.h"
#include "policy_features.h"
#include "pf_mlp.h"

namespace fair::pf {
#include "pf_weights.inc"
}

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
static void checkAlive(const sts::BattleContext &bc, const char *where) {
    int n = 0;
    for (int i = 0; i < bc.monsters.monsterCount; ++i) if (!bc.monsters.arr[i].isDeadOrEscaped()) ++n;
    if (n != bc.monsters.monstersAlive) {
        std::cerr << "ALIVE MISMATCH at " << where << ": monstersAlive=" << bc.monsters.monstersAlive
                  << " actual=" << n << " count=" << bc.monsters.monsterCount << " turn=" << bc.turn
                  << " dome=" << bc.player.hasRelic<sts::R::RUNIC_DOME>() << "\n";
        for (int i = 0; i < bc.monsters.monsterCount; ++i) {
            const auto &m = bc.monsters.arr[i];
            std::cerr << "  m" << i << " " << m.getName() << " hp=" << m.curHp << " halfDead=" << m.isHalfDead()
                      << " escaping=" << m.isEscaping() << " move0=" << (int)m.moveHistory[0] << " move1=" << (int)m.moveHistory[1] << "\n";
        }
        for (auto &x : traceRing()) std::cerr << "TRACE " << x << "\n";
        std::abort();
    }
}
static void traceAction(const sts::search::Action &a, const sts::BattleContext &bc, const char *tag) {
    std::ostringstream os; a.printDesc(os, bc);
    traceRing().push_back(std::string(tag) + " " + os.str() + " turn=" + std::to_string(bc.turn));
    if (traceRing().size() > 16) traceRing().erase(traceRing().begin());
}
#define FAIR_CHECK(bc, where) checkAlive(bc, where)
#define FAIR_TRACE_ACTION(a, bc, tag) traceAction(a, bc, tag)
#else
#define FAIR_CHECK(bc, where) ((void)0)
#define FAIR_TRACE_ACTION(a, bc, tag) ((void)0)
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
    const bool dome = bc.player.hasRelic<R::RUNIC_DOME>();
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        const auto &m = bc.monsters.arr[i];
        mix(h, static_cast<std::uint64_t>(m.id));
        mix(h, m.curHp);
        mix(h, m.block);
        mix(h, m.isTargetable());
        if (!dome) mix(h, static_cast<std::uint64_t>(m.moveHistory[0]));  // intent hidden under Runic Dome
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

void determinise(BattleContext &bc, std::mt19937_64 &rng, int knownTop) {
    auto &dp = bc.cards.drawPile;
    // The top `knownTop` cards (back of the vector) are ones the player legitimately
    // knows (Headbutt / Warcry placed them). Everything below is unknown: canonical
    // order first so the result is independent of the true order, then shuffle.
    knownTop = std::max(0, std::min<int>(knownTop, static_cast<int>(dp.size())));
    auto unknownEnd = dp.end() - knownTop;
    std::sort(dp.begin(), unknownEnd, [](const CardInstance &a, const CardInstance &b) {
        if (a.id != b.id) return a.id < b.id;
        if (a.upgraded != b.upgraded) return a.upgraded < b.upgraded;
        return a.uniqueId < b.uniqueId;
    });
    std::shuffle(dp.begin(), unknownEnd, rng);

    bc.aiRng = Random(rng());
    bc.cardRandomRng = Random(rng());
    bc.miscRng = Random(rng());
    bc.monsterHpRng = Random(rng());
    bc.potionRng = Random(rng());
    bc.shuffleRng = Random(rng());

    // Runic Dome: the player cannot see intents. Re-sample the pending move, but only
    // where it was produced by an RNG roll (sim flag lastMoveWasRolled). Scripted
    // sequences (set via setMove in the monster's own turn, e.g. Bronze Automaton) are
    // deterministic and predictable to a knowledgeable player, and re-rolling them can
    // corrupt state (double orb spawn), so they are left alone.
    if (bc.player.hasRelic<R::RUNIC_DOME>() && bc.inputState != InputState::EXECUTING_ACTIONS) {
        for (int i = 0; i < bc.monsters.monsterCount; ++i) {
            auto &m = bc.monsters.arr[i];
            if (m.isDeadOrEscaped() || !m.lastMoveWasRolled) continue;
            m.moveHistory[0] = m.moveHistory[1];
            m.moveHistory[1] = MonsterMoveId::INVALID;
            m.rollMove(bc);  // approximate revert of the last roll, then roll again
        }
    }
}

static inline bool potionAllowedHere(const BattleContext &bc, int pIdx, bool allowPotions) {
    if (!allowPotions) return false;
    // real-game rule: Smoke Bomb cannot be used in boss fights
    return !(bc.potions[pIdx] == Potion::SMOKE_BOMB && isBossEncounter(bc.encounter));
}

// Allocation-free enumeration for the common PLAYER_NORMAL state. Mirrors lightspeed's
// BattleScumSearcher2::enumerateCardActions/enumeratePotionActions exactly (same dedupe rule,
// same play-order sort, same target order) so the search behaves identically, just faster.
int enumerateActionsInto(const BattleContext &bc, bool allowPotions, search::Action *out, int cap) {
    int n = 0;
    if (bc.inputState != InputState::PLAYER_NORMAL) {
        auto v = enumerateActions(bc, allowPotions);  // rare: card-select states, use the generic path
        for (auto &a : v) if (n < cap) out[n++] = a;
        return n;
    }
    if (bc.isCardPlayAllowed()) {
        std::pair<int,int> playable[CardManager::MAX_HAND_SIZE];
        int np = 0;
        for (int h = 0; h < bc.cards.cardsInHand; ++h) {
            const auto &c = bc.cards.hand[h];
            if (!c.canUseOnAnyTarget(bc)) continue;
            if (h > 0) {
                const auto &l = bc.cards.hand[h - 1];
                const bool same = c.id == l.id && c.getUpgradeCount() == l.getUpgradeCount()
                               && c.costForTurn == l.costForTurn && c.cost == l.cost
                               && c.freeToPlayOnce == l.freeToPlayOnce && c.specialData == l.specialData;
                if (same) continue;
            }
            playable[np++] = {h, search::Expert::getPlayOrdering(c.getId())};
        }
        std::sort(playable, playable + np, [](auto a, auto b) { return a.second < b.second; });
        for (int i = 0; i < np; ++i) {
            const int h = playable[i].first;
            const auto &c = bc.cards.hand[h];
            if (c.requiresTarget()) {
                for (int t = bc.monsters.monsterCount - 1; t >= 0; --t) {
                    if (!bc.monsters.arr[t].isTargetable()) continue;
                    if (n < cap) out[n++] = search::Action(search::ActionType::CARD, h, t);
                }
            } else if (n < cap) {
                out[n++] = search::Action(search::ActionType::CARD, h);
            }
        }
    }
    if (allowPotions) {
        const bool hasValidTarget = bc.monsters.getTargetableCount() > 0;
        for (int pIdx = 0; pIdx < bc.potionCapacity; ++pIdx) {
            const auto p = bc.potions[pIdx];
            if (p == Potion::EMPTY_POTION_SLOT) continue;
            if (!potionAllowedHere(bc, pIdx, true)) continue;
            if (p == Potion::FAIRY_POTION) { if (n < cap) out[n++] = search::Action(search::ActionType::POTION, pIdx, -1); continue; }
            if (!potionRequiresTarget(p)) { if (n < cap) out[n++] = search::Action(search::ActionType::POTION, pIdx); continue; }
            if (!hasValidTarget) { if (n < cap) out[n++] = search::Action(search::ActionType::POTION, pIdx, -1); continue; }
            for (int t = 0; t < bc.monsters.monsterCount; ++t) {
                if (bc.monsters.arr[t].isTargetable() && n < cap) out[n++] = search::Action(search::ActionType::POTION, pIdx, t);
            }
        }
    }
    if (n < cap) out[n++] = search::Action(search::ActionType::END_TURN);
    return n;
}

// Generic path: reuse lightspeed's action enumeration through a dummy searcher node.
std::vector<search::Action> enumerateActions(const BattleContext &bc, bool allowPotions) {
    static thread_local std::unique_ptr<search::BattleScumSearcher2> dummy;
    if (!dummy) dummy = std::make_unique<search::BattleScumSearcher2>(bc);
    search::BattleScumSearcher2::Node node;
    dummy->enumerateActionsForNode(node, bc);
    std::vector<search::Action> out;
    out.reserve(node.edges.size());
    for (auto &e : node.edges) {
        if (e.action.getActionType() == search::ActionType::POTION
            && !potionAllowedHere(bc, e.action.getSourceIdx(), allowPotions)) continue;
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

int FairSearcher::newNode(const BattleContext &bc, int depth) {
    Node n;
    search::Action buf[MAX_ACTIONS];
    const int cnt = enumerateActionsInto(bc, p.allowPotions, buf, MAX_ACTIONS);
    n.edges.reserve(cnt);
    for (int i = 0; i < cnt; ++i) {
        Edge e;
        e.action = buf[i];
        e.prior = cnt > 0 ? 1.f / cnt : 0.f;
        n.edges.push_back(std::move(e));
    }
    if (p.priorMlp && pfm::PFM_TRAINED && cnt > 1 && depth <= p.priorMlpDepth) {
        float pr[MAX_ACTIONS];
        pfm::priors(bc, buf, cnt, pr);
        if (p.priorTemp != 1.0) {
            float z = 0.f;
            for (int i = 0; i < cnt; ++i) { pr[i] = std::pow(std::max(pr[i], 1e-6f), static_cast<float>(1.0 / p.priorTemp)); z += pr[i]; }
            for (int i = 0; i < cnt; ++i) pr[i] /= z;
        }
        for (int i = 0; i < cnt; ++i) n.edges[i].prior = pr[i];
    } else if (p.usePrior && pf::PF_TRAINED && cnt > 1) {
        // bilinear prior: h = W s + b ; logit(a) = h.E[tok] + up*(h.Eup) + AB[tok] + T.tf
        float x[vf::DIM];
        vf::extract(bc, x);
        float h[pf::PF_K];
        for (int k = 0; k < pf::PF_K; ++k) {
            float acc = pf::PF_B[k];
            const float *w = pf::PF_W + k * pf::PF_S_DIM;
            for (int i = 0; i < vf::DIM; ++i) acc += w[i] * x[i];
            h[k] = acc;
        }
        float mx = -1e30f;
        for (int i = 0; i < cnt; ++i) {
            bool up = false;
            const int tok = pf::token(bc, buf[i], up);
            float tf[pf::T_DIM];
            pf::targetFeats(bc, buf[i], tf);
            float lg = pf::PF_AB[tok];
            for (int k = 0; k < pf::PF_K; ++k) lg += h[k] * (pf::PF_E[tok * pf::PF_K + k] + (up ? pf::PF_EUP[k] : 0.f));
            for (int t = 0; t < pf::T_DIM; ++t) lg += pf::PF_T[t] * tf[t];
            n.edges[i].prior = lg;
            mx = std::max(mx, lg);
        }
        float z = 0.f;
        for (int i = 0; i < cnt; ++i) { n.edges[i].prior = std::exp(n.edges[i].prior - mx); z += n.edges[i].prior; }
        for (int i = 0; i < cnt; ++i) n.edges[i].prior /= z;
    }
    nodes.push_back(std::move(n));
    return static_cast<int>(nodes.size()) - 1;
}

double FairSearcher::evaluate(const BattleContext &bc) const { return terminalValue(bc, p); }

double terminalValue(const BattleContext &bc, const SearchParams &p) {
    if (bc.outcome == Outcome::PLAYER_VICTORY) {
        double v = p.winBonus + p.hpWeight * bc.player.curHp + p.potionValue * bc.potionCount
                   + p.maxHpWeight * bc.player.maxHp - bc.turn * 0.01;
        if (bc.player.curHp < p.lowHpThresh) v -= p.lowHpPen * (p.lowHpThresh - bc.player.curHp);  // concave HP utility
        if (bc.miscBits.test(1)) v -= p.escapePenalty;  // Smoke Bomb escape forfeits the fight's rewards
        return v;
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
    if (bc.outcome == Outcome::UNDECIDED) {
        if (p.leafVf && vf::VF_TRAINED) return vf::value(bc);  // learned leaf value (Phase A/B)
        // rollout cutoff (fight unresolved): monotone heuristic between loss and win —
        // credit for enemy HP removed, current HP, potions kept. Tunable via cut_* params.
        return p.cutWinFrac * p.winBonus * (1.0 - ratio)
             + p.cutHpFrac * p.hpWeight * bc.player.curHp
             + p.potionValue * bc.potionCount / 2;
    }
    // loss: partial credit for damage dealt and survival time
    return (1.0 - ratio) * 10 + bc.turn * 0.2 + p.potionValue * bc.potionCount / 2;
}

double FairSearcher::rollout(BattleContext &bc) {
    const int stopTurn = bc.turn + p.rolloutTurns;
    std::uniform_real_distribution<double> U(0, 1);
    while (bc.outcome == Outcome::UNDECIDED && bc.turn < stopTurn) {
        search::Action acts[MAX_ACTIONS];
        const int nActs = enumerateActionsInto(bc, p.allowPotions, acts, MAX_ACTIONS);
        if (nActs == 0) break;
        // weighted pick: END_TURN has weight endTurnRolloutWeight, others 1
        double total = 0;
        // block-aware heuristic: visible intent damage vs current block
        int need = 0;
        if (p.rollBlockW != 1.0 || p.rollAtkW != 1.0) {
            int incoming = 0;
            for (int i = 0; i < bc.monsters.monsterCount; ++i) {
                const auto &m = bc.monsters.arr[i];
                if (m.isDeadOrEscaped() || m.isHalfDead()) continue;
                auto d = m.getMoveBaseDamage(bc);
                incoming += m.calculateDamageToPlayer(bc, d.damage) * d.attackCount;
            }
            need = incoming - bc.player.block;
        }
        auto wOf = [&](const search::Action &a) {
            switch (a.getActionType()) {
                case search::ActionType::END_TURN: return p.endTurnRolloutWeight;
                case search::ActionType::POTION: return p.potionRolloutWeight;
                case search::ActionType::CARD:
                    if (need > 0 || p.rollAtkW != 1.0) {
                        const auto t = bc.cards.hand[a.getSourceIdx()].getType();
                        if (need > 0 && t == CardType::SKILL) return p.rollBlockW;
                        if (need <= 0 && t == CardType::ATTACK) return p.rollAtkW;
                    }
                    if (p.rolloutOrderK != 0.0) {
                        const auto id = bc.cards.hand[a.getSourceIdx()].getId();
                        return std::exp(-p.rolloutOrderK * search::Expert::getPlayOrdering(id) / 10.0);
                    }
                    return 1.0;
                default: return 1.0;
            }
        };
        for (int i = 0; i < nActs; ++i) total += wOf(acts[i]);
        double r = U(rng) * total;
        search::Action chosen = acts[nActs - 1];
        for (int i = 0; i < nActs; ++i) {
            r -= wOf(acts[i]);
            if (r <= 0) { chosen = acts[i]; break; }
        }
        FAIR_TRACE_ACTION(chosen, bc, "roll");
        chosen.execute(bc);
        ++actionsExecuted;
        FAIR_CHECK(bc, "rollout");
    }
    return evaluate(bc);
}

int FairSearcher::select(const Node &node) const {
    const double range = (vmax > vmin) ? (vmax - vmin) : 1.0;
    int best = 0;
    double bestV = -1e18;
    if ((p.usePrior && pf::PF_TRAINED) || (p.priorMlp && pfm::PFM_TRAINED)) {
        // PUCT (AlphaZero form); unvisited edges use the parent's mean as first-play urgency
        const double sqrtN = std::sqrt(static_cast<double>(node.visits) + 1);
        const double fpu = node.visits > 0 ? (node.wsum / node.visits - vmin) / range : 0.5;
        for (int i = 0; i < static_cast<int>(node.edges.size()); ++i) {
            const auto &e = node.edges[i];
            const double q = e.n > 0 ? (e.w / e.n - vmin) / range : fpu;
            const double u = q + p.priorC * e.prior * sqrtN / (1.0 + e.n);
            if (u > bestV) { bestV = u; best = i; }
        }
        return best;
    }
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
    FAIR_CHECK(bc, "root");
    determinise(bc, rng, p.knownTop);
    FAIR_CHECK(bc, "determinise");

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
        FAIR_TRACE_ACTION(action, bc, "tree");
        action.execute(bc);
        ++actionsExecuted;
        FAIR_CHECK(bc, "tree");
        const auto h = observationHash(bc);
        auto &children = nodes[cur].edges[ei].children;
        auto it = children.find(h);
        if (it == children.end()) {
            const bool terminal = bc.outcome != Outcome::UNDECIDED;
            const int child = terminal ? -1 : newNode(bc, static_cast<int>(path.size()));
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
    nodes[rootIdx].wsum += value;
    for (auto &[ni, ei] : path) {
        auto &e = nodes[ni].edges[ei];
        e.n += 1;
        e.w += value;
        if (ni != rootIdx) { nodes[ni].visits += 1; nodes[ni].wsum += value; }
    }
}

void FairSearcher::rootStats(std::vector<std::pair<search::Action, std::int64_t>> &out) const {
    out.clear();
    if (nodes.empty()) return;
    for (const auto &e : nodes[rootIdx].edges) out.emplace_back(e.action, e.n);
}

void FairSearcher::rootEdgeStats(std::vector<EdgeStat> &out, double &rootMean) const {
    out.clear(); rootMean = 0.0;
    if (nodes.empty()) return;
    const Node &r = nodes[rootIdx];
    rootMean = r.visits > 0 ? r.wsum / r.visits : 0.0;
    for (const auto &e : r.edges) out.push_back({e.action, e.n, e.n > 0 ? e.w / e.n : 0.0});
}

search::Action FairSearcher::search() {
    if (nodes.empty()) { newNode(*rootState); rootIdx = 0; }
    actionsExecuted = 0;
    simulationsRun = 0;
    if (nodes[rootIdx].edges.size() == 1) return nodes[rootIdx].edges[0].action;
    if (p.budgetActions > 0) {
        // compute-proportional budget: cheaper simulations (e.g. rollout cutoff) buy more of them
        while (actionsExecuted < p.budgetActions) { simulate(); ++simulationsRun; }
    } else {
        for (int i = 0; i < p.simulations; ++i) { simulate(); ++simulationsRun; }
    }
    const auto &edges = nodes[rootIdx].edges;
    int best = 0;
    for (int i = 1; i < static_cast<int>(edges.size()); ++i) {
        if (edges[i].n > edges[best].n) best = i;
    }
    return edges[best].action;
}

}  // namespace fair
