// Game-internal IDs (what Communication Mod reports) <-> sts_lightspeed enums.
#pragma once

#include "constants/Cards.h"
#include "constants/Events.h"
#include "constants/MonsterEncounters.h"
#include "constants/MonsterIds.h"
#include "constants/MonsterMoves.h"
#include "constants/MonsterStatusEffects.h"
#include "constants/PlayerStatusEffects.h"
#include "constants/Potions.h"
#include "constants/Relics.h"

#include <optional>
#include <string>

namespace realgame {

std::optional<sts::CardId> cardFromGameId(const std::string &id);
std::string cardToGameId(sts::CardId id);

std::optional<sts::RelicId> relicFromGameId(const std::string &id);
std::string relicToGameId(sts::RelicId id);

// "Potion Slot" is the game's empty slot and maps to Potion::EMPTY_POTION_SLOT.
std::optional<sts::Potion> potionFromGameId(const std::string &id);
std::string potionToGameId(sts::Potion p);

std::optional<sts::MonsterId> monsterFromGameId(const std::string &id);
std::string monsterToGameId(sts::MonsterId id);

std::optional<sts::MonsterStatus> monsterPowerFromGameId(const std::string &id);
std::string monsterPowerToGameId(sts::MonsterStatus s);

std::optional<PlayerStatus> playerPowerFromGameId(const std::string &id);
// Powers the game shows but the simulator models another way (see the definition).
bool isUntrackedPower(const std::string &id);
std::string playerPowerToGameId(PlayerStatus s);

std::optional<Orb> orbFromGameId(const std::string &id);
std::string orbToGameId(Orb orb);

// A monster's move byte (EnemyMoveInfo.nextMove) as the simulator's move id.
std::optional<sts::MMID> moveFromGameByte(sts::MonsterId monster, int moveByte);
// The byte the game uses for a simulator move, or nullopt if the table has no entry.
std::optional<int> moveToGameByte(sts::MonsterId monster, sts::MMID move);

std::optional<sts::Event> eventFromGameId(const std::string &id);
std::string eventToGameId(sts::Event e);

std::optional<sts::MonsterEncounter> encounterFromGameId(const std::string &id);
std::string encounterToGameId(sts::MonsterEncounter e);

}  // namespace realgame
