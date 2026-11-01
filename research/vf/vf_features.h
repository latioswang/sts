// (file named vf_features.h to avoid shadowing glibc <features.h>)
// Observable-only feature extractor for combat states (Phase A of the month program).
// Everything here is information a human player has on screen: HP/block/energy/statuses,
// own hand and pile *contents* (the deck is known; pile ORDER is not and is never used),
// visible monster HP/block/statuses and intents (zeroed under Runic Dome), encounter id.
#pragma once

#include "combat/BattleContext.h"
#include "constants/MonsterEncounters.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace fair {

double kbValue(sts::CardId id);  // defined in FairAgent.cpp (KB tier table)

namespace vf {

constexpr int N_ENCOUNTERS = 66;  // monsterEncounterEnumNames size (INVALID..MYSTERIOUS_SPHERE_EVENT)

// layout
constexpr int F_PLAYER = 44;
constexpr int F_HAND = 12;
constexpr int F_PILES = 12;
constexpr int F_KEYCARDS = 26;
constexpr int F_MONSTERS = 36;
constexpr int F_GLOBAL = 8;
constexpr int DIM = F_PLAYER + F_HAND + F_PILES + F_KEYCARDS + F_MONSTERS + F_GLOBAL + N_ENCOUNTERS;  // 204

inline bool isEliteEnc(sts::MonsterEncounter e) {
    using ME = sts::MonsterEncounter;
    switch (e) {
        case ME::GREMLIN_NOB: case ME::LAGAVULIN: case ME::THREE_SENTRIES:
        case ME::GREMLIN_LEADER: case ME::SLAVERS: case ME::BOOK_OF_STABBING:
        case ME::GIANT_HEAD: case ME::NEMESIS: case ME::REPTOMANCER: return true;
        default: return false;
    }
}

// Key cards whose presence anywhere in the combat deck changes the fight's dynamics.
inline int keyCardSlot(sts::CardId id) {
    using C = sts::CardId;
    switch (id) {
        case C::DEMON_FORM: return 0;      case C::BARRICADE: return 1;     case C::CORRUPTION: return 2;
        case C::FEEL_NO_PAIN: return 3;    case C::INFLAME: return 4;       case C::LIMIT_BREAK: return 5;
        case C::WHIRLWIND: return 6;       case C::IMMOLATE: return 7;      case C::REAPER: return 8;
        case C::OFFERING: return 9;        case C::BATTLE_TRANCE: return 10; case C::SHRUG_IT_OFF: return 11;
        case C::IMPERVIOUS: return 12;     case C::BLUDGEON: return 13;     case C::HEAVY_BLADE: return 14;
        case C::BODY_SLAM: return 15;      case C::ENTRENCH: return 16;     case C::FEED: return 17;
        case C::FIEND_FIRE: return 18;     case C::EXHUME: return 19;       case C::DISARM: return 20;
        case C::SHOCKWAVE: return 21;      case C::METALLICIZE: return 22;  case C::JUGGERNAUT: return 23;
        case C::DOUBLE_TAP: return 24;     case C::FLAME_BARRIER: return 25;
        default: return -1;
    }
}

// lightspeed keeps rare player statuses in a std::map; some (DRAW_REDUCTION) are set as a bit with no
// map value, so getStatus<s>() throws std::out_of_range even though hasStatus<s>() is true. That
// crashed 14.6% of logging games. Guard: absent -> 0; present but valueless -> 1 (boolean status).
template <PlayerStatus s> inline int pst(const sts::Player &p) {
    if (!p.hasStatus<s>()) return 0;
    try { return p.getStatus<s>(); } catch (const std::out_of_range &) { return 1; }
}
template <sts::MonsterStatus s> inline int mst(const sts::Monster &m) {
    if (!m.hasStatus<s>()) return 0;
    try { return m.getStatus<s>(); } catch (const std::out_of_range &) { return 1; }
}

// Fills out[0..DIM). All values roughly O(1).
inline void extract(const sts::BattleContext &bc, float *out) {
    using namespace sts;
    for (int i = 0; i < DIM; ++i) out[i] = 0.f;
    float *f = out;
    const auto &pl = bc.player;

    // ---- player (44) ----
    f[0] = pl.curHp / 80.f;  f[1] = pl.maxHp / 80.f;  f[2] = pl.maxHp ? (float)pl.curHp / pl.maxHp : 0.f;
    f[3] = pl.block / 20.f;  f[4] = pl.energy / 3.f;  f[5] = pl.energyPerTurn / 3.f;
    f[6] = pst<PS::STRENGTH>(pl) / 5.f;      f[7] = pst<PS::DEXTERITY>(pl) / 3.f;
    f[8] = pst<PS::VULNERABLE>(pl) / 2.f;    f[9] = pst<PS::WEAK>(pl) / 2.f;
    f[10] = pst<PS::FRAIL>(pl) / 2.f;        f[11] = pst<PS::ARTIFACT>(pl) / 2.f;
    f[12] = pst<PS::METALLICIZE>(pl) / 6.f;  f[13] = pst<PS::PLATED_ARMOR>(pl) / 6.f;
    f[14] = pst<PS::DEMON_FORM>(pl) / 2.f;   f[15] = pl.hasStatus<PS::BARRICADE>();
    f[16] = pst<PS::FEEL_NO_PAIN>(pl) / 4.f; f[17] = pst<PS::RAGE>(pl) / 4.f;
    f[18] = pl.hasStatus<PS::CORRUPTION>();         f[19] = pst<PS::DARK_EMBRACE>(pl) / 2.f;
    f[20] = pst<PS::COMBUST>(pl) / 6.f;      f[21] = pst<PS::BRUTALITY>(pl) / 2.f;
    f[22] = pst<PS::JUGGERNAUT>(pl) / 6.f;   f[23] = pst<PS::DOUBLE_TAP>(pl) / 2.f;
    f[24] = pst<PS::FLAME_BARRIER>(pl) / 10.f; f[25] = pst<PS::EVOLVE>(pl) / 2.f;
    f[26] = pst<PS::FIRE_BREATHING>(pl) / 8.f; f[27] = pst<PS::RUPTURE>(pl) / 2.f;
    f[28] = pst<PS::THORNS>(pl) / 5.f;       f[29] = pst<PS::INTANGIBLE>(pl) / 2.f;
    f[30] = pl.hasStatus<PS::NO_DRAW>();            f[31] = pl.hasStatus<PS::NO_BLOCK>();
    f[32] = pl.hasStatus<PS::ENTANGLED>();          f[33] = pl.hasStatus<PS::CONFUSED>();
    f[34] = pst<PS::ENERGIZED>(pl) / 2.f;    f[35] = pst<PS::DRAW_REDUCTION>(pl) / 2.f;
    f[36] = pst<PS::REGEN>(pl) / 5.f;        f[37] = pst<PS::PANACHE>(pl) / 5.f;
    f[38] = pst<PS::THE_BOMB>(pl) / 3.f;     f[39] = pl.hasStatus<PS::PEN_NIB>();
    f[40] = pst<PS::VIGOR>(pl) / 8.f;        f[41] = pst<PS::NEXT_TURN_BLOCK>(pl) / 10.f;
    f[42] = pl.hasStatus<PS::DOUBLE_DAMAGE>();      f[43] = pst<PS::BUFFER>(pl) / 2.f;
    f += F_PLAYER;

    // ---- hand (12) ----
    {
        int nA = 0, nS = 0, nP = 0, nX = 0, cost = 0, zero = 0;
        float kb = 0.f, dmgish = 0.f, blkish = 0.f;
        for (int i = 0; i < bc.cards.cardsInHand; ++i) {
            const auto &c = bc.cards.hand[i];
            switch (c.getType()) {
                case CardType::ATTACK: ++nA; break;
                case CardType::SKILL: ++nS; break;
                case CardType::POWER: ++nP; break;
                default: ++nX; break;
            }
            cost += c.costForTurn > 0 ? c.costForTurn : 0;
            zero += c.costForTurn == 0;
            kb += (float)kbValue(c.getId());
            if (c.getType() == CardType::ATTACK) dmgish += 1.f + 0.5f * c.getUpgradeCount();
            if (c.getType() == CardType::SKILL) blkish += 1.f;
        }
        f[0] = bc.cards.cardsInHand / 10.f; f[1] = nA / 5.f; f[2] = nS / 5.f; f[3] = nP / 3.f; f[4] = nX / 3.f;
        f[5] = cost / 8.f; f[6] = zero / 3.f; f[7] = kb / 30.f; f[8] = dmgish / 5.f; f[9] = blkish / 5.f;
        f[10] = bc.cards.handPainCount / 3.f; f[11] = bc.cards.handNormalityCount / 2.f;
    }
    f += F_HAND;

    // ---- piles (12): sizes and type counts (contents are known to the player; order is not used) ----
    {
        int d[4] = {0, 0, 0, 0}, s[4] = {0, 0, 0, 0};
        for (const auto &c : bc.cards.drawPile) d[std::min(3, (int)c.getType())]++;
        for (const auto &c : bc.cards.discardPile) s[std::min(3, (int)c.getType())]++;
        f[0] = bc.cards.drawPile.size() / 20.f; f[1] = bc.cards.discardPile.size() / 20.f; f[2] = bc.cards.exhaustPile.size() / 10.f;
        f[3] = d[0] / 10.f; f[4] = d[1] / 10.f; f[5] = d[2] / 3.f; f[6] = d[3] / 3.f;
        f[7] = s[0] / 10.f; f[8] = s[1] / 10.f; f[9] = s[2] / 3.f; f[10] = s[3] / 3.f;
        f[11] = bc.cards.strikeCount / 5.f;
    }
    f += F_PILES;

    // ---- key cards anywhere in the combat deck (26) ----
    {
        auto tally = [&](const CardInstance &c) { const int k = keyCardSlot(c.getId()); if (k >= 0) f[k] += 0.5f; };
        for (int i = 0; i < bc.cards.cardsInHand; ++i) tally(bc.cards.hand[i]);
        for (const auto &c : bc.cards.drawPile) tally(c);
        for (const auto &c : bc.cards.discardPile) tally(c);
    }
    f += F_KEYCARDS;

    // ---- monsters (36) ----
    {
        const bool dome = pl.hasRelic<R::RUNIC_DOME>();
        int alive = 0, attacking = 0, minions = 0;
        float sumHp = 0, maxHp = 0, sumMaxHp = 0, sumBlock = 0, intent = 0, maxIntent = 0, hits = 0;
        float st[24] = {0};
        for (int i = 0; i < bc.monsters.monsterCount; ++i) {
            const auto &m = bc.monsters.arr[i];
            if (m.isDeadOrEscaped()) continue;
            ++alive;
            sumHp += m.curHp; sumMaxHp += m.maxHp; maxHp = std::max(maxHp, (float)m.curHp); sumBlock += m.block;
            minions += m.hasStatus<MS::MINION>();
            if (!dome && m.isAttacking()) {
                ++attacking;
                auto d = m.getMoveBaseDamage(bc);
                const float dmg = (float)m.calculateDamageToPlayer(bc, d.damage) * d.attackCount;
                intent += dmg; maxIntent = std::max(maxIntent, dmg); hits += d.attackCount;
            }
            st[0] += mst<MS::STRENGTH>(m); st[1] += mst<MS::VULNERABLE>(m); st[2] += mst<MS::WEAK>(m);
            st[3] += mst<MS::ARTIFACT>(m); st[4] += mst<MS::METALLICIZE>(m); st[5] += mst<MS::PLATED_ARMOR>(m);
            st[6] += mst<MS::RITUAL>(m); st[7] += m.hasStatus<MS::ASLEEP>(); st[8] += mst<MS::INTANGIBLE>(m);
            st[9] += mst<MS::THORNS>(m); st[10] += mst<MS::ENRAGE>(m); st[11] += mst<MS::ANGRY>(m);
            st[12] += mst<MS::CURL_UP>(m); st[13] += m.hasStatus<MS::MODE_SHIFT>(); st[14] += m.hasStatus<MS::BARRICADE>();
            st[15] += mst<MS::REGEN>(m); st[16] += mst<MS::SHARP_HIDE>(m); st[17] += mst<MS::PAINFUL_STABS>(m);
            st[18] += mst<MS::SLOW>(m); st[19] += m.hasStatus<MS::TIME_WARP>(); st[20] += mst<MS::BEAT_OF_DEATH>(m);
            st[21] += mst<MS::FLIGHT>(m); st[22] += mst<MS::MALLEABLE>(m); st[23] += m.hasStatus<MS::SPORE_CLOUD>();
        }
        f[0] = alive / 3.f; f[1] = sumHp / 100.f; f[2] = maxHp / 100.f; f[3] = sumMaxHp ? sumHp / sumMaxHp : 0.f;
        f[4] = sumBlock / 20.f; f[5] = intent / 20.f; f[6] = maxIntent / 20.f; f[7] = hits / 3.f; f[8] = attacking / 3.f;
        f[9] = dome; f[10] = minions / 2.f; f[11] = pl.maxHp ? intent / pl.maxHp : 0.f;
        const float sc[24] = {5, 2, 2, 2, 5, 5, 3, 1, 1, 3, 2, 1, 5, 1, 1, 3, 3, 1, 1, 1, 1, 3, 3, 1};
        for (int k = 0; k < 24; ++k) f[12 + k] = st[k] / sc[k];
    }
    f += F_MONSTERS;

    // ---- global (8) ----
    f[0] = bc.turn / 10.f; f[1] = bc.potionCount / 3.f; f[2] = isBossEncounter(bc.encounter); f[3] = isEliteEnc(bc.encounter);
    f[4] = bc.floorNum / 50.f; f[5] = bc.ascension / 20.f; f[6] = bc.cards.exhaustPile.size() > 0; f[7] = bc.inputState == InputState::PLAYER_NORMAL;
    f += F_GLOBAL;

    // ---- encounter one-hot (66) ----
    const int e = static_cast<int>(bc.encounter);
    if (e >= 0 && e < N_ENCOUNTERS) f[e] = 1.f;
}

}  // namespace vf
}  // namespace fair
