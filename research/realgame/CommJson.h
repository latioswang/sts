// Simulator state in Communication Mod's game_state format (schema: GameStateConverter.java). Used only to test the
// converter offline: state -> JSON -> converter -> state. This says nothing about whether the simulator matches the
// game; only mirror mode tests that.
#pragma once

#include "combat/BattleContext.h"
#include "game/GameContext.h"

#include <nlohmann/json.hpp>

namespace realgame {

nlohmann::json gameStateFromSim(const sts::GameContext &gc, const sts::BattleContext *bc);

}  // namespace realgame
