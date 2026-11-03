#include "StateConverter.h"

#include "Compare.h"
#include "GameIds.h"

#include <algorithm>
#include <cctype>
#include <regex>

using namespace sts;

namespace realgame {

namespace {

// ---------------- runtime dispatch over the simulator's compile-time status setters ----------------

void setMonsterStatus(Monster &m, MonsterStatus s, int amount) {
    switch (s) {
#define MS_CASE(NAME) case MS::NAME: m.setHasStatus<MS::NAME>(true); m.setStatus<MS::NAME>(amount); break;
        MS_CASE(ARTIFACT) MS_CASE(BLOCK_RETURN) MS_CASE(CHOKED) MS_CASE(CORPSE_EXPLOSION) MS_CASE(LOCK_ON) MS_CASE(MARK)
        MS_CASE(METALLICIZE) MS_CASE(PLATED_ARMOR) MS_CASE(POISON) MS_CASE(REGEN) MS_CASE(SHACKLED) MS_CASE(VULNERABLE)
        MS_CASE(WEAK) MS_CASE(ANGRY) MS_CASE(BEAT_OF_DEATH) MS_CASE(CURIOSITY) MS_CASE(CURL_UP) MS_CASE(ENRAGE) MS_CASE(FADING)
        MS_CASE(FLIGHT) MS_CASE(GENERIC_STRENGTH_UP) MS_CASE(INTANGIBLE) MS_CASE(MALLEABLE) MS_CASE(MODE_SHIFT) MS_CASE(RITUAL)
        MS_CASE(SLOW) MS_CASE(SPORE_CLOUD) MS_CASE(THIEVERY) MS_CASE(THORNS) MS_CASE(TIME_WARP) MS_CASE(INVINCIBLE) MS_CASE(REACTIVE)
        MS_CASE(SHARP_HIDE)
#undef MS_CASE
#define MS_BOOL(NAME) case MS::NAME: m.setHasStatus<MS::NAME>(true); break;
        MS_BOOL(ASLEEP) MS_BOOL(BARRICADE) MS_BOOL(MINION) MS_BOOL(MINION_LEADER) MS_BOOL(PAINFUL_STABS) MS_BOOL(REGROW)
        MS_BOOL(SHIFTING) MS_BOOL(STASIS)
#undef MS_BOOL
        case MS::STRENGTH:
            m.strength = amount;
            break;
        default:
            break;
    }
}

void setPlayerStatus(Player &p, PlayerStatus s, int amount, bool justApplied) {
    switch (s) {
#define PS_CASE(NAME) case PS::NAME: p.setHasStatus<PS::NAME>(true); p.setStatusValueNoChecks<PS::NAME>(amount); break;
        PS_CASE(BIAS) PS_CASE(CONFUSED) PS_CASE(CONSTRICTED) PS_CASE(ENTANGLED) PS_CASE(FASTING) PS_CASE(HEX) PS_CASE(LOSE_DEXTERITY)
        PS_CASE(LOSE_STRENGTH) PS_CASE(NO_BLOCK) PS_CASE(NO_DRAW) PS_CASE(WRAITH_FORM) PS_CASE(BARRICADE) PS_CASE(BLASPHEMER)
        PS_CASE(CORRUPTION) PS_CASE(ELECTRO) PS_CASE(SURROUNDED) PS_CASE(MASTER_REALITY) PS_CASE(PEN_NIB) PS_CASE(WRATH_NEXT_TURN)
        PS_CASE(AMPLIFY) PS_CASE(BLUR) PS_CASE(BUFFER) PS_CASE(COLLECT) PS_CASE(DOUBLE_TAP) PS_CASE(DUPLICATION) PS_CASE(ECHO_FORM)
        PS_CASE(FREE_ATTACK_POWER) PS_CASE(REBOUND) PS_CASE(MANTRA) PS_CASE(ACCURACY) PS_CASE(AFTER_IMAGE) PS_CASE(BATTLE_HYMN)
        PS_CASE(BRUTALITY) PS_CASE(BURST) PS_CASE(COMBUST) PS_CASE(CREATIVE_AI) PS_CASE(DARK_EMBRACE) PS_CASE(DEMON_FORM) PS_CASE(DEVA)
        PS_CASE(DEVOTION) PS_CASE(DRAW_CARD_NEXT_TURN) PS_CASE(ENERGIZED) PS_CASE(ENVENOM) PS_CASE(ESTABLISHMENT) PS_CASE(EVOLVE)
        PS_CASE(FEEL_NO_PAIN) PS_CASE(FIRE_BREATHING) PS_CASE(FLAME_BARRIER) PS_CASE(FORESIGHT) PS_CASE(HELLO_WORLD) PS_CASE(INFINITE_BLADES)
        PS_CASE(JUGGERNAUT) PS_CASE(LIKE_WATER) PS_CASE(LOOP) PS_CASE(MAGNETISM) PS_CASE(MAYHEM) PS_CASE(METALLICIZE) PS_CASE(NEXT_TURN_BLOCK)
        PS_CASE(NOXIOUS_FUMES) PS_CASE(OMEGA) PS_CASE(PANACHE) PS_CASE(PHANTASMAL) PS_CASE(PLATED_ARMOR) PS_CASE(RAGE) PS_CASE(REGEN)
        PS_CASE(RITUAL) PS_CASE(RUPTURE) PS_CASE(SADISTIC) PS_CASE(STATIC_DISCHARGE) PS_CASE(THORNS) PS_CASE(THOUSAND_CUTS)
        PS_CASE(TOOLS_OF_THE_TRADE) PS_CASE(VIGOR) PS_CASE(WAVE_OF_THE_HAND) PS_CASE(EQUILIBRIUM) PS_CASE(THE_BOMB)
#undef PS_CASE
#define PS_TIMED(NAME) case PS::NAME: p.setHasStatus<PS::NAME>(true); p.setStatusValueNoChecks<PS::NAME>(amount); p.setJustApplied<PS::NAME>(justApplied); break;
        PS_TIMED(DOUBLE_DAMAGE) PS_TIMED(DRAW_REDUCTION) PS_TIMED(FRAIL) PS_TIMED(INTANGIBLE) PS_TIMED(VULNERABLE) PS_TIMED(WEAK)
#undef PS_TIMED
        case PS::ARTIFACT: p.artifact = amount; break;
        case PS::DEXTERITY: p.dexterity = amount; break;
        case PS::FOCUS: p.focus = amount; break;
        case PS::STRENGTH: p.strength = amount; break;
        default: break;
    }
}

void setMonsterJustApplied(Monster &m, MonsterStatus s, bool value) {
    switch (s) {
        case MS::VULNERABLE: m.setJustApplied<MS::VULNERABLE>(value); break;
        case MS::WEAK: m.setJustApplied<MS::WEAK>(value); break;
        case MS::RITUAL: m.setJustApplied<MS::RITUAL>(value); break;
        default: break;
    }
}

// ---------------- cards ----------------

Card cardFromJson(const json &c, ConvertNotes &notes) {
    const std::string id = c["id"].get<std::string>();
    const auto card = cardFromGameId(id);
    if (!card) {
        notes.warn("unknown card id " + id);
        return Card(CardId::WOUND);
    }
    Card out(*card, c.value("upgrades", 0));
    out.misc = static_cast<std::int16_t>(c.value("misc", 0));
    return out;
}

CardInstance instanceFromJson(const json &c, int uniqueId, ConvertNotes &notes, int energy = -1) {
    CardInstance out(cardFromJson(c, notes));
    out.costForTurn = static_cast<std::int8_t>(c.value("cost", static_cast<int>(out.costForTurn)));
    out.setUniqueId(uniqueId);
    // The game reports no "free once" flag, but a card marked playable that costs more than the energy left must be free.
    if (energy >= 0 && c.value("is_playable", false) && out.costForTurn > energy) out.freeToPlayOnce = true;
    return out;
}

// ---------------- run state ----------------

int relicDataFromCounter(RelicId r, int counter) {
    switch (r) {
        case RelicId::LIZARD_TAIL:  // the game marks a spent one with -2; the simulator stores "still available"
            return counter == -2 ? 0 : 1;
        default:
            return std::max(counter, 0);
    }
}

Room roomFromSymbol(const std::string &s) {
    if (s == "M") return Room::MONSTER;
    if (s == "E") return Room::ELITE;
    if (s == "R") return Room::REST;
    if (s == "$") return Room::SHOP;
    if (s == "T") return Room::TREASURE;
    if (s == "?") return Room::EVENT;
    return Room::NONE;
}

std::shared_ptr<Map> mapFromJson(const json &nodes) {
    auto map = std::make_shared<Map>();
    for (int y = 0; y < 15; ++y) {
        for (int x = 0; x < 7; ++x) {
            map->getNode(x, y).x = x;
            map->getNode(x, y).y = y;
        }
    }
    for (const auto &n : nodes) {
        const int y = n["y"].get<int>();
        if (y < 0 || y >= 15) continue;
        MapNode &node = map->getNode(n["x"].get<int>(), y);
        node.room = roomFromSymbol(n.value("symbol", ""));
        std::vector<int> edges;
        for (const auto &child : n["children"]) {
            if (child["y"].get<int>() < 15) edges.push_back(child["x"].get<int>());
        }
        std::sort(edges.begin(), edges.end());
        node.edgeCount = static_cast<int>(std::min<size_t>(edges.size(), node.edges.size()));
        std::copy_n(edges.begin(), node.edgeCount, node.edges.begin());
    }
    return map;
}

int firstNumberIn(const std::string &text) {
    static const std::regex number("[0-9]+");
    std::smatch m;
    return std::regex_search(text, m, number) ? std::stoi(m.str()) : 0;
}

void fillEventInfo(GameContext &gc, const json &screen, const RunMemory &memory, ConvertNotes &notes) {
    const json &options = screen["options"];
    const std::string id = screen.value("event_id", "");
    if (id == "Neow Event") {
        gc.curEvent = Event::NEOW;
        for (int i = 0; i < 4; ++i) {
            // A two-option mini blessing repeats its options so every index stays valid.
            const int n = std::max<int>(1, static_cast<int>(options.size()));
            gc.info.neowRewards[i] = neowOptionFromText(options[i % n].value("text", ""));
        }
        return;
    }
    const auto event = eventFromGameId(id);
    if (!event) {
        notes.warn("unknown event id " + id);
        gc.curEvent = Event::INVALID;
        return;
    }
    gc.curEvent = *event;

    auto numberAt = [&](size_t i) { return i < options.size() ? firstNumberIn(options[i].value("text", "")) : 0; };
    auto disabledAt = [&](size_t i) { return i >= options.size() || options[i].value("disabled", false); };
    gc.info.hpAmount0 = numberAt(0);
    gc.info.hpAmount1 = numberAt(1);
    gc.info.hpAmount2 = numberAt(2);
    gc.info.phase = memory.eventChoices;
    gc.info.eventData = memory.eventChoices;

    switch (gc.curEvent) {
        case Event::CURSED_TOME:
            // Two buttons either open the event or end it; lone [Continue] pages never reach the agent.
            gc.info.eventData = memory.eventChoices == 0 ? 0 : 4;
            break;
        case Event::COLOSSEUM:
            gc.info.eventData = 1;
            break;
        case Event::WE_MEET_AGAIN:
            gc.info.potionIdx = disabledAt(0) ? -1 : 0;
            gc.info.gold = disabledAt(1) ? -1 : numberAt(1);
            gc.info.cardIdx = disabledAt(2) ? -1 : 0;
            break;
        case Event::FALLING:
            gc.info.skillCardDeckIdx = disabledAt(0) ? -1 : 0;
            gc.info.powerCardDeckIdx = disabledAt(1) ? -1 : 0;
            gc.info.attackCardDeckIdx = disabledAt(2) ? -1 : 0;
            break;
        case Event::DESIGNER_IN_SPIRE: {
            std::string first = options.empty() ? "" : options[0].value("text", "");
            gc.info.upgradeOne = first.find("random") == std::string::npos;
            std::string second = options.size() > 1 ? options[1].value("text", "") : "";
            gc.info.cleanUpIsRemoveCard = second.find("Remove") != std::string::npos;
            break;
        }
        default:
            break;
    }
}

void fillRewards(GameContext &gc, const json &screen, ConvertNotes &notes) {
    Rewards r;
    for (const auto &item : screen["rewards"]) {
        const std::string type = item.value("reward_type", "");
        if (type == "GOLD" || type == "STOLEN_GOLD") {
            r.addGold(item["gold"].get<int>());
        } else if (type == "RELIC") {
            const auto relic = relicFromGameId(item["relic"]["id"].get<std::string>());
            if (relic) r.addRelic(*relic);
            else notes.warn("unknown relic reward " + item["relic"]["id"].get<std::string>());
        } else if (type == "POTION") {
            const auto potion = potionFromGameId(item["potion"]["id"].get<std::string>());
            if (potion) r.addPotion(*potion);
            else notes.warn("unknown potion reward " + item["potion"]["id"].get<std::string>());
        } else if (type == "CARD") {
            r.addCardReward(CardReward{});  // the game lists a reward's cards only once it is opened
        } else if (type == "EMERALD_KEY") {
            r.emeraldKey = true;
        } else if (type == "SAPPHIRE_KEY") {
            r.sapphireKey = true;
        }
    }
    gc.info.rewardsContainer = r;
}

void fillShop(GameContext &gc, const json &screen, ConvertNotes &notes) {
    Shop &s = gc.info.shop;
    for (int i = 0; i < 7; ++i) s.cardPrice(i) = -1;
    for (int i = 0; i < 3; ++i) {
        s.relicPrice(i) = -1;
        s.potionPrice(i) = -1;
    }
    int slot = 0;
    for (const auto &c : screen["cards"]) {
        if (slot >= 7) break;
        s.cards[slot] = cardFromJson(c, notes);
        s.cardPrice(slot++) = c["price"].get<int>();
    }
    slot = 0;
    for (const auto &r : screen["relics"]) {
        const auto relic = relicFromGameId(r["id"].get<std::string>());
        if (slot >= 3) break;
        if (!relic) {
            notes.warn("unknown shop relic " + r["id"].get<std::string>());
            continue;
        }
        s.relics[slot] = *relic;
        s.relicPrice(slot++) = r["price"].get<int>();
    }
    slot = 0;
    for (const auto &p : screen["potions"]) {
        const auto potion = potionFromGameId(p["id"].get<std::string>());
        if (slot >= 3) break;
        if (!potion) {
            notes.warn("unknown shop potion " + p["id"].get<std::string>());
            continue;
        }
        s.potions[slot] = *potion;
        s.potionPrice(slot++) = p["price"].get<int>();
    }
    s.removeCost = screen.value("purge_available", false) ? screen.value("purge_cost", -1) : -1;
}

void fillGrid(GameContext &gc, const json &screen, ConvertNotes &notes) {
    if (screen.value("for_upgrade", false)) gc.info.selectScreenType = CardSelectScreenType::UPGRADE;
    else if (screen.value("for_purge", false)) gc.info.selectScreenType = CardSelectScreenType::REMOVE;
    else if (screen.value("for_transform", false)) gc.info.selectScreenType = CardSelectScreenType::TRANSFORM;
    else if (gc.curEvent == Event::DUPLICATOR) gc.info.selectScreenType = CardSelectScreenType::DUPLICATE;
    else gc.info.selectScreenType = CardSelectScreenType::BOTTLE;
    const json remaining = unselectedCards(screen);
    const int alreadySelected = static_cast<int>(screen["cards"].size()) - static_cast<int>(remaining.size());
    gc.info.toSelectCount = std::max(1, screen.value("num_cards", 1) - alreadySelected);

    gc.info.toSelectCards.clear();
    std::vector<bool> used(gc.deck.size(), false);
    for (const auto &c : remaining) {
        const Card card = cardFromJson(c, notes);
        int deckIdx = -1;
        for (int i = 0; i < gc.deck.size(); ++i) {
            if (!used[i] && gc.deck.cards[i] == card) {
                deckIdx = i;
                used[i] = true;
                break;
            }
        }
        gc.info.toSelectCards.push_back(SelectScreenCard(card, deckIdx));
    }
}

// ---------------- monsters ----------------

// The newest entry in a monster's log is its current intent, which has not happened yet.
int countDone(const std::vector<MMID> &log, MMID move) {
    if (log.empty()) return 0;
    return static_cast<int>(std::count(log.begin(), log.end() - 1, move));
}

// What the simulator keeps for its next Boost: 0 or 1 Boosts this cycle. At A19 a cycle after a Hyper Beam starts from -1.
int bronzeAutomatonBoosts(const std::vector<MMID> &log, int ascension) {
    if (log.empty()) return 0;
    int boosts = 0;
    auto move = log.rbegin() + 1;  // the last entry is the move it intends, not one it has done
    for (; move != log.rend() && *move != MMID::BRONZE_AUTOMATON_HYPER_BEAM; ++move) {
        if (*move == MMID::BRONZE_AUTOMATON_BOOST) ++boosts;
    }
    const bool afterBeam = move != log.rend();
    if (afterBeam && ascension >= 19) return boosts == 3 ? 0 : boosts - 1;
    return boosts % 2;
}

// Simulator-only fields the JSON has no slot for, recovered from the moves the bridge has watched. A few are unknown
// until seen (a louse's bite damage before its first bite); the player does not know them either.
void fillMonsterInternals(Monster &m, int gameIndex, int ascension, const BattleMemory &memory) {
    static const std::vector<MMID> kNoMoves;
    const auto logIt = memory.moves.find(gameIndex);
    const std::vector<MMID> &log = logIt == memory.moves.end() ? kNoMoves : logIt->second;
    const auto damageIt = memory.attackDamage.find(gameIndex);
    const bool damageSeen = damageIt != memory.attackDamage.end();

    switch (m.id) {
        case MonsterId::GREEN_LOUSE:
        case MonsterId::RED_LOUSE:
            m.miscInfo = damageSeen ? damageIt->second : 6;
            break;
        case MonsterId::DARKLING:
            m.miscInfo = damageSeen ? damageIt->second : 9;
            break;
        case MonsterId::HEXAGHOST: {
            if (damageSeen) m.miscInfo = damageIt->second;
            int orbs = 0;
            for (size_t i = 0; i + 1 < log.size(); ++i) {
                if (log[i] == MMID::HEXAGHOST_DIVIDER || log[i] == MMID::HEXAGHOST_INFERNO) orbs = 0;
                else if (log[i] == MMID::HEXAGHOST_INFLAME || log[i] == MMID::HEXAGHOST_SEAR || log[i] == MMID::HEXAGHOST_TACKLE) ++orbs;
            }
            m.uniquePower0 = orbs;
            break;
        }
        case MonsterId::BOOK_OF_STABBING: {
            const auto it = memory.stabCount.find(gameIndex);
            m.miscInfo = it != memory.stabCount.end() ? it->second : 1;
            // From A18 a single stab adds a hit too, unseen until the next multi stab.
            if (ascension >= 18) {
                for (auto move = log.rbegin(); move != log.rend() && *move != MMID::BOOK_OF_STABBING_MULTI_STAB; ++move) ++m.miscInfo;
            }
            break;
        }
        case MonsterId::BRONZE_AUTOMATON:
            m.miscInfo = bronzeAutomatonBoosts(log, ascension);
            break;
        case MonsterId::BRONZE_ORB:
            m.miscInfo = countDone(log, MMID::BRONZE_ORB_STASIS) > 0 ? 1 : 0;
            break;
        case MonsterId::GREMLIN_WIZARD: {
            int charges = 0;
            for (size_t i = 0; i + 1 < log.size(); ++i) {
                if (log[i] == MMID::GREMLIN_WIZARD_ULTIMATE_BLAST) charges = 0;
                else if (log[i] == MMID::GREMLIN_WIZARD_CHARGING) ++charges;
            }
            m.miscInfo = charges;
            break;
        }
        case MonsterId::THE_GUARDIAN: {
            const int base = ascension >= 19 ? 40 : ascension >= 9 ? 35 : 30;
            m.miscInfo = base + 10 * countDone(log, MMID::THE_GUARDIAN_TWIN_SLAM);
            break;
        }
        case MonsterId::SPIKER:
            m.miscInfo = countDone(log, MMID::SPIKER_SPIKE);
            break;
        case MonsterId::RED_SLAVER:  // set when the move is rolled, so the pending one counts
            m.miscInfo = std::count(log.begin(), log.end(), MMID::RED_SLAVER_ENTANGLE) > 0 ? 1 : 0;
            break;
        case MonsterId::WRITHING_MASS:
            m.miscInfo = countDone(log, MMID::WRITHING_MASS_IMPLANT) > 0 ? 1 : 0;
            break;
        case MonsterId::TIME_EATER:
            m.miscInfo = countDone(log, MMID::TIME_EATER_HASTE) > 0 ? 1 : 0;
            break;
        case MonsterId::THE_CHAMP: {
            // low bits: Defensive Stance uses (counted when rolled, so the pending one counts too); 0x4: Anger used
            const int stances = static_cast<int>(std::count(log.begin(), log.end(), MMID::THE_CHAMP_DEFENSIVE_STANCE));
            const bool anger = std::count(log.begin(), log.end(), MMID::THE_CHAMP_ANGER) > 0;
            m.miscInfo = stances | (anger ? 0x4 : 0);
            break;
        }
        case MonsterId::AWAKENED_ONE:
            // Rebirth itself never shows as an intent; the second form's moves do, from its first turn.
            m.miscInfo = (std::count(log.begin(), log.end(), MMID::AWAKENED_ONE_DARK_ECHO) +
                          std::count(log.begin(), log.end(), MMID::AWAKENED_ONE_SLUDGE) +
                          std::count(log.begin(), log.end(), MMID::AWAKENED_ONE_TACKLE)) > 0 ? 1 : 0;
            break;
        default:
            break;
    }
    // Simulator bookkeeping statuses that have no power in the game.
    switch (m.id) {
        case MonsterId::BRONZE_AUTOMATON:
        case MonsterId::THE_COLLECTOR:
        case MonsterId::GREMLIN_LEADER:
        case MonsterId::REPTOMANCER:
            m.setHasStatus<MS::MINION_LEADER>(true);
            break;
        case MonsterId::LAGAVULIN:
            if (m.moveHistory[0] == MMID::LAGAVULIN_SLEEP) m.setHasStatus<MS::ASLEEP>(true);
            break;
        default:
            break;
    }
}

// The simulator hard-codes where these monsters sit relative to their summons; everyone else fills the other slots.
int fixedSlotFor(MonsterId id) {
    switch (id) {
        case MonsterId::BRONZE_AUTOMATON: return 1;
        case MonsterId::THE_COLLECTOR: return 2;
        case MonsterId::GREMLIN_LEADER: return 3;
        case MonsterId::REPTOMANCER: return 2;
        default: return -1;
    }
}

MonsterEncounter encounterForBoss(const json &gs, const std::string &roomType) {
    if (roomType != "MonsterRoomBoss") return MonsterEncounter::INVALID;
    for (const auto &[name, value] : std::initializer_list<std::pair<const char *, MonsterEncounter>>{
             {"The Guardian", MonsterEncounter::THE_GUARDIAN}, {"Hexaghost", MonsterEncounter::HEXAGHOST},
             {"Slime Boss", MonsterEncounter::SLIME_BOSS}, {"Champ", MonsterEncounter::CHAMP},
             {"Automaton", MonsterEncounter::AUTOMATON}, {"Collector", MonsterEncounter::COLLECTOR},
             {"Awakened One", MonsterEncounter::AWAKENED_ONE}, {"Time Eater", MonsterEncounter::TIME_EATER},
             {"Donu and Deca", MonsterEncounter::DONU_AND_DECA}, {"The Heart", MonsterEncounter::THE_HEART}}) {
        if (gs.value("act_boss", "") == name) return value;
    }
    return MonsterEncounter::INVALID;
}

// The search only asks "is this a boss" and "is this an elite"; a representative encounter answers both.
MonsterEncounter encounterFromRoom(const json &gs, const std::string &roomType) {
    const MonsterEncounter boss = encounterForBoss(gs, roomType);
    if (boss != MonsterEncounter::INVALID) return boss;
    if (roomType == "MonsterRoomElite") return MonsterEncounter::GREMLIN_NOB;
    return MonsterEncounter::CULTIST;
}

CardSelectTask taskFromAction(const std::string &action, const std::string &screen, int maxCards) {
    if (action == "ArmamentsAction") return CardSelectTask::ARMAMENTS;
    if (action == "DualWieldAction") return CardSelectTask::DUAL_WIELD;
    if (action == "ExhaustAction") return maxCards > 1 ? CardSelectTask::EXHAUST_MANY : CardSelectTask::EXHAUST_ONE;
    if (action == "ExhumeAction") return CardSelectTask::EXHUME;
    if (action == "ForethoughtAction") return CardSelectTask::FORETHOUGHT;
    if (action == "GamblingChipAction") return CardSelectTask::GAMBLE;
    if (action == "PutOnDeckAction" || action == "WarcryAction") return CardSelectTask::WARCRY;
    if (action == "DiscardPileToTopOfDeckAction" || action == "HeadbuttAction") return CardSelectTask::HEADBUTT;
    if (action == "BetterDiscardPileToHandAction") return CardSelectTask::LIQUID_MEMORIES_POTION;
    if (action == "SeekAction") return CardSelectTask::SEEK;
    if (action == "SkillFromDeckToHandAction") return CardSelectTask::SECRET_TECHNIQUE;
    if (action == "AttackFromDeckToHandAction") return CardSelectTask::SECRET_WEAPON;
    if (action == "CodexAction") return CardSelectTask::CODEX;
    if (action == "DiscoveryAction" || screen == "CARD_REWARD") return CardSelectTask::DISCOVERY;
    return CardSelectTask::INVALID;
}

}  // namespace

Neow::Option neowOptionFromText(const std::string &rawText) {
    std::string t = rawText;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto has = [&](const char *needle) { return t.find(needle) != std::string::npos; };

    Neow::Option o{Neow::Bonus::INVALID, Neow::Drawback::NONE};
    if (has("lose your starting relic")) o.d = Neow::Drawback::LOSE_STARTER_RELIC;
    else if (has("lose all gold")) o.d = Neow::Drawback::NO_GOLD;
    else if (has("obtain a curse")) o.d = Neow::Drawback::CURSE;
    else if (has("damage")) o.d = Neow::Drawback::PERCENT_DAMAGE;
    else if (has("lose") && has("max hp")) o.d = Neow::Drawback::TEN_PERCENT_HP_LOSS;

    const bool strong = o.d != Neow::Drawback::NONE;
    if (has("boss relic")) o.r = Neow::Bonus::BOSS_RELIC;
    else if (has("rare colorless")) o.r = Neow::Bonus::RANDOM_COLORLESS_2;
    else if (has("colorless")) o.r = Neow::Bonus::RANDOM_COLORLESS;
    else if (has("choose a rare card")) o.r = Neow::Bonus::THREE_RARE_CARDS;
    else if (has("random rare card")) o.r = Neow::Bonus::ONE_RANDOM_RARE_CARD;
    else if (has("rare relic")) o.r = Neow::Bonus::ONE_RARE_RELIC;
    else if (has("common relic")) o.r = Neow::Bonus::RANDOM_COMMON_RELIC;
    else if (has("choose a card")) o.r = Neow::Bonus::THREE_CARDS;
    else if (has("remove 2")) o.r = Neow::Bonus::REMOVE_TWO;
    else if (has("remove a card")) o.r = Neow::Bonus::REMOVE_CARD;
    else if (has("transform 2")) o.r = Neow::Bonus::TRANSFORM_TWO_CARDS;
    else if (has("transform a card")) o.r = Neow::Bonus::TRANSFORM_CARD;
    else if (has("upgrade a card")) o.r = Neow::Bonus::UPGRADE_CARD;
    else if (has("potions")) o.r = Neow::Bonus::THREE_SMALL_POTIONS;
    else if (has("three combats")) o.r = Neow::Bonus::THREE_ENEMY_KILL;
    else if (has("max hp +")) o.r = strong ? Neow::Bonus::TWENTY_PERCENT_HP_BONUS : Neow::Bonus::TEN_PERCENT_HP_BONUS;
    else if (has("gold")) o.r = strong ? Neow::Bonus::TWO_FIFTY_GOLD : Neow::Bonus::HUNDRED_GOLD;
    return o;
}

// ---------------- memory ----------------

void BattleMemory::reset(int newFloor) {
    *this = BattleMemory{};
    floor = newFloor;
}

void BattleMemory::observe(const json &gs) {
    const int gsFloor = gs.value("floor", -1);
    if (gsFloor != floor) reset(gsFloor);
    const json &cs = gs["combat_state"];
    const int currentTurn = cs.value("turn", 0);
    const bool newTurn = currentTurn != turn;
    if (newTurn) {
        turn = currentTurn;
        cardsPlayed = 0;
        attacksPlayed = 0;
        skillsPlayed = 0;
    }

    const json &monsters = cs["monsters"];
    for (int i = 0; i < static_cast<int>(monsters.size()); ++i) {
        const json &m = monsters[i];
        const auto id = monsterFromGameId(m.value("id", ""));
        if (!id || !m.contains("move_id")) continue;
        const auto move = moveFromGameByte(*id, m["move_id"].get<int>());
        if (!move) continue;
        // A monster can change its intent mid-turn (Writhing Mass when hit), so the turn's last intent is the move.
        auto &log = moves[i];
        if (newTurn || log.empty()) log.push_back(*move);
        else log.back() = *move;
        if (*move == MMID::GREEN_LOUSE_BITE || *move == MMID::RED_LOUSE_BITE || *move == MMID::DARKLING_NIP ||
            *move == MMID::HEXAGHOST_DIVIDER) {
            attackDamage[i] = m.value("move_base_damage", 0);
        }
        if (*move == MMID::BOOK_OF_STABBING_MULTI_STAB) stabCount[i] = m.value("move_hits", 1);
    }
}

void BattleMemory::onCardPlayed(CardType type) {
    ++cardsPlayed;
    if (type == CardType::ATTACK) ++attacksPlayed;
    if (type == CardType::SKILL) ++skillsPlayed;
}

// ---------------- conversion ----------------

GameContext gameContextFromState(const json &gs, std::uint64_t rngSeed, const RunMemory &memory, ConvertNotes &notes) {
    const int ascension = gs.value("ascension_level", 0);
    GameContext gc(CharacterClass::IRONCLAD, rngSeed, ascension);

    gc.act = gs.value("act", 1);
    gc.floorNum = gs.value("floor", 0);
    gc.curHp = gs.value("current_hp", 1);
    gc.maxHp = gs.value("max_hp", 1);
    gc.gold = gs.value("gold", 0);

    gc.relics = RelicContainer{};
    for (const auto &r : gs["relics"]) {
        const std::string id = r["id"].get<std::string>();
        const auto relic = relicFromGameId(id);
        if (!relic) {
            notes.warn("unknown relic " + id);
            continue;
        }
        gc.relics.add(RelicInstance{*relic, relicDataFromCounter(*relic, r.value("counter", -1))});
    }

    gc.deck = Deck{};
    for (const auto &c : gs["deck"]) gc.deck.obtainRaw(cardFromJson(c, notes));

    const json &potions = gs["potions"];
    gc.potionCapacity = static_cast<int>(potions.size());
    gc.potionCount = 0;
    gc.potions.fill(Potion::EMPTY_POTION_SLOT);
    for (int i = 0; i < gc.potionCapacity && i < 5; ++i) {
        const std::string id = potions[i]["id"].get<std::string>();
        const auto potion = potionFromGameId(id);
        if (!potion) {
            notes.warn("unknown potion " + id);
            continue;
        }
        gc.potions[i] = *potion;
        if (*potion != Potion::EMPTY_POTION_SLOT) ++gc.potionCount;
    }

    if (gs.contains("keys")) {
        gc.redKey = gs["keys"].value("ruby", false);
        gc.greenKey = gs["keys"].value("emerald", false);
        gc.blueKey = gs["keys"].value("sapphire", false);
    }

    gc.map = mapFromJson(gs["map"]);
    gc.curMapNodeX = memory.nodeX;
    gc.curMapNodeY = memory.nodeY;

    gc.screenState = ScreenState::INVALID;
    const std::string screen = gs.value("screen_type", "NONE");
    const json &screenState = gs["screen_state"];
    if (screen == "MAP") {
        gc.screenState = ScreenState::MAP_SCREEN;
        if (screenState.contains("current_node") && screenState.value("first_node_chosen", false)) {
            gc.curMapNodeX = screenState["current_node"]["x"].get<int>();
            gc.curMapNodeY = screenState["current_node"]["y"].get<int>();
        } else if (!screenState.value("first_node_chosen", true)) {
            gc.curMapNodeX = -1;
            gc.curMapNodeY = -1;
        }
    } else if (screen == "EVENT") {
        gc.screenState = ScreenState::EVENT_SCREEN;
        fillEventInfo(gc, screenState, memory, notes);
    } else if (screen == "COMBAT_REWARD") {
        gc.screenState = ScreenState::REWARDS;
        fillRewards(gc, screenState, notes);
    } else if (screen == "CARD_REWARD") {
        gc.screenState = ScreenState::REWARDS;
        Rewards r;
        CardReward cards;
        for (const auto &c : screenState["cards"]) cards.push_back(cardFromJson(c, notes));
        r.addCardReward(cards);
        gc.info.rewardsContainer = r;
    } else if (screen == "BOSS_REWARD") {
        gc.screenState = ScreenState::BOSS_RELIC_REWARDS;
        int i = 0;
        for (const auto &r : screenState["relics"]) {
            if (i >= 3) break;
            const auto relic = relicFromGameId(r["id"].get<std::string>());
            gc.info.bossRelics[i++] = relic ? *relic : RelicId::INVALID;
        }
    } else if (screen == "CHEST") {
        gc.screenState = ScreenState::TREASURE_ROOM;
        const std::string chest = screenState.value("chest_type", "");
        gc.info.chestSize = chest == "LargeChest" ? ChestSize::LARGE : chest == "MediumChest" ? ChestSize::MEDIUM : ChestSize::SMALL;
    } else if (screen == "REST") {
        gc.screenState = ScreenState::REST_ROOM;
    } else if (screen == "SHOP_SCREEN") {
        gc.screenState = ScreenState::SHOP_ROOM;
        fillShop(gc, screenState, notes);
    } else if (screen == "GRID") {
        gc.screenState = ScreenState::CARD_SELECT;
        fillGrid(gc, screenState, notes);
    }
    return gc;
}

ConvertedBattle battleFromState(const GameContext &gc, const json &gs, const BattleMemory &memory,
                                std::uint64_t rngSeed, ConvertNotes &notes) {
    const json &cs = gs["combat_state"];
    const std::string roomType = gs.value("room_type", "");
    const MonsterEncounter encounter = encounterFromRoom(gs, roomType);

    // Relics decide energy and draw per turn; a throwaway init reads those off the simulator's own rules.
    GameContext throwaway = gc;
    throwaway.curRoom = Room::MONSTER;
    BattleContext probe;
    probe.init(throwaway, MonsterEncounter::CULTIST);

    ConvertedBattle out;
    BattleContext &bc = out.bc;
    bc.seed = rngSeed;
    bc.floorNum = gc.floorNum;
    bc.encounter = encounter;
    const Random start(rngSeed);
    bc.aiRng = start;
    bc.monsterHpRng = start;
    bc.shuffleRng = start;
    bc.cardRandomRng = start;
    bc.miscRng = start;
    bc.potionRng = start;
    bc.ascension = gc.ascension;
    bc.outcome = Outcome::UNDECIDED;
    bc.monsterTurnIdx = 6;
    bc.turn = std::max(0, cs.value("turn", 1) - 1);

    bc.potionCount = gc.potionCount;
    bc.potionCapacity = gc.potionCapacity;
    bc.potions = gc.potions;

    Player &p = bc.player;
    const json &player = cs["player"];
    p.cc = gc.cc;
    p.gold = static_cast<std::int16_t>(gc.gold);
    p.curHp = player.value("current_hp", gc.curHp);
    p.maxHp = player.value("max_hp", gc.maxHp);
    p.block = player.value("block", 0);
    p.energy = player.value("energy", 0);
    p.energyPerTurn = probe.player.energyPerTurn;
    p.cardDrawPerTurn = probe.player.cardDrawPerTurn;
    p.relicBits0 = gc.relics.relicBits0;
    p.relicBits1 = gc.relics.relicBits1;
    p.timesDamagedThisCombat = static_cast<std::int16_t>(cs.value("times_damaged", 0));
    p.cardsDiscardedThisTurn = static_cast<std::int16_t>(cs.value("cards_discarded_this_turn", 0));
    p.cardsPlayedThisTurn = static_cast<std::int16_t>(memory.cardsPlayed);
    p.attacksPlayedThisTurn = static_cast<std::int16_t>(memory.attacksPlayed);
    p.skillsPlayedThisTurn = static_cast<std::int16_t>(memory.skillsPlayed);
    p.happyFlowerCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::HAPPY_FLOWER), 0));
    p.incenseBurnerCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::INCENSE_BURNER), 0));
    p.inkBottleCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::INK_BOTTLE), 0));
    p.inserterCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::INSERTER), 0));
    p.nunchakuCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::NUNCHAKU), 0));
    p.penNibCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::PEN_NIB), 0));
    p.sundialCounter = static_cast<std::int8_t>(std::max(gc.relics.getRelicValue(RelicId::SUNDIAL), 0));

    for (const auto &power : player["powers"]) {
        const std::string id = power["id"].get<std::string>();
        const auto status = playerPowerFromGameId(id);
        if (!status) {
            notes.warn("unknown player power " + id);
            continue;
        }
        if (*status == PS::THE_BOMB) {
            const int turnsLeft = power.value("amount", 0);
            auto &bomb = turnsLeft == 1 ? p.bomb1 : turnsLeft == 2 ? p.bomb2 : p.bomb3;
            bomb = static_cast<std::int16_t>(bomb + power.value("damage", 0));
            continue;
        }
        // Panache's amount is its countdown, and its damage is what the simulator keeps as the status.
        const int amount = *status == PS::PANACHE ? power.value("damage", 0) : power.value("amount", 0);
        setPlayerStatus(p, *status, amount, power.value("just_applied", false));
        if (*status == PS::PANACHE) p.panacheCounter = static_cast<std::int8_t>(power.value("amount", 0));
        if (*status == PS::COMBUST) p.combustHpLoss = static_cast<std::int8_t>(power.value("misc", 1));
        if (*status == PS::ECHO_FORM) p.echoFormCardsDoubled = static_cast<std::int8_t>(power.value("misc", 0));
    }

    p.redSkullActive = p.hasRelic<RelicId::RED_SKULL>() && p.curHp <= p.maxHp / 2;

    // The game's list has no gaps and keeps dead monsters; the simulator has five slots and expects summoners and
    // split slimes in particular ones. Monsters keep their game order either way.
    const json &monsters = cs["monsters"];
    std::vector<int> keep;
    int fixedSlot = -1;
    int fixedPosition = -1;
    int largeSlimes = 0;
    for (int i = 0; i < static_cast<int>(monsters.size()); ++i) {
        if (monsters[i].value("is_gone", false)) continue;
        const auto id = monsterFromGameId(monsters[i].value("id", ""));
        if (id && (*id == MonsterId::SPIKE_SLIME_L || *id == MonsterId::ACID_SLIME_L)) ++largeSlimes;
        if (id && fixedSlotFor(*id) >= 0) {
            fixedSlot = fixedSlotFor(*id);
            fixedPosition = static_cast<int>(keep.size());
        }
        keep.push_back(i);
    }
    const bool layoutMatters = fixedSlot >= 0 || largeSlimes >= 2;
    if (!layoutMatters && monsters.size() <= 5) {
        keep.clear();
        for (int i = 0; i < static_cast<int>(monsters.size()); ++i) keep.push_back(i);
    }
    if (keep.size() > 5) {
        notes.warn("more than five live monsters");
        keep.resize(5);
    }
    auto slotOf = [&](int position) {
        if (fixedSlot >= 0) return fixedSlot + position - fixedPosition;
        if (largeSlimes >= 2) return position == 0 ? 0 : position + 1;  // the simulator splits a large slime into idx, idx+1
        return position;
    };

    int alive = 0;
    int highestSlot = -1;
    for (int position = 0; position < static_cast<int>(keep.size()); ++position) {
        const int gameIndex = keep[position];
        const json &j = monsters[gameIndex];
        const std::string id = j["id"].get<std::string>();
        const auto monsterId = monsterFromGameId(id);
        if (!monsterId) notes.warn("unknown monster " + id);
        const MonsterId created = monsterId ? *monsterId : MonsterId::CULTIST;
        const int slot = slotOf(position);
        if (slot < 0 || slot >= 5) {
            notes.warn("no simulator slot for " + id);
            continue;
        }
        out.gameMonsterIndex[slot] = gameIndex;
        highestSlot = std::max(highestSlot, slot);
        bc.monsters.monsterCount = slot;
        bc.monsters.createMonster(bc, created);
        Monster &m = bc.monsters.arr[slot];
        m.curHp = j.value("current_hp", 0);
        m.maxHp = j.value("max_hp", 1);
        m.block = j.value("block", 0);
        m.halfDead = j.value("half_dead", false);
        m.miscInfo = 0;
        const bool gone = j.value("is_gone", false);
        if (gone) m.isEscapingB = m.curHp > 0;  // gone with HP left means it ran away
        else if (!m.halfDead) ++alive;  // the simulator doesn't count a half-dead monster (Awakened One, Darkling) as alive

        for (const auto &power : j["powers"]) {
            const std::string powerId = power["id"].get<std::string>();
            const auto status = monsterPowerFromGameId(powerId);
            if (!status) {
                if (!isUntrackedPower(powerId)) notes.warn("unknown monster power " + powerId);
                continue;
            }
            setMonsterStatus(m, *status, power.value("amount", 0));
            if (power.contains("just_applied")) setMonsterJustApplied(m, *status, power["just_applied"].get<bool>());
        }

        if (j.contains("move_id")) {
            const auto move = moveFromGameByte(m.id, j["move_id"].get<int>());
            if (move) m.moveHistory[0] = *move;
            else notes.warn(id + " has unmapped move byte " + std::to_string(j["move_id"].get<int>()));
        }
        if (j.contains("last_move_id")) {
            const auto last = moveFromGameByte(m.id, j["last_move_id"].get<int>());
            if (last) m.moveHistory[1] = *last;
        }
        fillMonsterInternals(m, gameIndex, gc.ascension, memory);
    }
    bc.monsters.monsterCount = highestSlot + 1;
    if (fixedSlot == 1) bc.monsters.monsterCount = std::max(bc.monsters.monsterCount, 2);  // Automaton's second orb slot
    bc.monsters.monstersAlive = alive;

    // Cards. The draw pile's true order is hidden information: the game happens to send it, so it is reordered
    // here into a form that depends only on the multiset plus the cards the player knows are on top.
    int nextId = 0;
    const json &drawJson = cs["draw_pile"];
    const int knownTop = std::min<int>(memory.knownTop, static_cast<int>(drawJson.size()));
    bc.cards.drawPile.clear();
    for (const auto &c : drawJson) {
        const CardInstance card = instanceFromJson(c, 0, notes);
        bc.cards.drawPile.push_back(card);
        bc.cards.notifyAddCardToCombat(card);
        bc.cards.notifyAddToDrawPile(card);
    }
    if (static_cast<int>(bc.cards.drawPile.size()) > knownTop) {
        std::mt19937_64 shuffler(rngSeed);
        const auto unknownEnd = bc.cards.drawPile.end() - knownTop;
        std::sort(bc.cards.drawPile.begin(), unknownEnd, [](const CardInstance &a, const CardInstance &b) {
            if (a.id != b.id) return a.id < b.id;
            if (a.upgraded != b.upgraded) return a.upgraded < b.upgraded;
            return a.costForTurn < b.costForTurn;
        });
        std::shuffle(bc.cards.drawPile.begin(), unknownEnd, shuffler);
    }
    // IDs go on after the shuffle: the search breaks ties on them, and they must carry no trace of the true order.
    for (auto &card : bc.cards.drawPile) card.setUniqueId(nextId++);
    for (const auto &c : cs["discard_pile"]) {
        const CardInstance card = instanceFromJson(c, nextId++, notes);
        bc.cards.discardPile.push_back(card);
        bc.cards.notifyAddCardToCombat(card);
        bc.cards.notifyAddToDiscardPile(card);
    }
    for (const auto &c : cs["exhaust_pile"]) {
        bc.cards.exhaustPile.push_back(instanceFromJson(c, nextId++, notes));
    }
    for (const auto &c : cs["hand"]) {
        if (bc.cards.cardsInHand >= CardManager::MAX_HAND_SIZE) break;
        const CardInstance card = instanceFromJson(c, nextId++, notes, p.energy);
        bc.cards.notifyAddCardToCombat(card);
        bc.cards.moveToHand(card);
    }
    bc.cards.nextUniqueCardId = nextId;

    // The game knows the true cards-played count (autoplayed cards count too; the bridge only sees its own plays).
    // When it refuses every card in hand for that reason, the count has reached Normality's or Velvet Choker's limit.
    bool anyPlayable = false;
    for (const auto &c : cs["hand"]) anyPlayable = anyPlayable || c.value("is_playable", true);
    if (!anyPlayable && !cs["hand"].empty()) p.cardsPlayedThisTurn = std::max<std::int16_t>(p.cardsPlayedThisTurn, 6);

    // Input state: a mid-card selection screen, or the normal turn.
    bc.inputState = InputState::PLAYER_NORMAL;
    const std::string screen = gs.value("screen_type", "NONE");
    if (screen == "HAND_SELECT" || screen == "GRID" || screen == "CARD_REWARD") {
        const json &screenState = gs["screen_state"];
        const int maxCards = screen == "HAND_SELECT" ? screenState.value("max_cards", 1) : screenState.value("num_cards", 1);
        const CardSelectTask task = taskFromAction(gs.value("current_action", ""), screen, maxCards);
        if (task == CardSelectTask::INVALID) notes.warn("unknown card select action " + gs.value("current_action", ""));
        bc.inputState = InputState::CARD_SELECT;
        bc.cardSelectInfo.cardSelectTask = task;
        bc.cardSelectInfo.pickCount = maxCards;
        if (screen == "CARD_REWARD") {
            int i = 0;
            for (const auto &c : screenState["cards"]) {
                if (i >= 3) break;
                bc.cardSelectInfo.discovery_Cards()[i++] = cardFromJson(c, notes).getId();
            }
        }
    }
    return out;
}

}  // namespace realgame
