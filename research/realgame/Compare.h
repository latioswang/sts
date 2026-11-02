// Field-by-field comparison of simulator state with a Communication Mod game_state.
#pragma once

#include "combat/BattleContext.h"
#include "game/GameContext.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace realgame {

using json = nlohmann::json;

struct Diff {
    std::string field;
    std::string simulator;
    std::string game;
    // hard: the states differ. soft: the comparison itself is unsure (ID table gap), logged but not a stop.
    bool hard = true;
};

using Diffs = std::vector<Diff>;

// HP, gold, floor, act, boss, ascension, keys, relics, potions, deck.
void compareRun(const sts::GameContext &gc, const json &gs, Diffs &out);
// Player, piles, monsters, potions and turn, using the battle's live values.
void compareBattle(const sts::BattleContext &bc, const json &gs, Diffs &out);

void compareMap(const sts::GameContext &gc, const json &gs, Diffs &out);
void compareRewards(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareCardReward(const sts::CardReward &cards, const json &screen, Diffs &out);
void compareShop(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareBossRelics(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareEventId(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareSelectCards(const sts::GameContext &gc, const json &cards, Diffs &out);

[[nodiscard]] std::string describe(const Diffs &diffs);
[[nodiscard]] bool anyHard(const Diffs &diffs);

}  // namespace realgame
