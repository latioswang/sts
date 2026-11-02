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

class CommClient {
public:
    explicit CommClient(const std::string &socketPath);
    ~CommClient();
    CommClient(const CommClient &) = delete;
    CommClient &operator=(const CommClient &) = delete;

    // Sends `cmd` and returns the next stable state (ready_for_command). Throws CommandRejected on a mod error.
    const json &command(const std::string &cmd, double timeoutSec = kDefaultTimeoutSec);

    // Asks for the current state without changing anything. Returns the latest message.
    const json &refresh();

    [[nodiscard]] const json &last() const { return last_; }
    [[nodiscard]] int commandsSent() const { return commandsSent_; }

    static constexpr double kDefaultTimeoutSec = 90.0;

private:
    int fd_ = -1;
    std::string buffer_;
    json last_;
    int commandsSent_ = 0;

    void sendLine(const std::string &line);
    bool readLine(std::string &out, double timeoutSec);
    // Takes in whatever has already arrived, until nothing more comes for quietSec.
    void drain(double quietSec);
};

}  // namespace realgame
