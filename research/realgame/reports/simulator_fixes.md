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
| 2004, 8 | Metamorphosis and Chrysalis made an X-cost card cost 0 | They only change a positive cost | `90deb45` |
| 1000, 54 | The player stayed surrounded after a Spire guard died | The survivor no longer attacks from behind | `fc24e0e` |
| 2002, 18 | Necronomicon's copy of Rampage saw the first hit's bonus | `ModifyDamageAction` finds Double Tap's copy in limbo, but not Necronomicon's | `5c502b2` |
| 2102, 50 | Upgrading Blood for Blood put its cost back to 3 | It lowers the current cost by 1 | `381ef31` |
| 2103, 33 | Brutality cost HP before it drew, and Rupture didn't see it or Pain | It draws first, and both count as the player's own loss | `8126ff3` |
| 2106, 23 | Dropkick's damage left out Vigor | It's worked out when the card is played | `3b47231` |
| 2111, 6 | Jack of All Trades rolled when its action ran | It rolls when played, and adds its cards in that order | `3b47231` |
| 2106, 31 | Sever Soul exhausted after its damage | It exhausts first | `84a699d` |
| 2116, 6 | No Draw ended before Combust's HP loss, so Runic Cube drew | It ends after, so Runic Cube draws nothing | `84a699d` |
| 2120, 50 | Skills kept in hand kept their cost under Corruption | They go back to 0 on the next draw | `84a699d` |
| 2126, 50 | Unceasing Top didn't draw when Time Warp ended the turn on an empty hand | It does | `84a699d` |
| 2102, 40 | A minion leader's death cleared the action queue, so the turn's draw and its shuffle were lost, which Sundial counts | `clearPostCombatActions` needs every monster dying, and its minions only die in actions to come | `f5c6799` |
| 2103, 16 | No Draw always ended after Combust's HP loss | Powers act in the order gained, so No Draw played first has gone, and Centennial Puzzle or Runic Cube draws | `f5c6799` |
| 2100, 23 | Toy Ornithopter's heal was lost with Smoke Bomb | Its `HealAction` still runs | `f5c6799` |
| 2110, 16 | A Blood for Blood made mid-fight cost 4 once its turn ended | `makeCopy` counts the fight's HP losses | `b2c3491` |
| 2113, 16 | Mind Blast off the draw pile counted the pile when played | `PlayTopCardAction` runs `applyPowers` as it takes the card, with the next cards still there | `b2c3491` |
| 2114, 33 | Meat on the Bone checked HP after Burning Blood's heal | `endBattle` triggers it before other relics' `onVictory` | `b2c3491` |
| 2119, 36 | A shuffle for a card off the top of the draw pile didn't trigger Abacus or Sundial | `EmptyDeckShuffleAction` calls `onShuffle` in its constructor | `2787535` |
| 2126, 33 | Unceasing Top with an empty draw pile handed control back before its shuffle and draw ran | They run first | `d2615b4` |
| 4011, 16 | Anger's copy was a new card | `makeStatEquivalentCopy` keeps a cost that Snecko Oil or Confusion changed | `d2615b4` |
| 1006, 50 | A card made free for the turn stayed free if drawn again that turn | `Soul.update` runs `clearPowers` when a card lands on a pile | `e1f0ce4` |
| 2110, 33 (A10) | A skill Snecko Oil gave a cost kept it under Corruption until the next draw | `onCardDrawOrDiscard` also runs from `UseCardAction` | `1fc1ef5` |
| 1118, 48 | A monster's Intangible raised 0 damage to 1, so Body Slam with no block hit | `atDamageFinalReceive` only lowers damage above 1 | `f00b16f` |
| 2110, 33 (A10) | Snecko Oil rolling a card's own cost reset a lower cost for the turn | `RandomizeHandCostAction` only assigns when the roll differs | `b83e4d4` |
| 2115, 23 (A10) | A Blood for Blood from Nilry's Codex didn't count the fight's HP losses | `generateCardChoices` uses `makeCopy` | `729fa36` |
| 2324, 1 | Hello World+ and Creative AI+ rolled each other's cards, because powers ran in a fixed order | Powers are sorted by priority, in the order gained within a priority | `780e3d0` |
| 7335, 16 | A copy of Stack from Echo Form counted the discard pile again | `makeStatEquivalentCopy` keeps `baseBlock`, and only `applyPowers` works it out | `5b16c97` |
| 7403, 1 | Hovering Kite did nothing | The first manual discard each turn gives 1 energy | `9c03d07` |
| 7314, 35 | Nuclear Battery's Plasma paid nothing on turn 1 | `applyStartOfTurnOrbs` runs on turn 1 too | `9c03d07` |
| code | Tingsha and Tough Bandages did nothing | Each manual discard queues 3 damage to a random enemy, or a flat 3 block | `9c03d07` |
| code | The Specimen left the fight in an input state nothing handled | The dead monster's poison goes to a random enemy | `9c03d07` |
| code | Ring of the Serpent, Frozen Core and Holy Water were added beside the relic they replace | They take its slot | `9c03d07` |
| code | Emotion Chip did nothing | HP lost sets a pulse, and next turn each orb triggers | `9c03d07` |
| code | Nilry's Codex always offered Ironclad's cards | It offers the class's own | `9c03d07` |
| code | Poison, Mark and Choke on an Intangible Nemesis did full damage | `Nemesis.damage` caps HP loss too | `6803eff` |
| 7410, 33 | A new Eviscerate from Dead Branch or a potion ignored the turn's discards | Its `makeCopy` sets the cost for the turn, as Blood for Blood's does for the fight | `0bcad57` |
| 7102, 16 | Random cards in combat were always Ironclad's, whatever the class | `returnTrulyRandomCardInCombat` reads the class's src pools, each in reverse, with no HEALING cards | `ffa2500` |
| 6018, 16 (A8) | The Guardian's Vent Steam applied Vulnerable first, so Artifact stopped the wrong debuff | `useVentSteam` applies Weak, then Vulnerable | `7ab42f5` |
| 2509, 29 | Smoke Bomb in the Colosseum's first fight went to the map | `Colosseum.reopen` offers the second choice however the fight ended | `a6cc442` |
| code | Smoke Bomb in the Colosseum's second fight kept its relics in their pools | The event takes them off when the fight is chosen | `a6cc442` |
| 2504, 36 | A Darkling killed by Thorns on its own turn revived a turn early | `Darkling.damage` queues a `SetMoveAction` that lands after its `RollMoveAction`, so the wait comes back | `766198c` |
| 2417, 44 | A Parasite from Writhing Mass paid no Ceramic Fish gold | `AddCardToDeckAction` triggers `onObtainCard` during the fight, and the simulator's fight-end add was overwritten by the player's gold | `c1e59d6` |
| 8054, 18 | After Vault, monsters that count their own turns moved a turn early, because the count came from the battle's turn number | Looter's `slashCount`, The Maw's `turnCount`, the Heart's `moveCount` and the like don't move on a turn the monster doesn't take | `db710c3` |
| code | Hologram or Liquid Memories with a full hand moved the card to the end of the discard pile | `BetterDiscardPileToHandAction` leaves it where it is, and the pile's order decides the next shuffle | `aa5b858` |
| code | Dead Branch and Dark Embrace acted after the last kill, and Dead Branch rolled `cardRandomRng` | Both check `areMonstersBasicallyDead` | `aa5b858` |
| code | Innate cards beyond the first turn's draw were drawn before the battle-start relics' cards | `initializeDeck` uses `addToTurnStart`, which waits for the queue to empty | `aa5b858` |
| code | Mummified Hand only checked the front of the card queue | It removes every queued card from its choices | `aa5b858` |
| code | Constricted acted in the order gained, and No Draw came off after every other power unless Combust was played after it | `ConstrictedPower` has priority 105, so it acts last, and No Draw comes off in its own place in the order | `aa5b858` |
| code | Equilibrium kept the hand without marking it retained, so Perseverance, Windmill Strike and Sands of Time got nothing | `EquilibriumPower` sets `retain` on each non-ethereal card, which runs `onRetained` | `aa5b858` |
| 8101, 33 | Mutagenic Strength always wore off, and Gremlin Visage's Weak always landed, with Clockwork Souvenir | Those relics go on top of the queue at battle start, so the later one acts first, and its Artifact can stop the Strength's loss. Visage's Weak is at the bottom, so the Artifact stops that too | `e869a20` |
| 8201, 42 | Nightmare's copies came after every other power's cards at the start of a turn | Each `NightmarePower` takes its turn among the powers, in the order gained | `47b4456` |
| 9629, 33 | A card back from Stasis wasn't upgraded by Master Reality | It comes back through `MakeTempCardInHandAction`, whose constructor applies it | `9cd50f5` |
| 8305, 33 | A Bronze Orb taking the last card in the draw and discard piles got Stasis | `ApplyStasisAction` looks for two empty piles on every frame, so the next frame ends it before it applies the power, and the card stays in limbo for the rest of the fight | `35241d7` |
| 8002, 25 | A Byrd's Flight and a Shelled Parasite's Plated Armor came off as the hit landed, so the rest of the card saw a stunned monster and Go for the Eyes gave no Weak | Both powers queue `ReducePowerAction` behind what's already queued, and `onRemove` queues the change of move behind that | `494aa45` |
| 2321, 16 | Stack off the draw pile counted the discard pile when played, so Distilled Chaos gave one block too many | `PlayTopCardAction` runs `applyPowers` as it takes the card, before the cards ahead of it have landed | `800cfad`, on `other-classes` |

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
| 2002, 12 | Lagavulin's third idle turn rolled a move and kept Metallicize | It wakes by itself: Metallicize goes and a `SetMoveAction` sets the attack, so no AI roll | `fc24e0e` |
| 2104, 35 | A Darkling downed by Thorns kept attacking | `DamageAction` also drops the remaining hits of a half-dead attacker | `381ef31` |
| 2103, 39 | The Spire Growth went straight to Smash after two tackles on a low roll | It uses Constrict if it can | `7b7d75f` |
| 2100, 30; 2105; 2114 | Enrage only worked in the first slot | It works in any, as Colosseum's Gremlin Nob is in the second | `84a699d` |
| 2118, 33 | Philosopher's Stone gave Strength to empty slots, which Bronze Orbs later fill | It only gives it to monsters that are there | `84a699d` |
| 2116, 37 | A Darkling that went down kept its old debuff amounts, which came back with the next one | `powers.clear()` removes them, so Shockwave+ gives 5 Weak, not 6 | `c54860c` |
| 2116, 50 | Combust did nothing while Awakened One was half dead with nothing else alive | `areMonstersBasicallyDead` only looks at `isDying` and `isEscaping` | `dad125b` |
| 2103, 16 | The Guardian lost its Defensive Mode block when Combust crossed the threshold at the end of a turn | `ChangeStateAction` queues the block behind the queued monster start of turn | `dc6e79d` |
| code | Lagavulin woke on the battle's third turn, and Hexaghost's Burn+ started by the battle's turn, so Vault moved both early | `idleCount` only moves on a turn it takes, and `burnUpgraded` is set by Inferno | `aa5b858` |

## Run and rewards

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 9610, 0 | Neow's curse with two transforms gave no curse | `NeowReward.update` rolls it at once, and its obtain effect lands after the picks and before the transformed cards | `aa5b858`, `9ec3e2c` |
| code | Neow's curse with colorless cards rolled the curse first | `activate` rolls the cards from `cardRng`, and the curse comes after, in `update` | `aa5b858` |
| code | Cursed Tome ignored books the player owned | `randomBook` offers the ones they lack, and Circlet with none left | `aa5b858` |
| code | Omamori from Big Fish's box or Addict's robbery stopped that event's own curse | The curse's obtain effect is created first, as at The Mausoleum | `aa5b858` |
| code | Drug Dealer's transform left out bottled cards | It opens on `getPurgeableCards`, unlike other events | `aa5b858` |
| code | `getEvent`'s fallback to a shrine rolled the live `eventRng` | It uses the copy it was given, so the live stream doesn't move | `aa5b858` |
| code | Max HP could go below 1 | `decreaseMaxHealth` stops at 1 | `aa5b858` |
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
| 2002, 9 | N'loth's Hungry Face did nothing | It starts with one charge, so the next chest loses its relic and the sapphire key linked to it | `90deb45` |
| code | Eight cards had the wrong color, so transforming Brutality or Combust used the wrong range | All 361 cards were compared with the game's class folders | `3a39cef` |
| 2100, 0 | `miscRng` started one roll early, so Astrolabe's transforms came out one card along | `MainMusic` rolls it while the first act sets up | `64d4805` |
| 1005, 12 | A potion could drop when every monster escaped | The chance is 0, though the roll still happens | `64d4805` |
| 1005, 41; 2106, 23 | An egg didn't upgrade the cards on the reward screen it came from | It does | `8d46c42` |
| 2100, 10 | Ancient Tea Set only worked in the room after a rest site | It stays ready until the next fight | `8d46c42` |
| 2100, 30; 2105, 29 | Colosseum's second fight reseeded its streams and didn't shuffle | It's the same room, so they carry on, and `Colosseum.reopen` runs `preBattlePrep`, whose shuffle nothing uses | `8126ff3`, `3b47231` |
| 2113, 39 | Smiling Mask's purge price came down further with discounts | `ShopScreen.applyDiscount` sets it to 50 whatever else applies | `23d1fbd` |
| 2115 | Transforming several different cards removed the wrong ones, and curses left out one card too many | `removeSelected` ignored its sorted indices; a transform picks from a list that leaves out only the card itself | `e4aeb1d` |
| 4004, 27 | Colosseum could start at its second phase after Scrap Ooze | Every event starts its own phase; the simulator kept one number between events | `66b4f61` |
| code | Torii cut event damage | Only Shining Light's: other events' damage has no owner or is HP loss | `66b4f61` |
| code | The act change moved a `cardRng` counter of exactly 250 or 500 on | `dungeonTransitionSetup` leaves those alone, and 0 | `66b4f61` |
| code | Du-Vu Doll gave no Strength | Its counter is every curse in the deck, including Ascender's Bane | `16613b4` |
| 2111, 20 | Knowing Skull's options were gold, card, potion | They are potion, gold, card | `68c57ef` |
| code | Tiny House's gold left out Golden Idol | `addGoldToRewards` counts it | `b425da6` |
| 1101, 22; 1103, 24 | Orrery's and Cauldron's cards in a shop rolled a 3% rare chance | `ShopRoom` sets `baseRareCardChance` to 9 | `3347784` |
| code | Removing a card with all three bottles full, or removing a curse, left the bottles' indices stale; a removed Necronomicurse stayed gone | Found in [jdc5549/sts_lightspeed](https://github.com/jdc5549/sts_lightspeed)'s fix list; `onRemoveFromMasterDeck` adds a new Necronomicurse | `8bf1499` |
| 9215, 34 | Transforming a colorless card picked from an alphabetical list, and Astrolabe didn't upgrade it | `srcColorlessCardPool` is `colorlessCardPool` backwards, and `transformCard` upgrades whatever it made | `7923664` |
| 8103, 11 | Omamori stopped a curse from Note For Yourself | The event adds the card itself and runs `onObtainCard`, and Omamori is in the obtain effects it doesn't use | `dfda16c` |
| 8200, 0 | Neow's "lose your starting relic" looked for the class's starting relic by name | `NeowReward` takes whatever is in slot 0, which a boss relic can already hold | `0d65403` |
| 7459, 0 | A map node on floor 8 or 14 that got no room counted as a monster at once, so the node to its right couldn't be one | It stays empty until `lastMinuteNodeChecker`, after every node has had its turn | `9fab0df` |
| 9108, 25 | Face of Cleric did nothing after a Smoke Bomb escape, and ignored Magic Flower | `endBattle` calls `onVictory` however the fight ended, and the room is still in combat | `8571846` |

## Ascension

Runs at ascension 0 can't show these. Rows marked "code" came from comparing each monster and event that reads `ascensionLevel` with the simulator, and wait to be seen on a run.

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 4000, 0 | Neow's 10% loss took 8 of 75 max HP | `(int)(maxHealth * 0.1f)` is 7; its bonuses use the same number, doubled for 20% | `66b4f61` |
| code | Neow's damage left `curHp / 10 * 7`, which is only right at a round number | It takes `currentHealth / 10 * 3` off | `66b4f61` |
| 4001, 3 | A16 cut shop prices by 20% | `applyDiscount(1.1f, false)` raises them and leaves the purge price | `8f1fb08` |
| 4000, 6 | Gremlin Nob at A18 never used Skull Bash | It opens with it, then goes Rush, Rush, Skull Bash | `69eb46c` |
| 4002, 22 | Snake Plant at A17 could Spore on consecutive turns | It keeps two turns between them | `69eb46c` |
| code | Book of Stabbing at A18 didn't add a hit on a single stab; Bronze Automaton at A19 fired Hyper Beam every 4 turns, not 6; Romeo at A17 never repeated Cross Slash; Large Acid Slime at A17 tested the medium one's spit, which skipped an AI roll; Blue Slaver at A17 could rake twice | `getMove` and `takeTurn` of each | `69eb46c` |
| code | Giant Head at A18 was a turn late, and The Collector's Mega Debuff stayed 3 at A19 | Its `count` starts one lower; 5 turns at A19 | `69eb46c` |
| code | Darkling's Nip added 2 at A2 | Its roll already moved to 9 to 13 | `69eb46c` |
| code | Gremlin Leader's block and Nemesis's Burns changed at A3; The Collector's block changed at A4 | A18, A18 and A9 | `69eb46c` |
| code | Snecko's A17 Tail Whip and Time Eater's Ripple applied debuffs in the other order, which decides what Artifact stops | Weak then Vulnerable, and Vulnerable then Weak | `69eb46c` |
| code | Mind Bloom's I Am War paid 50 gold up to A14 | `ascensionLevel >= 13` pays 25 | `66b4f61` |

## Differences no simulator can match

- **Discovery.** `DiscoveryAction.update` regenerates its three cards on every frame it runs, about 0.25 seconds after the choice, and each generation rolls `cardRandomRng`. Until the next floor reseeds it, that stream depends on the frame rate. On seed 1016, the next hand's costs came 43, 46 and 49 rolls after the first one on different runs of the same fight. Mirror mode reports these as `game_nondeterminism`.
- **Note For Yourself.** The card comes from the profile's previous run, not the seed. Mirror mode reads it from the profile before each run.
- **The Courier's replacement card.** `getCardFromPool(false)` picks with `MathUtils`, which has no seed. Mirror mode takes the game's card, checks its type and rarity and compares its price.
- **End-of-turn exhaust order.** `DiscardAtEndOfTurnAction` runs `Collections.shuffle` with no seed. Ethereal cards and the like exhaust in a random order, so mirror mode compares the exhaust pile as a multiset.
- **Secret Portal.** It joins act 3's events after 800 seconds of play. The simulator assumes it has, and the scenario mod holds the game's clock at that point during mirror runs.
- **A curse's transform preview.** While a transform's confirm screen is up, `GridCardSelectScreen` changes its preview every 0.1 seconds, and a curse's preview rolls `cardRng` through `CardLibrary.getCurse`. The rolls depend on how long the screen stays up. Mirror mode takes the game's state after one.
- **The time scale.** `STS_TIME_SCALE` speeds up the game's timers, but an action still takes at least a frame. When a minion leader dies with minions alive, the queue keeps running until the death animations end and `endBattle` clears it. At 5x those animations can end before a heal queued behind Reaper's effects runs. Seed 2123 reached the same state on floor 42 at both speeds, and HP afterwards was 76 at 5x and 86 at normal speed, which is what the simulator says. Rerun a difference like this at normal speed before believing it.

## Known gaps

- **Other classes' relics and runs.** All other classes' cards are ported, and have been played on the game inside Ironclad runs. Their starting relics and class relics have barely been checked, because runs have only just started as those classes.
- **Stance on the game.** The mod's JSON doesn't report stance, so it's only compared indirectly, through intents, energy and HP.
- **Choices the agent doesn't have.** Search can only exhaust no cards with Purity and only pick one card with Forethought+. That limits its play, not the simulation.
- **A potion reward with a full belt.** It stays on the screen in the game, so a player can discard a potion and take it. The simulator drops it. Making that action invalid breaks the agent, which expects to be able to choose it.
- **Rare interactions not fixed yet.** Rampage with both Double Tap and Duplication (the third hit doesn't see the second's bonus). Relic trigger order on card play is fixed rather than in the order acquired, which only showed up with Shuriken, Orange Pellets and negative Strength.
- **Found reading the bytecode, too rare to fix yet.** The Bomb fires before other end-of-turn powers, whatever the order gained, which shows with Combust. Start-of-turn relics run in a fixed order, which shows with Emotion Chip and Mercury Hourglass. Evolve's draw clears Scrape's draw history in the game. A stacked Hex gives as many Dazed as its amount, and no fight stacks it.
- **Player powers kept as plain fields.** Powers in the simulator's status map now trigger in the game's order. Strength, Dexterity and a few others live outside that map, so their place in the order isn't modeled. No Draw before Combust is still handled on its own.
- **Empty Cage and Tiny House on the game.** The agent never picks them, so they've only been compared with the game's code. Empty Cage's automatic removal with two or fewer removable cards isn't in the simulator.
