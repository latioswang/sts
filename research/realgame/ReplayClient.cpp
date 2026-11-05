#include "ReplayClient.h"

#include <array>
#include <cstdio>
#include <memory>

namespace realgame {

namespace {

constexpr std::size_t kReadChunk = 1 << 20;

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool isWait(std::string_view cmd) {
    return startsWith(cmd, "WAIT");
}

// Commands that only let time pass or ask for the state.
[[nodiscard]] bool isFiller(std::string_view cmd) {
    return isWait(cmd) || cmd == "STATE";
}

[[nodiscard]] bool isGzipped(const std::string &path) {
    return path.size() > 3 && path.compare(path.size() - 3, 3, ".gz") == 0;
}

[[nodiscard]] std::string shellQuoted(const std::string &text) {
    std::string out = "'";
    for (const char c : text) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

[[nodiscard]] std::string readAll(const std::string &path) {
    using Closer = int (*)(std::FILE *);
    const bool gz = isGzipped(path);
    std::unique_ptr<std::FILE, Closer> file(gz ? popen(("gzip -dc " + shellQuoted(path)).c_str(), "r") : std::fopen(path.c_str(), "r"),
                                            gz ? pclose : std::fclose);
    if (!file) throw std::runtime_error("cannot read recording " + path);
    std::string data;
    std::array<char, kReadChunk> chunk{};
    for (std::size_t n = 0; (n = std::fread(chunk.data(), 1, chunk.size(), file.get())) > 0;) data.append(chunk.data(), n);
    if (data.empty()) throw std::runtime_error("empty recording " + path);
    return data;
}

[[nodiscard]] json parseText(std::string_view text) {
    return json::parse(text.begin(), text.end());
}

}  // namespace

ReplayClient::ReplayClient(const std::string &path) : data_(readAll(path)) {
    const std::string_view all(data_);
    std::size_t start = 0;
    bool headerRead = false;
    while (start < all.size()) {
        std::size_t end = all.find('\n', start);
        if (end == std::string_view::npos) end = all.size();
        const std::string_view line = all.substr(start, end - start);
        start = end + 1;
        if (!headerRead) {
            const json h = parseText(line);
            header_.trace = h.at("trace").get<std::string>();
            header_.seed = h.at("seed").get<std::uint64_t>();
            header_.character = h.value("class", "IRONCLAD");
            header_.ascension = h.at("asc").get<int>();
            header_.scenario = h.value("scenario", "");
            header_.scenarioSource = h.value("scenario_source", "");
            header_.noteCard = h.value("note_card", "");
            header_.noteUpgrades = h.value("note_upgrades", 0);
            headerRead = true;
            continue;
        }
        if (line.size() < 2) continue;
        const std::string_view text = line.substr(2);
        switch (line[0]) {
            case 'C':
                events_.push_back({Kind::Client, text});
                if (!isFiller(text)) ++commandsTotal_;
                break;
            case 'G': events_.push_back({Kind::Game, text}); break;
            case 'N': events_.push_back({Kind::Note, text}); break;
            default: throw std::runtime_error("bad line in recording " + path + ": " + std::string(line.substr(0, 40)));
        }
    }
}

std::size_t ReplayClient::nextClient(std::size_t from) const {
    while (from < events_.size() && events_[from].kind != Kind::Client) ++from;
    return from;
}

void ReplayClient::adopt(std::size_t event) {
    last_ = parseText(events_[event].text);
    lastEvent_ = event;
}

// The game's first ready message after the command, as CommClient takes it. CommClient nudges a command the game is slow
// to answer with a WAIT of its own, so the answer can come after that.
void ReplayClient::answer(std::size_t client) {
    next_ = client + 1;
    for (std::size_t i = next_; i < events_.size(); ++i) {
        if (events_[i].kind == Kind::Client) {
            if (isWait(events_[i].text)) continue;
            return;
        }
        if (events_[i].kind != Kind::Game) continue;
        json msg = parseText(events_[i].text);
        if (msg.contains("error")) {
            throw CommandRejected(msg["error"].get<std::string>() + " (command: " + std::string(events_[client].text) + ")");
        }
        last_ = std::move(msg);
        lastEvent_ = i;
        if (last_.value("ready_for_command", false)) {
            next_ = i + 1;
            return;
        }
    }
}

const json &ReplayClient::command(const std::string &cmd, double) {
    ++commandsSent_;
    std::size_t i = nextClient(next_);
    if (isWait(cmd)) {
        if (i < events_.size() && isWait(events_[i].text)) answer(i);
        return last_;
    }
    while (i < events_.size() && isFiller(events_[i].text)) i = nextClient(i + 1);
    if (i >= events_.size()) throw RecordingStop("trace_end", "the recording ends before " + cmd);
    if (events_[i].text != cmd) {
        throw RecordingStop("trace_mismatch", "sent " + cmd + ", the recording has " + std::string(events_[i].text));
    }
    answer(i);
    if (last_.is_null()) throw RecordingStop("trace_end", "the recording has no answer to " + cmd);
    ++commandsReplayed_;
    for (int wait = 0; wait < kMaxIntentWaits && intentsPending(last_); ++wait) {
        const std::size_t w = nextClient(next_);
        if (w >= events_.size() || !isWait(events_[w].text)) break;
        answer(w);
    }
    return last_;
}

const json &ReplayClient::refresh() {
    const std::size_t i = nextClient(next_);
    if (i >= events_.size() || events_[i].text != "STATE") return last_;
    next_ = i + 1;
    for (std::size_t g = next_; g < events_.size() && events_[g].kind != Kind::Client; ++g) {
        if (events_[g].kind == Kind::Game) adopt(g);
    }
    return last_;
}

std::string ReplayClient::nextCommand() const {
    std::size_t i = nextClient(next_);
    while (i < events_.size() && isFiller(events_[i].text)) i = nextClient(i + 1);
    return i < events_.size() ? std::string(events_[i].text) : std::string();
}

std::map<std::string, RngState> ReplayClient::recordedRngs() const {
    static constexpr std::string_view kPrefix = "rng ";
    for (std::size_t i = next_; i < events_.size(); ++i) {
        const Event &e = events_[i];
        if (e.kind == Kind::Note && startsWith(e.text, kPrefix)) return parseGameRngs(std::string(e.text.substr(kPrefix.size())));
        if (e.kind == Kind::Client && !isFiller(e.text)) break;
    }
    return {};
}

void ReplayClient::restore(const Mark &m) {
    next_ = m.next;
    commandsReplayed_ = m.replayed;
    commandsSent_ = m.sent;
    if (m.lastEvent == lastEvent_) return;
    if (m.lastEvent == kNoEvent) {
        last_ = json();
        lastEvent_ = kNoEvent;
    } else {
        adopt(m.lastEvent);
    }
}

}  // namespace realgame
