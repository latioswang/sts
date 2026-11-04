#include "Mirror.h"

#include "GameIds.h"
#include "Scenario.h"
#include "StateConverter.h"

#include "game/Game.h"
#include "sim/search/GameAction.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace sts;
using search::GameAction;

namespace realgame {

namespace {

constexpr int kMaxAlignSteps = 12;
constexpr double kRoomChangeTimeoutSec = 180.0;
constexpr int kEffectWaits = 8;  // a transformed card reaches the deck after about two seconds at normal speed
constexpr int kEffectWaitFrames = 30;

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

// A grid keeps selected cards in its list, and choosing one again deselects it.
int firstUnselected(const json &grid) {
    const json &cards = grid["cards"];
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        bool selected = false;
        for (const auto &s : grid["selected_cards"]) selected = selected || s["uuid"] == cards[i]["uuid"];
        if (!selected) return i;
    }
    return 0;
}

// Note For Yourself gives the card stored by the profile's previous run (STSPlayer: NOTE_CARD, NOTE_UPGRADE). The
// simulator starts every run with Iron Wave, the game's default, so it is told what this run will really find.
void loadNoteCard(GameContext &gc) {
    const std::string dir = gameFolder();
    if (dir.empty()) return;
    std::ifstream in(dir + "/preferences/STSPlayer");
    if (!in) return;
    const json prefs = json::parse(in, nullptr, false);
    if (prefs.is_discarded() || !prefs.contains("NOTE_CARD")) return;
    const auto card = cardFromGameId(prefs["NOTE_CARD"].get<std::string>());
    if (!card) return;
    gc.noteForYourselfCard = Card(*card, std::atoi(prefs.value("NOTE_UPGRADE", "0").c_str()));
}

bool rewardsEmpty(const Rewards &r) {
    return r.goldRewardCount == 0 && r.cardRewardCount == 0 && r.relicCount == 0 && r.potionCount == 0 && !r.emeraldKey && !r.sapphireKey;
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

// DiscoveryAction.update regenerates its three cards on every frame it runs (about 0.25s after the choice), and each
// generation rolls cardRandomRng. So after a discovery, that stream depends on the frame rate until the next floor
// reseeds it, and the same seed and actions can play out differently in the game itself. No simulator can match that.
void Mirror::stopOnDifference(const std::string &where, const Diffs &diffs) {
    if (!anyHard(diffs)) return;
    if (discoveryFloor_ == floor_) {
        throw StopRun{"game_nondeterminism", where, "difference after a discovery on this floor; cardRandomRng depends on the frame rate there", diffs};
    }
    throw StopRun{"diverged", where, "state differs", diffs};
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
    stopOnUnsupportedCard(gc);
    alignOutOfCombat(gc);
    adoptCourierCard();
    compareOutOfCombat(gc);
    noteCurseTransform(gc, a);
    if (gc.screenState == ScreenState::SHOP_ROOM && gc.hasRelic(RelicId::THE_COURIER) && a.getRewardsActionType() == GameAction::RewardsActionType::CARD) {
        courierCardSlot_ = a.getIdx1() < 5 ? a.getIdx1() : -1;
    }
    applyGameAction(gc, a);
}

// Prismatic Shard offers every class's cards, and the simulator can't play the Silent's or the Watcher's yet.
void Mirror::stopOnUnsupportedCard(const GameContext &gc) const {
    for (const auto &card : gc.deck.cards) {
        const CardColor color = getCardColor(card.getId());
        if (color == CardColor::GREEN || color == CardColor::PURPLE) {
            throw StopRun{"unsupported_card", screenStateName(gc.screenState),
                          std::string("the simulator can't play ") + card.getName(), {}};
        }
    }
}

// While a transform's confirm screen is up, GridCardSelectScreen changes its preview every 0.1 seconds, and a curse's
// preview rolls cardRng (CardLibrary.getCurse). The rolls depend on how long the screen stays up, so no simulator can
// match them, and the simulator takes the game's state instead.
void Mirror::noteCurseTransform(const GameContext &gc, const GameAction &a) {
    if (gc.screenState != ScreenState::CARD_SELECT) return;
    const auto type = gc.info.selectScreenType;
    if (type != CardSelectScreenType::TRANSFORM && type != CardSelectScreenType::TRANSFORM_UPGRADE) return;
    if (getCardColor(gc.info.toSelectCards[a.getIdx1()].card.getId()) == CardColor::CURSE) copyCardRng_ = true;
}

// The Courier's new class card is drawn with the game's unseeded MathUtils, so no simulator can predict it. Its type,
// rarity and price are seeded and still checked: the simulator takes the game's card only if the type and rarity agree,
// and the price is compared with everything else in the shop.
void Mirror::adoptCourierCard() {
    if (courierCardSlot_ < 0) return;
    const int slot = courierCardSlot_;
    courierCardSlot_ = -1;
    if (screenType() != "SHOP_SCREEN") return;
    Shop &shop = target_->info.shop;
    int position = 0;
    for (int i = 0; i < slot; ++i) position += shop.cardPrice(i) != -1 ? 1 : 0;
    const json &cards = gameState()["screen_state"]["cards"];
    if (position >= static_cast<int>(cards.size())) return;
    const json &real = cards[position];
    const auto id = cardFromGameId(real["id"].get<std::string>());
    if (!id) return;
    const Card card(*id, real.value("upgrades", 0));
    if (card.getType() != shop.cards[slot].getType() || card.getRarity() != shop.cards[slot].getRarity()) return;
    log_ << json{{"t", "note"}, {"note", "courier card: game " + real["id"].get<std::string>() + ", simulator " + cardToGameId(shop.cards[slot].id)}}.dump() << "\n";
    shop.cards[slot] = card;
}

// Real-only steps the simulator has no action for: entering the merchant screen, clicking through a finished
// room, opening the boss chest. They consume no game randomness, so the mirror takes them on its own.
void Mirror::alignOutOfCombat(const GameContext &gc) {
    const ScreenState want = gc.screenState;
    for (int i = 0; i < kMaxAlignSteps; ++i) {
        const std::string real = screenType();
        // The simulator ends a battle as soon as a leader dies. The game first finishes the card that killed it, which can
        // still open a selection (Headbutt on seed 1003). Nothing picked there outlasts the fight.
        if (gameState().value("room_phase", "") == "COMBAT" && (real == "GRID" || real == "HAND_SELECT")) {
            send(hasCommand(state(), "confirm") ? "CONFIRM" : "CHOOSE 0");
            continue;
        }
        // The fight ends at once in the simulator, but the game waits for the player to run off after a Smoke Bomb.
        if (gameState().value("room_phase", "") == "COMBAT" && real == "NONE") {
            send("WAIT 120");  // about two seconds at the game's normal speed
            continue;
        }
        // A confirmation grid shows what a relic gave (Pandora's Box, Calling Bell). The simulator has no such screen.
        if (want != ScreenState::CARD_SELECT && real == "GRID" && hasCommand(state(), "confirm")) {
            send("CONFIRM");
            continue;
        }
        switch (want) {
            case ScreenState::MAP_SCREEN:
                if (real == "MAP") return;
                if (real == "SHOP_SCREEN") { send("LEAVE"); continue; }
                if (real == "COMBAT_REWARD" || real == "CHEST" || real == "SHOP_ROOM" || real == "COMPLETE" || real == "REST") {
                    send("PROCEED");
                    continue;
                }
                if (real == "EVENT" && choiceCount() == 1) { send("CHOOSE 0"); continue; }
                // Match and Keep's board stays up for a moment after the last attempt.
                if (real == "EVENT" && gc.curEvent == Event::MATCH_AND_KEEP) { send("WAIT 30"); continue; }
                break;
            case ScreenState::SHOP_ROOM:
                if (real == "SHOP_SCREEN") return;
                if (real == "SHOP_ROOM") { send("CHOOSE shop"); continue; }
                break;
            case ScreenState::REWARDS:
                if (real == "COMBAT_REWARD" || real == "CARD_REWARD") return;
                // Calling Bell shows its curse on a confirm-only grid first.
                // Single-button event pages the simulator skips: Lab's [Search], Wheel of Change's spin and [Prize!], and
                // Neow's [Leave], where the simulator leaves through an empty reward screen.
                if (real == "EVENT" && choiceCount() == 1) { send("CHOOSE 0"); continue; }
                // The same empty screen after Dream Catcher's card, where the game returns to the finished campfire.
                if ((real == "MAP" || real == "REST") && rewardsEmpty(gc.info.rewardsContainer)) return;
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
                // Events reach their card screen through pages the simulator skips ([Continue], [Offer]).
                if (real == "EVENT" && choiceCount() == 1) { send("CHOOSE 0"); continue; }
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

// A card or relic from an event reaches the deck through an effect that outlives the action queue, so the mod can
// report a state from before it lands. A real difference survives the wait.
void Mirror::waitForEffects() {
    log_ << json{{"t", "wait"}, {"step", step_}}.dump() << "\n";
    game_.command("WAIT " + std::to_string(kEffectWaitFrames), kRoomChangeTimeoutSec);
}

void Mirror::compareOutOfCombat(const GameContext &gc) {
    Diffs d = outOfCombatDiffs(gc);
    for (int i = 0; i < kEffectWaits && anyHard(d); ++i) {
        waitForEffects();
        d = outOfCombatDiffs(gc);
    }
    gameRngs_ = readGameRngs();
    if (copyCardRng_) {
        copyCardRng_ = false;
        const auto it = gameRngs_.find("cardRng");
        if (it != gameRngs_.end()) {
            target_->cardRng.seed0 = it->second.seed0;
            target_->cardRng.seed1 = it->second.seed1;
            target_->cardRng.counter = it->second.counter;
        }
    }
    for (const auto &[name, stream] : {std::pair<const char *, const Random *>{"cardRng", &gc.cardRng}, {"treasureRng", &gc.treasureRng},
                                       {"relicRng", &gc.relicRng}, {"potionRng", &gc.potionRng}, {"merchantRng", &gc.merchantRng},
                                       {"eventRng", &gc.eventRng}, {"monsterRng", &gc.monsterRng}, {"miscRng", &gc.miscRng}}) {
        // miscRng is reseeded on entering every room, and the game rolls it once while setting up an act.
        if (gc.screenState == ScreenState::MAP_SCREEN && stream == &gc.miscRng) continue;
        addRngDiff(d, name, *stream);
    }
    if (gc.screenState == ScreenState::MAP_SCREEN) {
        rewardsCompared_ = false;
        mapComparedForAct_ = gc.act;
    } else if (gc.screenState == ScreenState::REWARDS && screenType() == "COMBAT_REWARD") {
        rewardsCompared_ = true;
    }
    logStep(screenStateName(gc.screenState), "", d);
    stopOnDifference(screenStateName(gc.screenState), d);
}

Diffs Mirror::outOfCombatDiffs(const GameContext &gc) const {
    Diffs d;
    const json &gs = gameState();
    compareRun(gc, gs, d);
    // The game charges for a shop removal once the card is chosen, the simulator when the removal starts.
    if (gc.screenState == ScreenState::CARD_SELECT && gs.value("room_type", "") == "ShopRoom") {
        d.erase(std::remove_if(d.begin(), d.end(), [](const Diff &x) { return x.field == "gold"; }), d.end());
    }
    const json &screen = gs["screen_state"];
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN:
            if (mapComparedForAct_ != gc.act) compareMap(gc, gs, d);
            break;
        case ScreenState::REWARDS:
            // Claiming a reward changes the run fields compared above, so the list is only worth checking once.
            if (!rewardsCompared_ && screenType() == "COMBAT_REWARD") compareRewards(gc, screen, d);
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
    return d;
}

// ---------------- combat ----------------

// While a card's selection screen is up, the game still holds the rest of that card's effects and anything they set
// off in its action queue (a Guardian's mode change behind a Headbutt, say), and the simulator may already have run
// them. The next comparison sees both sides settled, so here only the piles the selection reads from are compared.
Diffs Mirror::battleDiffs(const BattleContext &bc) const {
    Diffs d;
    compareBattle(bc, gameState(), d);
    if (screenType() != "NONE") {
        d.erase(std::remove_if(d.begin(), d.end(), [](const Diff &x) {
            return x.field.rfind("monster", 0) == 0 || x.field.rfind("player", 0) == 0;
        }), d.end());
    }
    return d;
}

// The game's discovery action rolls cardRandomRng once per frame while its screen opens. With the scenario mod the
// simulator can take the game's state afterwards and the run goes on. Without it, a difference on this floor stops the
// run as game_nondeterminism. The agent's battle context isn't const; only the callback's view of it is.
void Mirror::copyGameCardRandomRng(const BattleContext &bc) {
    gameRngs_ = readGameRngs();
    if (!copyCardRandomRng_) return;
    copyCardRandomRng_ = false;
    const auto it = gameRngs_.find("cardRandomRng");
    if (it == gameRngs_.end()) return;
    auto &live = const_cast<BattleContext &>(bc);
    live.cardRandomRng.seed0 = it->second.seed0;
    live.cardRandomRng.seed1 = it->second.seed1;
    live.cardRandomRng.counter = it->second.counter;
    discoveryFloor_ = -1;
}

// A stream that has moved apart shows up here at once, before it changes anything else the game shows.
void Mirror::addRngDiff(Diffs &out, const std::string &name, const Random &sim) const {
    const auto it = gameRngs_.find(name);
    if (it == gameRngs_.end() || (it->second.seed0 == sim.seed0 && it->second.seed1 == sim.seed1)) return;
    out.push_back({"rng." + name, "rolls " + std::to_string(sim.counter), "rolls " + std::to_string(it->second.counter), true});
}

void Mirror::onCombatAction(const BattleContext &bc, const search::Action &a) {
    ++step_;
    if (options_.maxFloor > 0 && bc.floorNum > options_.maxFloor) {
        throw StopRun{"reached_limit", "BATTLE", "floor limit", {}};
    }
    // An event's fight starts behind a [Fight] button the simulator doesn't have.
    for (int i = 0; i < kMaxAlignSteps && screenType() == "EVENT" && choiceCount() == 1; ++i) send("CHOOSE 0");
    // So does A20's second boss, behind a Proceed button.
    if (screenType() == "COMPLETE") send("PROCEED");
    // Nilry's Codex's screen can close with the next turn still being dealt, which shows at normal speed.
    for (int i = 0; i < kMaxAlignSteps && screenType() == "NONE" && !hasCommand(state(), "play") && !hasCommand(state(), "end"); ++i) send("WAIT 30");
    if (!state().value("in_game", false) || gameState().value("room_phase", "") != "COMBAT") {
        Diffs d{{"screen", "BATTLE", screenType(), true}};
        throw StopRun{"diverged", "BATTLE", "the simulator is in a battle, the game is not", d};
    }
    const bool afterDiscovery = copyCardRandomRng_;
    copyGameCardRandomRng(bc);
    std::ostringstream desc;
    a.printDesc(desc, bc);
    Diffs d = battleDiffs(bc);
    for (int i = 0; i < kEffectWaits && anyHard(d); ++i) {
        waitForEffects();
        d = battleDiffs(bc);
    }
    for (const auto &[name, stream] : {std::pair<const char *, const Random *>{"aiRng", &bc.aiRng}, {"monsterHpRng", &bc.monsterHpRng},
                                       {"shuffleRng", &bc.shuffleRng}, {"cardRandomRng", &bc.cardRandomRng},
                                       {"miscRng", &bc.miscRng}, {"potionRng", &bc.potionRng}}) {
        addRngDiff(d, name, *stream);
    }
    logStep("BATTLE", desc.str(), d);
    lastAction_ = desc.str();
    if (a.getActionType() == search::ActionType::SINGLE_CARD_SELECT && bc.cardSelectInfo.cardSelectTask == CardSelectTask::DISCOVERY &&
        bc.cardSelectInfo.discoveryIsFree) {
        discoveryFloor_ = floor_;
        copyCardRandomRng_ = true;
    }
    // Something queued behind a discovery, such as Hex's Dazed, rolled the stream before the simulator could take it.
    if (afterDiscovery && anyHard(d)) {
        throw StopRun{"game_nondeterminism", "BATTLE turn " + std::to_string(bc.turn), "difference right after a discovery; cardRandomRng depends on the frame rate", d};
    }
    stopOnDifference("BATTLE turn " + std::to_string(bc.turn), d);
    applyCombatAction(bc, a, gameIndexBySlot(bc, gameState()["combat_state"]["monsters"]));
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
    loadNoteCard(gc);
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
        if (!options_.scenario.empty()) {
            const auto items = parseScenario(options_.scenario);
            applyScenarioToSimulator(gc, items);
            applyScenarioToGame(items);
            log_ << json{{"t", "note"}, {"note", "scenario: " + options_.scenario}}.dump() << "\n";
            game_.refresh();
        }
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
    } catch (const ScenarioError &e) {
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
            } else if (screen == "GRID") {
                game.command("CHOOSE " + std::to_string(firstUnselected(gs["screen_state"])), kRoomChangeTimeoutSec);
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
