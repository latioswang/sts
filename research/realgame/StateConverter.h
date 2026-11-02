// Live mode: rebuild simulator state from what the game shows.
//
// Leak rule: only fields the player can see are read. RNG streams are seeded by the caller's own seed, and the
// draw pile is treated as a multiset (see battleFromState), except for the cards the player knows are on top.
#pragma once

#include "combat/BattleContext.h"
#include "game/GameContext.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace realgame {

using json = nlohmann::json;

// Facts the bridge has to remember itself: the game shows them one turn at a time, or not at all.
struct RunMemory {
    int nodeX = -1;
    int nodeY = -1;
    int eventChoices = 0;  // choices made in the current event room
    int eventFloor = -1;
};

struct BattleMemory {
    int floor = -1;
    int turn = -1;
    int cardsPlayed = 0;
    int attacksPlayed = 0;
    int skillsPlayed = 0;
    int knownTop = 0;
    // Each monster's moves, by the game's index in its monster list.
    std::map<int, std::vector<sts::MMID>> moves;
    std::map<int, int> attackDamage;  // bite/nip/divider base damage last seen
    std::map<int, int> stabCount;     // Book of Stabbing hits last seen

    void reset(int newFloor);
    // Call once per decision, before converting: records moves and starts a new turn's counters.
    void observe(const json &gs);
    void onCardPlayed(sts::CardType type);
};

struct ConvertNotes {
    std::vector<std::string> warnings;
    void warn(const std::string &w) { warnings.push_back(w); }
};

struct ConvertedBattle {
    sts::BattleContext bc;
    // simulator monster slot -> index in the game's monster list (they differ only once more than five have appeared)
    std::array<int, 5> gameMonsterIndex{0, 1, 2, 3, 4};
};

sts::GameContext gameContextFromState(const json &gs, std::uint64_t rngSeed, const RunMemory &memory, ConvertNotes &notes);

ConvertedBattle battleFromState(const sts::GameContext &gc, const json &gs, const BattleMemory &memory,
                                std::uint64_t rngSeed, ConvertNotes &notes);

}  // namespace realgame
