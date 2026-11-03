// Gives the player the same extra cards, relics and potions on both sides of a mirror run, so a batch reaches things
// the agent rarely sees. The game side needs the test-only mod in scenario_mod/; live mode never uses any of this.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "game/GameContext.h"

namespace realgame {

class ScenarioError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class ScenarioKind { Relic, Card, Potion, PoolFront, Monster, Elite, Keys };

struct ScenarioItem {
    ScenarioKind kind = ScenarioKind::Relic;
    std::string id;     // the game's id, or an encounter's key for Monster and Elite
    std::string tier;   // PoolFront only: common, uncommon, rare, shop or boss
    int upgrades = 0;   // Card only
};

// Text form, items separated by ';':
//   relic:Anchor  card:Clash  card:Searing Blow+2  potion:Fire Potion  front:rare:Bottled Flame
//   monster:Jaw Worm  elite:Lagavulin
// monster and elite replace the next encounter of that kind; front puts a relic next in line in its pool.
// Throws ScenarioError on anything the simulator has no id for.
std::vector<ScenarioItem> parseScenario(const std::string &text);

// Reads "<seed> <TAB> <scenario>" lines. Blank lines and lines starting with '#' are skipped.
std::map<std::uint64_t, std::string> loadScenarioFile(const std::string &path);

void applyScenarioToSimulator(sts::GameContext &gc, const std::vector<ScenarioItem> &items);

// Hands the items to the mod and waits for its answer. Throws ScenarioError if it reports an error or never answers.
void applyScenarioToGame(const std::vector<ScenarioItem> &items);

// The state of one of the game's random streams.
struct RngState {
    std::uint64_t seed0 = 0;
    std::uint64_t seed1 = 0;
    int counter = 0;
};

// Turns the player to a monster, as the game's card and potion screens do. Spire Shield and Spear's back attacks depend on it.
void faceGameMonster(int gameIndex);

// Every random stream in the game by name, read through the mod. Empty if the mod isn't installed.
std::map<std::string, RngState> readGameRngs();

// The folder the game runs from: STS_GAME_DIR, or ~/.sts_realgame. Empty if neither is known.
std::string gameFolder();

}  // namespace realgame
