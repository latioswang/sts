// Readable full-game trace for replay audits (opt-in: FAIR_TRACE=<file>; only the top-level game agent writes, never the
// hypothetical copies used by the search or by decision simulations). Writing the trace changes nothing in play.
#pragma once

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "combat/BattleContext.h"
#include "constants/Events.h"
#include "constants/MonsterEncounters.h"
#include "constants/MonsterMoves.h"
#include "constants/Potions.h"
#include "constants/Relics.h"
#include "constants/Rooms.h"
#include "game/GameContext.h"

namespace fair::trace {

inline const void *g_owner = nullptr;  // the FairAgent playing the real game
inline std::FILE *g_f = nullptr;
inline bool on(const void *self) { return g_f && self == g_owner; }

inline const char *screenName(sts::ScreenState s) {
    using S = sts::ScreenState;
    switch (s) {
        case S::EVENT_SCREEN: return "EVENT"; case S::REWARDS: return "REWARDS"; case S::BOSS_RELIC_REWARDS: return "BOSS_RELIC";
        case S::CARD_SELECT: return "CARD_SELECT"; case S::MAP_SCREEN: return "MAP"; case S::TREASURE_ROOM: return "TREASURE";
        case S::REST_ROOM: return "REST"; case S::SHOP_ROOM: return "SHOP"; case S::BATTLE: return "BATTLE"; default: return "OTHER";
    }
}

inline std::string cardName(const sts::Card &c) { return std::string(c.getName()) + (c.upgraded ? "+" : ""); }

struct Snap {
    int floor = 0, act = 0, hp = 0, maxHp = 0, gold = 0;
    std::map<std::string, int> deck;
    std::vector<std::string> relics, potions;
};
inline Snap snap(const sts::GameContext &gc) {
    Snap s; s.floor = gc.floorNum; s.act = gc.act; s.hp = gc.curHp; s.maxHp = gc.maxHp; s.gold = gc.gold;
    for (const auto &c : gc.deck.cards) ++s.deck[cardName(c)];
    for (const auto &r : gc.relics.relics) s.relics.emplace_back(sts::getRelicName(r.id));
    for (int i = 0; i < gc.potionCapacity; ++i)
        if (gc.potions[i] != sts::Potion::EMPTY_POTION_SLOT && gc.potions[i] != sts::Potion::INVALID) s.potions.emplace_back(sts::getPotionName(gc.potions[i]));
    return s;
}
inline std::string state(const sts::GameContext &gc) {
    char b[160];
    std::snprintf(b, sizeof b, "F%d A%d hp %d/%d gold %d", gc.floorNum, gc.act, gc.curHp, gc.maxHp, gc.gold);
    return b;
}

// what the screen offers, before the agent decides
inline void offer(const sts::GameContext &gc) {
    using S = sts::ScreenState;
    std::string o;
    if (gc.screenState == S::REWARDS) {
        const auto &r = gc.info.rewardsContainer;
        for (int i = 0; i < r.goldRewardCount; ++i) o += " gold:" + std::to_string(r.gold[i]);
        for (int i = 0; i < r.relicCount; ++i) o += std::string(" relic:") + sts::getRelicName(r.relics[i]);
        for (int i = 0; i < r.potionCount; ++i) o += std::string(" potion:") + sts::getPotionName(r.potions[i]);
        for (int i = 0; i < r.cardRewardCount; ++i) {
            o += " cards:[";
            for (int j = 0; j < static_cast<int>(r.cardRewards[i].size()); ++j) o += (j ? ", " : "") + cardName(r.cardRewards[i][j]);
            o += "]";
        }
        if (r.emeraldKey) o += " emerald_key";
        if (r.sapphireKey) o += " sapphire_key";
    } else if (gc.screenState == S::BOSS_RELIC_REWARDS) {
        for (int i = 0; i < 3; ++i) o += std::string(" ") + sts::getRelicName(gc.info.bossRelics[i]);
    } else if (gc.screenState == S::EVENT_SCREEN) {
        o += std::string(" ") + sts::eventGameNames[static_cast<int>(gc.curEvent)];
    } else if (gc.screenState == S::MAP_SCREEN && gc.map) {
        const int y = gc.curMapNodeY + 1;
        if (gc.curMapNodeY < 0) {
            for (int x = 0; x < 7; ++x) if (gc.map->getNode(x, 0).edgeCount > 0) o += std::string(" x") + std::to_string(x) + ":" + sts::roomStrings[static_cast<int>(gc.map->getNode(x, 0).room)];
        } else if (y <= 14) {
            const auto &n = gc.map->getNode(gc.curMapNodeX, gc.curMapNodeY);
            for (int i = 0; i < n.edgeCount; ++i) o += std::string(" x") + std::to_string(n.edges[i]) + ":" + sts::roomStrings[static_cast<int>(gc.map->getNode(n.edges[i], y).room)];
        } else o += " boss";
    }
    std::fprintf(g_f, "  [%s] %s offers:%s\n", screenName(gc.screenState), state(gc).c_str(), o.c_str());
}

// what changed through one out-of-combat step
inline void diff(const Snap &a, const sts::GameContext &gc, sts::ScreenState before) {
    const Snap b = snap(gc);
    std::string d;
    for (const auto &[k, v] : b.deck) { const int w = a.deck.count(k) ? a.deck.at(k) : 0; if (v > w) d += " +card:" + k + (v - w > 1 ? "x" + std::to_string(v - w) : ""); }
    for (const auto &[k, v] : a.deck) { const int w = b.deck.count(k) ? b.deck.at(k) : 0; if (v > w) d += " -card:" + k + (v - w > 1 ? "x" + std::to_string(v - w) : ""); }
    for (size_t i = a.relics.size(); i < b.relics.size(); ++i) d += " +relic:" + b.relics[i];
    if (b.potions != a.potions) {
        d += " potions:[";
        for (size_t i = 0; i < b.potions.size(); ++i) d += (i ? "," : "") + b.potions[i];
        d += "]";
    }
    if (b.gold != a.gold) d += " gold" + std::string(b.gold > a.gold ? "+" : "") + std::to_string(b.gold - a.gold);
    if (b.hp != a.hp) d += " hp" + std::string(b.hp > a.hp ? "+" : "") + std::to_string(b.hp - a.hp);
    if (b.maxHp != a.maxHp) d += " maxhp" + std::string(b.maxHp > a.maxHp ? "+" : "") + std::to_string(b.maxHp - a.maxHp);
    if (before == sts::ScreenState::MAP_SCREEN) d += std::string(" -> room ") + sts::roomStrings[static_cast<int>(gc.curRoom)] + " (x" + std::to_string(gc.curMapNodeX) + ")";
    if (gc.screenState == sts::ScreenState::BATTLE) d += std::string(" -> fight ") + sts::monsterEncounterStrings[static_cast<int>(gc.info.encounter)];
    std::fprintf(g_f, "    = %s\n", d.empty() ? " (no change)" : d.c_str());
}

inline void battleLine(const sts::BattleContext &bc, const std::string &act) {
    std::string h;
    for (int i = 0; i < bc.cards.cardsInHand; ++i) {
        const auto &c = bc.cards.hand[i];
        h += (i ? ", " : "") + std::string(c.getName()) + (c.upgraded ? "+" : "") + "(" + std::to_string(c.costForTurn) + ")";
    }
    std::string m;
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        const auto &mo = bc.monsters.arr[i];
        if (mo.isDeadOrEscaped()) continue;
        char b[200];
        std::string intent = sts::monsterMoveStrings[static_cast<int>(mo.moveHistory[0])];
        if (mo.isAttacking()) {
            const auto di = mo.getMoveBaseDamage(bc);
            intent += " " + std::to_string(mo.calculateDamageToPlayer(bc, di.damage)) + (di.attackCount > 1 ? "x" + std::to_string(di.attackCount) : "");
        }
        std::snprintf(b, sizeof b, " | %s %d/%d blk %d -> %s", mo.getName(), mo.curHp, mo.maxHp, mo.block, intent.c_str());
        m += b;
    }
    std::fprintf(g_f, "    t%d hp %d/%d blk %d e %d | hand: %s%s\n      => %s\n", bc.turn, bc.player.curHp, bc.player.maxHp,
                 bc.player.block, bc.player.energy, h.c_str(), m.c_str(), act.c_str());
}

}  // namespace fair::trace
