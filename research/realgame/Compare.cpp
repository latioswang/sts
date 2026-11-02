#include "Compare.h"

#include "GameIds.h"

#include <algorithm>
#include <map>
#include <sstream>

using namespace sts;

namespace realgame {

namespace {

template <typename A, typename B>
void check(Diffs &out, const std::string &field, const A &sim, const B &game, bool hard = true) {
    std::ostringstream s, g;
    s << sim;
    g << game;
    if (s.str() != g.str()) out.push_back({field, s.str(), g.str(), hard});
}

std::string cardKey(CardId id, int upgrades) {
    std::string key = cardToGameId(id);
    if (upgrades > 0) key += "+" + std::to_string(upgrades);
    return key;
}

std::string cardKey(const Card &c) { return cardKey(c.getId(), c.getUpgraded()); }

std::string cardKey(const CardInstance &c) {
    return cardKey(c.getId(), c.id == CardId::SEARING_BLOW ? c.specialData : (c.upgraded ? 1 : 0));
}

std::string cardKey(const json &c) { return c["id"].get<std::string>() + (c["upgrades"].get<int>() > 0 ? "+" + std::to_string(c["upgrades"].get<int>()) : ""); }

std::string join(const std::vector<std::string> &items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + items[i];
    return out + "]";
}

template <typename Range>
std::string simPile(const Range &pile) {
    std::vector<std::string> items;
    for (const auto &c : pile) items.push_back(cardKey(c));
    return join(items);
}

std::string gamePile(const json &pile) {
    std::vector<std::string> items;
    for (const auto &c : pile) items.push_back(cardKey(c));
    return join(items);
}

// Sim cards keep their identity by index across a pile, so the whole pile compares as one ordered string:
// any draw-order or shuffle difference shows up here.
template <typename Range>
void comparePile(Diffs &out, const std::string &field, const Range &sim, const json &game) {
    check(out, field, simPile(sim), gamePile(game));
}

void compareRelics(const GameContext &gc, const json &gs, Diffs &out) {
    std::vector<std::string> sim;
    for (const auto &r : gc.relics.relics) sim.push_back(relicToGameId(r.id));
    std::vector<std::string> game;
    for (const auto &r : gs["relics"]) game.push_back(r["id"].get<std::string>());
    check(out, "relics", join(sim), join(game));
}

void comparePotions(const std::array<Potion, 5> &potions, int capacity, const json &gs, Diffs &out) {
    std::vector<std::string> sim;
    for (int i = 0; i < capacity; ++i) {
        sim.push_back(potionToGameId(potions[i] == Potion::INVALID ? Potion::EMPTY_POTION_SLOT : potions[i]));
    }
    std::vector<std::string> game;
    for (const auto &p : gs["potions"]) game.push_back(p["id"].get<std::string>());
    check(out, "potions", join(sim), join(game));
}

std::map<std::string, int> simMonsterPowers(const Monster &m) {
    std::map<std::string, int> out;
    for (int i = 0; i < static_cast<int>(MonsterStatus::INVALID); ++i) {
        const auto s = static_cast<MonsterStatus>(i);
        const std::string id = monsterPowerToGameId(s);
        if (id == "?") continue;  // simulator bookkeeping with no power in the game (Asleep, Minion Leader)
        if (s == MonsterStatus::STRENGTH ? m.strength != 0 : m.hasStatusInternal(s)) {
            out[id] = isBooleanPower(s) ? -1 : m.getStatusInternal(s);
        }
    }
    return out;
}

std::map<std::string, int> simPlayerPowers(const Player &p) {
    std::map<std::string, int> out;
    for (int i = 1; i <= static_cast<int>(PlayerStatus::THE_BOMB); ++i) {
        const auto s = static_cast<PlayerStatus>(i);
        if (p.hasStatusRuntime(s)) out[playerPowerToGameId(s)] = playerPowerAmount(p, s);
    }
    return out;
}

std::string describePowers(const std::map<std::string, int> &powers) {
    std::vector<std::string> items;
    for (const auto &[id, amount] : powers) items.push_back(id + "=" + std::to_string(amount));
    return join(items);
}

// amount -1 means "no amount" in the game (boolean power); both sides then compare by presence only.
void comparePowers(Diffs &out, const std::string &field, std::map<std::string, int> sim, const json &game) {
    std::map<std::string, int> real;
    for (const auto &p : game) {
        const std::string id = p["id"].get<std::string>();
        if (!isUntrackedPower(id)) real[id] = p["amount"].get<int>();
    }
    for (auto &[id, amount] : real) {
        const auto it = sim.find(id);
        if (it != sim.end() && (amount == -1 || it->second == -1)) {
            amount = -1;
            it->second = -1;
        }
    }
    check(out, field, describePowers(sim), describePowers(real));
}

void compareMonster(const BattleContext &bc, const Monster &m, const json &game, const std::string &prefix, Diffs &out) {
    check(out, prefix + ".id", monsterToGameId(m.id), game["id"].get<std::string>());
    check(out, prefix + ".current_hp", m.curHp, game["current_hp"].get<int>());
    check(out, prefix + ".max_hp", m.maxHp, game["max_hp"].get<int>());
    check(out, prefix + ".block", m.block, game["block"].get<int>());
    check(out, prefix + ".half_dead", m.halfDead, game["half_dead"].get<bool>() ? 1 : 0);
    const bool gone = game["is_gone"].get<bool>();
    check(out, prefix + ".is_gone", m.isDeadOrEscaped() ? 1 : 0, gone ? 1 : 0);
    if (gone) return;  // a dead monster's leftover fields don't affect play

    comparePowers(out, prefix + ".powers", simMonsterPowers(m), game["powers"]);

    if (!game.contains("move_id")) return;  // Runic Dome hides the move
    const int moveByte = game["move_id"].get<int>();
    const auto expected = moveFromGameByte(m.id, moveByte);
    if (!expected) {
        out.push_back({prefix + ".move_id", monsterMoveStrings[static_cast<int>(m.moveHistory[0])],
                       "unmapped byte " + std::to_string(moveByte), false});
    } else {
        check(out, prefix + ".move", monsterMoveStrings[static_cast<int>(m.moveHistory[0])],
              monsterMoveStrings[static_cast<int>(*expected)]);
    }
    const DamageInfo info = m.getMoveBaseDamage(bc);
    if (info.damage > 0) {
        check(out, prefix + ".move_adjusted_damage", m.calculateDamageToPlayer(bc, info.damage),
              game.value("move_adjusted_damage", -1));
        check(out, prefix + ".move_hits", info.attackCount, game.value("move_hits", -1));
    } else {
        check(out, prefix + ".move_base_damage", 0, std::max(0, game.value("move_base_damage", 0)));
    }
}

const char *roomSymbol(Room r) {
    switch (r) {
        case Room::MONSTER: return "M";
        case Room::ELITE: return "E";
        case Room::REST: return "R";
        case Room::SHOP: return "$";
        case Room::TREASURE: return "T";
        case Room::EVENT: return "?";
        default: return "*";
    }
}

template <typename T, typename F>
std::string sortedList(const std::vector<T> &items, F toText) {
    std::vector<std::string> text;
    for (const auto &item : items) text.push_back(toText(item));
    std::sort(text.begin(), text.end());
    return join(text);
}

}  // namespace

void compareRun(const GameContext &gc, const json &gs, Diffs &out) {
    check(out, "current_hp", gc.curHp, gs["current_hp"].get<int>());
    check(out, "max_hp", gc.maxHp, gs["max_hp"].get<int>());
    check(out, "gold", gc.gold, gs["gold"].get<int>());
    check(out, "floor", gc.floorNum, gs["floor"].get<int>());
    check(out, "act", gc.act, gs["act"].get<int>());
    check(out, "ascension_level", gc.ascension, gs["ascension_level"].get<int>());
    check(out, "act_boss", encounterToGameId(gc.boss), gs["act_boss"].get<std::string>());
    if (gs.contains("keys")) {
        check(out, "keys.ruby", gc.hasKey(Key::RUBY_KEY) ? 1 : 0, gs["keys"]["ruby"].get<bool>() ? 1 : 0);
        check(out, "keys.emerald", gc.hasKey(Key::EMERALD_KEY) ? 1 : 0, gs["keys"]["emerald"].get<bool>() ? 1 : 0);
        check(out, "keys.sapphire", gc.hasKey(Key::SAPPHIRE_KEY) ? 1 : 0, gs["keys"]["sapphire"].get<bool>() ? 1 : 0);
    }
    compareRelics(gc, gs, out);
    comparePotions(gc.potions, gc.potionCapacity, gs, out);
    comparePile(out, "deck", gc.deck.cards, gs["deck"]);
}

void compareBattle(const BattleContext &bc, const json &gs, Diffs &out, const std::array<int, 5> *gameIndex) {
    const json &cs = gs["combat_state"];
    const json &p = cs["player"];
    check(out, "player.current_hp", bc.player.curHp, p["current_hp"].get<int>());
    check(out, "player.max_hp", bc.player.maxHp, p["max_hp"].get<int>());
    check(out, "player.block", bc.player.block, p["block"].get<int>());
    check(out, "player.energy", bc.player.energy, p["energy"].get<int>());
    comparePowers(out, "player.powers", simPlayerPowers(bc.player), p["powers"]);

    std::vector<std::string> simHand;
    std::vector<std::string> gameHand;
    for (int i = 0; i < bc.cards.cardsInHand; ++i) {
        const auto &c = bc.cards.hand[i];
        // The simulator writes a curse's "unplayable" cost as -3, the game as -2. Neither ever pays it.
        const int cost = c.costForTurn == -3 ? -2 : c.costForTurn;
        simHand.push_back(cardKey(c) + "(" + std::to_string(cost) + ")");
    }
    // While a hand-select screen is up the game moves selected and ineligible cards out of `hand`.
    if (gs["screen_type"] == "NONE") {
        for (const auto &c : cs["hand"]) gameHand.push_back(cardKey(c) + "(" + std::to_string(c["cost"].get<int>()) + ")");
        check(out, "hand", join(simHand), join(gameHand));
    }
    comparePile(out, "draw_pile", bc.cards.drawPile, cs["draw_pile"]);
    comparePile(out, "discard_pile", bc.cards.discardPile, cs["discard_pile"]);
    comparePile(out, "exhaust_pile", bc.cards.exhaustPile, cs["exhaust_pile"]);

    comparePotions(bc.potions, bc.potionCapacity, gs, out);

    const json &monsters = cs["monsters"];
    const std::vector<int> slots = occupiedMonsterSlots(bc);
    if (gameIndex) {
        for (const int slot : slots) {
            const int index = (*gameIndex)[slot];
            compareMonster(bc, bc.monsters.arr[slot], monsters[index], "monster[" + std::to_string(index) + "]", out);
        }
        return;
    }
    const std::array<int, 5> aligned = gameIndexBySlot(bc, monsters);
    const int count = static_cast<int>(monsters.size());
    std::vector<bool> matched(monsters.size(), false);
    for (const int slot : slots) {
        const int index = aligned[slot];
        if (index >= count) {
            out.push_back({"monsters.count", std::to_string(slots.size()), std::to_string(count), true});
            return;
        }
        matched[index] = true;
        compareMonster(bc, bc.monsters.arr[slot], monsters[index], "monster[" + std::to_string(index) + "]", out);
    }
    for (int i = 0; i < count; ++i) {
        if (!matched[i] && !monsters[i]["is_gone"].get<bool>()) {
            out.push_back({"monster[" + std::to_string(i) + "].id", "(none)", monsters[i]["id"].get<std::string>(), true});
        }
    }
}

void compareMap(const GameContext &gc, const json &gs, Diffs &out) {
    std::map<std::pair<int, int>, const json *> real;
    for (const auto &node : gs["map"]) real[{node["x"].get<int>(), node["y"].get<int>()}] = &node;

    const int rows = static_cast<int>(gc.map->nodes.size());
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < 7; ++x) {
            const MapNode &node = gc.map->getNode(x, y);
            const auto it = real.find({x, y});
            const bool simHas = node.edgeCount > 0;
            const bool gameHas = it != real.end();
            const std::string name = "map(" + std::to_string(x) + "," + std::to_string(y) + ")";
            if (!simHas && !gameHas) continue;
            if (simHas != gameHas) {
                out.push_back({name, simHas ? "node" : "no node", gameHas ? "node" : "no node", true});
                continue;
            }
            check(out, name + ".symbol", roomSymbol(node.room), (*it->second)["symbol"].get<std::string>());
            if (y >= 14) continue;  // the last row's only edge is the boss
            std::vector<int> simEdges(node.edges.begin(), node.edges.begin() + node.edgeCount);
            std::vector<int> gameEdges;
            for (const auto &c : (*it->second)["children"]) gameEdges.push_back(c["x"].get<int>());
            auto text = [](const int &v) { return std::to_string(v); };
            check(out, name + ".children", sortedList(simEdges, text), sortedList(gameEdges, text));
        }
    }
}

void compareRewards(const GameContext &gc, const json &screen, Diffs &out) {
    const Rewards &r = gc.info.rewardsContainer;
    std::vector<int> simGold(r.gold.begin(), r.gold.begin() + r.goldRewardCount);
    std::vector<std::string> simRelics;
    std::vector<std::string> simPotions;
    for (int i = 0; i < r.relicCount; ++i) simRelics.push_back(relicToGameId(r.relics[i]));
    for (int i = 0; i < r.potionCount; ++i) simPotions.push_back(potionToGameId(r.potions[i]));

    std::vector<int> gameGold;
    std::vector<std::string> gameRelics;
    std::vector<std::string> gamePotions;
    int gameCards = 0;
    for (const auto &item : screen["rewards"]) {
        const std::string type = item["reward_type"];
        if (type == "GOLD" || type == "STOLEN_GOLD") gameGold.push_back(item["gold"].get<int>());
        else if (type == "RELIC") gameRelics.push_back(item["relic"]["id"].get<std::string>());
        else if (type == "POTION") gamePotions.push_back(item["potion"]["id"].get<std::string>());
        else if (type == "CARD") ++gameCards;
    }
    auto number = [](const int &v) { return std::to_string(v); };
    auto text = [](const std::string &v) { return v; };
    check(out, "rewards.gold", sortedList(simGold, number), sortedList(gameGold, number));
    check(out, "rewards.relics", sortedList(simRelics, text), sortedList(gameRelics, text));
    check(out, "rewards.potions", sortedList(simPotions, text), sortedList(gamePotions, text));
    check(out, "rewards.card_count", r.cardRewardCount, gameCards);
}

void compareCardReward(const CardReward &cards, const json &screen, Diffs &out) {
    comparePile(out, "card_reward", cards, screen["cards"]);
}

void compareShop(const GameContext &gc, const json &screen, Diffs &out) {
    const Shop &s = gc.info.shop;
    std::vector<std::string> sim;
    std::vector<std::string> game;
    for (int i = 0; i < 7; ++i) {
        if (s.cardPrice(i) != -1) sim.push_back(cardKey(s.cards[i]) + "@" + std::to_string(s.cardPrice(i)));
    }
    for (const auto &c : screen["cards"]) game.push_back(cardKey(c) + "@" + std::to_string(c["price"].get<int>()));
    check(out, "shop.cards", join(sim), join(game));

    sim.clear();
    game.clear();
    for (int i = 0; i < 3; ++i) {
        if (s.relicPrice(i) != -1) sim.push_back(relicToGameId(s.relics[i]) + "@" + std::to_string(s.relicPrice(i)));
    }
    for (const auto &r : screen["relics"]) game.push_back(r["id"].get<std::string>() + "@" + std::to_string(r["price"].get<int>()));
    check(out, "shop.relics", join(sim), join(game));

    sim.clear();
    game.clear();
    for (int i = 0; i < 3; ++i) {
        if (s.potionPrice(i) != -1) sim.push_back(potionToGameId(s.potions[i]) + "@" + std::to_string(s.potionPrice(i)));
    }
    for (const auto &p : screen["potions"]) game.push_back(p["id"].get<std::string>() + "@" + std::to_string(p["price"].get<int>()));
    check(out, "shop.potions", join(sim), join(game));

    check(out, "shop.purge_available", s.removeCost != -1 ? 1 : 0, screen["purge_available"].get<bool>() ? 1 : 0);
    if (s.removeCost != -1) check(out, "shop.purge_cost", s.removeCost, screen["purge_cost"].get<int>());
}

void compareBossRelics(const GameContext &gc, const json &screen, Diffs &out) {
    std::vector<std::string> sim;
    std::vector<std::string> game;
    for (int i = 0; i < 3; ++i) sim.push_back(relicToGameId(gc.info.bossRelics[i]));
    for (const auto &r : screen["relics"]) game.push_back(r["id"].get<std::string>());
    check(out, "boss_relics", join(sim), join(game));
}

void compareEventId(const GameContext &gc, const json &screen, Diffs &out) {
    const std::string gameId = screen.value("event_id", "");
    if (gc.curEvent == Event::NEOW) {
        check(out, "event_id", "Neow Event", gameId);
    } else {
        check(out, "event_id", eventToGameId(gc.curEvent), gameId);
    }
}

void compareSelectCards(const GameContext &gc, const json &cards, Diffs &out) {
    std::vector<std::string> sim;
    std::vector<std::string> game;
    for (int i = 0; i < gc.info.toSelectCards.size(); ++i) sim.push_back(cardKey(gc.info.toSelectCards[i].card));
    for (const auto &c : cards) game.push_back(cardKey(c));
    auto text = [](const std::string &v) { return v; };
    // The game's grid may sort differently; which cards are offered is what matters.
    check(out, "select_cards", sortedList(sim, text), sortedList(game, text));
}

json unselectedCards(const json &screen) {
    json out = json::array();
    std::vector<std::string> selected;
    if (screen.contains("selected_cards")) {
        for (const auto &c : screen["selected_cards"]) selected.push_back(c.value("uuid", ""));
    }
    for (const auto &c : screen["cards"]) {
        if (std::find(selected.begin(), selected.end(), c.value("uuid", "")) == selected.end()) out.push_back(c);
    }
    return out;
}

std::vector<int> occupiedMonsterSlots(const BattleContext &bc) {
    std::vector<int> slots;
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        if (bc.monsters.arr[i].id != MonsterId::INVALID) slots.push_back(i);
    }
    return slots;
}

std::array<int, 5> gameIndexBySlot(const BattleContext &bc) {
    std::array<int, 5> out{0, 1, 2, 3, 4};
    const std::vector<int> slots = occupiedMonsterSlots(bc);
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) out[slots[i]] = i;
    return out;
}

// The game keeps split slimes and dead summons in its list, and the simulator reuses their slots. So a dead game monster
// that isn't the next simulator monster has no slot of its own, and a live simulator monster can't be a dead game one.
std::array<int, 5> gameIndexBySlot(const BattleContext &bc, const json &gameMonsters) {
    std::array<int, 5> out{0, 1, 2, 3, 4};
    const int count = static_cast<int>(gameMonsters.size());
    int index = 0;
    for (const int slot : occupiedMonsterSlots(bc)) {
        const Monster &m = bc.monsters.arr[slot];
        const std::string id = monsterToGameId(m.id);
        while (index < count && gameMonsters[index]["is_gone"].get<bool>()
               && (!m.isDeadOrEscaped() || gameMonsters[index]["id"].get<std::string>() != id)) {
            ++index;
        }
        out[slot] = index;
        ++index;
    }
    return out;
}

int playerPowerAmount(const Player &p, PlayerStatus s) {
    switch (s) {
        case PS::ARTIFACT:
        case PS::DEXTERITY:
        case PS::FOCUS:
        case PS::STRENGTH:
            return p.getStatusRuntime(s);
        default: {
            const auto it = p.statusMap.find(s);
            return it == p.statusMap.end() ? -1 : it->second;
        }
    }
}

std::string describe(const Diffs &diffs) {
    std::ostringstream os;
    for (const auto &d : diffs) {
        os << (d.hard ? "  " : "  (soft) ") << d.field << ": sim=" << d.simulator << " game=" << d.game << "\n";
    }
    return os.str();
}

bool anyHard(const Diffs &diffs) {
    return std::any_of(diffs.begin(), diffs.end(), [](const Diff &d) { return d.hard; });
}

}  // namespace realgame
