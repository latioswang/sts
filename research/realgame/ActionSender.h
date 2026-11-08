// Turns simulator actions into Communication Mod commands. Shared by mirror mode and live mode.
#pragma once

#include "CommClient.h"
#include "Compare.h"

#include "combat/BattleContext.h"
#include "game/GameContext.h"
#include "sim/search/Action.h"
#include "sim/search/GameAction.h"

#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

namespace realgame {

// Ends the current run's loop. status: diverged | bridge_gap | reached_limit.
struct StopRun : std::runtime_error {
    StopRun(std::string status_, std::string where_, std::string message_, Diffs diffs_ = {})
        : std::runtime_error(message_), status(std::move(status_)), where(std::move(where_)), message(std::move(message_)), diffs(std::move(diffs_)) {}
    std::string status;
    std::string where;
    std::string message;
    Diffs diffs;
};

// Keeps the agent's view of out-of-combat actions set only while `observer` is in scope, however the scope ends.
class ObserverScope {
public:
    explicit ObserverScope(std::function<void(const sts::GameContext &, const sts::search::GameAction &)> observer) {
        sts::search::gameActionObserver = std::move(observer);
    }
    ~ObserverScope() { sts::search::gameActionObserver = nullptr; }
    ObserverScope(const ObserverScope &) = delete;
    ObserverScope &operator=(const ObserverScope &) = delete;
};

class ActionSender {
public:
    explicit ActionSender(CommClient &game);
    virtual ~ActionSender() = default;

protected:
    CommClient &game_;
    std::ofstream log_;
    int step_ = 0;
    int floor_ = 0;
    int act_ = 1;
    std::string lastAction_;

    [[nodiscard]] const json &state() const { return game_.last(); }
    [[nodiscard]] const json &gameState() const { return game_.last().at("game_state"); }
    [[nodiscard]] std::string screenType() const;

    void send(const std::string &command);

    void applyGameAction(const sts::GameContext &gc, const sts::search::GameAction &a);
    // monsterIndex maps a simulator monster slot to the game's monster list.
    void applyCombatAction(const sts::BattleContext &bc, const sts::search::Action &a, const std::array<int, 5> &monsterIndex);

    // Whether the simulator's draw pile is in the game's order, as in mirror mode.
    [[nodiscard]] virtual bool drawPileMatchesGame() const { return false; }

    // A card reward's cards become visible only once it is opened.
    virtual void onCardRewardOpened(const sts::CardReward &) {}
    // Declining a reward leaves it on the game's list; a mode that tracks this overrides it.
    virtual void onCardRewardDeclined() {}

private:
    // When the game answered the last command sent inside a room, on any screen but the map.
    std::chrono::steady_clock::time_point roomCommandAnswered_{};

    void settleBeforeLeavingRoom();
    void applyEvent(const sts::GameContext &gc, int option);
    void applyMatchAndKeep(const sts::GameContext &gc, int first, int second);
    void chooseMatchCard(const std::vector<int> &faceDown, int position);
    void applyRewards(const sts::GameContext &gc, const sts::search::GameAction &a);
    void applyShop(const sts::GameContext &gc, const sts::search::GameAction &a);
    void applyCardSelect(const sts::GameContext &gc, int index);
    void applyMap(const sts::GameContext &gc, int x);
    void chooseCardOnScreen(const std::string &key, int copy = 0);
    void chooseDrawPileCardOnScreen(const sts::BattleContext &bc, int drawIdx);
    [[nodiscard]] bool chooseCardByUuid(const std::string &uuid);
    void pressConfirm();
    void confirmIfRequired();  // after a pick
};

}  // namespace realgame
