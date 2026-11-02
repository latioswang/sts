// sts_bridge: connects the research agent to the real game through commrelay.py. See README.md.
//
//   sts_bridge mirror --seed_start N --count K [--max_floor F] [--asc A] [--params FILE] [--param k=v ...]
#include "CommClient.h"
#include "Mirror.h"

#include "bf_mlp.h"
#include "game/Game.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

using namespace realgame;

namespace {

struct Args {
    std::string mode;
    std::uint64_t seedStart = 1000;
    int count = 1;
    int ascension = 0;
    int maxFloor = 0;
    std::string socketPath = "/tmp/sts_commrelay.sock";
    std::string researchDir;
    std::string logDir;
    std::string paramsFile;
    fair::Params params;
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
        else if (a == "--max_floor") args.maxFloor = std::stoi(next());
        else if (a == "--sock") args.socketPath = next();
        else if (a == "--research_dir") args.researchDir = next();
        else if (a == "--log_dir") args.logDir = next();
        else if (a == "--params") args.paramsFile = next();
        else if (a == "--param") addParam(overrides, next());
        else throw std::runtime_error("unknown option " + a);
    }
    if (args.researchDir.empty()) throw std::runtime_error("--research_dir is required (the research/ directory)");
    if (args.logDir.empty()) args.logDir = args.researchDir + "/realgame/logs";
    if (args.paramsFile.empty()) args.paramsFile = args.researchDir + "/best_params.txt";

    std::ifstream file(args.paramsFile);
    if (!file) throw std::runtime_error("cannot read params file " + args.paramsFile);
    for (std::string line; std::getline(file, line);) {
        if (!line.empty()) addParam(args.params, line);
    }
    for (const auto &[k, v] : overrides) args.params[k] = v;
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

int runMirror(const Args &args) {
    CommClient game(args.socketPath);
    game.refresh();
    const std::string resultsPath = args.logDir + "/mirror_results.jsonl";
    std::ofstream results(resultsPath, std::ios::app);
    int diverged = 0;
    for (int i = 0; i < args.count; ++i) {
        if (game.last().value("in_game", false)) {
            std::cerr << "leaving the current run first\n";
            returnToMainMenu(game);
        }
        MirrorOptions options;
        options.seed = args.seedStart + i;
        options.ascension = args.ascension;
        options.maxFloor = args.maxFloor;
        options.params = args.params;
        options.logDir = args.logDir;
        Mirror mirror(game, options);
        const MirrorResult r = mirror.run();

        json row{{"seed", r.seed}, {"seed_text", r.seedText}, {"status", r.status}, {"floor", r.floor}, {"act", r.act},
                 {"step", r.step}, {"commands", r.commands}, {"where", r.where}, {"last_action", r.lastAction},
                 {"message", r.message}, {"seconds", r.seconds}};
        json list = json::array();
        for (const auto &d : r.diffs) list.push_back({{"field", d.field}, {"sim", d.simulator}, {"game", d.game}, {"hard", d.hard}});
        row["diffs"] = list;
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

}  // namespace

int main(int argc, char **argv) {
    try {
        const Args args = parseArgs(argc, argv);
        mkdir(args.logDir.c_str(), 0755);
        loadModelsOrDie(args);
        if (args.mode == "mirror") return runMirror(args);
        throw std::runtime_error("unknown mode " + args.mode);
    } catch (const std::exception &e) {
        std::cerr << "sts_bridge: " << e.what() << "\n";
        return 1;
    }
}
