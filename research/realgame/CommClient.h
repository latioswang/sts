// Client side of commrelay.py: one Communication Mod message per line over a Unix socket.
#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace realgame {

using json = nlohmann::json;

// The mod rejected a command. `what()` carries the mod's message and the command.
struct CommandRejected : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The connection died or the game stopped answering.
struct GameUnavailable : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The first turn becomes playable a few frames before monsters show their intents. Until then the mod reports the
// placeholder intent DEBUG and a damage of -1.
[[nodiscard]] bool intentsPending(const json &msg);

// Runic Dome hides intents from the mod, the placeholder too, so intentsPending can't see them come. On the first turn of
// a fight with the relic held this names the fight, and its start waits a fixed time instead. Empty otherwise.
[[nodiscard]] std::string fightWithHiddenIntents(const json &msg);

class CommClient {
public:
    explicit CommClient(const std::string &socketPath);
    virtual ~CommClient();
    CommClient(const CommClient &) = delete;
    CommClient &operator=(const CommClient &) = delete;

    // Sends `cmd` and returns the next stable state (ready_for_command). Throws CommandRejected on a mod error.
    virtual const json &command(const std::string &cmd, double timeoutSec = kDefaultTimeoutSec);

    // Asks for the current state without changing anything. Returns the latest message.
    virtual const json &refresh();

    // Writes a line to the relay's transcript. The game never sees it, and no answer comes back.
    virtual void note(const std::string &text);

    [[nodiscard]] const json &last() const { return last_; }
    [[nodiscard]] int commandsSent() const { return commandsSent_; }

    static constexpr double kDefaultTimeoutSec = 90.0;
    static constexpr int kMaxIntentWaits = 20;
    static constexpr int kIntentWaitFrames = 5;
    static constexpr int kHiddenIntentWaitFrames = 60;  // the placeholder has lasted up to 45 frames

protected:
    CommClient() = default;  // for a stand-in that has no socket

    json last_;
    int commandsSent_ = 0;

private:
    int fd_ = -1;
    std::string buffer_;
    std::string hiddenIntentFightWaited_;  // the last fight fightWithHiddenIntents named, which has had its wait

    void sendLine(const std::string &line);
    void sendAndWait(const std::string &cmd, double timeoutSec);
    bool readLine(std::string &out, double timeoutSec);
    // Takes in whatever has already arrived, until nothing more comes for quietSec.
    void drain(double quietSec);
};

}  // namespace realgame
