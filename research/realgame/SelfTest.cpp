// Offline test of the state converter: play games in the simulator and, at every decision, send the state through
// JSON and the converter. The rebuilt state must offer the same actions and hold the same visible values.
// This tests the converter only. It cannot show that the simulator matches the real game; mirror mode does that.
#include "SelfTest.h"

#include "ActionSender.h"
#include "CommJson.h"
#include "Compare.h"
#include "FairAgent.h"
#include "GameIds.h"
#include "StateConverter.h"

#include "sim/search/GameAction.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>

using namespace sts;

namespace realgame {

namespace {

struct Counters {
    long battleChecks = 0;
    long battleActionMismatch = 0;
    long battleFieldMismatch = 0;
    long drawMultisetMismatch = 0;
    long leakMismatch = 0;
    long screenChecks = 0;
    long screenActionMismatch = 0;
    long screenHardMismatch = 0;
    std::map<std::string, long> internals;      // monster + field whose rebuilt hidden value differs
    std::map<std::string, long> fields;         // visible field that differs
    std::map<std::string, long> screenMisses;   // screen or event whose action set differs
    std::map<std::string, long> warnings;
    std::vector<std::string> examples;
};

// Actions by what they do: the same card on the same game monster, whichever simulator slot that monster is in.
std::vector<std::string> actionKeys(const BattleContext &bc, const std::array<int, 5> &gameIndex) {
    std::vector<std::string> out;
    for (const auto &a : fair::enumerateActions(bc)) {
        std::ostringstream os;
        const auto type = a.getActionType();
        if (type == search::ActionType::CARD) {
            const auto &card = bc.cards.hand[a.getSourceIdx()];
            os << "card:" << cardToGameId(card.id) << (card.upgraded ? "+" : "");
            if (card.requiresTarget()) os << ">" << gameIndex[a.getTargetIdx()];
        } else if (type == search::ActionType::POTION) {
            const Potion p = bc.potions[a.getSourceIdx()];
            os << "potion" << a.getSourceIdx();
            if (a.getTargetIdx() > 5) os << " discard";
            else if (potionRequiresTarget(p)) os << ">" << gameIndex[a.getTargetIdx()];
        } else if (type == search::ActionType::SINGLE_CARD_SELECT && (bc.cardSelectInfo.cardSelectTask == CardSelectTask::SECRET_TECHNIQUE ||
                   bc.cardSelectInfo.cardSelectTask == CardSelectTask::SECRET_WEAPON || bc.cardSelectInfo.cardSelectTask == CardSelectTask::SEEK)) {
            // Choosing from the draw pile: the converter shuffles it on purpose, so a card has no stable index.
            const auto &card = bc.cards.drawPile[a.getSelectIdx()];
            os << "select:" << cardToGameId(card.id) << (card.upgraded ? "+" : "");
        } else {
            os << "bits" << a.bits;
        }
        out.push_back(os.str());
    }
    // Identical cards are one choice; the search's own de-duplication decides how many it lists.
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::string joined(const std::vector<std::string> &items) {
    std::string out;
    for (const auto &item : items) out += item + " ";
    return out;
}

std::vector<std::uint32_t> sortedBits(const std::vector<search::GameAction> &actions) {
    std::vector<std::uint32_t> out;
    for (const auto &a : actions) out.push_back(a.bits);
    std::sort(out.begin(), out.end());
    return out;
}

void checkBattle(const GameContext &gc, const BattleContext &bc, BattleMemory &memory, Counters &c) {
    ++c.battleChecks;
    const json gs = gameStateFromSim(gc, &bc);
    memory.observe(gs);
    ConvertNotes notes;
    const RunMemory run;
    const GameContext rebuiltRun = gameContextFromState(gs, 777, run, notes);
    const ConvertedBattle rebuilt = battleFromState(rebuiltRun, gs, memory, 777, notes);
    for (const auto &w : notes.warnings) ++c.warnings[w];

    Diffs diffs;
    compareBattle(rebuilt.bc, gs, diffs, &rebuilt.gameMonsterIndex);
    bool fieldMismatch = false;
    for (const auto &d : diffs) {
        if (!d.hard || d.field == "draw_pile") continue;  // the draw pile is shuffled on purpose
        fieldMismatch = true;
        ++c.fields[d.field];
        if (c.examples.size() < 12) c.examples.push_back(d.field + ": rebuilt=" + d.simulator + " json=" + d.game);
    }
    if (fieldMismatch) ++c.battleFieldMismatch;

    auto multiset = [](const std::vector<CardInstance> &pile) {
        std::vector<std::pair<int, int>> out;
        for (const auto &card : pile) out.emplace_back(static_cast<int>(card.id), card.upgraded ? 1 : 0);
        std::sort(out.begin(), out.end());
        return out;
    };
    if (multiset(bc.cards.drawPile) != multiset(rebuilt.bc.cards.drawPile)) ++c.drawMultisetMismatch;

    const auto original = actionKeys(bc, gameIndexBySlot(bc));
    const auto converted = actionKeys(rebuilt.bc, rebuilt.gameMonsterIndex);
    if (original != converted) {
        ++c.battleActionMismatch;
        if (c.examples.size() < 12) {
            c.examples.push_back("actions differ, floor " + std::to_string(gc.floorNum) + " turn " + std::to_string(bc.turn) +
                                 ": sim " + joined(original) + "| rebuilt " + joined(converted));
        }
    }

    for (int i = 0; i < bc.monsters.monsterCount && i < rebuilt.bc.monsters.monsterCount; ++i) {
        const Monster &a = bc.monsters.arr[i];
        const Monster &b = rebuilt.bc.monsters.arr[i];
        if (a.id == MonsterId::INVALID || a.isDeadOrEscaped()) continue;
        const std::string name = monsterToGameId(a.id);
        if (a.id != b.id) {
            ++c.internals[name + ".slot"];
            continue;
        }
        if (a.miscInfo != b.miscInfo) ++c.internals[name + ".miscInfo"];
        if (a.moveHistory[0] != b.moveHistory[0]) ++c.internals[name + ".move=" + monsterMoveStrings[static_cast<int>(a.moveHistory[0])]];
        if (a.moveHistory[1] != b.moveHistory[1]) ++c.internals[name + ".lastMove=" + monsterMoveStrings[static_cast<int>(a.moveHistory[1])]];
    }

    // Leak rule: permuting the true draw-pile order in the JSON must not change the rebuilt pile.
    if (gs["combat_state"]["draw_pile"].size() > 1) {
        json permuted = gs;
        json &pile = permuted["combat_state"]["draw_pile"];
        std::reverse(pile.begin(), pile.end());
        ConvertNotes ignored;
        const ConvertedBattle other = battleFromState(rebuiltRun, permuted, memory, 777, ignored);
        auto order = [](const std::vector<CardInstance> &p) {
            std::vector<std::pair<int, int>> out;
            for (const auto &card : p) out.emplace_back(static_cast<int>(card.id), card.upgraded ? 1 : 0);
            return out;
        };
        if (order(other.bc.cards.drawPile) != order(rebuilt.bc.cards.drawPile)) ++c.leakMismatch;
    }
}

void checkScreen(const GameContext &gc, Counters &c) {
    ++c.screenChecks;
    const json gs = gameStateFromSim(gc, nullptr);
    ConvertNotes notes;
    RunMemory run;
    run.nodeX = gc.curMapNodeX;
    run.nodeY = gc.curMapNodeY;
    const GameContext rebuilt = gameContextFromState(gs, 777, run, notes);
    for (const auto &w : notes.warnings) ++c.warnings[w];

    if (gc.screenState == ScreenState::REWARDS) return;  // card lists are hidden until opened, by design
    const auto original = sortedBits(search::GameAction::getAllActionsInState(gc));
    const auto converted = sortedBits(search::GameAction::getAllActionsInState(rebuilt));
    // Sold shop items keep their simulator slot but leave the game's list, so only the count must match there.
    const bool same = gc.screenState == ScreenState::SHOP_ROOM ? original.size() == converted.size() : original == converted;
    // An event's later phases have no fixed page structure here: only the real game's pages can test them (mirror mode).
    if (!same && gc.screenState != ScreenState::EVENT_SCREEN) ++c.screenHardMismatch;
    if (!same) {
        ++c.screenActionMismatch;
        std::string where = gs["screen_type"].get<std::string>();
        if (gc.screenState == ScreenState::EVENT_SCREEN) where += " " + gs["screen_state"].value("event_id", "");
        ++c.screenMisses[where];
    }
}

}  // namespace

int runSelfTest(const fair::Params &params, std::uint64_t seedStart, int count, int ascension) {
    Counters c;
    for (int i = 0; i < count; ++i) {
        const std::uint64_t seed = seedStart + i;
        GameContext gc(CharacterClass::IRONCLAD, seed, ascension);
        fair::FairAgent agent(params, seed * 0x9E3779B97F4A7C15ULL + 12345);
        BattleMemory memory;
        const GameContext *target = &gc;

        const ObserverScope observing([&](const GameContext &g, const search::GameAction &) {
            if (&g == target) checkScreen(g, c);
        });
        agent.onCombatAction = [&](const BattleContext &bc, const search::Action &a, const fair::FairSearcher &) {
            checkBattle(gc, bc, memory, c);
            if (a.getActionType() == search::ActionType::CARD) memory.onCardPlayed(bc.cards.hand[a.getSourceIdx()].getType());
        };
        agent.playout(gc);
    }

    std::cout << "battle decisions checked: " << c.battleChecks << "\n"
              << "  legal action set differs: " << c.battleActionMismatch << "\n"
              << "  visible field differs:    " << c.battleFieldMismatch << "\n"
              << "  draw multiset differs:    " << c.drawMultisetMismatch << "\n"
              << "  depends on draw order:    " << c.leakMismatch << "\n"
              << "screens checked: " << c.screenChecks << ", action set differs: " << c.screenActionMismatch << "\n";
    auto printMap = [](const char *title, const std::map<std::string, long> &m) {
        if (m.empty()) return;
        std::cout << title << ":\n";
        for (const auto &[k, v] : m) std::cout << "  " << k << ": " << v << "\n";
    };
    printMap("visible fields", c.fields);
    printMap("hidden monster fields rebuilt differently", c.internals);
    printMap("screens with a different action set", c.screenMisses);
    printMap("converter warnings", c.warnings);
    if (!c.examples.empty()) std::cout << "examples:\n";
    for (const auto &e : c.examples) std::cout << "  " << e << "\n";

    const bool ok = c.battleActionMismatch == 0 && c.battleFieldMismatch == 0 && c.drawMultisetMismatch == 0 &&
                    c.leakMismatch == 0 && c.screenHardMismatch == 0;
    std::cout << (ok ? "SELFTEST PASS" : "SELFTEST FAIL") << "\n";
    return ok ? 0 : 1;
}

}  // namespace realgame
