#include "FairAgent.h"

#include "combat/BattleContext.h"
#include "constants/MonsterEncounters.h"
#include "sim/search/GameAction.h"
#include "sim/search/ExpertKnowledge.h"
#include <cstdlib>
#include <memory>
#include <iostream>

using namespace sts;

namespace fair {

double getp(const Params &p, const std::string &k, double def) {
    auto it = p.find(k);
    return it == p.end() ? def : it->second;
}

FairAgent::FairAgent(const Params &params, std::uint64_t agentSeed) : P(params), rng(agentSeed) {
    sp.simulations = static_cast<int>(getp(P, "sims", 500));
    sp.exploration = getp(P, "c", 0.7);
    sp.rolloutTurns = static_cast<int>(getp(P, "rollout_turns", 99));
    sp.endTurnRolloutWeight = getp(P, "end_turn_w", 1.0);
    sp.potionRolloutWeight = getp(P, "potion_roll_w", 1.0);
    sp.hpWeight = getp(P, "hp_w", 1.0);
    sp.potionValue = getp(P, "potion_v", 4.0);
    sp.winBonus = getp(P, "win_bonus", 35.0);
    sp.maxHpWeight = getp(P, "maxhp_w", 0.0);
    legacy.rng = std::default_random_engine(static_cast<unsigned>(agentSeed ^ 0xabcdefULL));
}

static bool isEliteEncounter(MonsterEncounter e) {
    using ME = MonsterEncounter;
    switch (e) {
        case ME::GREMLIN_NOB: case ME::LAGAVULIN: case ME::THREE_SENTRIES:
        case ME::GREMLIN_LEADER: case ME::SLAVERS: case ME::BOOK_OF_STABBING:
        case ME::GIANT_HEAD: case ME::NEMESIS: case ME::REPTOMANCER:
            return true;
        default: return false;
    }
}

SearchParams FairAgent::searchParamsFor(const BattleContext &bc) const {
    SearchParams s = sp;
    if (isBossEncounter(bc.encounter)) {
        s.simulations = static_cast<int>(s.simulations * getp(P, "boss_mult", 1.0));
        s.potionValue = getp(P, "potion_v_boss", s.potionValue);
    } else if (isEliteEncounter(bc.encounter)) {
        s.potionValue = getp(P, "potion_v_elite", s.potionValue);
    }
    return s;
}

void FairAgent::playBattle(GameContext &gc) {
    BattleContext bc;
    bc.init(gc);
    ++stats.battles;
    int steps = 0;
    const bool reuse = getp(P, "tree_reuse", 0) > 0;
    std::unique_ptr<FairSearcher> searcher;
    while (bc.outcome == Outcome::UNDECIDED) {
        if (++steps > 3000) {  // safety: pathological loop
            stats.aborted = true;
            bc.outcome = Outcome::PLAYER_LOSS;
            break;
        }
        if (!reuse || !searcher) searcher = std::make_unique<FairSearcher>(bc, searchParamsFor(bc), rng());
        FairSearcher &s = *searcher;
        s.setRoot(bc);
        auto a = s.search();
        ++stats.combatDecisions;
        static const bool dbg = std::getenv("FAIR_DEBUG") != nullptr;
        if (dbg) {
            std::cerr << "turn " << bc.turn << " hp " << bc.player.curHp << " e " << bc.player.energy << " ";
            a.printDesc(std::cerr, bc) << " visits=" << s.rootVisits() << "\n";
        }
        a.execute(bc);
        if (reuse) s.advance(a, bc);
    }
    bc.exitBattle(gc);
}

// ---------------- card values from the repo KB tier list ----------------
double kbValue(CardId id) {
    switch (id) {
#include "kb_card_values.inc"
        default: return 3.0;  // colorless / unknown: treat as C-
    }
}

static double cardValue(const Card &c) {
    return kbValue(c.getId()) + (c.isUpgraded() ? 0.5 : 0.0);
}

static bool isAoe(CardId id) {
    switch (id) {
        case CardId::CLEAVE: case CardId::WHIRLWIND: case CardId::THUNDERCLAP:
        case CardId::IMMOLATE: case CardId::REAPER: return true;
        default: return false;
    }
}

// deck-aware value of adding card c (defaults reduce to plain KB value)
double FairAgent::addValue(const GameContext &gc, const Card &c) const {
    double v = cardValue(c);
    int aoe = 0, dups = 0;
    for (const auto &d : gc.deck.cards) {
        if (isAoe(d.getId())) ++aoe;
        if (d.getId() == c.getId()) ++dups;
    }
    if (isAoe(c.getId()) && aoe == 0 && gc.act <= 2) v += getp(P, "aoe_bonus", 0.0);
    v -= getp(P, "dup_pen", 0.0) * dups;
    if (c.getType() == CardType::POWER) v += getp(P, "power_bonus", 0.0);
    return v;
}

// ---------------- events (rules written from lightspeed's event code) ----------------
int FairAgent::eventChoice(const GameContext &gc) const {
    const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
    (void)hpf;
#include "event_policy.inc"
    return -1;
}

// ---------------- boss relics ----------------
static double kbBossRelicValue(RelicId id) {
    switch (id) {
#include "kb_boss_relic_values.inc"
        default: return 5.0;
    }
}

int FairAgent::bossRelicChoice(const GameContext &gc) const {
    // GUARDRAIL: never take Runic Dome. The simulator still exposes monster
    // intents to the search with Dome, so taking it would be a hidden-info leak.
    int best = 3;  // 3 = skip
    double bestV = -1e18;
    for (int i = 0; i < 3; ++i) {
        const RelicId r = gc.info.bossRelics[i];
        if (r == RelicId::RUNIC_DOME) continue;
        const double v = getp(P, "boss_pol", 0) > 0 ? kbBossRelicValue(r)
                                                    : -static_cast<double>(search::Expert::getBossRelicOrdering(r));
        if (v > bestV) { bestV = v; best = i; }
    }
    return best;
}

// ---------------- map ----------------
double FairAgent::roomValue(const GameContext &gc, Room r) const {
    const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
    switch (r) {
        case Room::MONSTER: return getp(P, "w_monster", 1.0);
        case Room::ELITE: {
            const double th = getp(P, "elite_hp", 0.6);
            double v = hpf >= th ? getp(P, "w_elite_hi", 1.5) : getp(P, "w_elite_lo", -3.0);
            if (gc.act == 1) v += getp(P, "w_elite_act1", 0.0);
            return v;
        }
        case Room::REST: return hpf < getp(P, "rest_hp", 0.5) ? getp(P, "w_rest_lo", 3.0) : getp(P, "w_rest_hi", 1.0);
        case Room::SHOP: return gc.gold >= getp(P, "shop_gold", 150) ? getp(P, "w_shop_rich", 2.0) : getp(P, "w_shop_poor", 0.0);
        case Room::EVENT: return getp(P, "w_event", 0.8);
        case Room::TREASURE: return getp(P, "w_treasure", 1.5);
        default: return 0.0;
    }
}

int FairAgent::chooseMapX(const GameContext &gc) {
    const Map &m = *gc.map;
    // DP over rows: best[y][x] = value(room) + max over edges best[y+1][e]
    double best[15][7];
    const double discount = getp(P, "map_discount", 0.9);
    for (int y = 14; y >= 0; --y) {
        for (int x = 0; x < 7; ++x) {
            const auto &n = m.getNode(x, y);
            double nxt = 0;
            if (y < 14 && n.edgeCount > 0) {
                nxt = -1e9;
                for (int i = 0; i < n.edgeCount; ++i) nxt = std::max(nxt, best[y + 1][n.edges[i]]);
            }
            best[y][x] = roomValue(gc, n.room) + discount * nxt;
        }
    }
    auto acts = search::GameAction::getAllActionsInState(gc);
    const int nextY = gc.curMapNodeY + 1;
    int bestX = acts.front().getIdx1();
    double bestV = -1e18;
    for (auto &a : acts) {
        const int x = a.getIdx1();
        const double v = best[nextY][x] + 1e-6 * x;
        if (v > bestV) { bestV = v; bestX = x; }
    }
    return bestX;
}

// ---------------- card rewards ----------------
void FairAgent::cardRewards(GameContext &gc) {
    auto &r = gc.info.rewardsContainer;
    for (int rIdx = r.cardRewardCount - 1; rIdx >= 0; --rIdx) {
        int bestI = -1;
        double bestV = -1e18;
        for (int i = 0; i < r.cardRewards[rIdx].size(); ++i) {
            const double v = addValue(gc, r.cardRewards[rIdx][i]);
            if (v > bestV) { bestV = v; bestI = i; }
        }
        const double thresh = getp(P, "card_thresh", 5.0)
                + getp(P, "card_size_pen", 0.1) * std::max(0, gc.deck.size() - 15)
                - (gc.act == 1 ? getp(P, "card_act1_bonus", 1.0) : 0.0);
        if (bestI >= 0 && bestV >= thresh) {
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, bestI).execute(gc);
        } else {
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, 5).execute(gc);  // skip
        }
    }
}

// ---------------- shop ----------------
void FairAgent::shop(GameContext &gc) {
    using RT = search::GameAction::RewardsActionType;
    auto &s = gc.info.shop;
    if (s.removeCost != -1 && gc.gold >= s.removeCost && getp(P, "shop_remove", 1) > 0) {
        search::GameAction(RT::CARD_REMOVE).execute(gc);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        if (s.relicPrice(i) != -1 && gc.gold >= s.relicPrice(i) && getp(P, "shop_relic", 1) > 0) {
            search::GameAction(RT::RELIC, i).execute(gc);
            return;
        }
    }
    int bestI = -1; double bestV = -1e18;
    for (int i = 0; i < 7; ++i) {
        if (s.cardPrice(i) == -1 || gc.gold < s.cardPrice(i)) continue;
        const double v = addValue(gc, s.cards[i]);
        if (v > bestV) { bestV = v; bestI = i; }
    }
    if (bestI >= 0 && bestV >= getp(P, "shop_card_thresh", 7.0)) {
        search::GameAction(RT::CARD, bestI).execute(gc);
        return;
    }
    search::GameAction(RT::SKIP).execute(gc);
}

// ---------------- removal / upgrade selection ----------------
void FairAgent::cardSelect(GameContext &gc) {
    int bestI = 0; double bestV = -1e18;
    for (int i = 0; i < gc.info.toSelectCards.size(); ++i) {
        const auto &c = gc.info.toSelectCards[i].card;
        double v;
        switch (gc.info.selectScreenType) {
            case CardSelectScreenType::REMOVE:
            case CardSelectScreenType::TRANSFORM:
            case CardSelectScreenType::TRANSFORM_UPGRADE:
            case CardSelectScreenType::BONFIRE_SPIRITS:
                // remove curses first, then strikes, then defends, then lowest value
                v = c.getType() == CardType::CURSE ? 100
                    : c.getId() == CardId::STRIKE_RED ? 50 + getp(P, "strike_before_defend", 1)
                    : c.getId() == CardId::DEFEND_RED ? 50
                    : 10 - cardValue(c);
                break;
            case CardSelectScreenType::UPGRADE:
                v = kbValue(c.getId()) + (c.getId() == CardId::BASH ? getp(P, "bash_upg_bonus", 2.0) : 0.0);
                break;
            default:
                v = cardValue(c);
                break;
        }
        if (v > bestV) { bestV = v; bestI = i; }
    }
    search::GameAction(bestI).execute(gc);
}

void FairAgent::stepOutOfCombat(GameContext &gc) {
    using RT = search::GameAction::RewardsActionType;
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN:
            if (getp(P, "map_pol", 0) > 0 && gc.curMapNodeY < 14) {
                search::GameAction(chooseMapX(gc)).execute(gc);
                return;
            }
            break;
        case ScreenState::BOSS_RELIC_REWARDS:
            search::GameAction(bossRelicChoice(gc)).execute(gc);
            return;
        case ScreenState::EVENT_SCREEN:
            if (getp(P, "ev_pol", 0) > 0) {
                const int idx = eventChoice(gc);
                if (idx >= 0) { search::GameAction(idx).execute(gc); return; }
            }
            break;
        case ScreenState::REST_ROOM:
            if (getp(P, "rest_pol", 0) > 0) {
                const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
                const bool preBoss = gc.curMapNodeY >= 13;
                const double th = preBoss ? getp(P, "rest_boss_frac", 0.7) : getp(P, "rest_heal_frac", 0.5);
                const bool canHeal = !gc.relics.has(RelicId::COFFEE_DRIPPER);
                const bool canUpg = !gc.relics.has(RelicId::FUSION_HAMMER) && gc.deck.getUpgradeableCount() > 0;
                if (canHeal && (hpf < th || !canUpg)) { search::GameAction(0).execute(gc); return; }
                if (canUpg) { search::GameAction(1).execute(gc); return; }
            }
            break;
        case ScreenState::REWARDS: {
            auto &r = gc.info.rewardsContainer;
            if (getp(P, "card_pol", 0) > 0 && r.goldRewardCount == 0 && r.relicCount == 0
                && r.potionCount == 0 && r.cardRewardCount > 0) {
                cardRewards(gc);
                return;
            }
            break;
        }
        case ScreenState::SHOP_ROOM:
            if (getp(P, "shop_pol", 0) > 0) { shop(gc); return; }
            break;
        case ScreenState::CARD_SELECT:
            if (getp(P, "select_pol", 0) > 0) { cardSelect(gc); return; }
            break;
        default:
            break;
    }
    legacy.stepOutOfCombatPolicy(gc);
}

void FairAgent::playout(GameContext &gc) {
    int guard = 0;
    while (gc.outcome == GameOutcome::UNDECIDED) {
        if (++guard > 20000) { stats.aborted = true; break; }
        if (gc.screenState == ScreenState::BATTLE) {
            playBattle(gc);
            continue;
        }
        stepOutOfCombat(gc);
    }
}

}  // namespace fair
