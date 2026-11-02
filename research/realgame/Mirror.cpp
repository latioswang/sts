#include "Mirror.h"

#include "GameIds.h"
#include "StateConverter.h"

#include "game/Game.h"
#include "sim/search/GameAction.h"

#include <chrono>
#include <sstream>

using namespace sts;
using search::GameAction;

namespace realgame {

namespace {

constexpr int kMaxAlignSteps = 12;
constexpr double kRoomChangeTimeoutSec = 180.0;

const char *screenStateName(ScreenState s) {
    switch (s) {
        case ScreenState::EVENT_SCREEN: return "EVENT";
        case ScreenState::REWARDS: return "REWARDS";
        case ScreenState::BOSS_RELIC_REWARDS: return "BOSS_RELIC";
        case ScreenState::CARD_SELECT: return "CARD_SELECT";
        case ScreenState::MAP_SCREEN: return "MAP";
        case ScreenState::TREASURE_ROOM: return "TREASURE";
        case ScreenState::REST_ROOM: return "REST";
        case ScreenState::SHOP_ROOM: return "SHOP";
        case ScreenState::BATTLE: return "BATTLE";
        default: return "INVALID";
    }
}

bool hasCommand(const json &msg, const std::string &name) {
    for (const auto &c : msg["available_commands"]) {
        if (c == name) return true;
    }
    return false;
}

}  // namespace

Mirror::Mirror(CommClient &game, MirrorOptions options) : ActionSender(game), options_(std::move(options)) {}

void Mirror::logStep(const std::string &where, const std::string &action, const Diffs &diffs) {
    json entry{{"t", "step"}, {"step", step_}, {"where", where}, {"action", action}, {"floor", floor_}};
    if (!diffs.empty()) {
        json list = json::array();
        for (const auto &d : diffs) list.push_back({{"field", d.field}, {"sim", d.simulator}, {"game", d.game}, {"hard", d.hard}});
        entry["diffs"] = list;
    }
    log_ << entry.dump() << "\n";
    log_.flush();
}

void Mirror::stopOnDifference(const std::string &where, const Diffs &diffs) {
    if (anyHard(diffs)) throw StopRun{"diverged", where, "state differs", diffs};
}

// ---------------- out of combat ----------------

int Mirror::choiceCount() const {
    const json &gs = gameState();
    return gs.contains("choice_list") ? static_cast<int>(gs["choice_list"].size()) : 0;
}

// Neow's blessings are visible before the agent's first decision, so their rolls are compared here. Seeded runs should
// always get four. If the game ever offers two, the simulator is handed those two (repeated over the four indices)
// and the mirror carries on, since the difference says nothing about the seed.
void Mirror::prepareNeow(GameContext &gc) {
    for (int page = 0; page < 3 && screenType() == "EVENT"; ++page) {
        const json &options = gameState()["screen_state"]["options"];
        if (options.size() != 1) break;
        send("CHOOSE 0");
    }
    if (screenType() != "EVENT") return;
    const json &options = gameState()["screen_state"]["options"];
    Diffs d;
    if (options.size() == 2) {
        for (int i = 0; i < 4; ++i) gc.info.neowRewards[i] = neowOptionFromText(options[i % 2].value("text", ""));
        log_ << json{{"t", "note"}, {"note", "two-option Neow blessing: four-option rolls are not verified on this seed"}}.dump() << "\n";
    } else {
        for (int i = 0; i < 4 && i < static_cast<int>(options.size()); ++i) {
            const Neow::Option seen = neowOptionFromText(options[i].value("text", ""));
            const Neow::Option &sim = gc.info.neowRewards[i];
            if (seen.r != sim.r || seen.d != sim.d) {
                d.push_back({"neow[" + std::to_string(i) + "]",
                             std::string(Neow::bonusStrings[static_cast<int>(sim.r)]) + " / " + Neow::drawbackStrings[static_cast<int>(sim.d)],
                             options[i].value("text", ""), true});
            }
        }
    }
    logStep("NEOW", "", d);
    stopOnDifference("NEOW", d);
}

void Mirror::onCardRewardOpened(const CardReward &cards) {
    Diffs d;
    compareCardReward(cards, gameState()["screen_state"], d);
    logStep("CARD_REWARD", "", d);
    stopOnDifference("CARD_REWARD", d);
}

void Mirror::onGameAction(const GameContext &gc, const GameAction &a) {
    if (&gc != target_) return;  // the agent's what-if copies
    ++step_;
    if (options_.maxFloor > 0 && gc.floorNum > options_.maxFloor) {
        throw StopRun{"reached_limit", screenStateName(gc.screenState), "floor limit", {}};
    }
    alignOutOfCombat(gc);
    compareOutOfCombat(gc);
    applyGameAction(gc, a);
}

// Real-only steps the simulator has no action for: entering the merchant screen, clicking through a finished
// room, opening the boss chest. They consume no game randomness, so the mirror takes them on its own.
void Mirror::alignOutOfCombat(const GameContext &gc) {
    const ScreenState want = gc.screenState;
    for (int i = 0; i < kMaxAlignSteps; ++i) {
        const std::string real = screenType();
        switch (want) {
            case ScreenState::MAP_SCREEN:
                if (real == "MAP") return;
                if (real == "SHOP_SCREEN") { send("LEAVE"); continue; }
                if (real == "COMBAT_REWARD" || real == "CHEST" || real == "SHOP_ROOM" || real == "COMPLETE" || real == "REST") {
                    send("PROCEED");
                    continue;
                }
                if (real == "EVENT" && choiceCount() == 1) { send("CHOOSE 0"); continue; }
                break;
            case ScreenState::SHOP_ROOM:
                if (real == "SHOP_SCREEN") return;
                if (real == "SHOP_ROOM") { send("CHOOSE shop"); continue; }
                break;
            case ScreenState::REWARDS:
                if (real == "COMBAT_REWARD" || real == "CARD_REWARD") return;
                break;
            case ScreenState::BOSS_RELIC_REWARDS:
                if (real == "BOSS_REWARD") return;
                if (real == "COMBAT_REWARD") { send("PROCEED"); continue; }
                if (real == "CHEST") { send("CHOOSE open"); continue; }
                if (real == "MAP") { send("CHOOSE boss"); continue; }
                break;
            case ScreenState::TREASURE_ROOM:
                if (real == "CHEST") return;
                break;
            case ScreenState::REST_ROOM:
                if (real == "REST") return;
                break;
            case ScreenState::CARD_SELECT:
                if (real == "GRID" || real == "CARD_REWARD") return;
                break;
            case ScreenState::EVENT_SCREEN:
                if (real == "EVENT") return;
                if (real == "COMBAT_REWARD") { send("PROCEED"); continue; }
                break;
            default:
                break;
        }
        break;
    }
    Diffs d{{"screen", screenStateName(want), screenType(), true}};
    throw StopRun{"diverged", screenStateName(want), "the game is on a different screen", d};
}

void Mirror::compareOutOfCombat(const GameContext &gc) {
    Diffs d;
    const json &gs = gameState();
    compareRun(gc, gs, d);
    const json &screen = gs["screen_state"];
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN:
            rewardsCompared_ = false;
            if (mapComparedForAct_ != gc.act) {
                compareMap(gc, gs, d);
                mapComparedForAct_ = gc.act;
            }
            break;
        case ScreenState::REWARDS:
            // Claiming a reward changes the run fields compared above, so the list is only worth checking once.
            if (!rewardsCompared_ && screenType() == "COMBAT_REWARD") {
                compareRewards(gc, screen, d);
                rewardsCompared_ = true;
            }
            break;
        case ScreenState::SHOP_ROOM:
            compareShop(gc, screen, d);
            break;
        case ScreenState::BOSS_RELIC_REWARDS:
            compareBossRelics(gc, screen, d);
            break;
        case ScreenState::EVENT_SCREEN:
            compareEventId(gc, screen, d);
            break;
        case ScreenState::CARD_SELECT:
            if (screenType() == "GRID") compareSelectCards(gc, unselectedCards(screen), d);
            break;
        default:
            break;
    }
    logStep(screenStateName(gc.screenState), "", d);
    stopOnDifference(screenStateName(gc.screenState), d);
}

// ---------------- combat ----------------

void Mirror::onCombatAction(const BattleContext &bc, const search::Action &a) {
    ++step_;
    if (!state().value("in_game", false) || gameState().value("room_phase", "") != "COMBAT") {
        Diffs d{{"screen", "BATTLE", screenType(), true}};
        throw StopRun{"diverged", "BATTLE", "the simulator is in a battle, the game is not", d};
    }
    std::ostringstream desc;
    a.printDesc(desc, bc);
    Diffs d;
    compareBattle(bc, gameState(), d);
    logStep("BATTLE", desc.str(), d);
    lastAction_ = desc.str();
    stopOnDifference("BATTLE turn " + std::to_string(bc.turn), d);
    applyCombatAction(bc, a, gameIndexBySlot(bc));
}

// ---------------- run lifecycle ----------------

void Mirror::finishRun(const GameContext &gc) {
    ++step_;
    const bool won = gc.outcome == GameOutcome::PLAYER_VICTORY;
    for (int i = 0; i < kMaxAlignSteps && screenType() != "GAME_OVER"; ++i) {
        const std::string real = screenType();
        if (real == "COMBAT_REWARD" || real == "CHEST" || real == "COMPLETE") {
            send("PROCEED");
        } else if (real == "EVENT" && choiceCount() == 1) {
            send("CHOOSE 0");
        } else {
            break;
        }
    }
    Diffs d;
    if (screenType() != "GAME_OVER") {
        d.push_back({"run_end", won ? "victory" : "death", screenType(), true});
    } else {
        const bool gameWon = gameState()["screen_state"].value("victory", false);
        if (gameWon != won) d.push_back({"run_end", won ? "victory" : "death", gameWon ? "victory" : "death", true});
    }
    logStep("RUN_END", "", d);
    stopOnDifference("RUN_END", d);
}

MirrorResult Mirror::run() {
    MirrorResult result;
    result.seed = options_.seed;
    result.seedText = SeedHelper::getString(options_.seed);
    const auto t0 = std::chrono::steady_clock::now();
    log_.open(options_.logDir + "/mirror-" + result.seedText + ".jsonl", std::ios::out | std::ios::trunc);

    GameContext gc(CharacterClass::IRONCLAD, options_.seed, options_.ascension);
    // Same agent seed derivation as fair_eval, so a mirror run is the run the research harness would play.
    fair::FairAgent agent(options_.params, options_.seed * 0x9E3779B97F4A7C15ULL + 12345);
    target_ = &gc;
    const ObserverScope observing([this](const GameContext &g, const GameAction &a) { onGameAction(g, a); });
    agent.onCombatAction = [this](const BattleContext &bc, const search::Action &a, const fair::FairSearcher &) {
        onCombatAction(bc, a);
    };

    const int commandsBefore = game_.commandsSent();
    try {
        send("START IRONCLAD " + std::to_string(options_.ascension) + " " + result.seedText);
        prepareNeow(gc);
        agent.playout(gc);
        finishRun(gc);
        result.status = "completed";
    } catch (const StopRun &stop) {
        result.status = stop.status;
        result.where = stop.where;
        result.message = stop.message;
        result.diffs = stop.diffs;
    } catch (const CommandRejected &e) {
        result.status = "bridge_gap";
        result.message = e.what();
    }

    result.floor = floor_;
    result.act = act_;
    result.step = step_;
    result.commands = game_.commandsSent() - commandsBefore;
    result.lastAction = lastAction_;
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    json end{{"t", "end"}, {"status", result.status}, {"where", result.where}, {"message", result.message},
             {"floor", result.floor}, {"act", result.act}, {"step", result.step}, {"last_action", result.lastAction}};
    log_ << end.dump() << "\n";
    return result;
}

// ---------------- leaving a run ----------------

void returnToMainMenu(CommClient &game) {
    constexpr int kMaxSteps = 6000;
    for (int i = 0; i < kMaxSteps; ++i) {
        const json &msg = game.last();
        if (!msg.value("in_game", false)) return;
        const json &gs = msg["game_state"];
        const std::string screen = gs.value("screen_type", "NONE");
        try {
            if (hasCommand(msg, "end") && screen == "NONE") {
                game.command("END", kRoomChangeTimeoutSec);
            } else if (screen == "GAME_OVER") {
                game.command("PROCEED", kRoomChangeTimeoutSec);
            } else if (screen == "CARD_REWARD" && hasCommand(msg, "skip")) {
                game.command("SKIP", kRoomChangeTimeoutSec);
            } else if (screen == "SHOP_SCREEN") {
                game.command("LEAVE", kRoomChangeTimeoutSec);
            } else if (hasCommand(msg, "confirm")) {
                game.command("CONFIRM", kRoomChangeTimeoutSec);
            } else if (hasCommand(msg, "proceed")) {
                game.command("PROCEED", kRoomChangeTimeoutSec);
            } else if (hasCommand(msg, "choose")) {
                game.command("CHOOSE 0", kRoomChangeTimeoutSec);
            } else {
                game.command("WAIT 30", kRoomChangeTimeoutSec);
            }
        } catch (const CommandRejected &) {
            game.refresh();
            game.command("WAIT 30", kRoomChangeTimeoutSec);
        }
    }
    throw GameUnavailable("could not return to the main menu");
}

}  // namespace realgame
