#include "Live.h"

#include "game/Game.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

using namespace sts;
using search::GameAction;

namespace realgame {

namespace {

constexpr int kMaxSteps = 40000;
constexpr int kWaitFrames = 30;
constexpr int kMaxRepeatedStates = 25;  // this many identical states in a row means nothing is progressing

// Thrown by the action observer to stop the agent at its first decision: the state is a throwaway copy.
struct CapturedAction {
    GameAction action;
};

bool hasCommand(const json &msg, const std::string &name) {
    for (const auto &c : msg["available_commands"]) {
        if (c == name) return true;
    }
    return false;
}

int enabledOptions(const json &screen) {
    int enabled = 0;
    for (const auto &o : screen["options"]) enabled += o.value("disabled", false) ? 0 : 1;
    return enabled;
}

json monsterSummary(const json &monsters) {
    json out = json::array();
    for (const auto &m : monsters) {
        out.push_back({{"id", m.value("id", "")}, {"hp", m.value("current_hp", 0)}, {"block", m.value("block", 0)},
                       {"intent", m.value("intent", "")}, {"damage", m.value("move_adjusted_damage", 0)},
                       {"hits", m.value("move_hits", 0)}, {"gone", m.value("is_gone", false)}});
    }
    return out;
}

json cardSummary(const json &cards) {
    json out = json::array();
    for (const auto &c : cards) {
        const int up = c.value("upgrades", 0);
        out.push_back(c.value("id", "") + (up > 0 ? "+" : "") + "(" + std::to_string(c.value("cost", 0)) + ")");
    }
    return out;
}

}  // namespace

Live::Live(CommClient &game, LiveOptions options)
    : ActionSender(game),
      options_(std::move(options)),
      agent_(options_.params, options_.seed * 0x9E3779B97F4A7C15ULL + 12345),
      rng_(options_.seed * 0xA24BAED4963EE407ULL + 67890) {}

// The agent's battle knowledge of the draw pile: cards it put on top stay known until drawn; any other change to the
// pile (a shuffle, a random insert) means the order is unknown again. Same rule as FairAgent::playBattle.
void Live::updateKnownTop(int drawPileSize) {
    if (lastDrawPileSize_ >= 0) {
        if (drawPileSize < lastDrawPileSize_) knownTop_ = std::max(0, knownTop_ - (lastDrawPileSize_ - drawPileSize));
        else if (drawPileSize == lastDrawPileSize_ + 1 && putOnTopPending_) knownTop_ += 1;
        else if (drawPileSize != lastDrawPileSize_) knownTop_ = 0;
    }
    knownTop_ = std::min(knownTop_, drawPileSize);
    lastDrawPileSize_ = drawPileSize;
    putOnTopPending_ = false;
}

void Live::decideBattle() {
    const json &gs = gameState();
    ConvertNotes notes;
    const int floor = gs.value("floor", 0);
    if (floor != battleMemory_.floor) {
        knownTop_ = 0;
        lastDrawPileSize_ = -1;
    }
    battleMemory_.observe(gs);
    updateKnownTop(static_cast<int>(gs["combat_state"]["draw_pile"].size()));
    battleMemory_.knownTop = knownTop_;

    // The run's own seed is never read: RNG streams in the rebuilt state come from the bridge's generator.
    const GameContext gc = gameContextFromState(gs, rng_(), runMemory_, notes);
    const ConvertedBattle converted = battleFromState(gc, gs, battleMemory_, rng_(), notes);
    const BattleContext &bc = converted.bc;

    fair::SearchParams params = agent_.searchParamsFor(bc);
    params.knownTop = knownTop_;
    const auto t0 = std::chrono::steady_clock::now();
    fair::FairSearcher searcher(bc, params, rng_());
    searcher.setRoot(bc);
    const search::Action action = searcher.search();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::vector<fair::FairSearcher::EdgeStat> edges;
    double rootMean = 0.0;
    searcher.rootEdgeStats(edges, rootMean);
    json edgeLog = json::array();
    for (const auto &e : edges) {
        std::ostringstream desc;
        e.action.printDesc(desc, bc);
        edgeLog.push_back({{"action", desc.str()}, {"visits", e.n}, {"q", e.q}});
    }
    std::ostringstream chosen;
    action.printDesc(chosen, bc);

    const json &cs = gs["combat_state"];
    const json &player = cs["player"];
    json entry{{"t", "battle"}, {"decision", decisions_}, {"floor", floor}, {"turn", cs.value("turn", 0)},
               {"screen", gs.value("screen_type", "")}, {"hp", player.value("current_hp", 0)}, {"block", player.value("block", 0)},
               {"energy", player.value("energy", 0)}, {"hand", cardSummary(cs["hand"])}, {"draw_count", cs["draw_pile"].size()},
               {"known_top", knownTop_}, {"monsters", monsterSummary(cs["monsters"])}, {"action", chosen.str()},
               {"simulations", searcher.simulationsRun}, {"root_visits", searcher.rootVisits()}, {"root_mean", rootMean},
               {"game_actions", searcher.actionsExecuted}, {"search_ms", ms}, {"edges", edgeLog}, {"warnings", notes.warnings}};
    log_ << entry.dump() << "\n";
    log_.flush();
    ++decisions_;

    if (action.getActionType() == search::ActionType::CARD) {
        battleMemory_.onCardPlayed(bc.cards.hand[action.getSourceIdx()].getType());
    }
    // The game's JSON has no stance. The simulator's next state does, apart from anything a random draw would change.
    BattleContext after = bc;
    action.execute(after);
    battleMemory_.stance = after.player.stance;
    battleMemory_.mantraGained = after.player.mantraGained;
    battleMemory_.devaEnergy = after.player.devaFormEnergyPerTurn;
    if (action.getActionType() == search::ActionType::SINGLE_CARD_SELECT && bc.inputState == InputState::CARD_SELECT) {
        const auto task = bc.cardSelectInfo.cardSelectTask;
        putOnTopPending_ = task == CardSelectTask::HEADBUTT || task == CardSelectTask::WARCRY;
    }
    try {
        applyCombatAction(bc, action, converted.gameMonsterIndex);
    } catch (const StopRun &gap) {
        fallback(gap.message);
    }
}

// Screens with one sensible command, where the simulator has nothing to decide.
bool Live::advanceWithoutDecision() {
    const json &gs = gameState();
    const std::string screen = screenType();
    const int floor = gs.value("floor", 0);
    const json &ss = gs["screen_state"];

    if (screen == "COMPLETE") {
        send("PROCEED");
        return true;
    }
    if (screen == "MAP" && ss.value("boss_available", false)) {
        send("CHOOSE boss");
        return true;
    }
    if (screen == "CHEST") {
        const bool closed = !ss.value("chest_open", true);
        if (!closed) {
            send("PROCEED");
            return true;
        }
        if (ss.value("chest_type", "") == "BossChest") {
            send("CHOOSE open");
            return true;
        }
        return false;
    }
    if (screen == "REST" && !hasCommand(state(), "choose")) {
        send("PROCEED");
        return true;
    }
    if (screen == "SHOP_ROOM") {
        if (visitedShopFloor_ != floor) {
            visitedShopFloor_ = floor;
            send("CHOOSE shop");
        } else {
            send("PROCEED");
        }
        return true;
    }
    if (screen == "EVENT" && enabledOptions(ss) <= 1 && hasCommand(state(), "choose")) {
        send("CHOOSE 0");
        return true;
    }
    if (screen == "GRID" && ss.value("confirm_up", false) && hasCommand(state(), "confirm")) {
        send("CONFIRM");
        return true;
    }
    if (screen == "COMBAT_REWARD") {
        if (rewardFloor_ != floor) {
            rewardFloor_ = floor;
            declinedCardRewards_ = 0;
        }
        bool potionSlotFree = false;
        for (const auto &p : gs["potions"]) potionSlotFree = potionSlotFree || p.value("id", "") == "Potion Slot";
        const bool wantKeys = fair::getp(options_.params, "keys", 0) > 0;
        bool otherRewards = false;
        std::vector<int> cardEntries;
        const json &rewards = ss["rewards"];
        for (int i = 0; i < static_cast<int>(rewards.size()); ++i) {
            const std::string type = rewards[i].value("reward_type", "");
            if (type == "CARD") cardEntries.push_back(i);
            else if (type == "POTION") otherRewards = otherRewards || potionSlotFree;
            else if (type == "EMERALD_KEY" || type == "SAPPHIRE_KEY") otherRewards = otherRewards || wantKeys;
            else otherRewards = true;
        }
        if (otherRewards) return false;  // the agent decides the order of these
        if (static_cast<int>(cardEntries.size()) > declinedCardRewards_) {
            // Declined rewards stay on the list ahead of the next one to open.
            send("CHOOSE " + std::to_string(cardEntries[declinedCardRewards_]));
            return true;
        }
        send("PROCEED");
        return true;
    }
    return false;
}

void Live::decideOutOfCombat() {
    const json &gs = gameState();
    const int floor = gs.value("floor", 0);
    if (runMemory_.eventFloor != floor) {
        runMemory_.eventFloor = floor;
        runMemory_.eventChoices = 0;
    }
    ConvertNotes notes;
    GameContext gc = gameContextFromState(gs, rng_(), runMemory_, notes);
    if (gc.screenState == ScreenState::INVALID) {
        fallback("no simulator screen for " + screenType());
        return;
    }
    // Rewards the game keeps on screen but cannot grant: a full belt leaves potions there for good.
    auto &rewards = gc.info.rewardsContainer;
    if (gc.screenState == ScreenState::REWARDS && gc.potionCount >= gc.potionCapacity) {
        while (rewards.potionCount > 0) rewards.removePotionReward(0);
    }
    // A closed card reward has no cards to judge; advanceWithoutDecision opens each once nothing else is left.
    const bool rewardList = screenType() == "COMBAT_REWARD";
    if (rewardList) {
        while (rewards.cardRewardCount > 0) rewards.removeCardReward(0);
    }
    if (search::GameAction::getAllActionsInState(gc).empty()) {
        fallback("the simulator has no actions for this screen");
        return;
    }

    const GameContext *target = &gc;
    GameAction action;
    bool captured = false;
    const auto t0 = std::chrono::steady_clock::now();
    try {
        const ObserverScope observing([target](const GameContext &g, const GameAction &a) {
            if (&g == target) throw CapturedAction{a};
        });
        agent_.stepOutOfCombat(gc);
    } catch (const CapturedAction &c) {
        action = c.action;
        captured = true;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    json entry{{"t", "choice"}, {"decision", decisions_}, {"floor", floor}, {"screen", screenType()},
               {"hp", gs.value("current_hp", 0)}, {"gold", gs.value("gold", 0)}, {"bits", action.bits}, {"idx1", action.getIdx1()},
               {"idx2", action.getIdx2()}, {"rewards_type", static_cast<int>(action.getRewardsActionType())},
               {"choices", gs.value("choice_list", json::array())}, {"screen_state", gs["screen_state"]},
               {"search_ms", ms}, {"warnings", notes.warnings}};
    log_ << entry.dump() << "\n";
    log_.flush();
    ++decisions_;

    if (!captured) {
        fallback("the agent took no action");
        return;
    }
    if (gc.screenState == ScreenState::MAP_SCREEN) {
        runMemory_.nodeX = action.getIdx1();
        runMemory_.nodeY = gc.curMapNodeY + 1;
    }
    if (gc.screenState == ScreenState::EVENT_SCREEN) ++runMemory_.eventChoices;
    try {
        applyGameAction(gc, action);
    } catch (const StopRun &gap) {
        fallback(gap.message);
    }
}

// Keeps the run moving when the agent cannot decide. Each use is logged so the case can be fixed afterwards.
void Live::fallback(const std::string &reason) {
    ++fallbacks_;
    log_ << json{{"t", "fallback"}, {"floor", floor_}, {"screen", screenType()}, {"reason", reason}}.dump() << "\n";
    log_.flush();
    const json &msg = state();
    const std::string screen = screenType();
    if (screen == "EVENT" && hasCommand(msg, "choose") && pickMatchGameCard()) return;
    if (screen == "CARD_REWARD" && hasCommand(msg, "skip")) send("SKIP");
    else if (hasCommand(msg, "confirm")) send("CONFIRM");
    else if (hasCommand(msg, "choose")) send("CHOOSE 0");
    else if (hasCommand(msg, "proceed")) send("PROCEED");
    else if (hasCommand(msg, "end")) send("END");
    else send("WAIT " + std::to_string(kWaitFrames));
}

// The pause point: the map, with nothing sent yet for the next node.
bool Live::shouldPause() const {
    if (screenType() != "MAP") return false;
    const int floor = gameState().value("floor", 0);
    if (floor == resumeFloor_) return false;
    if (options_.pauseFloor > 0 && floor >= options_.pauseFloor) return true;
    return options_.pauseBeforeBoss && gameState()["screen_state"].value("boss_available", false);
}

// Takes over the run in the game. The agent and the bridge keep no state between decisions that a fresh start on the
// map would get wrong, apart from the map node, which the map screen shows. The game's seed is read only to make sure
// the log being appended to is this run's; it never reaches the agent.
void Live::beginResumedRun() {
    if (!state().value("in_game", false)) throw StopRun{"resume", screenType(), "no run in progress to resume", {}};
    const json &gs = gameState();
    const auto gameSeed = static_cast<std::uint64_t>(gs.value("seed", static_cast<std::int64_t>(0)));
    if (gameSeed != options_.seed) {
        throw StopRun{"resume", screenType(), "the run in the game has seed " + SeedHelper::getString(gameSeed) + ", not " +
                                                  SeedHelper::getString(options_.seed), {}};
    }
    floor_ = gs.value("floor", 0);
    act_ = gs.value("act", 1);
    const bool onMap = screenType() == "MAP";
    // Only the map screen it resumes on is skipped: resumed elsewhere, this floor's map is still a pause point.
    if (onMap) resumeFloor_ = floor_;
    const json &ss = gs["screen_state"];
    if (onMap && ss.value("first_node_chosen", false) && ss.contains("current_node")) {
        runMemory_.nodeX = ss["current_node"]["x"].get<int>();
        runMemory_.nodeY = ss["current_node"]["y"].get<int>();
    }
    log_ << json{{"t", "resume"}, {"floor", floor_}, {"act", act_}, {"screen", screenType()}, {"hp", gs.value("current_hp", 0)},
                 {"gold", gs.value("gold", 0)}, {"on_map", onMap}}.dump() << "\n";
    log_.flush();
}

// Match and Keep!'s card grid, which the simulator has no actions for. See MatchGame.h for the rule.
bool Live::pickMatchGameCard() {
    const json &gs = gameState();
    const json &ss = gs["screen_state"];
    if (ss.value("event_id", "") != "Match and Keep!" || ss["options"].size() < 2) return false;
    const int floor = gs.value("floor", 0);
    if (floor != matchGameFloor_) {
        matchGameFloor_ = floor;
        matchGame_ = MatchGameMemory{};
    }
    std::vector<std::string> labels;
    for (const auto &o : ss["options"]) {
        std::string label = o.value("label", "");
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return std::tolower(c); });
        labels.push_back(label);
    }
    send("CHOOSE " + std::to_string(matchGamePick(labels, matchGame_)));
    return true;
}

// One command's worth of progress. Returns false when the run is over.
bool Live::step() {
    if (!state().value("in_game", false)) return false;
    const json &gs = gameState();
    const std::string screen = screenType();
    if (screen == "GAME_OVER") {
        // Unlock screens that follow also count as GAME_OVER, without the result on them.
        const std::string name = gs.value("screen_name", "");
        if (name == "DEATH" || name == "VICTORY") {
            victory_ = gs["screen_state"].value("victory", false);
            score_ = gs["screen_state"].value("score", 0);
        }
        send("PROCEED");
        return state().value("in_game", false);
    }
    if (gs.value("room_phase", "") == "COMBAT" && (screen == "NONE" || screen == "HAND_SELECT" || screen == "GRID" || screen == "CARD_REWARD")) {
        decideBattle();
        return true;
    }
    if (shouldPause()) {
        paused_ = true;
        log_ << json{{"t", "pause"}, {"floor", floor_}, {"act", act_}, {"hp", gs.value("current_hp", 0)},
                     {"max_hp", gs.value("max_hp", 0)}, {"gold", gs.value("gold", 0)},
                     {"boss", gs.value("act_boss", "")}}.dump() << "\n";
        log_.flush();
        return false;
    }
    if (advanceWithoutDecision()) return true;
    if (screen == "NONE") {
        send("WAIT " + std::to_string(kWaitFrames));
        return true;
    }
    decideOutOfCombat();
    return true;
}

LiveResult Live::run() {
    LiveResult result;
    result.seed = options_.seed;
    result.seedText = SeedHelper::getString(options_.seed);
    const auto t0 = std::chrono::steady_clock::now();
    log_.open(options_.logDir + "/live-" + result.seedText + ".jsonl",
              options_.resume ? std::ios::out | std::ios::app : std::ios::out | std::ios::trunc);

    result.status = "stalled";
    try {
        if (options_.resume) beginResumedRun();
        else send("START IRONCLAD " + std::to_string(options_.ascension) + " " + result.seedText);
        std::string lastState;
        int repeats = 0;
        for (int i = 0; i < kMaxSteps; ++i) {
            ++step_;
            const std::string current = state().dump();
            repeats = current == lastState ? repeats + 1 : 0;
            lastState = current;
            if (repeats >= kMaxRepeatedStates) {
                log_ << json{{"t", "stalled"}, {"screen", screenType()}, {"state", state()}}.dump() << "\n";
                break;
            }
            bool running = true;
            try {
                running = step();
            } catch (const StopRun &gap) {
                // A refused command changes nothing; re-read the game and let the next step try something else.
                log_ << json{{"t", "rejected"}, {"floor", floor_}, {"screen", screenType()}, {"message", gap.message}}.dump() << "\n";
                log_.flush();
                ++fallbacks_;
                game_.refresh();
                if (repeats > 0) fallback(gap.message);
            }
            if (!running) {
                result.status = paused_ ? "paused" : "finished";
                break;
            }
        }
    } catch (const StopRun &stop) {
        result.message = stop.message;
    } catch (const GameUnavailable &e) {
        result.message = e.what();
    }
    result.victory = victory_;
    result.score = score_;
    result.floor = floor_;
    result.act = act_;
    result.decisions = decisions_;
    result.fallbacks = fallbacks_;
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    log_ << json{{"t", "end"}, {"status", result.status}, {"victory", result.victory}, {"score", result.score},
                 {"floor", result.floor}, {"act", result.act}, {"decisions", result.decisions}, {"fallbacks", result.fallbacks},
                 {"message", result.message}}.dump() << "\n";
    return result;
}

}  // namespace realgame
