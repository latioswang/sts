// ---- out-of-combat event policy (Ironclad, A0, goal: Act 3 boss) ----
// Included inside FairAgent::eventChoice(gc). Uses gc, hpf, P, getp, cardValue.
// Only visible info is used (HP, gold, deck, relics, event phase, displayed costs).
{
    const int valid = search::GameAction::getValidEventSelectBits(gc);
    auto ok = [&](int i) { return i >= 0 && i < 31 && ((valid >> i) & 1); };
    // first valid option from a preference list, or -1
    auto pick = [&](std::initializer_list<int> prefs) {
        for (int i : prefs) if (ok(i)) return i;
        return -1;
    };
    const int hp = gc.curHp;
    const int mhp = gc.maxHp;
    // HP left after paying `cost` stays above a safety floor
    auto affordHp = [&](int cost, double frac) {
        return (hp - cost) >= std::max(frac * mhp, getp(P, "ev_hp_floor", 20.0));
    };
    const bool potionSlotFree = gc.potionCount < gc.potionCapacity;

    switch (gc.curEvent) {
        case Event::NEOW:
            return -1;  // legacy

        case Event::OMINOUS_FORGE:  // 0 upgrade, 1 Pain+Warped Tongs, 2 leave
            return pick({0, 2});

        case Event::PLEADING_VAGRANT:  // 0 pay 85 for relic, 1 relic+Shame, 2 leave
            return pick({0, 2});

        case Event::ANCIENT_WRITING: {  // 0 remove, 1 upgrade all starter Strike/Defend
            int basics = 0;
            for (const auto &c : gc.deck.cards) if (c.isStarterStrikeOrDefend()) ++basics;
            return basics >= (int)getp(P, "ev_simplicity_basics", 5) ? pick({1, 0}) : pick({0, 1});
        }

        case Event::OLD_BEGGAR:  // 0 pay 75 remove, 1 leave
            return pick({0, 1});

        case Event::BIG_FISH: {  // 0 heal 1/3, 1 +5 maxHP, 2 relic+Regret
            const bool wantHeal = hpf < getp(P, "ev_fish_heal_hp", 0.65) && (mhp - hp) >= gc.info.hpAmount0 / 2;
            return wantHeal ? pick({0, 1}) : pick({1, 0});
        }

        case Event::COLOSSEUM: {  // phase 0: forced slaver fight; phase 1: 0 leave, 1 fight Taskmaster+Nob (100g, rare+uncommon relic)
            if (gc.info.eventData == 0) return pick({0});
            const bool strong = hpf >= getp(P, "ev_colo_hp", 0.65) && gc.curHp >= getp(P, "ev_colo_minhp", 45);
            return strong ? pick({1, 0}) : pick({0, 1});
        }

        case Event::CURSED_TOME: {  // phase in eventData; total cost 1+2+3+10 = 16 HP for a book relic
            const int ph = gc.info.eventData;
            if (ph == 0) return affordHp(16, getp(P, "ev_tome_hp", 0.45)) ? pick({0, 1}) : pick({1, 0});
            if (ph >= 1 && ph <= 3) return pick({ph + 1});
            if (ph == 4) return affordHp(10, 0.3) ? pick({5, 6}) : pick({6, 5});
            return -1;
        }

        case Event::DEAD_ADVENTURER: {  // 0 search (elite fight chance 25/50/75%), 1 leave
            const int ph = gc.info.phase;
            if (ph >= 3) return pick({1});
            const double need = ph == 0 ? getp(P, "ev_deadadv_hp0", 0.65) : getp(P, "ev_deadadv_hp1", 0.85);
            if (ph <= 1 && hpf >= need && hp >= 40) return pick({0, 1});
            return pick({1});
        }

        case Event::DESIGNER_IN_SPIRE:
            // 0 upgrade one (40g) / 1 upgrade 2 random (40g); 2 remove (60g) / 3 transform 2 random (60g);
            // 4 remove + random upgrade (90g); 5 leave (-3 HP). Validity bits decide which variants are offered.
            if (gc.gold >= 90 && getp(P, "ev_designer_full", 1) > 0) return pick({4, 2, 0, 1, 3, 5});
            return pick({2, 0, 1, 3, 5});

        case Event::AUGMENTER:  // 0 J.A.X., 1 transform 2, 2 Mutagenic Strength
            return pick({2, 1});

        case Event::DUPLICATOR:  // 0 duplicate a card, 1 leave
            return pick({0, 1});

        case Event::FACE_TRADER:  // 0 +75 gold, take 10% maxHP dmg; 1 random face relic; 2 leave
            return affordHp(gc.info.hpAmount0, 0.4) ? pick({0, 2}) : pick({2});

        case Event::FALLING: {  // lose the displayed skill(0) / power(1) / attack(2); 3 only if none
            int best = -1;
            double bestV = 1e9;
            const int idxs[3] = {gc.info.skillCardDeckIdx, gc.info.powerCardDeckIdx, gc.info.attackCardDeckIdx};
            for (int o = 0; o < 3; ++o) {
                const int di = idxs[o];
                if (!ok(o) || di < 0 || di >= (int)gc.deck.cards.size()) continue;
                const auto &c = gc.deck.cards[di];
                double v = cardValue(c);
                if (c.isStarterStrikeOrDefend()) v -= 10.0;  // always lose a basic if offered
                if (v < bestV) { bestV = v; best = o; }
            }
            if (best != -1) return best;
            return pick({3, 0, 1, 2});
        }

        case Event::FORGOTTEN_ALTAR:  // 0 Golden Idol->Bloody Idol, 1 +5 maxHP & lose 25% HP, 2 Decay
            if (ok(0)) return 0;
            return hp - gc.info.hpAmount0 >= 12 ? pick({1, 2}) : pick({2, 1});

        case Event::THE_DIVINE_FOUNTAIN:  // 0 remove all curses
            return pick({0, 1});

        case Event::GHOSTS:  // 0 lose 50% maxHP for 5 Apparitions, 1 refuse
            return getp(P, "ev_ghosts_accept", 0.0) > 0 && hpf >= 0.5 ? pick({0, 1}) : pick({1, 0});

        case Event::GOLDEN_IDOL:
            if (!gc.relics.has(RelicId::GOLDEN_IDOL)) return pick({0, 1});  // phase 1: take idol
            // phase 2 escape: 2 Injury, 3 take 25% maxHP dmg, 4 lose 8% maxHP
            return affordHp(gc.info.hpAmount0, getp(P, "ev_idol_dmg_hp", 0.6)) ? pick({3, 4, 2}) : pick({4, 3, 2});

        case Event::GOLDEN_SHRINE:  // 0 +100 gold, 1 +275 gold & Regret, 2 leave
            return pick({0, 2});

        case Event::WING_STATUE:  // 0 take 7 dmg + remove, 1 gold (needs 10+ dmg card), 2 leave
            return affordHp(7, 0.25) ? pick({0, 1, 2}) : pick({1, 2});

        case Event::KNOWING_SKULL: {  // repeated; each pick costs hpAmountN (rises by 1), leaving costs 6
            const double frac = getp(P, "ev_skull_hp", 0.55);
            if (affordHp(gc.info.hpAmount0 + 6, frac) && gc.gold < 300) return pick({0, 3});
            if (potionSlotFree && affordHp(gc.info.hpAmount2 + 6, frac)) return pick({2, 3});
            return pick({3});
        }

        case Event::LAB:
        case Event::WHEEL_OF_CHANGE:
            return pick({0});

        case Event::THE_SSSSSERPENT:  // 0 +175 gold & Doubt, 1 leave
            return pick({1, 0});

        case Event::LIVING_WALL:  // 0 remove, 1 transform, 2 upgrade
            return pick({0, 2, 1});

        case Event::MASKED_BANDITS:  // 0 hand over all gold, 1 fight (Red Mask + card)
            if (hpf < getp(P, "ev_bandits_hp", 0.35) && gc.gold < 100) return pick({0, 1});
            return pick({1, 0});

        case Event::MATCH_AND_KEEP:
            return -1;  // separate card-grid action space

        case Event::MINDBLOOM:  // 0 fight Act 1 boss (rare relic), 1 upgrade all + Mark of Bloom, 2 999 gold + 2 Normality, 3 (floor>40) full heal + Doubt
            if (ok(3) && hpf < 0.5) return 3;
            return pick({0, 3, 1});

        case Event::HYPNOTIZING_COLORED_MUSHROOMS:  // 0 fight (Odd Mushroom), 1 +99 gold (sim)
            return pick({1, 0});

        case Event::MYSTERIOUS_SPHERE:  // 0 fight 2 Orb Walkers for rare relic, 1 +99 gold (sim)
            return (hpf >= getp(P, "ev_sphere_hp", 0.75) && hp >= 50) ? pick({0, 1}) : pick({1, 0});

        case Event::THE_NEST:  // 0 +99 gold, 1 lose 6 HP for Ritual Dagger
            return pick({0, 1});

        case Event::NLOTH:  // 0/1 trade a relic for N'loth's Gift, 2 leave
            return pick({2});

        case Event::NOTE_FOR_YOURSELF:  // 0 take stored card + remove a card
            return pick({0, 1});

        case Event::PURIFIER:
        case Event::TRANSMORGRIFIER:
        case Event::UPGRADE_SHRINE:  // 0 remove / transform / upgrade, 1 leave
            return pick({0, 1});

        case Event::SCRAP_OOZE:  // 0 take 3 dmg for growing relic chance, 1 leave
            return affordHp(3, getp(P, "ev_ooze_hp", 0.4)) ? pick({0, 1}) : pick({1});

        case Event::SECRET_PORTAL:  // 0 skip to boss, 1 leave
            return getp(P, "ev_portal", 0.0) > 0 && hpf >= 0.8 ? pick({0, 1}) : pick({1, 0});

        case Event::SENSORY_STONE:  // 0 one colorless reward (free), 1 two (-5 HP), 2 three (-10 HP)
            return pick({0});

        case Event::SHINING_LIGHT:  // 0 lose 20% HP, upgrade 2 random; 1 leave
            if (gc.deck.getUpgradeableCount() >= 2 && affordHp(gc.info.hpAmount0, getp(P, "ev_light_hp", 0.5)))
                return pick({0, 1});
            return pick({1});

        case Event::THE_CLERIC:  // 0 heal 25% (35g), 1 remove (50g), 2 leave
            if (hpf < getp(P, "ev_cleric_heal_hp", 0.35) && gc.gold >= 35) return pick({0, 1, 2});
            if (ok(1)) return 1;
            if (hpf < 0.7 && gc.gold >= 35) return pick({0, 2});
            return pick({2});

        case Event::THE_JOUST:  // must bet 50: 0 murderer (70% -> 100), 1 owner (30% -> 250)
            return pick({0, 1});

        case Event::THE_LIBRARY:  // 0 pick 1 of 20 cards, 1 heal 33%
            return hpf < getp(P, "ev_library_heal_hp", 0.5) ? pick({1, 0}) : pick({0, 1});

        case Event::THE_MAUSOLEUM:  // 0 open: random relic + 50% Writhe; 1 leave
            if (getp(P, "ev_mausoleum", 1) > 0 && !gc.deck.hasCurse()) return pick({0, 1});
            return pick({1, 0});

        case Event::THE_MOAI_HEAD:  // 0 lose 12.5% maxHP + full heal, 1 Golden Idol -> 333 gold, 2 leave
            if (hpf < 0.4) return pick({0, 1, 2});
            if (ok(1)) return 1;
            return hpf < 0.6 ? pick({0, 2}) : pick({2});

        case Event::THE_WOMAN_IN_BLUE:  // 0/1/2 buy 1/2/3 potions (20/30/40g), 3 leave
            if (gc.gold >= 40) return pick({2, 1, 0, 3});
            if (gc.gold >= 30) return pick({1, 0, 3});
            if (gc.gold >= 20) return pick({0, 3});
            return pick({3});

        case Event::TOMB_OF_LORD_RED_MASK:  // 0 (have mask) +222 gold, 1 all gold for Red Mask, 2 leave
            if (ok(0)) return 0;
            return gc.gold <= (int)getp(P, "ev_redmask_gold", 60) ? pick({1, 2}) : pick({2});

        case Event::VAMPIRES:  // 0 give Blood Vial, 1 lose 30% maxHP; both: Strikes -> 5 Bites. 2 leave
            return pick({0, 2});

        case Event::WE_MEET_AGAIN: {  // 0 give potion, 1 give gold, 2 give shown card -> relic; 3 leave
            if (ok(0)) return 0;
            if (ok(1) && gc.info.gold <= 120) return 1;
            if (ok(2)) {
                const int di = gc.info.cardIdx;
                if (di >= 0 && di < (int)gc.deck.cards.size() && cardValue(gc.deck.cards[di]) <= 4.5) return 2;
            }
            return pick({1, 3});
        }

        case Event::WINDING_HALLS:  // 0 lose 12.5% HP + 2 Madness, 1 heal 25% + Writhe, 2 lose 5% maxHP
            return pick({2, 0});

        case Event::WORLD_OF_GOOP:  // 0 take 11 dmg, +75 gold; 1 lose some gold
            return affordHp(11, getp(P, "ev_goop_hp", 0.45)) ? pick({0, 1}) : pick({1, 0});

        case Event::BONFIRE_SPIRITS:  // select phase skipped by sim
        default:
            break;
    }
}
