// Field-by-field comparison of simulator state with a Communication Mod game_state.
#pragma once

#include "combat/BattleContext.h"
#include "game/GameContext.h"

#include <nlohmann/json.hpp>

#include <array>
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
// `gameIndex`, when given, says which game monster each simulator slot holds; otherwise the n-th occupied slot is
// the game's n-th monster.
void compareBattle(const sts::BattleContext &bc, const json &gs, Diffs &out, const std::array<int, 5> *gameIndex = nullptr);

void compareMap(const sts::GameContext &gc, const json &gs, Diffs &out);
void compareRewards(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareCardReward(const sts::CardReward &cards, const json &screen, Diffs &out);
void compareShop(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareBossRelics(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareEventId(const sts::GameContext &gc, const json &screen, Diffs &out);
void compareSelectCards(const sts::GameContext &gc, const json &cards, Diffs &out);
// A grid screen's cards minus the ones already selected (the game keeps those in its list).
[[nodiscard]] json unselectedCards(const json &screen);

// Simulator slots holding a monster. Some encounters leave a slot empty for a later summon, while the game's list
// has no gaps, so the n-th occupied slot is compared with the game's n-th monster.
[[nodiscard]] std::vector<int> occupiedMonsterSlots(const sts::BattleContext &bc);
[[nodiscard]] std::array<int, 5> gameIndexBySlot(const sts::BattleContext &bc);

// A player power's amount, or -1 for a power the simulator only stores as a flag.
[[nodiscard]] int playerPowerAmount(const sts::Player &p, PlayerStatus s);

[[nodiscard]] std::string describe(const Diffs &diffs);
[[nodiscard]] bool anyHard(const Diffs &diffs);

}  // namespace realgame
