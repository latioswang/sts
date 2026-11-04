#include "CommJson.h"

#include "Compare.h"
#include "GameIds.h"

#include "sim/search/GameAction.h"

using namespace sts;

namespace realgame {

using json = nlohmann::json;

namespace {

json cardJson(CardId id, int upgrades, int cost, int misc, bool hasTarget) {
    return json{{"id", cardToGameId(id)}, {"upgrades", upgrades}, {"cost", cost}, {"misc", misc}, {"has_target", hasTarget}};
}

json cardJson(const Card &c) { return cardJson(c.getId(), c.getUpgraded(), 0, c.misc, false); }

json cardJson(const CardInstance &c) {
    const int upgrades = c.id == CardId::SEARING_BLOW ? c.specialData : (c.upgraded ? 1 : 0);
    // The game reports a curse's "unplayable" cost as -2; the simulator stores -3.
    const int cost = c.costForTurn == -3 ? -2 : c.costForTurn;
    return cardJson(c.getId(), upgrades, cost, c.usesSpecialData() ? c.specialData : 0, c.requiresTarget());
}

template <typename Range>
json pileJson(const Range &pile) {
    json out = json::array();
    for (const auto &c : pile) out.push_back(cardJson(c));
    return out;
}

char symbolOf(Room r) {
    switch (r) {
        case Room::MONSTER: return 'M';
        case Room::ELITE: return 'E';
        case Room::REST: return 'R';
        case Room::SHOP: return '$';
        case Room::TREASURE: return 'T';
        case Room::EVENT: return '?';
        default: return '*';
    }
}

json mapJson(const GameContext &gc) {
    json out = json::array();
    for (int y = 0; y < 15; ++y) {
        for (int x = 0; x < 7; ++x) {
            const MapNode &n = gc.map->getNode(x, y);
            if (n.edgeCount == 0) continue;
            json children = json::array();
            for (int e = 0; e < n.edgeCount; ++e) children.push_back({{"x", n.edges[e]}, {"y", y + 1}});
            out.push_back({{"x", x}, {"y", y}, {"symbol", std::string(1, symbolOf(n.room))}, {"children", children}});
        }
    }
    return out;
}

int relicCounter(const RelicInstance &r) {
    if (r.id == RelicId::LIZARD_TAIL) return r.data == 0 ? -2 : -1;
    return r.data;
}

json monsterJson(const BattleContext &bc, const Monster &m) {
    json powers = json::array();
    for (int i = 0; i < static_cast<int>(MonsterStatus::INVALID); ++i) {
        const auto s = static_cast<MonsterStatus>(i);
        const bool present = s == MS::STRENGTH ? m.strength != 0 : m.hasStatusInternal(s);
        if (!present || monsterPowerToGameId(s) == "?") continue;
        json p{{"id", monsterPowerToGameId(s)}, {"amount", isBooleanPower(s) ? -1 : m.getStatusInternal(s)}};
        if (s == MS::VULNERABLE) p["just_applied"] = m.wasJustApplied<MS::VULNERABLE>();
        if (s == MS::WEAK) p["just_applied"] = m.wasJustApplied<MS::WEAK>();
        if (s == MS::RITUAL) p["just_applied"] = m.wasJustApplied<MS::RITUAL>();
        powers.push_back(p);
    }
    json out{{"id", monsterToGameId(m.id)}, {"current_hp", m.curHp}, {"max_hp", m.maxHp}, {"block", m.block},
             {"half_dead", m.halfDead}, {"is_gone", m.isDeadOrEscaped()}, {"powers", powers}};
    const auto moveByte = moveToGameByte(m.id, m.moveHistory[0]);
    if (moveByte) {
        const DamageInfo info = m.getMoveBaseDamage(bc);
        out["move_id"] = *moveByte;
        out["move_base_damage"] = info.damage;
        out["move_adjusted_damage"] = info.damage > 0 ? m.intentDamageToPlayer(bc, info.damage) : info.damage;
        out["move_hits"] = info.attackCount;
    }
    const auto lastByte = moveToGameByte(m.id, m.moveHistory[1]);
    if (lastByte) out["last_move_id"] = *lastByte;
    return out;
}

json combatJson(const BattleContext &bc) {
    json powers = json::array();
    for (int i = 1; i <= static_cast<int>(kLastPlayerStatus); ++i) {
        const auto s = static_cast<PlayerStatus>(i);
        if (!bc.player.hasStatusRuntime(s)) continue;
        if (s == PS::PANACHE) {
            powers.push_back({{"id", playerPowerToGameId(s)}, {"amount", bc.player.panacheCounter}, {"damage", playerPowerAmount(bc.player, s)}});
        } else {
            powers.push_back({{"id", playerPowerToGameId(s)}, {"amount", playerPowerAmount(bc.player, s)}});
        }
    }
    for (int i = 0; i < bc.player.nightmareCount; ++i) {
        powers.push_back({{"id", "Night Terror"}, {"amount", Player::NIGHTMARE_COPIES}, {"card", cardJson(bc.player.nightmareCards[i])}});
    }
    const int bombs[] = {bc.player.bomb1, bc.player.bomb2, bc.player.bomb3};
    for (int turnsLeft = 1; turnsLeft <= 3; ++turnsLeft) {
        const int damage = bombs[turnsLeft - 1];
        if (damage) powers.push_back({{"id", "TheBomb" + std::to_string(turnsLeft)}, {"amount", turnsLeft}, {"damage", damage}});
    }
    json orbs = json::array();
    for (int i = 0; i < bc.player.orbSlots; ++i) {
        orbs.push_back({{"id", orbToGameId(bc.player.orbs[i])}, {"evoke_amount", bc.player.orbEvokeAmount(i)}, {"passive_amount", bc.player.orbPassiveAmount(i)}});
    }

    json monsters = json::array();
    for (const int slot : occupiedMonsterSlots(bc)) monsters.push_back(monsterJson(bc, bc.monsters.arr[slot]));

    // ExhumeAction takes Exhumes out of the pile while its grid is open.
    json exhaustJson = pileJson(bc.cards.exhaustPile);
    if (bc.inputState == InputState::CARD_SELECT && bc.cardSelectInfo.cardSelectTask == CardSelectTask::EXHUME) {
        json visible = json::array();
        for (const auto &c : exhaustJson) {
            if (c.value("id", "") != "Exhume") visible.push_back(c);
        }
        exhaustJson = visible;
    }

    json hand = json::array();
    for (int i = 0; i < bc.cards.cardsInHand; ++i) {
        json card = cardJson(bc.cards.hand[i]);
        card["is_playable"] = bc.isCardPlayAllowed() && bc.cards.hand[i].canUseOnAnyTarget(bc);
        hand.push_back(card);
    }

    return json{{"turn", bc.turn + 1},
                {"hand", hand},
                {"draw_pile", pileJson(bc.cards.drawPile)},
                {"discard_pile", pileJson(bc.cards.discardPile)},
                {"exhaust_pile", exhaustJson},
                {"limbo", json::array()},
                {"monsters", monsters},
                {"times_damaged", bc.player.timesDamagedThisCombat},
                {"cards_discarded_this_turn", bc.player.cardsDiscardedThisTurn},
                {"player", {{"current_hp", bc.player.curHp}, {"max_hp", bc.player.maxHp}, {"block", bc.player.block},
                            {"energy", bc.player.energy}, {"powers", powers}, {"orbs", orbs}}}};
}

const char *actionFor(CardSelectTask task) {
    switch (task) {
        case CardSelectTask::ARMAMENTS: return "ArmamentsAction";
        case CardSelectTask::DUAL_WIELD: return "DualWieldAction";
        case CardSelectTask::EXHAUST_ONE:
        case CardSelectTask::EXHAUST_MANY: return "ExhaustAction";
        case CardSelectTask::EXHUME: return "ExhumeAction";
        case CardSelectTask::FORETHOUGHT: return "ForethoughtAction";
        case CardSelectTask::GAMBLE: return "GamblingChipAction";
        case CardSelectTask::WARCRY: return "PutOnDeckAction";
        case CardSelectTask::HEADBUTT: return "DiscardPileToTopOfDeckAction";
        case CardSelectTask::HOLOGRAM:
        case CardSelectTask::LIQUID_MEMORIES_POTION: return "BetterDiscardPileToHandAction";
        case CardSelectTask::SEEK: return "BetterDrawPileToHandAction";
        case CardSelectTask::RECYCLE: return "RecycleAction";
        case CardSelectTask::DISCARD: return "DiscardAction";
        case CardSelectTask::NIGHTMARE: return "NightmareAction";
        case CardSelectTask::RETAIN: return "RetainCardsAction";
        case CardSelectTask::SETUP: return "SetupAction";
        case CardSelectTask::SCRY: return "ScryAction";
        case CardSelectTask::MEDITATE: return "MeditateAction";
        case CardSelectTask::OMNISCIENCE: return "OmniscienceAction";
        case CardSelectTask::FOREIGN_INFLUENCE: return "ForeignInfluenceAction";
        case CardSelectTask::WISH: return "ChooseOneAction";
        case CardSelectTask::SECRET_TECHNIQUE: return "SkillFromDeckToHandAction";
        case CardSelectTask::SECRET_WEAPON: return "AttackFromDeckToHandAction";
        case CardSelectTask::CODEX: return "CodexAction";
        case CardSelectTask::DISCOVERY: return "DiscoveryAction";
        default: return "";
    }
}

void addBattleScreen(const BattleContext &bc, json &gs) {
    gs["room_phase"] = "COMBAT";
    gs["screen_type"] = "NONE";
    gs["screen_state"] = json::object();
    gs["room_type"] = isBossEncounter(bc.encounter) ? "MonsterRoomBoss" : "MonsterRoom";
    if (bc.inputState != InputState::CARD_SELECT) return;
    const CardSelectTask task = bc.cardSelectInfo.cardSelectTask;
    gs["current_action"] = actionFor(task);
    if (task == CardSelectTask::DISCOVERY || task == CardSelectTask::CODEX || task == CardSelectTask::FOREIGN_INFLUENCE || task == CardSelectTask::WISH) {
        gs["screen_type"] = "CARD_REWARD";
        json cards = json::array();
        CardSelectInfo info = bc.cardSelectInfo;  // discovery_Cards() has no const overload
        const int upgrades = task == CardSelectTask::WISH ? info.wish_Upgraded() : 0;
        for (int i = 0; i < 3; ++i) cards.push_back(cardJson(info.discovery_Cards()[i], upgrades, 0, 0, false));
        gs["screen_state"] = {{"cards", cards}};
    } else if (task == CardSelectTask::EXHUME || task == CardSelectTask::HEADBUTT || task == CardSelectTask::HOLOGRAM ||
               task == CardSelectTask::LIQUID_MEMORIES_POTION || task == CardSelectTask::SEEK ||
               task == CardSelectTask::SECRET_TECHNIQUE || task == CardSelectTask::SECRET_WEAPON ||
               task == CardSelectTask::MEDITATE || task == CardSelectTask::OMNISCIENCE || task == CardSelectTask::SCRY) {
        gs["screen_type"] = "GRID";
        gs["screen_state"] = {{"num_cards", bc.cardSelectInfo.pickCount}, {"cards", json::array()}};
        if (task == CardSelectTask::SCRY) {
            json cards = json::array();
            const int size = static_cast<int>(bc.cards.drawPile.size());
            for (int i = 0; i < bc.cardSelectInfo.pickCount; ++i) cards.push_back(cardJson(bc.cards.drawPile[size - 1 - i]));
            gs["screen_state"]["cards"] = cards;
            gs["screen_state"]["any_number"] = true;
        }
    } else {
        gs["screen_type"] = "HAND_SELECT";
        gs["screen_state"] = {{"max_cards", bc.cardSelectInfo.pickCount}, {"hand", json::array()}};
        if (task == CardSelectTask::RETAIN) {
            // The simulator keeps the picks at the end of the hand. The game's screen holds them apart.
            const int picked = bc.cardSelectInfo.data0;
            json selected = json::array();
            json &hand = gs["combat_state"]["hand"];
            for (int i = 0; i < picked; ++i) {
                selected.insert(selected.begin(), hand.back());
                hand.erase(hand.size() - 1);
            }
            gs["screen_state"]["selected"] = selected;
            gs["screen_state"]["max_cards"] = bc.cardSelectInfo.pickCount + picked;
        }
    }
}

void addOutOfCombatScreen(const GameContext &gc, json &gs) {
    json screen = json::object();
    const char *type = "NONE";
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN: {
            type = "MAP";
            screen["first_node_chosen"] = gc.curMapNodeY >= 0;
            if (gc.curMapNodeY >= 0) screen["current_node"] = {{"x", gc.curMapNodeX}, {"y", gc.curMapNodeY}};
            break;
        }
        case ScreenState::EVENT_SCREEN: {
            type = "EVENT";
            screen["event_id"] = gc.curEvent == Event::NEOW ? "Neow Event" : eventToGameId(gc.curEvent);
            const int bits = gc.curEvent == Event::NEOW ? 0xF : search::GameAction::getValidEventSelectBits(gc);
            json options = json::array();
            for (int i = 0; i < 31 && (bits >> i) != 0; ++i) {
                const bool enabled = (bits >> i) & 1;
                std::string text = "[option " + std::to_string(i) + "]";
                if (gc.curEvent == Event::NEOW) {
                    text = std::string(Neow::bonusStrings[static_cast<int>(gc.info.neowRewards[i].r)]) + " " +
                           Neow::drawbackStrings[static_cast<int>(gc.info.neowRewards[i].d)];
                }
                options.push_back({{"text", text}, {"label", text}, {"disabled", !enabled}});
            }
            screen["options"] = options;
            break;
        }
        case ScreenState::REWARDS: {
            type = "COMBAT_REWARD";
            const Rewards &r = gc.info.rewardsContainer;
            json rewards = json::array();
            for (int i = 0; i < r.goldRewardCount; ++i) rewards.push_back({{"reward_type", "GOLD"}, {"gold", r.gold[i]}});
            for (int i = 0; i < r.relicCount; ++i) rewards.push_back({{"reward_type", "RELIC"}, {"relic", {{"id", relicToGameId(r.relics[i])}}}});
            for (int i = 0; i < r.potionCount; ++i) rewards.push_back({{"reward_type", "POTION"}, {"potion", {{"id", potionToGameId(r.potions[i])}}}});
            for (int i = 0; i < r.cardRewardCount; ++i) rewards.push_back({{"reward_type", "CARD"}});
            if (r.emeraldKey) rewards.push_back({{"reward_type", "EMERALD_KEY"}});
            if (r.sapphireKey) rewards.push_back({{"reward_type", "SAPPHIRE_KEY"}});
            screen["rewards"] = rewards;
            break;
        }
        case ScreenState::BOSS_RELIC_REWARDS: {
            type = "BOSS_REWARD";
            json relics = json::array();
            for (int i = 0; i < 3; ++i) relics.push_back({{"id", relicToGameId(gc.info.bossRelics[i])}});
            screen["relics"] = relics;
            break;
        }
        case ScreenState::CARD_SELECT: {
            type = "GRID";
            json cards = json::array();
            for (int i = 0; i < gc.info.toSelectCards.size(); ++i) cards.push_back(cardJson(gc.info.toSelectCards[i].card));
            screen["cards"] = cards;
            screen["num_cards"] = gc.info.toSelectCount;
            screen["for_upgrade"] = gc.info.selectScreenType == CardSelectScreenType::UPGRADE;
            screen["for_purge"] = gc.info.selectScreenType == CardSelectScreenType::REMOVE;
            screen["for_transform"] = gc.info.selectScreenType == CardSelectScreenType::TRANSFORM;
            break;
        }
        case ScreenState::TREASURE_ROOM: {
            type = "CHEST";
            screen["chest_type"] = gc.info.chestSize == ChestSize::LARGE ? "LargeChest" : gc.info.chestSize == ChestSize::MEDIUM ? "MediumChest" : "SmallChest";
            break;
        }
        case ScreenState::REST_ROOM:
            type = "REST";
            break;
        case ScreenState::SHOP_ROOM: {
            type = "SHOP_SCREEN";
            const Shop &s = gc.info.shop;
            json cards = json::array();
            json relics = json::array();
            json potions = json::array();
            for (int i = 0; i < 7; ++i) {
                if (s.cardPrice(i) == -1) continue;
                json c = cardJson(s.cards[i]);
                c["price"] = s.cardPrice(i);
                cards.push_back(c);
            }
            for (int i = 0; i < 3; ++i) {
                if (s.relicPrice(i) != -1) relics.push_back({{"id", relicToGameId(s.relics[i])}, {"price", s.relicPrice(i)}});
                if (s.potionPrice(i) != -1) potions.push_back({{"id", potionToGameId(s.potions[i])}, {"price", s.potionPrice(i)}});
            }
            screen = {{"cards", cards}, {"relics", relics}, {"potions", potions},
                      {"purge_available", s.removeCost != -1}, {"purge_cost", s.removeCost}};
            break;
        }
        default:
            break;
    }
    gs["screen_type"] = type;
    gs["screen_state"] = screen;
}

}  // namespace

json gameStateFromSim(const GameContext &gc, const BattleContext *bc) {
    json relics = json::array();
    for (const auto &r : gc.relics.relics) relics.push_back({{"id", relicToGameId(r.id)}, {"counter", relicCounter(r)}});

    json deck = json::array();
    for (int i = 0; i < gc.deck.size(); ++i) deck.push_back(cardJson(gc.deck.cards[i]));

    json potions = json::array();
    const auto &belt = bc ? bc->potions : gc.potions;
    for (int i = 0; i < gc.potionCapacity; ++i) {
        potions.push_back({{"id", potionToGameId(belt[i] == Potion::INVALID ? Potion::EMPTY_POTION_SLOT : belt[i])}});
    }

    json gs{{"current_hp", bc ? bc->player.curHp : gc.curHp}, {"max_hp", bc ? bc->player.maxHp : gc.maxHp},
            {"gold", bc ? static_cast<int>(bc->player.gold) : gc.gold}, {"floor", gc.floorNum}, {"act", gc.act},
            {"ascension_level", gc.ascension}, {"act_boss", encounterToGameId(gc.boss)}, {"relics", relics},
            {"deck", deck}, {"potions", potions}, {"map", mapJson(gc)},
            {"keys", {{"ruby", gc.hasKey(Key::RUBY_KEY)}, {"emerald", gc.hasKey(Key::EMERALD_KEY)}, {"sapphire", gc.hasKey(Key::SAPPHIRE_KEY)}}}};
    if (bc) {
        gs["combat_state"] = combatJson(*bc);
        addBattleScreen(*bc, gs);
    } else {
        addOutOfCombatScreen(gc, gs);
    }
    return gs;
}

}  // namespace realgame
