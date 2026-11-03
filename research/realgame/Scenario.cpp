#include "Scenario.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#include "GameIds.h"

using namespace sts;

namespace realgame {

namespace {

constexpr int kModAnswerTimeoutSec = 30;

std::vector<RelicId> *relicPool(GameContext &gc, const std::string &tier) {
    if (tier == "common") return &gc.commonRelicPool;
    if (tier == "uncommon") return &gc.uncommonRelicPool;
    if (tier == "rare") return &gc.rareRelicPool;
    if (tier == "shop") return &gc.shopRelicPool;
    if (tier == "boss") return &gc.bossRelicPool;
    return nullptr;
}

void removeFromPools(GameContext &gc, RelicId relic) {
    for (const char *tier : {"common", "uncommon", "rare", "shop", "boss"}) {
        auto &pool = *relicPool(gc, tier);
        pool.erase(std::remove(pool.begin(), pool.end(), relic), pool.end());
    }
}

ScenarioItem parseItem(const std::string &text) {
    const auto colon = text.find(':');
    if (colon == std::string::npos) throw ScenarioError("scenario item needs a kind: " + text);
    const std::string kind = text.substr(0, colon);
    const std::string rest = text.substr(colon + 1);
    ScenarioItem item;
    item.id = rest;
    if (kind == "relic") {
        item.kind = ScenarioKind::Relic;
        if (!relicFromGameId(rest)) throw ScenarioError("unknown relic: " + rest);
    } else if (kind == "card") {
        item.kind = ScenarioKind::Card;
        const auto plus = rest.rfind('+');
        if (plus != std::string::npos) {
            item.id = rest.substr(0, plus);
            item.upgrades = std::atoi(rest.c_str() + plus + 1);
        }
        if (!cardFromGameId(item.id)) throw ScenarioError("unknown card: " + item.id);
    } else if (kind == "potion") {
        item.kind = ScenarioKind::Potion;
        if (!potionFromGameId(rest)) throw ScenarioError("unknown potion: " + rest);
    } else if (kind == "front") {
        item.kind = ScenarioKind::PoolFront;
        const auto split = rest.find(':');
        if (split == std::string::npos) throw ScenarioError("front needs a tier and a relic: " + text);
        item.tier = rest.substr(0, split);
        item.id = rest.substr(split + 1);
        if (!relicFromGameId(item.id)) throw ScenarioError("unknown relic: " + item.id);
    } else if (kind == "monster" || kind == "elite") {
        item.kind = kind == "monster" ? ScenarioKind::Monster : ScenarioKind::Elite;
        if (!encounterFromGameId(rest)) throw ScenarioError("unknown encounter: " + rest);
    } else if (kind == "keys") {
        item.kind = ScenarioKind::Keys;
        if (rest != "all") throw ScenarioError("keys only takes all: " + text);
    } else {
        throw ScenarioError("unknown scenario kind: " + kind);
    }
    return item;
}

std::string commandLine(const ScenarioItem &item) {
    switch (item.kind) {
        case ScenarioKind::Relic: return "relic\t" + item.id;
        case ScenarioKind::Card: return "card\t" + item.id + "\t" + std::to_string(item.upgrades);
        case ScenarioKind::Potion: return "potion\t" + item.id;
        case ScenarioKind::PoolFront: return "front\t" + item.tier + "\t" + item.id;
        case ScenarioKind::Monster: return "monster\t" + item.id;
        case ScenarioKind::Elite: return "elite\t" + item.id;
        case ScenarioKind::Keys: return "keys";
    }
    return "";
}

// Writes commands for the mod and returns what follows "ok" in its answer.
std::string askMod(const std::string &commands) {
    const std::string folder = gameFolder();
    if (folder.empty()) throw ScenarioError("set STS_GAME_DIR or HOME so the bridge can find the game folder");
    const std::string in = folder + "/bridge_scenario_in.txt";
    const std::string out = folder + "/bridge_scenario_out.txt";
    std::remove(out.c_str());
    {
        std::ofstream tmp(in + ".tmp");
        tmp << commands;
        if (!tmp) throw ScenarioError("cannot write " + in + ".tmp");
    }
    if (std::rename((in + ".tmp").c_str(), in.c_str()) != 0) throw ScenarioError("cannot write " + in);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kModAnswerTimeoutSec);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream answer(out);
        std::string line;
        if (answer && std::getline(answer, line)) {
            std::remove(out.c_str());
            if (line.rfind("ok", 0) != 0) throw ScenarioError("scenario mod: " + line);
            return line.size() > 3 ? line.substr(3) : "";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::remove(in.c_str());
    throw ScenarioError("the scenario mod did not answer. Build it with scenario_mod/build.sh and restart the game");
}

}  // namespace

std::string gameFolder() {
    const char *gameDir = std::getenv("STS_GAME_DIR");
    if (gameDir) return gameDir;
    const char *home = std::getenv("HOME");
    return home ? std::string(home) + "/.sts_realgame" : "";
}

std::vector<ScenarioItem> parseScenario(const std::string &text) {
    std::vector<ScenarioItem> items;
    std::stringstream in(text);
    for (std::string part; std::getline(in, part, ';');) {
        if (!part.empty()) items.push_back(parseItem(part));
    }
    return items;
}

std::map<std::uint64_t, std::string> loadScenarioFile(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw ScenarioError("cannot read scenario file " + path);
    std::map<std::uint64_t, std::string> bySeed;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) throw ScenarioError("scenario line needs a seed and a tab: " + line);
        bySeed[std::stoull(line.substr(0, tab))] = line.substr(tab + 1);
    }
    return bySeed;
}

void applyScenarioToSimulator(GameContext &gc, const std::vector<ScenarioItem> &items) {
    for (const ScenarioItem &item : items) {
        switch (item.kind) {
            case ScenarioKind::Relic: {
                const RelicId relic = *relicFromGameId(item.id);
                removeFromPools(gc, relic);
                gc.obtainRelic(relic);
                break;
            }
            case ScenarioKind::Card: {
                Card card(*cardFromGameId(item.id));
                for (int i = 0; i < item.upgrades && card.canUpgrade(); ++i) card.upgrade();
                gc.deck.obtainRaw(card);
                break;
            }
            case ScenarioKind::Potion:
                gc.obtainPotion(*potionFromGameId(item.id));
                break;
            case ScenarioKind::PoolFront: {
                const RelicId relic = *relicFromGameId(item.id);
                auto *pool = relicPool(gc, item.tier);
                if (!pool) throw ScenarioError("unknown relic tier: " + item.tier);
                removeFromPools(gc, relic);
                pool->insert(pool->begin(), relic);
                break;
            }
            case ScenarioKind::Monster:
                gc.monsterList[gc.monsterListOffset] = *encounterFromGameId(item.id);
                break;
            case ScenarioKind::Elite:
                gc.eliteMonsterList[gc.eliteMonsterListOffset] = *encounterFromGameId(item.id);
                break;
            case ScenarioKind::Keys:
                gc.obtainKey(Key::EMERALD_KEY);
                gc.obtainKey(Key::RUBY_KEY);
                gc.obtainKey(Key::SAPPHIRE_KEY);
                break;
        }
    }
}

void applyScenarioToGame(const std::vector<ScenarioItem> &items) {
    std::string commands;
    for (const ScenarioItem &item : items) commands += commandLine(item) + "\n";
    askMod(commands);
}

std::map<std::string, RngState> readGameRngs() {
    std::map<std::string, RngState> states;
    if (!std::ifstream(gameFolder() + "/mods/BridgeScenario.jar")) return states;
    std::stringstream answer(askMod("rng\n"));
    for (std::string entry; std::getline(answer, entry, '\t');) {
        const auto equals = entry.find('=');
        const auto comma = entry.find(',');
        const auto comma2 = entry.rfind(',');
        if (equals == std::string::npos || comma == std::string::npos || comma2 == comma) {
            throw ScenarioError("scenario mod: bad answer to rng: " + entry);
        }
        states[entry.substr(0, equals)] = RngState{static_cast<std::uint64_t>(std::stoll(entry.substr(equals + 1, comma - equals - 1))),
                                                   static_cast<std::uint64_t>(std::stoll(entry.substr(comma + 1, comma2 - comma - 1))),
                                                   std::stoi(entry.substr(comma2 + 1))};
    }
    return states;
}

}  // namespace realgame
