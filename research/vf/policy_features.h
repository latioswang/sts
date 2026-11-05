// Action tokens + target features for the learned move prior (Phase B).
// Prior model (bilinear, cheap enough to run at every expanded node):
//   h = PF_W * state(204) + PF_B                 (K = 16)
//   logit(a) = h . E[token(a)] + up(a) * (h . E_UP) + AB[token(a)] + PF_T . tf(a)
//   prior = softmax over the node's legal actions.
#pragma once

#include "combat/BattleContext.h"
#include "sim/search/Action.h"

namespace fair::pf {

constexpr int N_CARD_TOK = 384;                 // CardId one token each
constexpr int TOK_POTION = N_CARD_TOK;          // + potion id (< 48)
constexpr int TOK_END_TURN = TOK_POTION + 48;   // 432
constexpr int TOK_SELECT = TOK_END_TURN + 1;    // 433: card-select sub-choices (uniform prior)
constexpr int N_TOK = TOK_SELECT + 1;           // 434
constexpr int T_DIM = 4;                        // target features

inline int token(const sts::BattleContext &bc, const sts::search::Action &a, bool &upgraded) {
    using namespace sts;
    upgraded = false;
    switch (a.getActionType()) {
        case search::ActionType::CARD: {
            const auto &c = bc.cards.hand[a.getSourceIdx()];
            upgraded = c.isUpgraded();
            const int id = static_cast<int>(c.getId());
            return (id >= 0 && id < N_CARD_TOK) ? id : 0;
        }
        case search::ActionType::POTION: {
            const int pid = static_cast<int>(bc.potions[a.getSourceIdx()]);
            return TOK_POTION + ((pid >= 0 && pid < 48) ? pid : 0);
        }
        case search::ActionType::END_TURN: return TOK_END_TURN;
        default: return TOK_SELECT;
    }
}

// target features: [has target, target hp frac, target is attacking (0 under Dome), target is lowest-hp alive]
inline void targetFeats(const sts::BattleContext &bc, const sts::search::Action &a, float *tf) {
    using namespace sts;
    tf[0] = tf[1] = tf[2] = tf[3] = 0.f;
    const auto t = a.getActionType();
    if (t != search::ActionType::CARD && t != search::ActionType::POTION) return;
    const int ti = a.getTargetIdx();
    if (ti < 0 || ti >= bc.monsters.monsterCount) return;
    const auto &m = bc.monsters.arr[ti];
    if (m.isDeadOrEscaped()) return;
    tf[0] = 1.f;
    tf[1] = m.maxHp ? (float)m.curHp / m.maxHp : 0.f;
    tf[2] = (!bc.player.hasRelic<R::RUNIC_DOME>() && m.isAttacking()) ? 1.f : 0.f;
    int lowest = m.curHp;
    for (int i = 0; i < bc.monsters.monsterCount; ++i)
        if (!bc.monsters.arr[i].isDeadOrEscaped()) lowest = std::min(lowest, bc.monsters.arr[i].curHp);
    tf[3] = m.curHp == lowest ? 1.f : 0.f;
}

}  // namespace fair::pf
