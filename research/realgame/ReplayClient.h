// Stands in for the game by playing back a recorded run, so old runs can be checked again with no game running.
//
// The recording holds what was sent and what the game answered. Mirror mode runs unchanged on top of it, except that
// the moves come from the recording rather than the agent. A recording is made by tools/export_traces.py.
#pragma once

#include "CommClient.h"
#include "Scenario.h"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace realgame {

// The recording can't answer. status: trace_end | trace_mismatch | trace_stuck | no_rng. None of these says the
// simulator is wrong, only that nothing past this point was checked.
struct RecordingStop : std::runtime_error {
    RecordingStop(std::string status_, const std::string &message) : std::runtime_error(message), status(std::move(status_)) {}
    std::string status;
};

struct TraceHeader {
    std::string trace;  // <transcript>#<run>
    std::uint64_t seed = 0;
    int ascension = 0;
    std::string scenario;
    std::string scenarioSource;  // none | mirror_log | plan | rebuilt, which may be missing items
    std::string noteCard;  // the game's id for the card Note For Yourself held, if the run took it
    int noteUpgrades = 0;
};

class ReplayClient : public CommClient {
public:
    // Reads a recording, gzipped or not. Throws std::runtime_error if it can't.
    explicit ReplayClient(const std::string &path);

    // A wait only lets time pass, so one the recording doesn't have changes nothing, and a recorded one that isn't asked
    // for is passed over. Any other command must be the next one recorded. Throws RecordingStop if it isn't.
    const json &command(const std::string &cmd, double timeoutSec = kDefaultTimeoutSec) override;
    const json &refresh() override;
    void note(const std::string &) override {}

    [[nodiscard]] const TraceHeader &header() const { return header_; }
    // The game's random streams at this point. Empty if the recording doesn't have them, as older ones don't.
    [[nodiscard]] std::map<std::string, RngState> recordedRngs() const;
    // Commands other than WAIT and STATE.
    [[nodiscard]] int commandsReplayed() const { return commandsReplayed_; }
    [[nodiscard]] int commandsTotal() const { return commandsTotal_; }
    // The next command other than WAIT and STATE, or nothing if there is none.
    [[nodiscard]] std::string nextCommand() const;
    // The recorded run sent nothing more that changes the game. Waits may be left, from a stop on a difference.
    [[nodiscard]] bool exhausted() const { return nextCommand().empty(); }

    // Lets a move be tried and taken back.
    struct Mark {
        std::size_t next = 0;
        std::size_t lastEvent = 0;
        int replayed = 0;
        int sent = 0;
    };
    [[nodiscard]] Mark mark() const { return {next_, lastEvent_, commandsReplayed_, commandsSent_}; }
    void restore(const Mark &m);

private:
    enum class Kind { Client, Game, Note };
    struct Event {
        Kind kind;
        std::string_view text;
    };
    static constexpr std::size_t kNoEvent = static_cast<std::size_t>(-1);

    std::string data_;  // the file, which events_ point into
    std::vector<Event> events_;
    TraceHeader header_;
    std::size_t next_ = 0;
    std::size_t lastEvent_ = kNoEvent;
    int commandsReplayed_ = 0;
    int commandsTotal_ = 0;

    [[nodiscard]] std::size_t nextClient(std::size_t from) const;
    void answer(std::size_t client);
    void adopt(std::size_t event);
};

}  // namespace realgame
