# C2 — The strategy layer and the harness, as the code is (2026-11-16)

Sources: `research/cpp/FairAgent.{h,cpp}` (**FA**), `event_policy.inc` (**EP**), `FairSearch.cpp` (**FS**), `research/vf/run_features.h` (**RF**), the model headers and trainers, `research/eval/`, and lightspeed's legacy policy `/home/user/sts_lightspeed/src/sim/search/ScumSearchAgent2.cpp` (**SSA2**), FairAgent's fallback for anything it does not handle (FA:1391-1392). A0 / A10 / A20 / Heart = `research/best_params.txt`, `best_a10_params.txt`, `best_a20_params.txt`, `best_heart_params.txt`. **[computed]** = my tabulation of the bests' 2,000-seed files in `research/runs/` (12,000 / 12,000 / 14,000 / 6,000 games).

## 0. Where the code disagrees with the brief

1. **Root move prior:** A0/A10/A20 only. The Heart file has no `prior_mlp`.
2. **bf_\* decides only at A0 (bf_g1, ×10) and A20 (bf_g3, ×25), in card rewards.** On A10 and Heart `vr_pure=1` returns (FA:894-908) before the bf bonus is read (FA:913-926), and `bf_shop`, `bf_upg_bonus`, `bf_upgrade`, `bf_remove` are unset, so `bf_bonus` and `bf_model` are inert there. Heart's `bf_relic_w=10` is inert too: `vr_pure_relic=1` returns (FA:562-574) before FA:581.
3. **pm_\*:** A10 (pm_3) and A20 (pm_1) only. A0 and Heart use the hand map DP.
4. **"Knowledge-base heuristics":** on A10 and Heart, card rewards and boss relics have no KB term (pure V).
5. **No active configuration simulates a future fight** (`cf_pick`, `bf_pick`, `elite_sim` unset) **or rolls a copy forward** (`vr_event`, `vr_key` unset). The code exists (FA:394-446, 529-547, 686-702, 1158-1216).
6. **Neow** is neither KB nor learned: lightspeed's legacy rule, mostly random (§1).
7. **Score in code:** `min(floor,51)/51 + win` (run.py:49-50; program.md:1760 still says floor/55). Heart: a win counts only if act ≥ 4 (run.py:38); floors 52–55 add nothing.
8. **"Fixed compute"** in code is a simulation cap per combat decision (`sims`, early stop, FS:562-574), not a time limit. Wall-clock equality is an offline calibration (§5).
9. **Inert A20 settings:** `rest_boss_frac=1.0` (vr_rest=2 returns first, FA:1327-1359). At A10/A20 the hand-DP weights act only through one input of the path model (FA:815).

## 1. Decision inventory

| Decision | How it is decided today | Where |
|---|---|---|
| Neow | Legacy: option 0 if slot 1 or 2 shows a curse drawback, else **uniformly random**. A ranking exists (FA:629-660) but `neow_pol` is off (tested neutral, program.md:2777) | FA:1292-1299 → SSA2:366-372 |
| Card reward | A0/A20: KB tier value + hand bonuses + `bf_bonus`×Δbf + `vr_bonus`×ΔV; take the best iff ≥ threshold. A10/Heart: argmax V(deck+card), taken iff > V(skip) | FA:827-938 |
| Boss relic | A0/A20: KB tier + `vr_relic_w`×ΔV. A10/Heart: argmax V. Never skips | FA:549-626 |
| Shop | Fixed order, one purchase per call: (1) removal whenever affordable; (2) relic — A0/Heart the first affordable slot, A10/A20 the best ΔV (always buys); (3) potions only on Heart in act 4, first affordable; (4) best card by KB + `vr_bonus`×ΔV (gold paid) iff ≥ 7.0; (5) leave | FA:941-1028 |
| Removal / transform target | A0/Heart: curse > Strike > Defend > lowest KB value. A10/A20 (remove screens only): curse, else argmax V after removal | FA:1056-1070, 1095-1104 |
| Upgrade target | KB value + 2 for Bash + 2 for Powers + `vr_upg_w`×ΔV | FA:1105-1124 |
| Duplicate / obtain / bottle | Highest KB value | FA:1125-1127 |
| Rest vs smith | A0/A10/Heart: rest iff HP < 50% (A10 60%; 70% before the boss), else smith. A20: V(heal) vs V(after the agent's own upgrade) | FA:1324-1362 |
| Recall (ruby key) | Heart only: HP ≥ 65%, never at the pre-boss fire, acts 1–3 | FA:1305-1322 |
| Lift / dig / toke | Never chosen on purpose: the handler issues only rest, smith, recall (legacy random fallback when neither rest nor smith is legal) | FA:1361-1362; SSA2:204-214 |
| Events | Hand table, 52 event cases with HP thresholds, headed "Ironclad, A0, goal: Act 3 boss", used on every track. Match and Keep: first legal action | EP:1-234; SSA2:362-364 |
| Map path | A0/Heart: backward DP over the act map with constant room values (below). A10/A20: path MLP, argmax over legal next nodes | FA:663-684, 744-824 |
| Chest | Open, unless Cursed Key and the chest is not large | FA:1264-1273 |
| Emerald / sapphire key | Heart: emerald always; sapphire at the first chest that offers it (gives up that relic) | FA:1367-1374 |
| Gold, relic, potion rewards | Always taken, in that order. A potion is silently lost when the belt is full | SSA2:269-279; sim GameContext.cpp:1224-1227 |
| Potion use | In combat only, as MCTS actions. Hallway fights exclude potions unless HP < 25% (`hall_potions=0`). Smoke Bomb barred vs bosses | FA:133-136; FS:135-139 |
| Potion discard, out-of-combat drinking | Never (`oc_potions` off) | FA:1229-1240 |

Map DP room values at A0 (FA:663-684): monster 0.5; elite +1.5 if HP ≥ 60% else −3 (+1 in act 1); rest 3 if HP < 50% else 1; shop 3 if gold ≥ 150 else 0; event 2; treasure 1.5; discount 0.85 per row. Heart adds +2 for the burning elite while the emerald key is missing and HP ≥ 60% (+3 more in act 3) (FA:773-777).

## 2. The learned models

**Shared input: 666 run features** (RF:22-66): 24 scalars (floor, act, HP fraction, HP, max HP, gold, potion count and capacity, deck size, counts of upgrades / curses / Strikes / Defends / attacks / skills / powers / rares / uncommons, mean and sum of KB tier values, ascension, map row, relic count, upgradeable count); act-boss one-hot (66); relic one-hot (192); card-id vector (384) where each copy adds 0.34, or 0.5 if upgraded.
**Not represented:** the map ahead (only the row index); potion identities; relic counters; per-card state (Ritual Dagger, Searing Blow level); upgrade state beyond the 0.34/0.5 sum (three plain copies ≈ two upgraded); draw structure; the A20 second boss; keys (only in the 670-input variant, RF:68-73).

| Model | Shape | Target and data | How it enters a decision |
|---|---|---|---|
| **vr_171** (A0, A10) | 666→64→64→1 ReLU, 46,913 params (`models/vr_171.meta`) | Final score of the run the state came from, `min(floor,51)/51 + win`: **one label per run**, shared by its ≈160 states (train_vr3.py:3, 89, 178). 519,300 training runs, pooled tracks, 16 states per run per epoch. Holdout RMSE 0.455 vs 0.715 for predicting the mean | Always as V(after choice) − V(now). A0 weights: cards 60, relic 30, upgrade 60, shop cards 60. A10: pure argmax for cards, boss relics, removal, shop relics; 45 for upgrades and shop cards |
| **vr_251** (A20) | same shape | vr_171 fine-tuned by train_dec.py on 17,556 A20 card-reward decisions; label = mean score of 8 playouts per option, centred within the decision, anchored to vr_171 (λ=1) | Weight 162 on cards, relics, upgrades, shop cards; removal, rest-vs-smith and shop relics by V directly |
| **vr_222** (Heart) | 670→256→256→1, 237,825 params | Same regression with Heart wins only (`VR_WINMODE=heart`, train_vr3.py:16-17); ≈270k runs incl. 15,000 expert-iteration Heart games (program.md:512); holdout corr 0.44 | Pure argmax for cards and boss relics; weight 20 for upgrades and shop cards |
| **bf_g1 / bf_g3** (A0 / A20) | 666→128→8 sigmoids (bf_mlp.h:54-68) | Outcomes of simulated fights from logged decision states and each hypothetical option (card added, upgraded, removed), same seeds per decision, 100-sim search, no potions (FA:422-446, 461-497): win and HP left vs the act boss and vs one sampled hallway/elite fight; boss damage fraction; win / HP / damage vs a random next-act boss. Loss = BCE + 2 × MSE on option-minus-base differences (train_bf.py:118-127). g1: 1,000 A0 games; g3: 128,594 rows from 1,500 games (program.md:1330). Paired correlation of predicted vs observed differences ≈ 0.10–0.14 for g1 (program.md:1810-1811) | Weighted sum of the 8 heads (FA:499-511) × `bf_bonus`, added to the KB card value. Card rewards only |
| **pm_1 / pm_3** (A20 / A10) | 686→64→64→1 | Input: 666 features of the pre-move state + 20 path features (FA:1448-1471): destination room one-hot, min and max count of each room type over all paths to the boss row, log path count, the hand DP's value. Label: mean score of the agent's own playouts from re-seeded copies per option, centred within the decision (train_path.py). 11,937 / 25,304 training decisions (`pm_*.meta`) | Replaces the DP's argmax (FA:796-822) |
| **Root prior** (`pfm_weights.inc`) | 204 combat features ⊕ 16-d action-token embedding ⊕ 4 target features → 64 → 1, softmax over legal actions (pf_mlp.h) | MCTS root visit distributions, 298k decisions (program.md:1572) | PUCT prior at the root only (`prior_mlp_depth=0`, FS:303, 451) |

`rv_mlp.h` (666→32→1 P(win)) is compiled in but unused (`rv_pick` off).

## 3. How deck building actually works

- **Take or skip, A0/A20** (FA:909-936): value = KB tier (1–10 over 73 cards, +0.5 if upgraded; colorless and unknown = 3.0, FA:242-251) + 2 if it is the deck's first AoE card in acts 1–2 + 3 if the deck has fewer than three of ten listed "scaling" cards (FA:316-339) + bf and V bonuses. Take the best iff value ≥ 5.0 + 0.1 × (deck size − 15)⁺ − 1.0 in act 1.
- **Take or skip, A10/Heart:** the option with the highest V, iff above V of the unchanged deck.
- **Synergy and scaling** exist in three forms: hand lists (5 AoE, 10 scaling, 9 block cards) with want-counts; a KB pair table (`kb_synergy.inc`: 1,422 card–card and 1,741 relic–card entries, capped ±3) that is on only at A10, where it reaches only shop purchases; and whatever V's 64 first-layer units learned from run outcomes over card counts.
- **No plan or archetype is carried.** FairAgent's state is parameters, RNG, loggers and counters (FA.h:58-79). The boss-specific and Act-4 card bonuses (`boss_prep`, `act4_prep`, FA:340-358) are off in all four bests; the act boss reaches decisions only as a one-hot input of V and bf.
- **Removal:** bought at every shop where affordable, before relics and cards (FA:944-947).

## 4. Planning horizon

- **Looks ahead structurally:** only the map. The DP covers the rest of the act, with constant room values at the *current* HP and gold; the path model sees room-type counts, not their order.
- **One-step with a long-horizon estimate:** every V hook compares states one choice apart; V estimates the final score under past agents' play. bf looks to the act boss and one next-act boss.
- **Greedy, no lookahead:** events, Neow, chest, keys, shop ordering, potion pickup.
- **Simulated futures:** none in play (§0.5). Offline only: bf labels, and `branchEval`/`branchEval2` (FA:1395-1573), which label card, relic, path and rest options with 8–16 full playouts.
- **Potions:** spending is decided inside combat search by a flat credit per potion kept: `potion_v` on a win (4; A0 8 outside bosses, 4 in bosses; A20 2 in bosses), half of that on a loss or rollout cutoff (FS:347, 370, 373), against 1 per HP and 35 for the win. Identity, upcoming rooms and run state do not enter; V sees only the potion count. Lost runs end with 0.15–0.21 potions unused [computed].

## 5. The harness

- **Score:** per game `min(floor,51)/51 + win` (run.py:49-50). Comparison: paired by seed, 4,000-sample bootstrap, keep iff dscore > 0 and P(improve) ≥ 0.9 (run.py:89-99; exp_fresh.sh:31-34).
- **Seeds:** agent seed = `seed × 0x9E3779B97F4A7C15 + 12345` (eval_main.cpp:51). Fixed splits in run.py:13-17 (dev 100000+100, val 200000+300, test 900000+500, train 300000+). Since 2026-11-05 every keep decision uses a never-used slice `fs<start>n<count>` from `eval/fresh_next.txt` (now 1,443,700), with the best re-run on it (exp_fresh.sh).
- **Compute accounting:** the agent's budget is `sims` per combat decision (A0 1000, A10 950, A20 1050, Heart 775; `early_stop=1 es_frac=0.1`). Two equalisers: `tools/calibrate.sh` times 8 single-process games of best vs candidate on an idle machine and rescales `sims`; later phases match mean search actions per game within 1% (`tools/f43_calib.sh`). Games run in forked children, 4 in parallel, with a 900 s hang guard; a crash is an `aborted=2` row excluded from pairing (eval_main.cpp:159-194).
- **Cost [computed]:** A0 1.59 s/game, 465 combat decisions, 3.67M search actions; A10 1.32 s; A20 0.78 s (286 decisions); Heart 1.83 s. That is ≈ 3 ms and ≈ 6–8k simulator actions per combat decision. Out-of-combat cost is negligible [inferred: a few MLP evaluations per screen].
- **Per-game log** (one TSV row, eval_main.cpp:215-220): seed, win, floor, act, hp, max_hp, deck size, battles, combat decisions, aborted, secs, search actions, death_enc (encounter of the last battle), potions_left. **Not logged:** deck or relic contents, per-fight HP loss, path taken, any strategic choice (only in opt-in `FAIR_TRACE` traces and the binary training logs).
- **Leak test** (eval_main.cpp:80-111) covers combat search only; out-of-combat code has none (except `ev_leaktest` for the unused `vr_event`).

## 6. Structural weaknesses (ranked) [inferred]

Context [computed]: A20 reaches act 2 in 78% and act 3 in 31% of games; 53% of its deaths are on the four boss floors (16, 33, 50, 51). Heart: 59% reach act 4, 48.5% of those win; 40% of all Heart-track losses are at the Heart.

1. **Combat and strategy share no currency.** Fights are scored by a fixed linear utility (35 + HP + flat potion credit, FS:345-373) that never sees the run: HP is worth the same before a fire as before the boss. V prices HP and potions for the run but is never asked what a fight outcome is worth. *Change:* value fight outcomes by the run value of the post-fight state (or a learned context-conditioned utility), and value strategic options by predicted fight outcomes. A keyword search of program.md found only constant re-tunings (`lowhp`, `hp_w`, `potion_v`).
2. **One noisy label per run cannot resolve single decisions.** V's RMSE is 0.455 on a target with spread 0.715, while at A20 a random card pick costs only 0.029 against the agent's own, and an 8-playout oracle did not beat it (program.md:373). *Change:* dense targets — a fight-outcome model (death probability, HP and potions spent per encounter given deck, relics, HP), about 20 real labels per game, composed along the map into a run value.
3. **The deck is a bag of counts.** No draw order, energy curve, per-copy upgrades, relic counters or potion identities (RF:42-65); synergy must be inferred from wins by 64 hidden units. *Change:* a structured deck encoder trained on fight-level targets, or cheap simulation statistics of the deck as features.
4. **Nothing is planned across decisions.** Every hook is a one-step argmax; removal is unconditional, relics are bought in slot order on A0/Heart, gold is never budgeted, nothing commits to what the act boss or the Heart requires. *Change:* improve the ≈100 strategic decisions of a run as a sequence: search over act-level plans with a fast outcome model, or reinforcement learning on the strategic policy (games cost 1–2 s).
5. **The map layer ignores HP dynamics.** Room values are constants at today's HP; elite choice is an HP threshold that never looks at the deck (FA:667-677); A0 and Heart have no learned path model. Path choice is the one decision with measured headroom (program.md:333-337). *Change:* a DP over (node, HP distribution) driven by the fight-outcome model of item 2.
6. **Potions are an unmanaged inventory.** First come, first kept; no swap, no purchase, no identity value, a hard hallway ban (FA:135; SSA2:277-278). *Change:* identity- and context-aware potion value in item 1's currency.
7. **A hand-rule tail written for A0 / Act 3:** random Neow, the event table (EP:1), unused Girya / Shovel / Peace Pipe, sapphire key at the first chest, colorless cards at 3.0. Each is small (Neow ranking neutral, V-on-events neutral or worse, program.md:543, 2777); only a unified evaluator would recover it.
8. **Strategic compute is zero online and small offline.** Every combat decision gets the same cap and the strategy layer spends nothing, while its models were trained on 1,000–1,500 games (bf) or 12–25k decisions (pm). Online reallocation to boss fights did not keep (F19, F22); the open lever is offline computation distilled into models, which the equal-wall-clock rule does not charge.
