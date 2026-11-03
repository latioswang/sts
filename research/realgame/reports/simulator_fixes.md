# Simulator fixes found against the real game

Every fix below is on branch `fidelity` of [latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed), after the research baseline `e47bb63`. That baseline is upstream `7476a81` plus `research/patches/sts_lightspeed.patch`. The research loop still builds the baseline, so none of this changes its results until it switches.

Rows with a seed were found by mirror mode and then checked against the game's bytecode. To reproduce one, build the baseline and run `sts_bridge mirror --seed_start <seed> --count 1`. Agent decisions depend on the simulator, so a run can take another path on a later build. Rows marked "code" were found by reading the game's bytecode and have not been seen on the real game yet. Seeds from 2000 up use a scenario from `scenario_plan.py`, so add `--scenario_file`.

## Combat

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 1001, 13 | Distilled Chaos and Mayhem played their cards in reverse | `NewQueueCardAction` appends to the card queue, so cards play in the order they left the draw pile | `335c6da` |
| 1004, 16 | Mayhem played before the turn's draw | `MayhemPower` queues an action that queues the card, so it lands behind the draw | `672691b` |
| 1000, 14 | Curl Up triggered on a killing blow | `CurlUpPower.onAttacked` needs the damage to be less than current HP | `77bf15c` |
| 1012, 16 | Lizard Tail never revived the player | Its ready flag started at 0 and was never set | `15c4b0a` |
| code | Fairy Potion with Sacred Bark healed 1 HP out of combat | An operator precedence slip; it should heal 60% of max HP | `15c4b0a` |
| 1013, 33 | A card held by Stasis came back at once when the orb died | `StasisPower.onDeath` queues it, so on the monsters' turn it lands after the next draw | `c365f7f` |
| 1006, 23 | Mummified Hand could pick the Power being played | The game's card queue still holds that card | `941630d` |
| 1015, 29 | Corruption changed costs when it resolved | It does so when its `ApplyPowerAction` is created, so Mummified Hand already sees 0-cost skills | `5f93daf` |
| 1017, 22 | Armaments+ upgraded Dazed | It only upgrades cards that can upgrade | `8d6e4e7` |
| 1001, 33 | Seeing Red didn't exhaust; Rage, Trip and Infernal Blade+ had the wrong cost; Pride wasn't innate | All 133 red, colorless, curse and status cards now match on cost, type, exhaust, ethereal and innate | `f04f05b` |
| 1003, 35 | Thorns needed damage to get through; Angry didn't | `ThornsPower.onAttacked` has no damage check, and `AngryPower.onAttacked` needs damage after block | `c4ee75b` |
| 1006, 50 | Buffer didn't stop HP loss | HP loss skips block, but `BufferPower.onAttackedToChangeDamage` still applies | `b82e0a4` |
| 1007, 36 | A debuff stuck to a Darkling downed by the same blow | `ApplyPowerAction` does nothing to a dead or half-dead target | `0ea85b4` |
| 1018, 33 | Velvet Choker couldn't stop a Double Tap copy | Copies go through the same play checks as other cards | `0ea85b4` |
| 1016, 23 | A Perfected Strike played off the draw pile counted itself | It doesn't | `0ea85b4` |
| 1022, 16 | True Grit rolled for the last card in hand | `ExhaustAction` takes from the top without rolling when the hand has no more cards than it needs | `6eb7f3a` |
| 1029, 16 | A monster killed by Thorns kept attacking | `DamageAction.shouldCancelAction` drops the remaining hits of a dying attacker | `77c6929` |
| 1003, 29 | Magic Flower missed victory heals and rounded down | Those heals run while the room is still in combat, and `onPlayerHeal` uses `MathUtils.round` | `03562e8` |
| 1016, 40 | Slaver's Collar and Pantograph checked for a boss room | They look for a monster of type BOSS, so they work in Mind Bloom's fight | `5f93daf` |
| code | Elite relics checked the room type | Preserved Insect, Slaver's Collar and Sling of Courage check `eliteTrigger`, which Dead Adventurer and Colosseum's second fight also set | `098fc82` |
| 2006, 1 | Pride didn't come back, and couldn't be played | It puts a copy on top of the draw pile at the end of the turn, and costs 1 | `968e0f3` |
| code | Temp cards couldn't go on top of the draw pile, so Spire Spear's A18 Burns were lost | `MakeTempCardInDrawPileAction` adds them on top when it isn't a random spot | `968e0f3` |
| 2005, 1 | Panache's countdown started at 0 and didn't reset | It starts at 5 and goes back to 5 when it fires | `968e0f3` |
| 2006, 1 | Ritual Dagger dealt no damage, including The Nest's | It starts at 15 | `968e0f3` |
| 2004, 6 | Toolbox's card cost 0 | `ChooseOneColorless` leaves its cost alone | `968e0f3` |
| 2004, 6 | Magnetism did nothing | It adds a random colorless card each turn | `968e0f3` |
| 2000, 16 | Fiend Fire didn't roll for the last card | Its `ExhaustAction` takes any number, so it always rolls | `968e0f3` |
| 2001, 1 | Gremlin Visage's Weak lasted a round too long | Its `WeakPower` isn't from a monster | `968e0f3` |
| 2008, 1 | Violence left its cards in the draw pile when there were fewer attacks than it wanted | It takes what there is | `968e0f3` |
| 2006, 19 | Purity with an empty hand opened a screen | `ExhaustAction` ends at once | `968e0f3` |
| 2002, 7 | Exhume picked by itself when only one card could be chosen | It opens its grid whenever the pile holds more than one card | `69816d5` |
| 2004, 18 | Upgrading a card in combat raised a lower cost for the turn | `upgradeBaseCost` leaves it alone, as with Warped Tongs under Snecko Eye | `69816d5` |
| 2006, 36 | A Darkling that only went down counted as a Ritual Dagger kill | `RitualDaggerAction` needs it not to be half dead | `69816d5` |
| code | An autoplay card that couldn't be used vanished | `GameActionManager` sends it to `UseCardAction`: Havoc exhausts it, Mayhem discards it | `968e0f3` |
| code | Red Skull added 3 more Strength when healed past half | It takes the 3 away, and checks HP after Blood Vial's heal | `968e0f3` |
| code | Necronomicon tested the wrong flag | `freeToPlayOnce`, so it copies a Havoc card and not a Forethought one | `968e0f3` |
| code | Hand of Greed was treated as HP loss | It's an attack, so Curl Up, Angry, Thorns and The Boot see it | `968e0f3` |
| code | Enlightenment+ raised a lower cost for the turn to 1; Liquid Memories with Sacred Bark took one card; Forethought+ with one card opened no screen | `EnlightenmentAction`, `BetterDiscardPileToHandAction`, `ForethoughtAction` | `968e0f3` |

## Monsters

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 1002, 4 | Red Slaver could Entangle repeatedly; threshold 50; A17 Scrape check wrong | `usedEntangle` is set; threshold 55; A17 checks `lastMove` alone | `aebf989` |
| 1001, 16 | Hexaghost's Inferno only dealt damage | `BurnIncreaseAction` upgrades Burns in the discard and draw piles and adds three Burn+ | `495e4e9` |
| 1002, 18 | A dying Mugger used no AI roll, which shifted later moves | `Mugger.die` calls `playDeathSfx`, which rolls `aiRng` | `191b7f1` |
| 1000, 18 | A grounded Byrd kept Flight at 0: half damage, and Flight came back | `FlightPower` is removed at 0 | `41c7222` |
| 1006, 11 | Burning elites gained Strength equal to the act | They gain act + 1 | `1722a34` |
| 1010, 14; 1012, 13; 1019, 8 | Every large slime's split used two extra AI rolls | Spike Slime uses one (`RollMoveAction`), Acid Slime none | `846dde6`, `33725c6` |
| 1010, 16 | A move roll could replace a slime's split (Thorns during its own turn) | `damage()` also queues a `SetMoveAction`, which runs after the roll | `6838467` |
| 1000, 23; 1001, 21 | AI rolls differed on several moves; Centurion's Defend always rolled and had no self-block | Checked move by move: Mad Gremlin, Hexaghost's Activate and Transient don't roll, Deca and Donu do | `8c9d71e` |
| 1000, 23 | A summoned Mad Gremlin had no Angry | `SummonGremlinAction` calls `usePreBattleAction` | `d3437bb` |
| 1004, 24 | Bear, Centurion and Awakened One had wrong HP ranges | Every monster's range was compared with its constructor | `ffe8b7c` |
| 1002, 24 | The Mystic looked at each monster's missing HP alone, with no limit | It adds up all of them, needs more than 15 (20 on A17), and doesn't heal a third time in a row | `260a6c8` |
| 1000, 47 | The hard-mode Jaw Worm started with no block | `bellowBlock` is 6 below A17 | `2589229` |
| 1002, 35 | Darkling's Chomp hit once | It hits twice | `992ff6b` |
| 1005, 23 | Shelled Parasite's reroll of 60 or more did nothing | It went on testing the first roll | `992ff6b` |
| code | Gremlin Leader didn't reroll on an 80 | An 80 lands back in the same branch of `getMove` | `992ff6b` |
| 1002, 50 | Awakened One's Curiosity did nothing | The code was commented out | `f59eb45` |
| 1000, 39 | Shackled overflowed 8 bits, as Transient takes more than 127 in a turn | Poison, Plated Armor, Metallicize, Regen, Block Return and Corpse Explosion widen with it | `2781b85` |
| 1001, 33 | The Champ's Gloat gave 3, 4 or 5 Strength | 2, 3 or 4 | `a388a54` |
| 1003, 48 | Writhing Mass's Wither read as 0 damage; Flail's block was off | Wither has a damage row now; Flail's block equals its damage, 15 or 16 | `30a9315`, `f8cbe61` |
| 1028, 39 | Spot Weakness didn't see Wither as an attack | All 115 attacking moves in the game are now in the table, and nothing else is | `f6f4975` |
| 1013, 47 | Nemesis's Scythe was ruled out for two moves | Its cooldown only rules out the very next one | `f8cbe61` |
| 2005, 35 | Exploder's Explode showed 30 damage | Its intent is unknown; the 30 damage itself is unchanged | `fc8113e` |
| code | Back attacks multiplied before Weak and Vulnerable | The attack multiplies its rounded output last, and the intent its unrounded damage | `fc8113e` |
| code | Reptomancer stopped summoning one dagger early | `canSpawn` allows three others | `fc8113e` |

## Run and rewards

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 1000, 5; 1003, 7 | Match and Keep was off, which changed most shrine rolls | It is in the pool. Search can play it now, without seeing face-down cards | `2865777` |
| 1004, 0 | Neow's "transform two cards" always added a curse | A curse comes only with the curse drawback | `341261d` |
| 1010, 0 | Neow's max HP bonuses didn't heal | `increaseMaxHp(amount, true)` heals by the same amount | `21433d5` |
| 1001, 17 | Black Blood went to the end of the relic list | It takes Burning Blood's slot, and relic order decides trigger order | `8059444`, `0850514` |
| 1005, 12; 1014, 11 | Gold reward given when every monster escaped | `haveMonstersEscaped` skips the gold and its `treasureRng` roll | `cbf121f`, `b584fe0` |
| 1009, 3 | Orrery did nothing | It opens five card rewards | `b986778` |
| code | Old Coin, Necronomicon and Dolly's Mirror did nothing | 300 gold; adds Necronomicurse; duplicates a card | `b986778` |
| 1009, 3 | Could return to the shop after Orrery or Cauldron | Proceed opens the map, and the map's Return only goes back to the reward screen | `608cb5d` |
| 1009, 17 | First strong encounter exclusions existed for act 1 only | All three acts, so acts 2 and 3 got different encounters and bosses | `cfc0237` |
| 1014, 17 | A boss relic that can't spawn was replaced from the end of the pool | `returnEndRandomRelicKey` uses the front of the boss pool only | `c5088fa` |
| 1018, 13 | Eggs upgraded only some starting shop cards | `initCards` previews every card | `e2f6986` |
| 1005, 37 | An upgraded Searing Blow from a card reward had level 0 | The two-argument constructor set the flag but not the level | `234bec5` |
| 1005, 19 | Event fights gave wrong rewards | Masked Bandits gets a potion and gives Circlet if the player has Red Mask; Colosseum's first fight still rolls; gold counts Golden Idol | `a680855` |
| 1017, 28 and others | A Smoke Bomb skipped the end of a fight | It still rolls gold, an elite's relic and a potion, then throws them away | `9ea6d0c` |
| 1018, 46 | Falling could pick a bottled card | It can't | `9ea6d0c` |
| 1026, 21 | The Mausoleum gave its relic before checking Omamori | The curse is checked first | `9ea6d0c` |
| 1014, 34 | Ectoplasm spawned after act 1, and The Courier could spawn in a shop | All 35 `canSpawn` overrides in the game were compared with `relicCanSpawn` | `c1f4863` |
| 1017, 27 | Darkstone Periapt did nothing | 6 max HP for each curse | `180c455` |
| 1019, 43 | Matryoshka never added a relic | It starts with 2 charges | `ce8d6cc` |
| 1006, 47 | Sensory Stone used no roll | It rolls `miscRng` to pick its text | `5f93daf` |
| code | The Courier's replacement items were wrong | A new card has the bought card's type; a new relic's or potion's price keeps its base price and discounts; Smiling Mask and eggs apply | `6432f9d` |
| 2000, 7; 2004, 5 | Curses counted as transformable, so Fountain of Cleansing could spawn too often | `obtainRaw` counted each card twice and took curses with it | `d865633` |
| 2003, 1 | Prismatic Shard's rewards were off | The pools match the game's card library, but see below | `a0ef860` |
| 2001, 0 | Calling Bell's screen rolled no cards | `CombatRewardScreen.open` rolls a card reward, and the relic clears the screen after | `968e0f3` |
| code | Cauldron's screen rolled no cards | The same, and the relic takes it back out | `a02bcd0` |
| code | Designer's Clean Up took the gold and did nothing | It opens a two-card transform grid | `a02bcd0` |
| code | We Meet Again removed the first identical copy; Knowing Skull rolled a potion under Sozu | It removes the copy it picked; it rolls none | `a02bcd0` |

## Differences no simulator can match

- **Discovery.** `DiscoveryAction.update` regenerates its three cards on every frame it runs, about 0.25 seconds after the choice, and each generation rolls `cardRandomRng`. Until the next floor reseeds it, that stream depends on the frame rate. On seed 1016, the next hand's costs came 43, 46 and 49 rolls after the first one on different runs of the same fight. Mirror mode reports these as `game_nondeterminism`.
- **Note For Yourself.** The card comes from the profile's previous run, not the seed. Mirror mode reads it from the profile before each run.
- **The Courier's replacement card.** `getCardFromPool(false)` picks with `MathUtils`, which has no seed. Mirror mode takes the game's card, checks its type and rarity and compares its price.
- **End-of-turn exhaust order.** `DiscardAtEndOfTurnAction` runs `Collections.shuffle` with no seed. Ethereal cards and the like exhaust in a random order, so mirror mode compares the exhaust pile as a multiset.
- **Secret Portal.** It joins act 3's events after 800 seconds of play. The simulator assumes it has, and the scenario mod holds the game's clock at that point during mirror runs.
- **A curse's transform preview.** While a transform's confirm screen is up, `GridCardSelectScreen` changes its preview every 0.1 seconds, and a curse's preview rolls `cardRng` through `CardLibrary.getCurse`. The rolls depend on how long the screen stays up. Mirror mode takes the game's state after one.

## Known gaps

- **Other classes' cards.** The simulator has use code for 108 of the 320 cards in Prismatic Shard's pools. Rewards roll correctly, but the rest do nothing when played. Mirror mode stops with status `unsupported_card` when the agent takes one.
- **Choices the agent doesn't have.** Search can only exhaust no cards with Purity and only pick one card with Forethought+. That limits its play, not the simulation.
- **A potion reward with a full belt.** It stays on the screen in the game, so a player can discard a potion and take it. The simulator drops it. Making that action invalid breaks the agent, which expects to be able to choose it.
- **Rare interactions not fixed yet.** Rampage with both Double Tap and Duplication (the third hit doesn't see the second's bonus). Relic trigger order on card play is fixed rather than in the order acquired, which only showed up with Shuriken, Orange Pellets and negative Strength.
