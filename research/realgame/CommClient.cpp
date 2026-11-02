#include "CommClient.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

namespace realgame {

namespace {

// Some commands change nothing the mod's listener notices (it then never reports a new state). After this long
// without an answer the client asks the mod to report anyway, instead of hanging until the timeout.
constexpr double kStallSec = 4.0;
constexpr int kNudgeFrames = 30;

double secondsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

CommClient::CommClient(const std::string &socketPath) {
    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) throw GameUnavailable("socket() failed");
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socketPath.size() >= sizeof(addr.sun_path)) throw GameUnavailable("socket path too long: " + socketPath);
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd_, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        close(fd_);
        fd_ = -1;
        throw GameUnavailable("cannot connect to " + socketPath + " (is the game running with commrelay.py?)");
    }
}

CommClient::~CommClient() {
    if (fd_ >= 0) close(fd_);
}

void CommClient::sendLine(const std::string &line) {
    const std::string out = line + "\n";
    size_t sent = 0;
    while (sent < out.size()) {
        const ssize_t n = write(fd_, out.data() + sent, out.size() - sent);
        if (n <= 0) throw GameUnavailable("relay connection closed while sending: " + line);
        sent += static_cast<size_t>(n);
    }
}

bool CommClient::readLine(std::string &out, double timeoutSec) {
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        const auto nl = buffer_.find('\n');
        if (nl != std::string::npos) {
            out = buffer_.substr(0, nl);
            buffer_.erase(0, nl + 1);
            return true;
        }
        const double left = timeoutSec - secondsSince(t0);
        if (left <= 0) return false;
        pollfd pfd{fd_, POLLIN, 0};
        const int ready = poll(&pfd, 1, static_cast<int>(left * 1000) + 1);
        if (ready == 0) return false;
        if (ready < 0) throw GameUnavailable("poll() failed on relay connection");
        char chunk[65536];
        const ssize_t n = read(fd_, chunk, sizeof(chunk));
        if (n <= 0) throw GameUnavailable("relay connection closed (game exited?)");
        buffer_.append(chunk, static_cast<size_t>(n));
    }
}

const json &CommClient::command(const std::string &cmd, double timeoutSec) {
    // A replayed or late message from before this command must not be read as its answer.
    std::string line;
    while (readLine(line, 0.0)) last_ = json::parse(line);

    sendLine(cmd);
    ++commandsSent_;

    const auto t0 = std::chrono::steady_clock::now();
    bool nudged = false;
    for (;;) {
        if (readLine(line, kStallSec)) {
            json msg = json::parse(line);
            if (msg.contains("error")) {
                throw CommandRejected(msg["error"].get<std::string>() + " (command: " + cmd + ")");
            }
            last_ = std::move(msg);
            if (last_.value("ready_for_command", false)) return last_;
            continue;
        }
        if (secondsSince(t0) > timeoutSec) {
            throw GameUnavailable("no stable state within " + std::to_string(timeoutSec) + "s of: " + cmd);
        }
        if (!nudged) {
            nudged = true;
            sendLine("WAIT " + std::to_string(kNudgeFrames));
        }
    }
}

const json &CommClient::refresh() {
    std::string line;
    while (readLine(line, 0.0)) last_ = json::parse(line);
    sendLine("STATE");
    if (!readLine(line, 10.0)) throw GameUnavailable("no answer to STATE");
    last_ = json::parse(line);
    return last_;
}

}  // namespace realgame
