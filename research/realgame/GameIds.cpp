#include "GameIds.h"

#include "constants/MonsterEncounters.h"

#include <map>
#include <utility>

using namespace sts;

namespace realgame {

#include "GameIdTables.inc"

namespace {

template <typename E, size_t N>
const std::map<std::string, E> &forwardMap(const std::pair<const char *, E> (&table)[N]) {
    static const std::map<std::string, E> m = [&] {
        std::map<std::string, E> out;
        for (const auto &[name, value] : table) out.emplace(name, value);
        return out;
    }();
    return m;
}

template <typename E, size_t N>
std::string reverseLookup(const std::pair<const char *, E> (&table)[N], E value) {
    for (const auto &[name, v] : table) {
        if (v == value) return name;
    }
    return "?";
}

}  // namespace

std::optional<CardId> cardFromGameId(const std::string &id) {
    static const std::map<std::string, CardId> table = [] {
        std::map<std::string, CardId> out;
        const int count = static_cast<int>(sizeof(cardStringIds) / sizeof(cardStringIds[0]));
        for (int i = 1; i < count; ++i) out.emplace(cardStringIds[i], static_cast<CardId>(i));
        return out;
    }();
    const auto it = table.find(id);
    if (it == table.end()) return std::nullopt;
    return it->second;
}

std::string cardToGameId(CardId id) { return cardStringIds[static_cast<int>(id)]; }

std::optional<RelicId> relicFromGameId(const std::string &id) {
    const auto &m = forwardMap(kRelicIds);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string relicToGameId(RelicId id) { return reverseLookup(kRelicIds, id); }

std::optional<Potion> potionFromGameId(const std::string &id) {
    if (id == "Potion Slot") return Potion::EMPTY_POTION_SLOT;
    const auto &m = forwardMap(kPotionIds);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string potionToGameId(Potion p) {
    if (p == Potion::EMPTY_POTION_SLOT) return "Potion Slot";
    return reverseLookup(kPotionIds, p);
}

std::optional<MonsterId> monsterFromGameId(const std::string &id) {
    const auto &m = forwardMap(kMonsterIds);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string monsterToGameId(MonsterId id) { return reverseLookup(kMonsterIds, id); }

std::optional<MonsterStatus> monsterPowerFromGameId(const std::string &id) {
    const auto &m = forwardMap(kMonsterPowers);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string monsterPowerToGameId(MonsterStatus s) { return reverseLookup(kMonsterPowers, s); }

// Split: large slimes split by monster id. Berserk: raises energyPerTurn. Explosive: the Exploder's move. Unawakened: the
// Awakened One's phase is in miscInfo. BackAttack: the simulator works out Surrounded facing from the last target.
bool isUntrackedPower(const std::string &id) {
    return id == "Split" || id == "Berserk" || id == "Explosive" || id == "Unawakened" || id == "BackAttack";
}

std::optional<PlayerStatus> playerPowerFromGameId(const std::string &id) {
    // Each bomb is a power of its own: TheBomb0, TheBomb1 and so on.
    if (id.rfind("TheBomb", 0) == 0) return PlayerStatus::THE_BOMB;
    const auto &m = forwardMap(kPlayerPowers);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string playerPowerToGameId(PlayerStatus s) { return reverseLookup(kPlayerPowers, s); }

std::optional<MMID> moveFromGameByte(MonsterId monster, int moveByte) {
    for (const auto &e : kMoveBytes) {
        if (e.monster == monster && e.byte == moveByte) return e.move;
    }
    return std::nullopt;
}

std::optional<int> moveToGameByte(MonsterId monster, MMID move) {
    for (const auto &e : kMoveBytes) {
        if (e.monster == monster && e.move == move) return e.byte;
    }
    return std::nullopt;
}

std::optional<Event> eventFromGameId(const std::string &id) {
    const auto &m = forwardMap(kEventIds);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string eventToGameId(Event e) { return reverseLookup(kEventIds, e); }

std::optional<MonsterEncounter> encounterFromGameId(const std::string &id) {
    const auto &m = forwardMap(kEncounterIds);
    const auto it = m.find(id);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

std::string encounterToGameId(MonsterEncounter e) { return reverseLookup(kEncounterIds, e); }

}  // namespace realgame
