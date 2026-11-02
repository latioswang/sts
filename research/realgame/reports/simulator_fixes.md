# Simulator fixes found against the real game

Every fix below is on branch `fidelity` of [latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed), one commit each after the research baseline `e47bb63`. That baseline is upstream `7476a81` plus `research/patches/sts_lightspeed.patch`. The research loop still builds the baseline, so none of this changes its results until it switches.

Rows with a seed were found by mirror mode and then checked against the game's bytecode. To reproduce one, build the baseline and run `sts_bridge mirror --seed_start <seed> --count 1`. Agent decisions depend on the simulator, so a run can take another path on a later build. Rows marked "code" were spotted while fixing a nearby bug and have not been seen on the real game yet.

## Combat

| Seed, floor | Simulator did | The game does | Commit |
|---|---|---|---|
| 1001, 13 | Distilled Chaos and Mayhem played their cards in reverse | `NewQueueCardAction` appends to the card queue, so cards play in the order they left the draw pile | `335c6da` |
| 1004, 16 | Mayhem played before the turn's draw | `MayhemPower` queues an action that queues the card, so it lands behind the draw | `672691b` |
| 1000, 14 | Curl Up triggered on a killing blow | `CurlUpPower.onAttacked` needs the damage to be less than current HP | `77bf15c` |
| 1012, 16 | Lizard Tail never revived the player | Its ready flag started at 0 and was never set | `15c4b0a` |
| code | Fairy Potion with Sacred Bark healed 1 HP out of combat | An operator precedence slip; it should heal 60% of max HP | `15c4b0a` |

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

## Differences no simulator can match

- **Discovery.** `DiscoveryAction.update` regenerates its three cards on every frame it runs, about 0.25 seconds after the choice, and each generation rolls `cardRandomRng`. Until the next floor reseeds it, that stream depends on the frame rate. On seed 1016, the next hand's costs came 43, 46 and 49 rolls after the first one on different runs of the same fight. Mirror mode reports these as `game_nondeterminism`.
- **Note For Yourself.** The card comes from the profile's previous run, not the seed. Mirror mode reads it from the profile before each run.
