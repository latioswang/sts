// Observable-only RUN-level feature extractor (Phase C): the state a player sees on the map /
// reward / shop screens. Used for V_run(state) = P(win | deck, relics, HP, gold, floor, act, boss).
#pragma once

#include "game/GameContext.h"

namespace fair {
double kbValue(sts::CardId id);  // KB tier table (FairAgent.cpp)

namespace rv {

constexpr int F_SCALAR = 24;
constexpr int N_ENC = 66;      // boss one-hot (MonsterEncounter index)
constexpr int N_RELIC = 192;   // RelicId one-hot (enum has ~180 entries; bound-checked)
constexpr int N_CARD = 384;    // CardId counts (enum has ~370 entries; bound-checked)
constexpr int DIM = F_SCALAR + N_ENC + N_RELIC + N_CARD;  // 666
// Phase F14 features v2: 4 key features appended AFTER the 666 (the v1 layout is the prefix): ruby / emerald / sapphire
// held, and keys mode (1 when this agent pursues the keys / act 4, i.e. param keys > 0). Rows of runs without keys mode
// are exactly the v1 row + 4 zeros.
constexpr int DIM2 = DIM + 4;

inline void extract(const sts::GameContext &gc, float *out) {
    using namespace sts;
    for (int i = 0; i < DIM; ++i) out[i] = 0.f;
    float *f = out;

    int upg = 0, curses = 0, strikes = 0, defends = 0, attacks = 0, skills = 0, powers = 0, rare = 0, unc = 0;
    float kb = 0.f;
    for (const auto &c : gc.deck.cards) {
        upg += c.isUpgraded();
        curses += c.getType() == CardType::CURSE;
        strikes += c.getId() == CardId::STRIKE_RED;
        defends += c.getId() == CardId::DEFEND_RED;
        attacks += c.getType() == CardType::ATTACK;
        skills += c.getType() == CardType::SKILL;
        powers += c.getType() == CardType::POWER;
        rare += c.getRarity() == CardRarity::RARE;
        unc += c.getRarity() == CardRarity::UNCOMMON;
        kb += (float)kbValue(c.getId());
    }
    const int n = gc.deck.size();
    f[0] = gc.floorNum / 55.f;  f[1] = gc.act / 3.f;  f[2] = gc.maxHp ? (float)gc.curHp / gc.maxHp : 0.f;
    f[3] = gc.curHp / 100.f;    f[4] = gc.maxHp / 100.f;  f[5] = gc.gold / 300.f;
    f[6] = gc.potionCount / 3.f;  f[7] = gc.potionCapacity / 3.f;  f[8] = n / 40.f;
    f[9] = upg / 20.f;  f[10] = curses / 3.f;  f[11] = strikes / 5.f;  f[12] = defends / 5.f;
    f[13] = attacks / 20.f;  f[14] = skills / 15.f;  f[15] = powers / 5.f;  f[16] = rare / 4.f;  f[17] = unc / 10.f;
    f[18] = n ? kb / n / 8.f : 0.f;  f[19] = kb / 200.f;  f[20] = gc.ascension / 20.f;
    f[21] = (gc.curMapNodeY + 1) / 16.f;  f[22] = gc.relics.relics.size() / 15.f;
    f[23] = gc.deck.getUpgradeableCount() / 20.f;
    f += F_SCALAR;

    const int b = static_cast<int>(gc.boss);
    if (b >= 0 && b < N_ENC) f[b] = 1.f;
    f += N_ENC;

    for (const auto &r : gc.relics.relics) {
        const int id = static_cast<int>(r.id);
        if (id >= 0 && id < N_RELIC) f[id] = 1.f;
    }
    f += N_RELIC;

    for (const auto &c : gc.deck.cards) {
        const int id = static_cast<int>(c.getId());
        if (id >= 0 && id < N_CARD) f[id] += c.isUpgraded() ? 0.5f : 0.34f;  // upgraded copies weigh more
    }
}

inline void extractKeys(const sts::GameContext &gc, float keysMode, float *out4) {
    out4[0] = gc.hasKey(sts::Key::RUBY_KEY) ? 1.f : 0.f;
    out4[1] = gc.hasKey(sts::Key::EMERALD_KEY) ? 1.f : 0.f;
    out4[2] = gc.hasKey(sts::Key::SAPPHIRE_KEY) ? 1.f : 0.f;
    out4[3] = keysMode;
}

}  // namespace rv
}  // namespace fair
