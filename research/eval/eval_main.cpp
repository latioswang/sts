// FROZEN evaluation harness. Do not edit during the autoresearch loop.
//
// usage: fair_eval --agent fair|scum --seed_start N --count K --asc A
//                  --threads T [--param key=value ...] [--leaktest]
// Prints one TSV row per seed:
//   seed  outcome(1 win/0 loss)  floor  act  hp  max_hp  deck  battles  decisions  aborted  seconds  actions
// (actions = game actions executed inside search; diagnostic compute proxy, not used in scoring)
#include "FairAgent.h"
#include "../vf/sv_mlp.h"
#include "bf_mlp.h"
#include "vr_mlp.h"

#include "combat/BattleContext.h"
#include "game/GameContext.h"
#include "sim/search/ScumSearchAgent2.h"

#include <atomic>
#include <map>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sts;

struct Row {
    std::uint64_t seed; int win; int floor; int act; int hp; int maxHp; int deck;
    std::int64_t battles; std::int64_t decisions; int aborted; double secs; std::int64_t actions = 0; std::string keys;
    int lastEnc = -1; int potionsLeft = 0; std::string legacy;
};

static Row runOne(const std::string &agent, std::uint64_t seed, int asc, const fair::Params &P) {
    auto t0 = std::chrono::steady_clock::now();
    GameContext gc(CharacterClass::IRONCLAD, seed, asc);
    Row r{seed, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (agent == "scum") {
        search::ScumSearchAgent2 a;
        a.simulationCountBase = static_cast<int>(fair::getp(P, "sims", 2000));
        a.rng = std::default_random_engine(static_cast<unsigned>(seed * 7919));
        a.playout(gc);
    } else {
        // agent seed derived from run seed via an independent hash stream; it
        // carries no information about game RNG state.
        fair::FairAgent a(P, seed * 0x9E3779B97F4A7C15ULL + 12345);
        a.playout(gc);
        r.battles = a.stats.battles; r.decisions = a.stats.combatDecisions; r.aborted = a.stats.aborted; r.actions = a.stats.actions;
        r.lastEnc = a.stats.lastEncounter;
        static const char *kScreens[10] = {"INVALID", "EVENT", "REWARDS", "BOSS_RELIC", "CARD_SELECT", "MAP", "TREASURE", "REST", "SHOP", "BATTLE"};
        for (int i = 0; i < 10; ++i)
            if (a.stats.legacyCalls[i]) r.legacy += std::string(kScreens[i]) + ":" + std::to_string(a.stats.legacyCalls[i]) + ",";
    }
    r.potionsLeft = gc.potionCount;
    r.win = gc.outcome == GameOutcome::PLAYER_VICTORY;
    r.keys = std::string(gc.hasKey(Key::RUBY_KEY) ? "R" : "-") + (gc.hasKey(Key::EMERALD_KEY) ? "G" : "-") + (gc.hasKey(Key::SAPPHIRE_KEY) ? "B" : "-");
    r.floor = gc.floorNum; r.act = gc.act; r.hp = gc.curHp; r.maxHp = gc.maxHp;
    r.deck = gc.deck.size();
    r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

// Leak test: for battle states encountered in real play, the chosen action
// must not change when hidden state is re-randomised (search seed fixed).
static int leakTest(std::uint64_t seed, int asc, const fair::Params &P, int &checked, bool giveDome) {
    GameContext gc(CharacterClass::IRONCLAD, seed, asc);
    fair::FairAgent agent(P, 777);
    search::ScumSearchAgent2 legacy;
    int mismatches = 0;
    std::mt19937_64 perturb(seed ^ 0x5555);
    int battles = 0;
    while (gc.outcome == GameOutcome::UNDECIDED && battles < 4) {
        if (gc.screenState != ScreenState::BATTLE) { legacy.stepOutOfCombatPolicy(gc); continue; }
        ++battles;
        BattleContext bc; bc.init(gc);
        if (giveDome) bc.player.setHasRelic<RelicId::RUNIC_DOME>(true);  // intents must then be invisible to the search
        int steps = 0;
        while (bc.outcome == Outcome::UNDECIDED && steps < 400) {
            ++steps;
            auto sp = agent.searchParamsFor(bc);
            sp.simulations = std::min(sp.simulations, 300);
            fair::FairSearcher s1(bc, sp, 4242);
            auto a1 = s1.search();
            BattleContext bc2 = bc;
            fair::determinise(bc2, perturb);   // different hidden state, same observation
            bc2.seed = perturb();              // seed must not matter either
            fair::FairSearcher s2(bc2, sp, 4242);
            auto a2 = s2.search();
            ++checked;
            if (a1.bits != a2.bits) ++mismatches;
            a1.execute(bc);
        }
        bc.exitBattle(gc);
    }
    return mismatches;
}

int main(int argc, char **argv) {
    std::string agent = "fair";
    std::uint64_t seedStart = 1000; int count = 10; int asc = 0; int threads = 4; bool leak = false; bool inproc = false; bool leakDome = false;
    int branchK = 0, branchM = 0;
    fair::Params P;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(argv[++i]); };
        if (a == "--agent") agent = next();
        else if (a == "--seed_start") seedStart = std::stoull(next());
        else if (a == "--count") count = std::stoi(next());
        else if (a == "--asc") asc = std::stoi(next());
        else if (a == "--threads") threads = std::stoi(next());
        else if (a == "--leaktest") leak = true;
        else if (a == "--leak_dome") { leak = true; leakDome = true; }
        else if (a == "--inproc") inproc = true;  // debug: no fork
        else if (a == "--branch_eval") { branchK = std::stoi(next()); branchM = std::stoi(next()); }  // Phase F26 (offline measurement)
        else if (a == "--param") {
            auto kv = next(); auto eq = kv.find('=');
            P[kv.substr(0, eq)] = std::stod(kv.substr(eq + 1));
        }
    }

    if (leak) {
        int checked = 0, mism = 0;
        for (int i = 0; i < count; ++i) mism += leakTest(seedStart + i, asc, P, checked, leakDome);
        std::printf("LEAKTEST checked=%d mismatches=%d %s\n", checked, mism, mism == 0 ? "PASS" : "FAIL");
        return mism == 0 ? 0 : 1;
    }

    if (inproc) {
        for (int i = 0; i < count; ++i) {
            Row r = runOne(agent, seedStart + i, asc, P);
            std::printf("%llu\t%d\t%d\t%d\t%lld\t%.2f\tactions=%lld\tper_decision=%.0f\tact=%d\tkeys=%s\tlegacy=%s\tdeath_enc=%d\tpotions_left=%d\n",
                        (unsigned long long)r.seed, r.win, r.floor, r.hp, (long long)r.decisions, r.secs, (long long)r.actions,
                        r.decisions ? (double)r.actions / r.decisions : 0.0, r.act, r.keys.c_str(), r.legacy.c_str(), r.lastEnc, r.potionsLeft);
        }
        return 0;
    }
    // Warm the deck-value model cache in the parent so forked games inherit it instead of each
    // re-parsing the ~1 MB text file (pure cost; identical results).
    { auto it = P.find("bf_model"); fair::bf::model(static_cast<int>(it == P.end() ? 0 : it->second)); }
    { auto it = P.find("leaf_sv"); if (it != P.end() && it->second > 0) fair::sv::model(static_cast<int>(it->second)); }  // Phase S: preload before the fork
    { auto it = P.find("vr_model"); if (it != P.end() && it->second > 0) fair::vr::model(static_cast<int>(it->second)); }  // Phase R: preload before the fork
    // Process pool: each game runs in a forked child so simulator crashes
    // (known lightspeed bugs) are recorded as aborted=2 rows instead of
    // killing the whole eval. Paired comparisons exclude crashed seeds.
    std::map<pid_t, std::pair<std::uint64_t,int>> running;  // pid -> (seed, read fd)
    int nextIdx = 0;
    auto reap = [&](bool block) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, block ? 0 : WNOHANG);
        if (pid <= 0) return false;
        auto it = running.find(pid);
        if (it == running.end()) return true;
        char buf[512]; std::string out;
        ssize_t k;
        while ((k = read(it->second.second, buf, sizeof buf)) > 0) out.append(buf, k);
        close(it->second.second);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && !out.empty()) {
            std::fputs(out.c_str(), stdout);
        } else {
            std::printf("%llu\t0\t0\t0\t0\t0\t0\t0\t0\t2\t0.00\t0\t-1\t0\n", (unsigned long long)it->second.first);
        }
        std::fflush(stdout);
        running.erase(it);
        return true;
    };
    while (nextIdx < count || !running.empty()) {
        while (nextIdx < count && (int)running.size() < threads) {
            std::uint64_t seed = seedStart + nextIdx++;
            int fds[2];
            if (pipe(fds) != 0) { perror("pipe"); return 2; }
            pid_t pid = fork();
            if (pid == 0) {
                close(fds[0]);
                // silence simulator spam WITHOUT closing fd 2: a closed fd 2 would be reused by the
                // agent's data loggers and std::cerr diagnostics would then corrupt the binary logs
                if (!std::freopen("/dev/null", "w", stderr)) _exit(4);
                alarm(900);           // hang guard -> counted as crash
                if (branchM > 0) {
                    // Phase F26: one branch-evaluation decision per seed (k = seed % branchK), rows to VF_LOG_DIR/branch_<seed>.bin
                    const char *dir = std::getenv("VF_LOG_DIR");
                    std::string path = std::string(dir ? dir : ".") + "/branch_" + std::to_string(seed) + ".bin";
                    std::FILE *f = std::fopen(path.c_str(), "wb");
                    if (!f) _exit(5);
                    GameContext gc(CharacterClass::IRONCLAD, seed, asc);
                    fair::FairAgent a(P, seed * 0x9E3779B97F4A7C15ULL + 12345);
                    const int nOpt = a.branchEval(gc, static_cast<int>(seed % static_cast<std::uint64_t>(branchK)), branchM, f);
                    std::fclose(f);
                    char bl[128];
                    int bn = std::snprintf(bl, sizeof bl, "%llu\tBRANCH\t%d\n", (unsigned long long)seed, nOpt);
                    if (write(fds[1], bl, bn) != bn) _exit(3);
                    close(fds[1]);
                    _exit(0);
                }
                Row r = runOne(agent, seed, asc, P);
                char line[512];
                // columns: seed win floor act hp max_hp deck battles decisions aborted secs actions death_enc potions_left
                // (the last two are diagnostics: the encounter of the final battle, potions unused at the end)
                int len = std::snprintf(line, sizeof line, "%llu\t%d\t%d\t%d\t%d\t%d\t%d\t%lld\t%lld\t%d\t%.2f\t%lld\t%d\t%d\n",
                            (unsigned long long)r.seed, r.win, r.floor, r.act, r.hp, r.maxHp, r.deck,
                            (long long)r.battles, (long long)r.decisions, r.aborted, r.secs, (long long)r.actions,
                            r.lastEnc, r.potionsLeft);
                if (write(fds[1], line, len) != len) _exit(3);
                close(fds[1]);
                _exit(0);
            }
            close(fds[1]);
            running[pid] = {seed, fds[0]};
        }
        reap(true);
    }
    return 0;
}
