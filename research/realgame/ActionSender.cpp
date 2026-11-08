#include "ActionSender.h"

#include "GameIds.h"
#include "Scenario.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

using namespace sts;
using search::GameAction;
using RT = GameAction::RewardsActionType;

namespace realgame {

namespace {

[[noreturn]] void bridgeGap(const std::string &where, const std::string &message) {
    throw StopRun{"bridge_gap", where, message, {}};
}

constexpr int kMaxAlignSteps = 12;
constexpr double kRoomChangeTimeoutSec = 180.0;
// ShowCardAndObtainEffect puts its card in the deck after 0.5 s in Fast Mode, which write_profile.py turns on (2 s
// without it). This adds a margin.
constexpr double kCardEffectSec = 0.6;
constexpr int kSettleWaitFrames = 30;
constexpr int kMaxSettleWaits = 12;  // 1.5 s at the game's 240 fps cap

bool isFiller(const std::string &command) {
    return command.rfind("WAIT", 0) == 0 || command == "STATE";
}

// Rooms with no ShowCardAndObtainEffect: card rewards use FastCardObtainEffect (0.01 s), and a fight's
// AddCardToDeckAction (Writhing Mass) lands turns before the fight can end. Every other room has one: events and Neow,
// Cursed Key at a chest, Astrolabe at the boss chest, Dolly's Mirror from the merchant.
bool roomWithoutCardEffects(const std::string &room) {
    return room == "MonsterRoom" || room == "MonsterRoomElite" || room == "MonsterRoomBoss" || room == "RestRoom";
}

// STS_TIME_SCALE speeds up the game's clock (scenario_mod). run_batch.sh starts the game and the bridge with the same value.
double timeScale() {
    const char *text = std::getenv("STS_TIME_SCALE");
    const double scale = text ? std::strtod(text, nullptr) : 1.0;
    return scale > 0.0 ? scale : 1.0;
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

// How many cards with the same name come before position idx.
template <typename Pile>
int copyIndex(const Pile &pile, int idx) {
    const std::string key = keyOf(pile[idx]);
    int copy = 0;
    for (int i = 0; i < idx; ++i) copy += keyOf(pile[i]) == key ? 1 : 0;
    return copy;
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

ActionSender::ActionSender(CommClient &game) : game_(game) {}

std::string ActionSender::screenType() const {
    if (!state().value("in_game", false)) return "MENU";
    return gameState().value("screen_type", "NONE");
}

// A card from an event or a relic reaches the deck through ShowCardAndObtainEffect, which Communication Mod doesn't wait
// for. Choosing the next room runs nextRoomTransition, which drops every pending effect and the card with it: on seed
// 5100, Cursed Key's curse from the floor 45 chest never arrived, since the next room came 0.19 s after the chest opened.
// The answer to the room's last command came after the effect started, so the effect has had its time once that long
// has passed since.
void ActionSender::settleBeforeLeavingRoom() {
    const std::string room = gameState().value("room_type", "");
    if (roomWithoutCardEffects(room)) return;
    const double needed = kCardEffectSec / timeScale();
    const auto secondsSinceRoomCommand = [this] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - roomCommandAnswered_).count();
    };
    int waits = 0;
    for (; waits < kMaxSettleWaits && secondsSinceRoomCommand() < needed; ++waits) {
        game_.command("WAIT " + std::to_string(kSettleWaitFrames), kRoomChangeTimeoutSec);
    }
    if (waits > 0) log_ << json{{"t", "settle"}, {"step", step_}, {"room", room}, {"waits", waits}}.dump() << "\n";
}

void ActionSender::send(const std::string &command) {
    const bool onMap = state().is_object() && screenType() == "MAP";  // nothing has been read before the run's first command
    if (onMap && command.rfind("CHOOSE", 0) == 0) settleBeforeLeavingRoom();
    lastAction_ = command;
    log_ << json{{"t", "cmd"}, {"step", step_}, {"cmd", command}}.dump() << "\n";
    try {
        game_.command(command, kRoomChangeTimeoutSec);
    } catch (const CommandRejected &e) {
        throw StopRun{"bridge_gap", screenType(), std::string("mod rejected command: ") + e.what(), {}};
    }
    if (!onMap && !isFiller(command)) roomCommandAnswered_ = std::chrono::steady_clock::now();
    if (state().value("in_game", false)) {
        floor_ = gameState().value("floor", floor_);
        act_ = gameState().value("act", act_);
    }
}

void ActionSender::applyGameAction(const GameContext &gc, const GameAction &a) {
    if (a.isPotionAction()) {
        send(std::string("POTION ") + (a.isPotionDiscard() ? "Discard " : "Use ") + std::to_string(a.getIdx1()));
    }
    switch (gc.screenState) {
        case ScreenState::EVENT_SCREEN:
            if (gc.curEvent == Event::MATCH_AND_KEEP) applyMatchAndKeep(gc, a.getIdx1(), a.getIdx2());
            else applyEvent(gc, a.getIdx1());
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
            constexpr int kLeave = 6;  // with no option left, such as Coffee Dripper and Fusion Hammer together
            if (a.getIdx1() == kLeave) {
                send("PROCEED");
                break;
            }
            if (a.getIdx1() > kLeave) bridgeGap("REST", "simulator has no rest action, option " + std::to_string(a.getIdx1()));
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
void ActionSender::applyEvent(const GameContext &gc, int option) {
    for (int page = 0; page < 4; ++page) {
        if (screenType() != "EVENT") break;  // an intro page led somewhere else
        const json &gs = gameState();
        const json &options = gs["screen_state"]["options"];
        std::vector<std::string> labels;
        int enabled = 0;
        for (const auto &o : options) {
            labels.push_back(o.value("label", ""));
            if (!o.value("disabled", false)) ++enabled;
        }
        const bool neow = gc.curEvent == Event::NEOW;
        const int bits = neow ? 0xF : GameAction::getValidEventSelectBits(gc);
        int validCount = 0;
        int rank = -1;
        for (int b = 0; b < 31; ++b) {
            if (!((bits >> b) & 1)) continue;
            if (b == option) rank = validCount;
            ++validCount;
        }
        // The simulator lists Vampires' Blood Vial first, and the game second.
        if (gc.curEvent == Event::VAMPIRES && validCount == 3 && rank >= 0 && rank < 2) rank = 1 - rank;
        log_ << json{{"t", "event"}, {"step", step_}, {"event", gs["screen_state"].value("event_id", "")},
                     {"sim_option", option}, {"sim_valid_bits", bits}, {"rank", rank}, {"labels", labels}, {"page", page}}.dump() << "\n";
        if (rank >= 0 && enabled == validCount) {
            send("CHOOSE " + std::to_string(rank));
            return;
        }
        if (enabled == 1) {  // an intro page the simulator skips ("[Talk]", "[Proceed]")
            send("CHOOSE 0");
            continue;
        }
        if (neow && enabled == 2) {  // the two-option blessing: both modes repeat its options over the four indices
            send("CHOOSE " + std::to_string(option % 2));
            return;
        }
        break;
    }
    std::ostringstream os;
    os << "event option mapping: sim option " << option << " of event " << static_cast<int>(gc.curEvent) << ", game is on "
       << screenType();
    if (screenType() == "EVENT") os << " offering " << gameState()["screen_state"]["options"].dump();
    bridgeGap("EVENT", os.str());
}

// The mod lists the face-down cards in grid order, and a card leaves the list while it is face up. The simulator's
// positions use the same grid (x + 4y), so a position converts to a list index by counting the face-down cards before it.
void ActionSender::chooseMatchCard(const std::vector<int> &faceDown, int position) {
    constexpr int kFlipWaits = 40;
    constexpr int kFlipWaitFrames = 5;
    for (int i = 0; i < kFlipWaits && gameState()["choice_list"].size() != faceDown.size(); ++i) {
        game_.command("WAIT " + std::to_string(kFlipWaitFrames), kRoomChangeTimeoutSec);
    }
    if (gameState()["choice_list"].size() != faceDown.size()) {
        bridgeGap("EVENT", "Match and Keep: expected " + std::to_string(faceDown.size()) + " cards, game lists " + gameState()["choice_list"].dump());
    }
    const auto it = std::find(faceDown.begin(), faceDown.end(), position);
    send("CHOOSE " + std::to_string(it - faceDown.begin()));
}

void ActionSender::applyMatchAndKeep(const GameContext &gc, int first, int second) {
    // The intro and rules pages have one button each. The board lists its cards as options, and always has at least four.
    for (int page = 0; page < 4 && gameState()["screen_state"]["options"].size() == 1; ++page) send("CHOOSE 0");
    std::vector<int> faceDown;
    const auto &cards = gc.info.toSelectCards;
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        if (cards[i].deckIdx != 0) faceDown.push_back(i);
    }
    chooseMatchCard(faceDown, first);
    faceDown.erase(std::find(faceDown.begin(), faceDown.end(), first));
    chooseMatchCard(faceDown, second);
}

void ActionSender::applyRewards(const GameContext &gc, const GameAction &a) {
    const Rewards &r = gc.info.rewardsContainer;
    const auto type = a.getRewardsActionType();
    if (type == RT::SKIP) {
        if (screenType() == "CARD_REWARD") send("SKIP");
        else if (screenType() != "MAP") send("PROCEED");  // the game is already past a reward screen it never showed
        return;
    }
    if (type == RT::CARD) {
        if (screenType() == "COMBAT_REWARD") {
            const int entry = nthReward(gameState()["screen_state"]["rewards"], {"CARD"}, a.getIdx1());
            if (entry < 0) bridgeGap("REWARDS", "no card entry " + std::to_string(a.getIdx1()) + " on the reward screen");
            send("CHOOSE " + std::to_string(entry));
        }
        if (screenType() != "CARD_REWARD") bridgeGap("REWARDS", "card reward did not open: " + screenType());
        onCardRewardOpened(r.cardRewards[a.getIdx1()]);
        if (a.getIdx2() == 5) {
            const bool bowl = gameState()["screen_state"].value("bowl_available", false);
            send(bowl ? "CHOOSE bowl" : "SKIP");
            if (!bowl) onCardRewardDeclined();
        } else {
            send("CHOOSE " + std::to_string(a.getIdx2()));
        }
        return;
    }
    const json &rewards = gameState()["screen_state"]["rewards"];
    int entry = -1;
    switch (type) {
        case RT::GOLD: {
            // The game lists stolen gold first, the simulator last, so the amount identifies the entry.
            const int amount = r.gold[a.getIdx1()];
            for (int i = 0; i < static_cast<int>(rewards.size()) && entry < 0; ++i) {
                const std::string t = rewards[i]["reward_type"];
                if ((t == "GOLD" || t == "STOLEN_GOLD") && rewards[i].value("gold", -1) == amount) entry = i;
            }
            break;
        }
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

void ActionSender::applyShop(const GameContext &gc, const GameAction &a) {
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

// Copies of a card play alike but sit at different places in their pile, and a selection can move the card it picks,
// so `copy` says which of the same-named cards to choose.
void ActionSender::chooseCardOnScreen(const std::string &key, int copy) {
    const std::string real = screenType();
    const char *listName = real == "HAND_SELECT" ? "hand" : "cards";
    const json &screen = gameState()["screen_state"];
    if (!screen.contains(listName)) bridgeGap(real, "expected a card selection screen, got: " + screen.dump());
    const json &cards = screen[listName];
    // A grid keeps already-selected cards in its list, and clicking one again would deselect it.
    std::vector<std::string> selected;
    if (screen.contains("selected_cards")) {
        for (const auto &c : screen["selected_cards"]) selected.push_back(c.value("uuid", ""));
    }
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        if (keyOf(cards[i]) != key) continue;
        if (std::find(selected.begin(), selected.end(), cards[i].value("uuid", "")) != selected.end()) continue;
        if (copy-- > 0) continue;
        send("CHOOSE " + std::to_string(i));
        return;
    }
    bridgeGap(real, "card " + key + " not on screen: " + cards.dump());
}

// Secret Technique and Secret Weapon list the draw pile in a random order, so same-named copies are only told apart by
// uuid. Where the simulator's pile isn't in the game's order the copies are interchangeable to the agent anyway.
void ActionSender::chooseDrawPileCardOnScreen(const BattleContext &bc, int drawIdx) {
    if (!drawPileMatchesGame()) {
        chooseCardOnScreen(keyOf(bc.cards.drawPile[drawIdx]), copyIndex(bc.cards.drawPile, drawIdx));
        return;
    }
    // Seek+ takes two cards and moves them when its grid closes, so the game's pile still holds the first pick.
    std::vector<std::string> selected;
    const json &screen = gameState()["screen_state"];
    if (screen.contains("selected_cards")) {
        for (const auto &c : screen["selected_cards"]) selected.push_back(c.value("uuid", ""));
    }
    std::string uuid;
    int position = 0;
    for (const auto &c : gameState()["combat_state"]["draw_pile"]) {
        const std::string candidate = c.value("uuid", "");
        if (std::find(selected.begin(), selected.end(), candidate) != selected.end()) continue;
        if (position++ == drawIdx) {
            uuid = candidate;
            break;
        }
    }
    if (!chooseCardByUuid(uuid)) {
        bridgeGap(screenType(), "draw pile card " + uuid + " not on screen: " + gameState()["screen_state"].dump());
    }
}

bool ActionSender::chooseCardByUuid(const std::string &uuid) {
    const json &screen = gameState()["screen_state"];
    if (uuid.empty() || !screen.contains("cards")) return false;
    const json &cards = screen["cards"];
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        if (cards[i].value("uuid", "") == uuid) {
            send("CHOOSE " + std::to_string(i));
            return true;
        }
    }
    return false;
}

void ActionSender::pressConfirm() {
    if (screenType() == "GRID" || screenType() == "HAND_SELECT") {
        if (hasCommand(state(), "confirm")) send("CONFIRM");
    }
}

void ActionSender::confirmIfRequired() {
    // A grid with nothing picked isn't the one the pick was for: that one closed, and the card's action went on to open
    // another. Meditate ends the turn, the draw shuffles, and Melange opens a scry. The scry waits for its own choice.
    if (screenType() == "GRID" && gameState()["screen_state"].value("selected_cards", json::array()).empty()) return;
    pressConfirm();
}

void ActionSender::applyCardSelect(const GameContext &gc, int index) {
    // Copies in the deck shuffle differently, so picking the right one matters to a bottle or a transform.
    const int deckIdx = gc.info.toSelectCards[index].deckIdx;
    const json &deck = gameState()["deck"];
    if (deckIdx >= 0 && deckIdx < static_cast<int>(deck.size()) && chooseCardByUuid(deck[deckIdx].value("uuid", ""))) {
        confirmIfRequired();
        return;
    }
    const std::string key = keyOf(gc.info.toSelectCards[index].card);
    int copy = 0;
    for (int i = 0; i < index; ++i) copy += keyOf(gc.info.toSelectCards[i].card) == key ? 1 : 0;
    chooseCardOnScreen(key, copy);
    confirmIfRequired();
}

void ActionSender::applyMap(const GameContext &gc, int x) {
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
    throw StopRun{"diverged", "MAP", "the agent's node is not reachable in the game",
                  {{"map.next_node", "x=" + std::to_string(x), nodes.dump(), true}}};
}

void ActionSender::applyCombatAction(const BattleContext &bc, const search::Action &a, const std::array<int, 5> &monsterIndex) {
    using search::ActionType;
    switch (a.getActionType()) {
        case ActionType::CARD: {
            const auto &card = bc.cards.hand[a.getSourceIdx()];
            std::string cmd = "PLAY " + std::to_string(a.getSourceIdx() + 1);
            if (card.requiresTarget()) {
                cmd += " " + std::to_string(monsterIndex[a.getTargetIdx()]);
                if (bc.player.hasStatus<PS::SURROUNDED>()) faceGameMonster(monsterIndex[a.getTargetIdx()]);
            }
            send(cmd);
            break;
        }
        case ActionType::POTION: {
            const int slot = a.getSourceIdx();
            if (a.getTargetIdx() > 5) {
                send("POTION Discard " + std::to_string(slot));
            } else if (potionRequiresTarget(bc.potions[slot])) {
                if (bc.player.hasStatus<PS::SURROUNDED>()) faceGameMonster(monsterIndex[a.getTargetIdx()]);
                send("POTION Use " + std::to_string(slot) + " " + std::to_string(monsterIndex[a.getTargetIdx()]));
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
                case CardSelectTask::FOREIGN_INFLUENCE:
                case CardSelectTask::WISH:
                case CardSelectTask::CHOOSE_STANCE:
                    send("CHOOSE " + std::to_string(idx));
                    break;
                case CardSelectTask::SCRY: {
                    // The screen lists the top of the pile, top card first, and any number can be picked, so its button is
                    // always up. It's pressed when the simulator is done with the screen.
                    if (idx != CardSelectInfo::SCRY_DONE_IDX) {
                        const auto &pile = bc.cards.drawPile;
                        const int size = static_cast<int>(pile.size());
                        const std::string key = keyOf(pile[size - 1 - idx]);
                        int copy = 0;
                        for (int above = 0; above < idx; ++above) {
                            if (keyOf(pile[size - 1 - above]) == key) ++copy;
                        }
                        chooseCardOnScreen(key, copy);
                    }
                    if (idx == CardSelectInfo::SCRY_DONE_IDX || idx + 1 >= bc.cardSelectInfo.pickCount) {
                        pressConfirm();  // this grid can be confirmed with nothing picked
                    }
                    break;
                }
                case CardSelectTask::OMNISCIENCE:
                    chooseDrawPileCardOnScreen(bc, idx);
                    // The one-card grid can close on the pick, and the chosen card may open a hand select at once, as
                    // Purity does. A confirm then would answer that screen with nothing picked.
                    if (screenType() == "GRID") confirmIfRequired();
                    break;
                case CardSelectTask::ARMAMENTS:
                case CardSelectTask::DUAL_WIELD:
                case CardSelectTask::EXHAUST_ONE:
                case CardSelectTask::FORETHOUGHT:
                case CardSelectTask::WARCRY:
                case CardSelectTask::RECYCLE:
                case CardSelectTask::SETUP:
                case CardSelectTask::NIGHTMARE:
                    chooseCardOnScreen(keyOf(bc.cards.hand[idx]), copyIndex(bc.cards.hand, idx));
                    confirmIfRequired();
                    break;
                case CardSelectTask::DISCARD:
                    // The screen's confirm button only comes up with the last pick.
                    chooseCardOnScreen(keyOf(bc.cards.hand[idx]), copyIndex(bc.cards.hand, idx));
                    confirmIfRequired();
                    break;
                case CardSelectTask::RETAIN: {
                    // This screen allows any number up to its limit, so its button is always up. The simulator keeps the
                    // picks at the end of the hand, where they don't change which copy of a card this one is.
                    const int unpicked = bc.cards.cardsInHand - bc.cardSelectInfo.data0;
                    if (idx != CardSelectInfo::RETAIN_DONE_IDX) {
                        chooseCardOnScreen(keyOf(bc.cards.hand[idx]), copyIndex(bc.cards.hand, idx));
                    }
                    if (idx == CardSelectInfo::RETAIN_DONE_IDX || bc.cardSelectInfo.pickCount <= 1 || unpicked <= 1) {
                        confirmIfRequired();
                    }
                    break;
                }
                case CardSelectTask::HEADBUTT:
                case CardSelectTask::HOLOGRAM:
                case CardSelectTask::LIQUID_MEMORIES_POTION:
                case CardSelectTask::MEDITATE:
                    chooseCardOnScreen(keyOf(bc.cards.discardPile[idx]), copyIndex(bc.cards.discardPile, idx));
                    confirmIfRequired();
                    break;
                case CardSelectTask::EXHUME:
                    chooseCardOnScreen(keyOf(bc.cards.exhaustPile[idx]), copyIndex(bc.cards.exhaustPile, idx));
                    confirmIfRequired();
                    break;
                case CardSelectTask::SECRET_TECHNIQUE:
                case CardSelectTask::SECRET_WEAPON:
                case CardSelectTask::SEEK:
                    chooseDrawPileCardOnScreen(bc, idx);
                    confirmIfRequired();
                    break;
                default:
                    bridgeGap("BATTLE", "no translation for card select task " +
                                            std::to_string(static_cast<int>(bc.cardSelectInfo.cardSelectTask)));
            }
            break;
        }
        case ActionType::MULTI_CARD_SELECT: {
            for (const int idx : a.getSelectedIdxs()) chooseCardOnScreen(keyOf(bc.cards.hand[idx]), copyIndex(bc.cards.hand, idx));
            confirmIfRequired();
            break;
        }
        default:
            bridgeGap("BATTLE", "unknown combat action type");
    }
}


}  // namespace realgame
