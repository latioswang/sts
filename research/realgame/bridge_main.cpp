// sts_bridge: connects the research agent to the real game through commrelay.py. See README.md.
//
//   sts_bridge mirror --research_dir DIR --seed_start N --count K [--class C] [--max_floor F] [--asc A] [--explore_events] [--params FILE] [--param k=v ...]
//   sts_bridge live --research_dir DIR --seed_start N --count K [--asc A] [--params FILE] [--param k=v ...]
//   sts_bridge selftest --research_dir DIR --seed_start N --count K    (converter round trip, no game needed)
//   sts_bridge replay --trace FILE [--trace FILE ...]    (checks recorded runs, no game needed; one JSON line per run)
#include "CommClient.h"
#include "GameIds.h"
#include "Live.h"
#include "Mirror.h"
#include "ReplayClient.h"
#include "Scenario.h"
#include "SelfTest.h"

#include "bf_mlp.h"
#include "game/Game.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>

using namespace realgame;

namespace {

struct Args {
    std::string mode;
    std::uint64_t seedStart = 1000;
    int count = 1;
    sts::CharacterClass character = sts::CharacterClass::IRONCLAD;
    int ascension = 0;
    int maxFloor = 0;
    std::string scenarioFile;
    bool exploreEvents = false;
    int neowOption = -1;
    std::string socketPath = "/tmp/sts_commrelay.sock";
    std::string researchDir;
    std::string logDir;
    std::string paramsFile;
    fair::Params params;
    std::vector<std::string> traces;
};

void addParam(fair::Params &params, const std::string &kv) {
    const auto eq = kv.find('=');
    if (eq == std::string::npos) throw std::runtime_error("bad parameter, expected key=value: " + kv);
    params[kv.substr(0, eq)] = std::stod(kv.substr(eq + 1));
}

Args parseArgs(int argc, char **argv) {
    if (argc < 2) throw std::runtime_error("usage: sts_bridge mirror|live [options]; see research/realgame/README.md");
    Args args;
    args.mode = argv[1];
    fair::Params overrides;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + a);
            return argv[++i];
        };
        if (a == "--seed_start") args.seedStart = std::stoull(next());
        else if (a == "--count") args.count = std::stoi(next());
        else if (a == "--asc") args.ascension = std::stoi(next());
        else if (a == "--class") args.character = characterFromGameId(next());
        else if (a == "--max_floor") args.maxFloor = std::stoi(next());
        else if (a == "--scenario_file") args.scenarioFile = next();
        else if (a == "--explore_events") args.exploreEvents = true;
        else if (a == "--neow_option") args.neowOption = std::stoi(next());
        else if (a == "--sock") args.socketPath = next();
        else if (a == "--research_dir") args.researchDir = next();
        else if (a == "--log_dir") args.logDir = next();
        else if (a == "--params") args.paramsFile = next();
        else if (a == "--param") addParam(overrides, next());
        else if (a == "--trace") args.traces.push_back(next());
        else throw std::runtime_error("unknown option " + a);
    }
    if (args.mode == "replay") return args;  // no agent, so no parameters or models
    if (args.researchDir.empty()) throw std::runtime_error("--research_dir is required (the research/ directory)");
    if (args.logDir.empty()) args.logDir = args.researchDir + "/realgame/logs";
    if (args.paramsFile.empty()) args.paramsFile = args.researchDir + "/best_params.txt";

    std::ifstream file(args.paramsFile);
    if (!file) throw std::runtime_error("cannot read params file " + args.paramsFile);
    for (std::string line; std::getline(file, line);) {
        if (!line.empty()) addParam(args.params, line);
    }
    for (const auto &[k, v] : overrides) args.params[k] = v;
    if (fair::getp(args.params, "oc_potions", 0) > 0) {
        // FairAgent drinks these directly, past the action observer, so the bridge would never send them to the game.
        throw std::runtime_error("oc_potions=1 is not supported by the bridge");
    }
    return args;
}

// The model path otherwise falls back to the cloud location, and a missing model silently plays as "untrained".
void loadModelsOrDie(const Args &args) {
    setenv("BF_MODEL_DIR", (args.researchDir + "/models").c_str(), 1);
    setenv("SV_MODEL_DIR", (args.researchDir + "/models").c_str(), 0);
    const int model = static_cast<int>(fair::getp(args.params, "bf_model", 0));
    if (model > 0 && !fair::bf::model(model).trained) {
        throw std::runtime_error("bf_model=" + std::to_string(model) + " did not load from " + args.researchDir + "/models");
    }
}

json diffsToJson(const Diffs &diffs) {
    json list = json::array();
    for (const auto &d : diffs) list.push_back({{"field", d.field}, {"sim", d.simulator}, {"game", d.game}, {"hard", d.hard}});
    return list;
}

int runReplay(const Args &args) {
    setNoGameRunning();
    for (const std::string &path : args.traces) {
        json row{{"file", path}};
        try {
            ReplayClient game(path);
            const TraceHeader &header = game.header();
            row["trace"] = header.trace;
            MirrorOptions options;
            options.seed = header.seed;
            options.ascension = header.ascension;
            options.character = characterFromGameId(header.character);
            options.scenario = header.scenario;
            options.noteCard = header.noteCard;
            options.noteUpgrades = header.noteUpgrades;
            options.replay = &game;
            Mirror mirror(game, options);
            const MirrorResult r = mirror.run();
            row["seed"] = r.seed;
            row["seed_text"] = r.seedText;
            row["asc"] = header.ascension;
            row["scenario_source"] = header.scenarioSource;
            row["status"] = r.status;
            row["floor"] = r.floor;
            row["act"] = r.act;
            row["step"] = r.step;
            row["where"] = r.where;
            row["last_action"] = r.lastAction;
            row["message"] = r.message;
            row["seconds"] = r.seconds;
            row["diffs"] = diffsToJson(r.diffs);
            row["commands_replayed"] = game.commandsReplayed();
            row["commands_total"] = game.commandsTotal();
        } catch (const std::exception &e) {
            row["status"] = "replay_error";
            row["message"] = e.what();
        }
        std::cout << row.dump(-1, ' ', false, json::error_handler_t::replace) << "\n" << std::flush;
    }
    return 0;
}

int runMirror(const Args &args) {
    CommClient game(args.socketPath);
    game.refresh();
    const std::string resultsPath = args.logDir + "/mirror_results.jsonl";
    std::ofstream results(resultsPath, std::ios::app);
    int diverged = 0;
    const auto scenarios = args.scenarioFile.empty() ? std::map<std::uint64_t, std::string>{} : loadScenarioFile(args.scenarioFile);
    for (int i = 0; i < args.count; ++i) {
        if (game.last().value("in_game", false)) {
            std::cerr << "leaving the current run first\n";
            returnToMainMenu(game);
        }
        MirrorOptions options;
        options.seed = args.seedStart + i;
        options.character = args.character;
        options.ascension = args.ascension;
        options.maxFloor = args.maxFloor;
        options.exploreEvents = args.exploreEvents;
        options.neowOption = args.neowOption;
        options.params = args.params;
        options.logDir = args.logDir;
        if (const auto it = scenarios.find(options.seed); it != scenarios.end()) options.scenario = it->second;
        Mirror mirror(game, options);
        const MirrorResult r = mirror.run();

        json row{{"seed", r.seed}, {"seed_text", r.seedText}, {"status", r.status}, {"floor", r.floor}, {"act", r.act},
                 {"step", r.step}, {"commands", r.commands}, {"where", r.where}, {"last_action", r.lastAction},
                 {"message", r.message}, {"seconds", r.seconds}};
        row["diffs"] = diffsToJson(r.diffs);
        results << row.dump() << "\n";
        results.flush();
        std::cout << r.seed << " (" << r.seedText << ") " << r.status << " floor " << r.floor << " step " << r.step
                  << " at " << r.where << " after [" << r.lastAction << "] " << r.seconds << "s\n";
        if (!r.message.empty()) std::cout << "  " << r.message << "\n";
        std::cout << describe(r.diffs) << std::flush;
        if (r.status != "completed" && r.status != "reached_limit") ++diverged;
    }
    std::cout << "seeds with a stop: " << diverged << " of " << args.count << "\n";
    return 0;
}

int runLive(const Args &args) {
    CommClient game(args.socketPath);
    game.refresh();
    std::ofstream results(args.logDir + "/live_results.jsonl", std::ios::app);
    for (int i = 0; i < args.count; ++i) {
        if (game.last().value("in_game", false)) {
            std::cerr << "leaving the current run first\n";
            returnToMainMenu(game);
        }
        LiveOptions options;
        options.seed = args.seedStart + i;
        options.ascension = args.ascension;
        options.params = args.params;
        options.logDir = args.logDir;
        Live live(game, options);
        const LiveResult r = live.run();
        json row{{"seed", r.seed}, {"seed_text", r.seedText}, {"status", r.status}, {"victory", r.victory}, {"score", r.score},
                 {"floor", r.floor}, {"act", r.act}, {"decisions", r.decisions}, {"fallbacks", r.fallbacks}, {"seconds", r.seconds},
                 {"message", r.message}};
        results << row.dump() << "\n";
        results.flush();
        std::cout << r.seed << " (" << r.seedText << ") " << r.status << (r.victory ? " WIN" : " loss") << " floor " << r.floor
                  << " score " << r.score << " decisions " << r.decisions << " fallbacks " << r.fallbacks << " " << r.seconds << "s\n"
                  << std::flush;
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    try {
        const Args args = parseArgs(argc, argv);
        if (args.mode == "replay") return runReplay(args);
        mkdir(args.logDir.c_str(), 0755);
        loadModelsOrDie(args);
        if (args.mode == "mirror") return runMirror(args);
        if (args.mode == "live") return runLive(args);
        if (args.mode == "selftest") return runSelfTest(args.params, args.seedStart, args.count, args.ascension);
        throw std::runtime_error("unknown mode " + args.mode);
    } catch (const std::exception &e) {
        std::cerr << "sts_bridge: " << e.what() << "\n";
        return 1;
    }
}
