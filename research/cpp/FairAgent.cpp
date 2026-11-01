#include "FairAgent.h"

#include "combat/BattleContext.h"
#include "constants/MonsterEncounters.h"
#include "sim/search/GameAction.h"
#include "sim/search/ExpertKnowledge.h"
#include "vf_features.h"
#include "run_features.h"
#include "policy_features.h"
#include "rv_mlp.h"
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <iostream>

using namespace sts;

namespace fair {

double getp(const Params &p, const std::string &k, double def) {
    auto it = p.find(k);
    return it == p.end() ? def : it->second;
}

FairAgent::FairAgent(const Params &params, std::uint64_t agentSeed_) : P(params), rng(agentSeed_), agentSeed(agentSeed_) {
    if (getp(P, "vf_log", 0) > 0) {
        if (const char *dir = std::getenv("VF_LOG_DIR")) {
            const std::string path = std::string(dir) + "/" + std::to_string(agentSeed_) + ".bin";
            vfLog = std::fopen(path.c_str(), "ab");
        }
    }
    if (getp(P, "pf_log", 0) > 0) {
        if (const char *dir = std::getenv("VF_LOG_DIR")) {
            const std::string path = std::string(dir) + "/pol_" + std::to_string(agentSeed_) + ".bin";
            pfLog = std::fopen(path.c_str(), "ab");
            if (pfLog) { const float hdr[2] = {-1.f, 2.f}; std::fwrite(hdr, sizeof(float), 2, pfLog); }  // format v2 marker
        }
    }
    if (getp(P, "rv_log", 0) > 0) {
        if (const char *dir = std::getenv("VF_LOG_DIR")) {
            const std::string path = std::string(dir) + "/run_" + std::to_string(agentSeed_) + ".bin";
            rvLog = std::fopen(path.c_str(), "ab");
        }
    }
    sp.simulations = static_cast<int>(getp(P, "sims", 500));
    sp.exploration = getp(P, "c", 0.7);
    sp.rolloutTurns = static_cast<int>(getp(P, "rollout_turns", 99));
    sp.endTurnRolloutWeight = getp(P, "end_turn_w", 1.0);
    sp.potionRolloutWeight = getp(P, "potion_roll_w", 1.0);
    sp.rolloutOrderK = getp(P, "roll_order_k", 0.0);
    sp.rollBlockW = getp(P, "roll_block_w", 1.0);
    sp.rollAtkW = getp(P, "roll_atk_w", 1.0);
    sp.hpWeight = getp(P, "hp_w", 1.0);
    sp.potionValue = getp(P, "potion_v", 4.0);
    sp.winBonus = getp(P, "win_bonus", 35.0);
    sp.maxHpWeight = getp(P, "maxhp_w", 0.0);
    sp.escapePenalty = getp(P, "escape_pen", 30.0);
    sp.budgetActions = static_cast<std::int64_t>(getp(P, "budget_actions", 0));
    sp.cutWinFrac = getp(P, "cut_win_frac", 1.0);
    sp.leafVf = getp(P, "leaf_vf", 0) > 0;
    sp.usePrior = getp(P, "prior", 0) > 0;
    sp.priorC = getp(P, "prior_c", 1.5);
    sp.priorMlp = getp(P, "prior_mlp", 0) > 0;
    sp.priorMlpDepth = static_cast<int>(getp(P, "prior_mlp_depth", 1));
    sp.priorTemp = getp(P, "prior_temp", 1.0);
    sp.selectQ = getp(P, "select_q", 0.0);
    sp.lowHpPen = getp(P, "lowhp_pen", 0.0);
    sp.lowHpThresh = getp(P, "lowhp_thresh", 20.0);
    sp.cutHpFrac = getp(P, "cut_hp_frac", 1.0);
    legacy.rng = std::default_random_engine(static_cast<unsigned>(agentSeed ^ 0xabcdefULL));
}

static bool isEliteEncounter(MonsterEncounter e) {
    using ME = MonsterEncounter;
    switch (e) {
        case ME::GREMLIN_NOB: case ME::LAGAVULIN: case ME::THREE_SENTRIES:
        case ME::GREMLIN_LEADER: case ME::SLAVERS: case ME::BOOK_OF_STABBING:
        case ME::GIANT_HEAD: case ME::NEMESIS: case ME::REPTOMANCER:
            return true;
        default: return false;
    }
}

SearchParams FairAgent::searchParamsFor(const BattleContext &bc) const {
    SearchParams s = sp;
    if (isBossEncounter(bc.encounter)) {
        s.simulations = static_cast<int>(s.simulations * getp(P, "boss_mult", 1.0));
        s.potionValue = getp(P, "potion_v_boss", s.potionValue);
    } else if (isEliteEncounter(bc.encounter)) {
        s.potionValue = getp(P, "potion_v_elite", s.potionValue);
    } else {
        // hallway fight: optionally keep potions out of the search (saves branching, keeps them for elites/bosses)
        s.allowPotions = getp(P, "hall_potions", 1) > 0 || bc.player.curHp * 4 < bc.player.maxHp;
    }
    return s;
}

void FairAgent::playBattle(GameContext &gc) {
    BattleContext bc;
    bc.init(gc);
    ++stats.battles;
    if (getp(P, "cheat_combat", 0) > 0) {
        // DIAGNOSTIC ONLY (never a result): lightspeed's RNG-aware scum search.
        legacy.simulationCountBase = sp.simulations;
        legacy.playoutBattle(bc);
        bc.exitBattle(gc);
        return;
    }
    int steps = 0;
    const bool reuse = getp(P, "tree_reuse", 0) > 0;
    const bool trackTop = getp(P, "known_top", 1) > 0;
    int knownTop = 0;  // cards on top of the draw pile whose identity the player knows
    std::unique_ptr<FairSearcher> searcher;
    while (bc.outcome == Outcome::UNDECIDED) {
        if (++steps > 3000) {  // safety: pathological loop
            stats.aborted = true;
            bc.outcome = Outcome::PLAYER_LOSS;
            break;
        }
        auto spNow = searchParamsFor(bc);
        spNow.knownTop = knownTop;
        if (vfLog && bc.inputState == InputState::PLAYER_NORMAL) {  // one row per decision point
            const size_t off = vfRows.size();
            vfRows.resize(off + vf::DIM);
            vf::extract(bc, vfRows.data() + off);
        }
        if (!reuse || !searcher) searcher = std::make_unique<FairSearcher>(bc, spNow, rng());
        FairSearcher &s = *searcher;
        s.setRoot(bc);
        auto a = s.search();
        ++stats.combatDecisions;
        stats.actions += s.actionsExecuted;
        stats.simulations += s.simulationsRun;
        if (pfLog && bc.inputState == InputState::PLAYER_NORMAL) {
            std::vector<FairSearcher::EdgeStat> rs; double rootMean = 0.0;
            s.rootEdgeStats(rs, rootMean);
            if (rs.size() > 1) {  // v2: [state 204][rootMean][nA][nA x (token, up, tf0..3, visits, q)]
                float x[vf::DIM];
                vf::extract(bc, x);
                std::fwrite(x, sizeof(float), vf::DIM, pfLog);
                const float hdr[2] = {static_cast<float>(rootMean), static_cast<float>(rs.size())};
                std::fwrite(hdr, sizeof(float), 2, pfLog);
                for (auto &es : rs) {
                    bool up = false;
                    float row[8];
                    row[0] = static_cast<float>(pf::token(bc, es.action, up));
                    row[1] = up ? 1.f : 0.f;
                    pf::targetFeats(bc, es.action, row + 2);
                    row[6] = static_cast<float>(es.n);
                    row[7] = static_cast<float>(es.q);
                    std::fwrite(row, sizeof(float), 8, pfLog);
                }
            }
        }
        static const bool dbg = std::getenv("FAIR_DEBUG") != nullptr;
        if (dbg) {
            std::cerr << "turn " << bc.turn << " hp " << bc.player.curHp << " e " << bc.player.energy << " ";
            a.printDesc(std::cerr, bc) << " visits=" << s.rootVisits() << "\n";
        }
        const int pileBefore = static_cast<int>(bc.cards.drawPile.size());
        bool putsOnTop = false;
        if (a.getActionType() == search::ActionType::SINGLE_CARD_SELECT && bc.inputState == InputState::CARD_SELECT) {
            const auto task = bc.cardSelectInfo.cardSelectTask;
            putsOnTop = task == CardSelectTask::HEADBUTT || task == CardSelectTask::WARCRY;
        }
        a.execute(bc);
        if (vfLog && bc.outcome != Outcome::UNDECIDED && !vfRows.empty()) {
            // label every row of this battle with its final outcome and flush
            const float win = bc.outcome == Outcome::PLAYER_VICTORY ? 1.f : 0.f;
            const float hp = static_cast<float>(bc.player.curHp);
            const float target = static_cast<float>(terminalValue(bc, sp));
            // must stay exactly representable in float32 (< 2^24): seed-derived prefix * 100 + battle index
            const float bid = static_cast<float>((agentSeed % 100000ULL) * 100 + static_cast<std::uint64_t>(std::min<std::int64_t>(stats.battles, 99)));
            for (size_t off = 0; off < vfRows.size(); off += vf::DIM) {
                std::fwrite(vfRows.data() + off, sizeof(float), vf::DIM, vfLog);
                const float extra[4] = {win, hp, target, bid};
                std::fwrite(extra, sizeof(float), 4, vfLog);
            }
            std::fflush(vfLog);
            vfRows.clear();
        }
        if (trackTop) {
            const int pileAfter = static_cast<int>(bc.cards.drawPile.size());
            if (pileAfter < pileBefore) knownTop = std::max(0, knownTop - (pileBefore - pileAfter));
            else if (pileAfter == pileBefore + 1 && putsOnTop) knownTop += 1;
            else if (pileAfter != pileBefore) knownTop = 0;  // reshuffle or random insert: be conservative
            knownTop = std::min(knownTop, pileAfter);
        }
        if (reuse) s.advance(a, bc);
    }
    bc.exitBattle(gc);
}

// ---------------- card values from the repo KB tier list ----------------
double kbValue(CardId id) {
    switch (id) {
#include "kb_card_values.inc"
        default: return 3.0;  // colorless / unknown: treat as C-
    }
}

static double cardValue(const Card &c) {
    return kbValue(c.getId()) + (c.isUpgraded() ? 0.5 : 0.0);
}

static bool isAoe(CardId id) {
    switch (id) {
        case CardId::CLEAVE: case CardId::WHIRLWIND: case CardId::THUNDERCLAP:
        case CardId::IMMOLATE: case CardId::REAPER: return true;
        default: return false;
    }
}

// ---------------- KB synergy tables (generated from the repo KB) ----------------
struct KbCardSyn { CardId a, b; float w; };
struct KbRelicSyn { RelicId r; CardId c; float w; };
#include "kb_synergy.inc"

namespace {
inline std::uint32_t packCC(CardId a, CardId b) { return (static_cast<std::uint32_t>(a) << 16) | static_cast<std::uint32_t>(b); }
inline std::uint32_t packRC(RelicId r, CardId c) { return (static_cast<std::uint32_t>(r) << 16) | static_cast<std::uint32_t>(c); }
const std::unordered_map<std::uint32_t, float> &cardSynMap() {
    static const std::unordered_map<std::uint32_t, float> m = [] {
        std::unordered_map<std::uint32_t, float> t;
        for (const auto &e : kKbCardSyn) { t[packCC(e.a, e.b)] = e.w; t.emplace(packCC(e.b, e.a), e.w); }
        return t;
    }();
    return m;
}
const std::unordered_map<std::uint32_t, float> &relicSynMap() {
    static const std::unordered_map<std::uint32_t, float> m = [] {
        std::unordered_map<std::uint32_t, float> t;
        for (const auto &e : kKbRelicSyn) t[packRC(e.r, e.c)] = e.w;
        return t;
    }();
    return m;
}
}  // namespace

// KB synergy of candidate card c with the current deck and relics (0 when kb_syn is off)
double FairAgent::kbSynergy(const GameContext &gc, const Card &c) const {
    if (getp(P, "kb_syn", 0) <= 0) return 0.0;
    const auto &cm = cardSynMap(); const auto &rm = relicSynMap();
    double cardSum = 0.0;
    for (const auto &d : gc.deck.cards) {
        if (d.getId() == c.getId()) continue;
        auto it = cm.find(packCC(c.getId(), d.getId()));
        if (it != cm.end()) cardSum += it->second;
    }
    const double cap = getp(P, "kb_syn_cap", 3.0);
    cardSum = std::max(-cap, std::min(cap, cardSum));
    double relicSum = 0.0;
    for (const auto &r : gc.relics.relics) {
        auto it = rm.find(packRC(r.id, c.getId()));
        if (it != rm.end()) relicSum += it->second;
    }
    relicSum = std::max(-cap, std::min(cap, relicSum));
    return getp(P, "kb_syn_card_w", 0.5) * cardSum + getp(P, "kb_syn_relic_w", 0.75) * relicSum;
}

// deck-aware value of adding card c (defaults reduce to plain KB value)
double FairAgent::addValue(const GameContext &gc, const Card &c) const {
    double v = cardValue(c);
    int aoe = 0, dups = 0;
    for (const auto &d : gc.deck.cards) {
        if (isAoe(d.getId())) ++aoe;
        if (d.getId() == c.getId()) ++dups;
    }
    if (isAoe(c.getId()) && aoe == 0 && gc.act <= 2) v += getp(P, "aoe_bonus", 0.0);
    v -= getp(P, "dup_pen", 0.0) * dups;
    if (c.getType() == CardType::POWER) v += getp(P, "power_bonus", 0.0);
    v += kbSynergy(gc, c);
    auto isScaling = [](CardId id) {
        switch (id) {
            case CardId::INFLAME: case CardId::DEMON_FORM: case CardId::SPOT_WEAKNESS:
            case CardId::LIMIT_BREAK: case CardId::RUPTURE: case CardId::FEED: case CardId::BARRICADE:
            case CardId::FEEL_NO_PAIN: case CardId::METALLICIZE: case CardId::JUGGERNAUT: return true;
            default: return false;
        }
    };
    auto isBlock = [](CardId id) {
        switch (id) {
            case CardId::SHRUG_IT_OFF: case CardId::FLAME_BARRIER: case CardId::IMPERVIOUS:
            case CardId::POWER_THROUGH: case CardId::TRUE_GRIT: case CardId::GHOSTLY_ARMOR:
            case CardId::ENTRENCH: case CardId::SECOND_WIND: case CardId::IRON_WAVE: return true;
            default: return false;
        }
    };
    int scal = 0, blk = 0;
    for (const auto &d : gc.deck.cards) { scal += isScaling(d.getId()); blk += isBlock(d.getId()); }
    if (isScaling(c.getId()) && scal < getp(P, "scaling_want", 2)) v += getp(P, "scaling_bonus", 0.0);
    if (isBlock(c.getId()) && blk < getp(P, "block_want", 3)) v += getp(P, "block_bonus", 0.0);
    if (getp(P, "act4_prep", 0) > 0 && gc.act == 3 && gc.hasKey(Key::RUBY_KEY) && gc.hasKey(Key::EMERALD_KEY) && gc.hasKey(Key::SAPPHIRE_KEY)) {
        // all keys held in Act 3: the run continues into Act 4 — Shield & Spear want AoE and block, the Heart wants burst and block
        if (isAoe(c.getId())) v += getp(P, "act4_aoe_bonus", 1.5);
        if (isBlock(c.getId())) v += getp(P, "act4_block_bonus", 1.5);
        if (isScaling(c.getId())) v += getp(P, "act4_scaling_bonus", 1.0);
    }
    if (getp(P, "boss_prep", 0) > 0) {
        // the act boss is shown on the map from the first floor: prepare the deck for it (71% of losses are bosses)
        using ME = MonsterEncounter;
        const ME b = gc.boss;
        const bool wantsAoe = b == ME::SLIME_BOSS || b == ME::AUTOMATON || b == ME::COLLECTOR || b == ME::DONU_AND_DECA;
        const bool wantsBlock = b == ME::THE_GUARDIAN || b == ME::CHAMP || b == ME::AUTOMATON || b == ME::TIME_EATER;
        const bool hatesPowers = b == ME::AWAKENED_ONE;   // powers give it strength
        const bool hatesCheap = b == ME::TIME_EATER;      // 12 cards -> extra turn
        if (wantsAoe && isAoe(c.getId()) && aoe < getp(P, "boss_aoe_want", 2)) v += getp(P, "boss_aoe_bonus", 1.5);
        if (wantsBlock && isBlock(c.getId())) v += getp(P, "boss_block_bonus", 1.0);
        if (hatesPowers && c.getType() == CardType::POWER) v -= getp(P, "boss_power_pen", 2.0);
        if (hatesCheap && !isXCost(c.getId()) && getEnergyCost(c.getId(), c.isUpgraded()) == 0) v -= getp(P, "boss_cheap_pen", 1.0);
    }
    return v;
}

// ---------------- Phase C v2: counterfactual fight evaluation ----------------
namespace {
// encounter pools copied from lightspeed's MonsterEncounters.h (public knowledge for a human player)
const MonsterEncounter kStrong[3][10] = {
    {MonsterEncounter::GREMLIN_GANG, MonsterEncounter::LOTS_OF_SLIMES, MonsterEncounter::RED_SLAVER, MonsterEncounter::EXORDIUM_THUGS,
     MonsterEncounter::EXORDIUM_WILDLIFE, MonsterEncounter::BLUE_SLAVER, MonsterEncounter::LOOTER, MonsterEncounter::LARGE_SLIME,
     MonsterEncounter::THREE_LOUSE, MonsterEncounter::TWO_FUNGI_BEASTS},
    {MonsterEncounter::CHOSEN_AND_BYRDS, MonsterEncounter::SENTRY_AND_SPHERE, MonsterEncounter::CULTIST_AND_CHOSEN, MonsterEncounter::THREE_CULTIST,
     MonsterEncounter::SHELLED_PARASITE_AND_FUNGI, MonsterEncounter::SNECKO, MonsterEncounter::SNAKE_PLANT, MonsterEncounter::CENTURION_AND_HEALER,
     MonsterEncounter::INVALID, MonsterEncounter::INVALID},
    {MonsterEncounter::SPIRE_GROWTH, MonsterEncounter::TRANSIENT, MonsterEncounter::FOUR_SHAPES, MonsterEncounter::MAW,
     MonsterEncounter::SPHERE_AND_TWO_SHAPES, MonsterEncounter::JAW_WORM_HORDE, MonsterEncounter::THREE_DARKLINGS, MonsterEncounter::WRITHING_MASS,
     MonsterEncounter::INVALID, MonsterEncounter::INVALID}};
const float kStrongW[3][10] = {
    {1, 1, 1, 1.5f, 1.5f, 2, 2, 2, 2, 2}, {2, 2, 3, 3, 3, 4, 6, 6, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 0, 0}};
const MonsterEncounter kElite[3][3] = {
    {MonsterEncounter::GREMLIN_NOB, MonsterEncounter::LAGAVULIN, MonsterEncounter::THREE_SENTRIES},
    {MonsterEncounter::GREMLIN_LEADER, MonsterEncounter::SLAVERS, MonsterEncounter::BOOK_OF_STABBING},
    {MonsterEncounter::GIANT_HEAD, MonsterEncounter::NEMESIS, MonsterEncounter::REPTOMANCER}};

MonsterEncounter sampleEncounter(const GameContext &gc, std::mt19937_64 &rng, double eliteFrac, double bossLateFrac) {
    const int a = std::min(3, std::max(1, gc.act)) - 1;
    std::uniform_real_distribution<double> U(0, 1);
    if (gc.curMapNodeY >= 11 && U(rng) < bossLateFrac && gc.boss != MonsterEncounter::INVALID) return gc.boss;
    if (U(rng) < eliteFrac) return kElite[a][static_cast<int>(U(rng) * 3) % 3];
    double tot = 0; for (int i = 0; i < 10; ++i) tot += kStrongW[a][i];
    double r = U(rng) * tot;
    for (int i = 0; i < 10; ++i) { r -= kStrongW[a][i]; if (r <= 0 && kStrong[a][i] != MonsterEncounter::INVALID) return kStrong[a][i]; }
    return kStrong[a][0];
}
}  // namespace

double FairAgent::cfScore(const GameContext &hyp, std::uint64_t cfSeed) {
    // common random numbers: the caller passes the same cfSeed for every option at this decision
    std::mt19937_64 rng(cfSeed);
    const int fights = static_cast<int>(getp(P, "cf_fights", 3));
    SearchParams s = sp;
    s.simulations = static_cast<int>(getp(P, "cf_sims", 60));
    s.allowPotions = false;  // potions are a run resource; don't spend them in hypothetical fights
    double total = 0;
    for (int f = 0; f < fights; ++f) {
        const auto enc = sampleEncounter(hyp, rng, getp(P, "cf_elite_frac", 0.25), getp(P, "cf_boss_late", 0.6));
        GameContext g = hyp;
        // fresh RNG streams for the hypothetical fight (never the run's real future RNG)
        g.monsterHpRng = Random(rng()); g.aiRng = Random(rng()); g.shuffleRng = Random(rng());
        g.cardRandomRng = Random(rng()); g.miscRng = Random(rng()); g.potionRng = Random(rng());
        BattleContext bc;
        bc.init(g, enc);
        int steps = 0;
        while (bc.outcome == Outcome::UNDECIDED && ++steps < 600) {
            FairSearcher srch(bc, s, rng());
            srch.search().execute(bc);
        }
        if (bc.outcome == Outcome::PLAYER_VICTORY) total += static_cast<double>(bc.player.curHp) / std::max(1, bc.player.maxHp);
        else total -= 0.5;
    }
    return total / fights;
}

// ---------------- events (rules written from lightspeed's event code) ----------------
int FairAgent::eventChoice(const GameContext &gc) const {
    const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
    (void)hpf;
#include "event_policy.inc"
    return -1;
}

// ---------------- boss relics ----------------
static double kbBossRelicValue(RelicId id) {
    switch (id) {
#include "kb_boss_relic_values.inc"
        default: return 5.0;
    }
}

int FairAgent::bossRelicChoice(const GameContext &gc) const {
    // Runic Dome is allowed: the searcher hides intents (observation hash + move re-roll)
    // when the player holds it, so there is no hidden-info leak.
    int best = 3;  // 3 = skip
    double bestV = -1e18;
    if (getp(P, "rv_pick", 0) > 0 && rv::RV_TRAINED) {
        float pBest = rv::pWin(gc);  // skipping keeps the current state
        for (int i = 0; i < 3; ++i) {
            const RelicId r = gc.info.bossRelics[i];
            if (r == RelicId::RUNIC_DOME && getp(P, "allow_dome", 1) <= 0) continue;
            GameContext hyp = gc;
            RelicInstance ri; ri.id = r; ri.data = 0;
            hyp.relics.add(ri);
            const float pw = rv::pWin(hyp);
            if (pw > pBest) { pBest = pw; best = i; }
        }
        return best;
    }
    for (int i = 0; i < 3; ++i) {
        const RelicId r = gc.info.bossRelics[i];
        if (r == RelicId::RUNIC_DOME && getp(P, "allow_dome", 1) <= 0) continue;
        const double v = getp(P, "boss_pol", 0) > 0 ? kbBossRelicValue(r)
                                                    : -static_cast<double>(search::Expert::getBossRelicOrdering(r));
        if (v > bestV) { bestV = v; best = i; }
    }
    return best;
}

// ---------------- Neow ----------------
int FairAgent::neowChoice(const GameContext &gc) const {
    using B = Neow::Bonus; using D = Neow::Drawback;
    auto bonus = [](B b) {
        switch (b) {
            case B::THREE_CARDS: return 3.0; case B::ONE_RANDOM_RARE_CARD: return 3.0;
            case B::REMOVE_CARD: return 4.0; case B::UPGRADE_CARD: return 3.0;
            case B::TRANSFORM_CARD: return 3.0; case B::RANDOM_COLORLESS: return 2.0;
            case B::THREE_SMALL_POTIONS: return 3.0; case B::RANDOM_COMMON_RELIC: return 4.0;
            case B::TEN_PERCENT_HP_BONUS: return 3.0; case B::THREE_ENEMY_KILL: return 4.0;
            case B::HUNDRED_GOLD: return 2.5; case B::RANDOM_COLORLESS_2: return 3.5;
            case B::REMOVE_TWO: return 6.0; case B::ONE_RARE_RELIC: return 7.0;
            case B::THREE_RARE_CARDS: return 6.0; case B::TWO_FIFTY_GOLD: return 6.0;
            case B::TRANSFORM_TWO_CARDS: return 5.0; case B::TWENTY_PERCENT_HP_BONUS: return 5.5;
            case B::BOSS_RELIC: return 6.5;
            default: return 0.0;
        }
    };
    auto drawback = [](D d) {
        switch (d) {
            case D::TEN_PERCENT_HP_LOSS: return -1.5; case D::NO_GOLD: return -1.0;
            case D::CURSE: return -4.0; case D::PERCENT_DAMAGE: return -1.0;
            case D::LOSE_STARTER_RELIC: return -2.0; default: return 0.0;
        }
    };
    int best = 0; double bestV = -1e18;
    for (int i = 0; i < 4; ++i) {
        const auto &o = gc.info.neowRewards[i];
        const double v = bonus(o.r) + drawback(o.d);
        if (v > bestV) { bestV = v; best = i; }
    }
    return best;
}

// ---------------- map ----------------
double FairAgent::roomValue(const GameContext &gc, Room r) const {
    const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
    switch (r) {
        case Room::MONSTER: return getp(P, "w_monster", 1.0);
        case Room::ELITE: {
            const double th = getp(P, "elite_hp", 0.6);
            double v = hpf >= th ? getp(P, "w_elite_hi", 1.5) : getp(P, "w_elite_lo", -3.0);
            if (gc.act == 1) v += getp(P, "w_elite_act1", 0.0);
            return v;
        }
        case Room::REST: return hpf < getp(P, "rest_hp", 0.5) ? getp(P, "w_rest_lo", 3.0) : getp(P, "w_rest_hi", 1.0);
        case Room::SHOP: return gc.gold >= getp(P, "shop_gold", 150) ? getp(P, "w_shop_rich", 2.0) : getp(P, "w_shop_poor", 0.0);
        case Room::EVENT: return getp(P, "w_event", 0.8);
        case Room::TREASURE: return getp(P, "w_treasure", 1.5);
        default: return 0.0;
    }
}

int FairAgent::chooseMapX(const GameContext &gc) {
    const Map &m = *gc.map;
    // DP over rows: best[y][x] = value(room) + max over edges best[y+1][e]
    double best[15][7];
    const double discount = getp(P, "map_discount", 0.9);
    for (int y = 14; y >= 0; --y) {
        for (int x = 0; x < 7; ++x) {
            const auto &n = m.getNode(x, y);
            double nxt = 0;
            if (y < 14 && n.edgeCount > 0) {
                nxt = -1e9;
                for (int i = 0; i < n.edgeCount; ++i) nxt = std::max(nxt, best[y + 1][n.edges[i]]);
            }
            double rv = roomValue(gc, n.room);
            if (getp(P, "keys", 0) > 0 && !gc.hasKey(Key::EMERALD_KEY) && m.burningEliteX == x && m.burningEliteY == y
                && static_cast<double>(gc.curHp) / gc.maxHp >= getp(P, "elite_hp", 0.6)) {
                rv += getp(P, "w_burning", 2.0);  // the burning elite drops the emerald key
            }
            best[y][x] = rv + discount * nxt;
        }
    }
    auto acts = search::GameAction::getAllActionsInState(gc);
    const int nextY = gc.curMapNodeY + 1;
    int bestX = acts.front().getIdx1();
    double bestV = -1e18;
    for (auto &a : acts) {
        const int x = a.getIdx1();
        const double v = best[nextY][x] + 1e-6 * x;
        if (v > bestV) { bestV = v; bestX = x; }
    }
    return bestX;
}

// ---------------- card rewards ----------------
void FairAgent::cardRewards(GameContext &gc) {
    auto &r = gc.info.rewardsContainer;
    for (int rIdx = r.cardRewardCount - 1; rIdx >= 0; --rIdx) {
        if (getp(P, "cf_pick", 0) > 0) {
            // Phase C v2: interventional — play sampled upcoming fights with and without each card
            const std::uint64_t cfSeed = agentSeed ^ (static_cast<std::uint64_t>(gc.floorNum) * 0x9E3779B97F4A7C15ULL) ^ (rIdx * 7919ULL);
            const double sSkip = cfScore(gc, cfSeed);
            int pick = -1; double best = sSkip + getp(P, "cf_margin", 0.0);
            for (int i = 0; i < r.cardRewards[rIdx].size(); ++i) {
                GameContext hyp = gc;
                hyp.deck.obtainRaw(r.cardRewards[rIdx][i]);
                const double sc = cfScore(hyp, cfSeed);
                if (sc > best) { best = sc; pick = i; }
            }
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, pick >= 0 ? pick : 5).execute(gc);
            continue;
        }
        if (getp(P, "rv_pick", 0) > 0 && rv::RV_TRAINED) {
            // Phase C: one-step lookahead with the learned run value
            const float pSkip = rv::pWin(gc);
            int pick = -1; float pBest = pSkip + static_cast<float>(getp(P, "rv_margin", 0.0));
            for (int i = 0; i < r.cardRewards[rIdx].size(); ++i) {
                GameContext hyp = gc;
                hyp.deck.obtainRaw(r.cardRewards[rIdx][i]);
                const float pw = rv::pWin(hyp);
                if (pw > pBest) { pBest = pw; pick = i; }
            }
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, pick >= 0 ? pick : 5).execute(gc);
            continue;
        }
        int bestI = -1;
        double bestV = -1e18;
        for (int i = 0; i < r.cardRewards[rIdx].size(); ++i) {
            const double v = addValue(gc, r.cardRewards[rIdx][i]);
            if (v > bestV) { bestV = v; bestI = i; }
        }
        const double thresh = getp(P, "card_thresh", 5.0)
                + getp(P, "card_size_pen", 0.1) * std::max(0, gc.deck.size() - 15)
                - (gc.act == 1 ? getp(P, "card_act1_bonus", 1.0) : 0.0);
        if (bestI >= 0 && bestV >= thresh) {
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, bestI).execute(gc);
        } else {
            search::GameAction(search::GameAction::RewardsActionType::CARD, rIdx, 5).execute(gc);  // skip
        }
    }
}

// ---------------- shop ----------------
void FairAgent::shop(GameContext &gc) {
    using RT = search::GameAction::RewardsActionType;
    auto &s = gc.info.shop;
    if (s.removeCost != -1 && gc.gold >= s.removeCost && getp(P, "shop_remove", 1) > 0) {
        search::GameAction(RT::CARD_REMOVE).execute(gc);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        if (s.relicPrice(i) != -1 && gc.gold >= s.relicPrice(i) && getp(P, "shop_relic", 1) > 0) {
            search::GameAction(RT::RELIC, i).execute(gc);
            return;
        }
    }
    if (getp(P, "shop_potion", 0) > 0 && gc.potionCount < gc.potionCapacity) {
        for (int i = 0; i < 3; ++i) {
            const int price = s.potionPrice(i);
            if (price != -1 && gc.gold >= price + getp(P, "shop_pot_reserve", 0)) {
                search::GameAction(RT::POTION, i).execute(gc);
                return;
            }
        }
    }
    if (getp(P, "rv_pick", 0) > 0 && rv::RV_TRAINED) {
        const float pNow = rv::pWin(gc);
        int pick = -1; float pBest = pNow + static_cast<float>(getp(P, "rv_margin", 0.0));
        for (int i = 0; i < 7; ++i) {
            if (s.cardPrice(i) == -1 || gc.gold < s.cardPrice(i)) continue;
            GameContext hyp = gc;
            hyp.gold -= s.cardPrice(i);
            hyp.deck.obtainRaw(s.cards[i]);
            const float pw = rv::pWin(hyp);
            if (pw > pBest) { pBest = pw; pick = i; }
        }
        if (pick >= 0) { search::GameAction(RT::CARD, pick).execute(gc); return; }
        search::GameAction(RT::SKIP).execute(gc);
        return;
    }
    int bestI = -1; double bestV = -1e18;
    for (int i = 0; i < 7; ++i) {
        if (s.cardPrice(i) == -1 || gc.gold < s.cardPrice(i)) continue;
        const double v = addValue(gc, s.cards[i]);
        if (v > bestV) { bestV = v; bestI = i; }
    }
    if (bestI >= 0 && bestV >= getp(P, "shop_card_thresh", 7.0)) {
        search::GameAction(RT::CARD, bestI).execute(gc);
        return;
    }
    search::GameAction(RT::SKIP).execute(gc);
}

// ---------------- removal / upgrade selection ----------------
void FairAgent::cardSelect(GameContext &gc) {
    int bestI = 0; double bestV = -1e18;
    for (int i = 0; i < gc.info.toSelectCards.size(); ++i) {
        const auto &c = gc.info.toSelectCards[i].card;
        double v;
        switch (gc.info.selectScreenType) {
            case CardSelectScreenType::REMOVE:
            case CardSelectScreenType::TRANSFORM:
            case CardSelectScreenType::TRANSFORM_UPGRADE:
            case CardSelectScreenType::BONFIRE_SPIRITS:
                // remove curses first, then strikes, then defends, then lowest value
                v = c.getType() == CardType::CURSE ? 100
                    : c.getId() == CardId::STRIKE_RED ? 50 + getp(P, "strike_before_defend", 1)
                    : c.getId() == CardId::DEFEND_RED ? 50
                    : 10 - cardValue(c);
                break;
            case CardSelectScreenType::UPGRADE:
                v = kbValue(c.getId()) + (c.getId() == CardId::BASH ? getp(P, "bash_upg_bonus", 2.0) : 0.0)
                    + (c.getType() == CardType::POWER ? getp(P, "upg_power_bonus", 0.0) : 0.0);
                break;
            default:
                v = cardValue(c);
                break;
        }
        if (v > bestV) { bestV = v; bestI = i; }
    }
    search::GameAction(bestI).execute(gc);
}

void FairAgent::stepOutOfCombat(GameContext &gc) {
    using RT = search::GameAction::RewardsActionType;
    if (getp(P, "oc_potions", 0) > 0 && gc.screenState == ScreenState::MAP_SCREEN) {
        // potions that are strictly better used outside combat (never done before):
        // Fruit Juice (permanent max HP) now; Blood Potion when low on HP (saves a rest);
        // Entropic Brew when >= 2 slots are empty (fills every empty slot).
        const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
        for (int i = 0; i < gc.potionCapacity; ++i) {
            const Potion pot = gc.potions[i];
            if (pot == Potion::FRUIT_JUICE) { gc.drinkPotionAtIdx(i); continue; }
            if (pot == Potion::BLOOD_POTION && hpf < getp(P, "oc_blood_hp", 0.45)) { gc.drinkPotionAtIdx(i); continue; }
            if (pot == Potion::ENTROPIC_BREW && gc.potionCapacity - gc.potionCount >= 2) { gc.drinkPotionAtIdx(i); continue; }
        }
    }
    if (rvLog) {
        switch (gc.screenState) {  // one row per strategic screen (not card-select sub-screens)
            case ScreenState::MAP_SCREEN: case ScreenState::REWARDS: case ScreenState::SHOP_ROOM:
            case ScreenState::REST_ROOM: case ScreenState::EVENT_SCREEN: case ScreenState::BOSS_RELIC_REWARDS: {
                const size_t off = rvRows.size();
                rvRows.resize(off + rv::DIM);
                rv::extract(gc, rvRows.data() + off);
                break;
            }
            default: break;
        }
    }
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN:
            if (getp(P, "map_pol", 0) > 0 && gc.curMapNodeY < 14) {
                search::GameAction(chooseMapX(gc)).execute(gc);
                return;
            }
            break;
        case ScreenState::BOSS_RELIC_REWARDS:
            search::GameAction(bossRelicChoice(gc)).execute(gc);
            return;
        case ScreenState::TREASURE_ROOM:
            // lightspeed's TREASURE_ROOM action: 0 = open the chest, anything else = leave. Its legacy
            // agent passes `true` (=1) meaning "take", so it NEVER opens chests; FairAgent inherited that.
            if (getp(P, "chest_pol", 0) > 0) {
                const bool open = !gc.relics.has(RelicId::CURSED_KEY) || gc.info.chestSize == ChestSize::LARGE
                                  || getp(P, "chest_cursed_open", 0) > 0;
                search::GameAction(open ? 0 : 1).execute(gc);
                return;
            }
            break;
        case ScreenState::EVENT_SCREEN:
            if (gc.curEvent == Event::NEOW && getp(P, "neow_pol", 0) > 0) {
                search::GameAction(neowChoice(gc)).execute(gc);
                return;
            }
            if (getp(P, "ev_pol", 0) > 0) {
                const int idx = eventChoice(gc);
                if (idx >= 0) { search::GameAction(idx).execute(gc); return; }
            }
            break;
        case ScreenState::REST_ROOM:
            if (getp(P, "rest_pol", 0) > 0) {
                const double hpf = static_cast<double>(gc.curHp) / gc.maxHp;
                const bool preBoss = gc.curMapNodeY >= 13;
                if (getp(P, "keys", 0) > 0 && !gc.hasKey(Key::RUBY_KEY) && !preBoss && hpf >= getp(P, "key_rest_hp", 0.65)
                    && gc.act <= 2) {
                    search::GameAction(2).execute(gc);  // RECALL: take the ruby key
                    return;
                }
                const double th = preBoss ? getp(P, "rest_boss_frac", 0.7) : getp(P, "rest_heal_frac", 0.5);
                const bool canHeal = !gc.relics.has(RelicId::COFFEE_DRIPPER);
                const bool canUpg = !gc.relics.has(RelicId::FUSION_HAMMER) && gc.deck.getUpgradeableCount() > 0;
                if (canHeal && (hpf < th || !canUpg)) { search::GameAction(0).execute(gc); return; }
                if (canUpg) { search::GameAction(1).execute(gc); return; }
            }
            break;
        case ScreenState::REWARDS: {
            auto &r = gc.info.rewardsContainer;
            if (getp(P, "keys", 0) > 0) {
                // emerald key is free (burning elite); sapphire key replaces the chest relic (real-game rule),
                // so it must be chosen before the relic is taken and only when we want it
                if (r.emeraldKey) { search::GameAction(RT::KEY).execute(gc); return; }
                if (r.sapphireKey && r.relicCount > 0 && getp(P, "sapphire", 1) > 0 && !gc.hasKey(Key::SAPPHIRE_KEY)) {
                    search::GameAction(RT::KEY).execute(gc); return;
                }
            }
            if (getp(P, "card_pol", 0) > 0 && r.goldRewardCount == 0 && r.relicCount == 0
                && r.potionCount == 0 && r.cardRewardCount > 0) {
                cardRewards(gc);
                return;
            }
            break;
        }
        case ScreenState::SHOP_ROOM:
            if (getp(P, "shop_pol", 0) > 0) { shop(gc); return; }
            break;
        case ScreenState::CARD_SELECT:
            if (getp(P, "select_pol", 0) > 0) { cardSelect(gc); return; }
            break;
        default:
            break;
    }
    legacy.stepOutOfCombatPolicy(gc);
}

void FairAgent::playout(GameContext &gc) {
    int guard = 0;
    while (gc.outcome == GameOutcome::UNDECIDED) {
        if (++guard > 20000) { stats.aborted = true; break; }
        if (gc.screenState == ScreenState::BATTLE) {
            playBattle(gc);
            continue;
        }
        stepOutOfCombat(gc);
    }
    if (rvLog && !rvRows.empty()) {  // label every row of this run with its outcome
        const float win = gc.outcome == GameOutcome::PLAYER_VICTORY ? 1.f : 0.f;
        const float extra[4] = {win, static_cast<float>(gc.floorNum), static_cast<float>(gc.act),
                                static_cast<float>(agentSeed % 10000000ULL)};
        for (size_t off = 0; off < rvRows.size(); off += rv::DIM) {
            std::fwrite(rvRows.data() + off, sizeof(float), rv::DIM, rvLog);
            std::fwrite(extra, sizeof(float), 4, rvLog);
        }
        std::fflush(rvLog);
        rvRows.clear();
    }
}

}  // namespace fair
