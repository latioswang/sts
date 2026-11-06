// Mirror mode: run the agent inside the simulator on the real game's seed, send each of its actions to the real
// game, and stop at the first state difference. Every difference is a simulator bug or a bridge bug.
#pragma once

#include "ActionSender.h"
#include "Scenario.h"
#include "FairAgent.h"

#include <cstdint>
#include <string>
#include <vector>

namespace realgame {

class ReplayClient;

struct MirrorOptions {
    std::uint64_t seed = 0;
    sts::CharacterClass character = sts::CharacterClass::IRONCLAD;
    int ascension = 0;
    int maxFloor = 0;  // stop cleanly after this floor (0 = play the whole run)
    std::string scenario;  // extra items for both sides, see Scenario.h
    fair::Params params;
    std::string logDir;
    // Plays a recording instead of the agent: the moves come from it, and nothing is logged. It is also the game client.
    ReplayClient *replay = nullptr;
    std::string noteCard;  // replay only: the card Note For Yourself held in the recorded run
    int noteUpgrades = 0;
    bool baseModFixes = false;  // replay only: the recorded game ran with BaseMod's fixes on, see TraceHeader
    // Takes the event option tried least so far instead of the agent's, so that the ones it never picks get compared too.
    bool exploreEvents = false;
    int neowOption = -1;  // with exploreEvents, takes this one at Neow, to reach a reward on a seed known to offer it
};

struct MirrorResult {
    std::uint64_t seed = 0;
    std::string seedText;
    // completed | reached_limit | diverged | game_nondeterminism | unsupported_card | bridge_gap,
    // and with a recording, what RecordingStop says
    std::string status;
    int floor = 0;
    int act = 0;
    int step = 0;
    int commands = 0;
    std::string where;    // screen or situation at the stop
    std::string lastAction;
    std::string message;
    Diffs diffs;
    double seconds = 0.0;
};

class Mirror : public ActionSender {
public:
    Mirror(CommClient &game, MirrorOptions options);
    MirrorResult run();

protected:
    void onCardRewardOpened(const sts::CardReward &cards) override;

private:
    MirrorOptions options_;
    sts::GameContext *target_ = nullptr;
    int courierCardSlot_ = -1;
    int mapComparedForAct_ = 0;
    int discoveryFloor_ = -1;
    bool copyCardRandomRng_ = false;
    bool copyCardRng_ = false;
    std::map<std::string, RngState> gameRngs_;
    bool rewardsCompared_ = false;

    [[nodiscard]] int choiceCount() const;
    void prepareNeow(sts::GameContext &gc);
    void logStep(const std::string &where, const std::string &action, const Diffs &diffs);
    void stopOnDifference(const std::string &where, const Diffs &diffs);

    void onGameAction(const sts::GameContext &gc, const sts::search::GameAction &a);
    void onCombatAction(const sts::BattleContext &bc, const sts::search::Action &a);
    void alignCombat();
    void updateGameRngs();
    void playExploringEvents(sts::GameContext &gc, fair::FairAgent &agent);
    bool takeLeastTriedEventOption(sts::GameContext &gc);
    void playRecorded(sts::GameContext &gc);
    void playRecordedBattle(sts::GameContext &gc);
    void stopIfNoRecordedMove() const;
    bool noRecordedMove_ = false;
    template <typename Action, typename Apply>
    [[nodiscard]] Action pickRecorded(const std::vector<Action> &actions, const Action &fallback, Apply apply);
    [[nodiscard]] bool drawPileMatchesGame() const override { return true; }
    void copyGameCardRandomRng(const sts::BattleContext &bc);
    void noteCurseTransform(const sts::GameContext &gc, const sts::search::GameAction &a);
    void addRngDiff(Diffs &out, const std::string &name, const sts::Random &sim) const;
    [[nodiscard]] Diffs battleDiffs(const sts::BattleContext &bc) const;

    void alignOutOfCombat(const sts::GameContext &gc);
    void adoptCourierCard();
    void compareOutOfCombat(const sts::GameContext &gc);
    [[nodiscard]] Diffs outOfCombatDiffs(const sts::GameContext &gc) const;
    void waitForEffects();
    void finishRun(const sts::GameContext &gc);
};

// Gets the game back to the main menu from anywhere, by losing the run as quickly as possible.
void returnToMainMenu(CommClient &game);

}  // namespace realgame
