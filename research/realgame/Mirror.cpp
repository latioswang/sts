#include "Mirror.h"

#include "GameIds.h"

#include "game/Game.h"
#include "sim/search/GameAction.h"

#include <chrono>
#include <sstream>

using namespace sts;
using search::GameAction;
using RT = GameAction::RewardsActionType;

namespace realgame {

namespace {

constexpr int kMaxAlignSteps = 12;
constexpr double kRoomChangeTimeoutSec = 180.0;

struct StopRun {
    std::string status;
    std::string where;
    std::string message;
    Diffs diffs;
};

[[noreturn]] void bridgeGap(const std::string &where, const std::string &message) {
    throw StopRun{"bridge_gap", where, message, {}};
}

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

std::string keyOf(const json &card) {
    const int up = card["upgrades"].get<int>();
    return card["id"].get<std::string>() + (up > 0 ? "+" + std::to_string(up) : "");
}

std::string keyOf(const Card &c) {
    const int up = c.getUpgraded();
    return cardToGameId(c.getId()) + (up > 0 ? "+" + std::to_string(up) : "");
}

std::string keyOf(const CardInstance &c) {
    const int up = c.id == CardId::SEARING_BLOW ? c.specialData : (c.upgraded ? 1 : 0);
    return cardToGameId(c.getId()) + (up > 0 ? "+" + std::to_string(up) : "");
}

int nthReward(const json &rewards, std::initializer_list<const char *> types, int n) {
    int seen = 0;
    for (int i = 0; i < static_cast<int>(rewards.size()); ++i) {
        const std::string t = rewards[i]["reward_type"];
        for (const char *wanted : types) {
            if (t == wanted) {
                if (seen == n) return i;
                ++seen;
                break;
            }
        }
    }
    return -1;
}

// The mod's shop choice list holds only affordable items, in this order: purge, cards, relics, potions.
int shopChoiceIndex(const json &gs, const std::string &kind, int kindIndex) {
    const json &shop = gs["screen_state"];
    const int gold = gs["gold"].get<int>();
    int idx = 0;
    if (shop["purge_available"].get<bool>() && gold >= shop["purge_cost"].get<int>()) {
        if (kind == "purge") return idx;
        ++idx;
    }
    for (const char *section : {"cards", "relics", "potions"}) {
        const json &items = shop[section];
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const bool affordable = items[i]["price"].get<int>() <= gold;
            if (kind == section && i == kindIndex) return affordable ? idx : -1;
            if (affordable) ++idx;
        }
    }
    return -1;
}

}  // namespace

Mirror::Mirror(CommClient &game, MirrorOptions options) : game_(game), options_(std::move(options)) {}

std::string Mirror::screenType() const {
    if (!state().value("in_game", false)) return "MENU";
    return gameState().value("screen_type", "NONE");
}

void Mirror::send(const std::string &command) {
    lastAction_ = command;
    log_ << json{{"t", "cmd"}, {"step", step_}, {"cmd", command}}.dump() << "\n";
    try {
        game_.command(command, kRoomChangeTimeoutSec);
    } catch (const CommandRejected &e) {
        throw StopRun{"bridge_gap", screenType(), std::string("mod rejected command: ") + e.what(), {}};
    }
    if (state().value("in_game", false)) {
        floor_ = gameState().value("floor", floor_);
        act_ = gameState().value("act", act_);
    }
}

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
                if (real == "EVENT" && gameState()["choice_list"].size() == 1) { send("CHOOSE 0"); continue; }
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
            if (screenType() == "GRID") compareSelectCards(gc, screen["cards"], d);
            break;
        default:
            break;
    }
    logStep(screenStateName(gc.screenState), "", d);
    stopOnDifference(screenStateName(gc.screenState), d);
}

void Mirror::applyGameAction(const GameContext &gc, const GameAction &a) {
    if (a.isPotionAction()) {
        send(std::string("POTION ") + (a.isPotionDiscard() ? "Discard " : "Use ") + std::to_string(a.getIdx1()));
    }
    switch (gc.screenState) {
        case ScreenState::EVENT_SCREEN:
            applyEvent(gc, a.getIdx1());
            break;
        case ScreenState::REWARDS:
            applyRewards(gc, a);
            break;
        case ScreenState::BOSS_RELIC_REWARDS:
            if (a.getIdx1() < 3) send("CHOOSE " + std::to_string(a.getIdx1()));
            else send("SKIP");
            break;
        case ScreenState::CARD_SELECT:
            applyCardSelect(gc, a.getIdx1());
            break;
        case ScreenState::MAP_SCREEN:
            applyMap(gc, a.getIdx1());
            break;
        case ScreenState::TREASURE_ROOM:
            if (a.getIdx1() == 0) send("CHOOSE open");
            else send("PROCEED");
            break;
        case ScreenState::REST_ROOM: {
            static const char *const kOptions[] = {"rest", "smith", "recall", "lift", "toke", "dig"};
            if (a.getIdx1() > 5) bridgeGap("REST", "simulator has no rest action, option 6");
            send(std::string("CHOOSE ") + kOptions[a.getIdx1()]);
            break;
        }
        case ScreenState::SHOP_ROOM:
            applyShop(gc, a);
            break;
        default:
            bridgeGap(screenStateName(gc.screenState), "no translation for this screen");
    }
}

// Simulator option i has rank r among the options valid right now; the game's choice_index counts enabled buttons
// the same way. Rank (not position) also covers events whose later phases use higher simulator indices.
void Mirror::applyEvent(const GameContext &gc, int option) {
    for (int page = 0; page < 4; ++page) {
        const json &gs = gameState();
        const json &options = gs["screen_state"]["options"];
        std::vector<std::string> labels;
        int enabled = 0;
        for (const auto &o : options) {
            labels.push_back(o.value("label", ""));
            if (!o.value("disabled", false)) ++enabled;
        }
        const int bits = gc.curEvent == Event::NEOW ? 0xF : GameAction::getValidEventSelectBits(gc);
        int validCount = 0;
        int rank = -1;
        for (int b = 0; b < 31; ++b) {
            if (!((bits >> b) & 1)) continue;
            if (b == option) rank = validCount;
            ++validCount;
        }
        log_ << json{{"t", "event"}, {"step", step_}, {"event", gs["screen_state"].value("event_id", "")},
                     {"sim_option", option}, {"sim_valid_bits", bits}, {"rank", rank}, {"labels", labels}, {"page", page}}.dump() << "\n";
        if (rank >= 0 && enabled == validCount) {
            send("CHOOSE " + std::to_string(rank));
            return;
        }
        if (enabled == 1) {  // an intro page the simulator skips ("[Talk]", "[Proceed]")
            send("CHOOSE 0");
            if (screenType() != "EVENT") break;
            continue;
        }
        break;
    }
    std::ostringstream os;
    os << "event option mapping: sim option " << option << " of event " << static_cast<int>(gc.curEvent)
       << ", game offers " << gameState()["screen_state"]["options"].dump();
    bridgeGap("EVENT", os.str());
}

void Mirror::applyRewards(const GameContext &gc, const GameAction &a) {
    const Rewards &r = gc.info.rewardsContainer;
    const auto type = a.getRewardsActionType();
    if (type == RT::SKIP) {
        if (screenType() == "CARD_REWARD") send("SKIP");
        else send("PROCEED");
        return;
    }
    if (type == RT::CARD) {
        if (screenType() == "COMBAT_REWARD") {
            const int entry = nthReward(gameState()["screen_state"]["rewards"], {"CARD"}, a.getIdx1());
            if (entry < 0) bridgeGap("REWARDS", "no card entry " + std::to_string(a.getIdx1()) + " on the reward screen");
            send("CHOOSE " + std::to_string(entry));
        }
        if (screenType() != "CARD_REWARD") bridgeGap("REWARDS", "card reward did not open: " + screenType());
        Diffs d;
        compareCardReward(r.cardRewards[a.getIdx1()], gameState()["screen_state"], d);
        logStep("CARD_REWARD", "", d);
        stopOnDifference("CARD_REWARD", d);
        if (a.getIdx2() == 5) {
            const bool bowl = gameState()["screen_state"].value("bowl_available", false);
            send(bowl ? "CHOOSE bowl" : "SKIP");
        } else {
            send("CHOOSE " + std::to_string(a.getIdx2()));
        }
        return;
    }
    const json &rewards = gameState()["screen_state"]["rewards"];
    int entry = -1;
    switch (type) {
        case RT::GOLD: entry = nthReward(rewards, {"GOLD", "STOLEN_GOLD"}, a.getIdx1()); break;
        case RT::RELIC: entry = nthReward(rewards, {"RELIC"}, a.getIdx1()); break;
        case RT::KEY: entry = nthReward(rewards, {"SAPPHIRE_KEY", "EMERALD_KEY"}, 0); break;
        case RT::POTION: {
            // A full belt makes the game keep the reward and report nothing; the simulator drops it. Same result.
            int empty = 0;
            for (const auto &p : gameState()["potions"]) empty += p["id"] == "Potion Slot" ? 1 : 0;
            if (empty == 0) return;
            entry = nthReward(rewards, {"POTION"}, a.getIdx1());
            break;
        }
        default: break;
    }
    if (entry < 0) bridgeGap("REWARDS", "reward entry not found on the screen: " + rewards.dump());
    send("CHOOSE " + std::to_string(entry));
}

void Mirror::applyShop(const GameContext &gc, const GameAction &a) {
    const json &gs = gameState();
    const auto type = a.getRewardsActionType();
    if (type == RT::SKIP) {
        send("LEAVE");
        return;
    }
    // Sold items leave the game's lists but keep their simulator slot, so a slot converts to a position by
    // counting the unsold slots before it.
    const Shop &s = gc.info.shop;
    const int slot = a.getIdx1();
    int choice = -1;
    if (type == RT::CARD_REMOVE) {
        choice = shopChoiceIndex(gs, "purge", 0);
    } else if (type == RT::CARD) {
        int position = 0;
        for (int i = 0; i < slot; ++i) position += s.cardPrice(i) != -1 ? 1 : 0;
        choice = shopChoiceIndex(gs, "cards", position);
    } else if (type == RT::RELIC) {
        int position = 0;
        for (int i = 0; i < slot; ++i) position += s.relicPrice(i) != -1 ? 1 : 0;
        choice = shopChoiceIndex(gs, "relics", position);
    } else if (type == RT::POTION) {
        int position = 0;
        for (int i = 0; i < slot; ++i) position += s.potionPrice(i) != -1 ? 1 : 0;
        choice = shopChoiceIndex(gs, "potions", position);
    }
    if (choice < 0) bridgeGap("SHOP", "item not offered by the game: " + gs["screen_state"].dump());
    send("CHOOSE " + std::to_string(choice));
}

void Mirror::chooseCardOnScreen(const std::string &key) {
    const std::string real = screenType();
    const char *listName = real == "HAND_SELECT" ? "hand" : "cards";
    const json &cards = gameState()["screen_state"][listName];
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        if (keyOf(cards[i]) == key) {
            send("CHOOSE " + std::to_string(i));
            return;
        }
    }
    bridgeGap(real, "card " + key + " not on screen: " + cards.dump());
}

void Mirror::confirmIfRequired() {
    if (screenType() == "GRID" || screenType() == "HAND_SELECT") {
        if (hasCommand(state(), "confirm")) send("CONFIRM");
    }
}

void Mirror::applyCardSelect(const GameContext &gc, int index) {
    const std::string key = keyOf(gc.info.toSelectCards[index].card);
    chooseCardOnScreen(key);
    confirmIfRequired();
}

void Mirror::applyMap(const GameContext &gc, int x) {
    const json &screen = gameState()["screen_state"];
    if (screen.value("boss_available", false)) {
        send("CHOOSE boss");
        return;
    }
    const json &nodes = screen["next_nodes"];
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[i]["x"].get<int>() == x) {
            send("CHOOSE " + std::to_string(i));
            return;
        }
    }
    (void)gc;
    Diffs d{{"map.next_node", "x=" + std::to_string(x), nodes.dump(), true}};
    throw StopRun{"diverged", "MAP", "the agent's node is not reachable in the game", d};
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
    applyCombatAction(bc, a);
}

void Mirror::applyCombatAction(const BattleContext &bc, const search::Action &a) {
    using search::ActionType;
    switch (a.getActionType()) {
        case ActionType::CARD: {
            const auto &card = bc.cards.hand[a.getSourceIdx()];
            std::string cmd = "PLAY " + std::to_string(a.getSourceIdx() + 1);
            if (card.requiresTarget()) cmd += " " + std::to_string(a.getTargetIdx());
            send(cmd);
            break;
        }
        case ActionType::POTION: {
            const int slot = a.getSourceIdx();
            if (a.getTargetIdx() > 5) {
                send("POTION Discard " + std::to_string(slot));
            } else if (potionRequiresTarget(bc.potions[slot])) {
                send("POTION Use " + std::to_string(slot) + " " + std::to_string(a.getTargetIdx()));
            } else {
                send("POTION Use " + std::to_string(slot));
            }
            break;
        }
        case ActionType::END_TURN:
            send("END");
            break;
        case ActionType::SINGLE_CARD_SELECT: {
            const int idx = a.getSelectIdx();
            switch (bc.cardSelectInfo.cardSelectTask) {
                case CardSelectTask::CODEX:
                    if (idx == 3) send("SKIP");
                    else send("CHOOSE " + std::to_string(idx));
                    break;
                case CardSelectTask::DISCOVERY:
                    send("CHOOSE " + std::to_string(idx));
                    break;
                case CardSelectTask::ARMAMENTS:
                case CardSelectTask::DUAL_WIELD:
                case CardSelectTask::EXHAUST_ONE:
                case CardSelectTask::FORETHOUGHT:
                case CardSelectTask::WARCRY:
                case CardSelectTask::RECYCLE:
                case CardSelectTask::SETUP:
                case CardSelectTask::NIGHTMARE:
                    chooseCardOnScreen(keyOf(bc.cards.hand[idx]));
                    confirmIfRequired();
                    break;
                case CardSelectTask::HEADBUTT:
                case CardSelectTask::HOLOGRAM:
                case CardSelectTask::LIQUID_MEMORIES_POTION:
                case CardSelectTask::MEDITATE:
                    chooseCardOnScreen(keyOf(bc.cards.discardPile[idx]));
                    confirmIfRequired();
                    break;
                case CardSelectTask::EXHUME:
                    chooseCardOnScreen(keyOf(bc.cards.exhaustPile[idx]));
                    confirmIfRequired();
                    break;
                case CardSelectTask::SECRET_TECHNIQUE:
                case CardSelectTask::SECRET_WEAPON:
                case CardSelectTask::SEEK:
                    chooseCardOnScreen(keyOf(bc.cards.drawPile[idx]));
                    confirmIfRequired();
                    break;
                default:
                    bridgeGap("BATTLE", "no translation for card select task " +
                                            std::to_string(static_cast<int>(bc.cardSelectInfo.cardSelectTask)));
            }
            break;
        }
        case ActionType::MULTI_CARD_SELECT: {
            for (const int idx : a.getSelectedIdxs()) chooseCardOnScreen(keyOf(bc.cards.hand[idx]));
            confirmIfRequired();
            break;
        }
        default:
            bridgeGap("BATTLE", "unknown combat action type");
    }
}

// ---------------- run lifecycle ----------------

void Mirror::finishRun(const GameContext &gc) {
    ++step_;
    const bool won = gc.outcome == GameOutcome::PLAYER_VICTORY;
    for (int i = 0; i < kMaxAlignSteps && screenType() != "GAME_OVER"; ++i) {
        const std::string real = screenType();
        if (real == "COMBAT_REWARD" || real == "CHEST" || real == "COMPLETE") {
            send("PROCEED");
        } else if (real == "EVENT" && gameState()["choice_list"].size() == 1) {
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
    search::gameActionObserver = [this](const GameContext &g, const GameAction &a) { onGameAction(g, a); };
    agent.onCombatAction = [this](const BattleContext &bc, const search::Action &a, const fair::FairSearcher &) {
        onCombatAction(bc, a);
    };

    const int commandsBefore = game_.commandsSent();
    try {
        send("START IRONCLAD " + std::to_string(options_.ascension) + " " + result.seedText);
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
    search::gameActionObserver = nullptr;

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
