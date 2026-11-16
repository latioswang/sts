# D1 — Anatomy of the losses (existing data only)

Written 2026-11-16, stage 1 ("understand"). Nothing was run, built or changed. Every number is [computed] from existing files unless marked [inferred]. Working scripts: `/tmp/claude-0/-home-user/db75817a-c741-5eaf-84e7-4b52f90d3cc8/scratchpad/gbdiag/*.py`.

## 1. Data used

**Row format** (14 columns): `seed win floor act hp max_hp deck battles decisions aborted secs actions death_enc potions_left` (`eval/eval_main.cpp:215-220`, `eval/run.py:19-20`; the header comment at `eval_main.cpp:5-7` lists 12 and is stale). `death_enc` is the encounter of the last battle started (`cpp/FairAgent.cpp:144`), so the killing enemy is already logged for every game. `hp`, `max_hp`, `deck` are end-of-run values; relics, gold and per-fight records are not logged. A Heart win needs `act >= 4` (`run.py:33-34`); Heart-track files end in `.a0.tsv` (ascension 0, `keys=1`). Score caps the floor at 51 (`run.py:50-51`).

| Track | Best (1x) | sims | Slices | Games | Wins | Win rate | Mean floor |
|---|---|---|---|---|---|---|---|
| A0 | f45a0_sf | 1000 | 6 x 2000 | 12,000 | 10,251 | 85.4% | 48.9 |
| A10 | f43a10_sf | 950 | 6 x 2000 | 12,000 | 6,303 | 52.5% | 42.1 |
| A20 | f43a20_sf2 | 1050 | 7 x 2000 | 14,000 | 1,058 | 7.6% | 31.3 |
| Heart | f45heart_sf | 775 | 3 x 2000 | 6,000 | 1,726 | 28.8% | 47.4 |
| A20 Heart (F47) | f47a20h_a | 1050 | 1 x 2000 | 2,000 | 6 | 0.3% | 27.1 |

No crashes; 4 games hit the 3,000-step fight guard (`FairAgent.cpp:159-162`). Search-scaling pairs: F39 (`tools/f39_scale.sh`: 2x and 4x, 1,000 seeds, older bests), F42 (`tools/f42_budget.sh`, older bests), F44 and F48 (current bests; 2,000 seeds each). Also read: 240 existing game traces of the current A20 best (F50, slice fs1424500; outside the repository, in `/tmp/claude-0/-home-user-sts/db75817a-c741-5eaf-84e7-4b52f90d3cc8/scratchpad/traces/a20_f50`), parsed with `analysis/trace_stats.py` and my own per-fight parser.

**Disagreements with the brief.** (i) Heart "about 30%" is 28.8% at 1x; the 35.0% milestone ran at 2x search (`results.tsv:686`). (ii) The Heart best uses neither the learned root prior nor the learned path model (`runs/f45heart_sf.params` has no `prior_mlp` or `map_model`; defaults 0 at `FairAgent.cpp:92`, `:796`); the A0 best has no `map_model` either. (iii) "2x adds several points on every track": at A20 it adds +1.45 points on the current best (+2.0 pooled).

## 2. Where runs end, per track

Each cell: death rate among runs that entered the stage / share of all games (%).

| Stage | A0 | A10 | A20 | Heart |
|---|---|---|---|---|
| Act 1 before boss (F1-15) | 0.8 / 0.8 | 3.7 / 3.7 | 7.2 / 7.2 | 1.2 / 1.2 |
| Act 1 boss (F16) | 1.2 / 1.2 | 5.1 / 4.9 | 15.7 / 14.5 | 4.4 / 4.3 |
| Act 2 before boss (F18-32) | 1.8 / 1.8 | 10.6 / 9.7 | **34.5 / 27.0** | 6.0 / 5.7 |
| Act 2 boss (F33) | 4.0 / 3.9 | 17.4 / 14.2 | **39.6 / 20.3** | 11.5 / 10.2 |
| Act 3 before boss (F35-49) | 2.2 / 2.0 | 8.8 / 6.0 | 30.7 / 9.5 | 7.3 / 5.7 |
| Act 3 boss (F50) | 5.4 / 4.9 | 14.7 / 9.0 | 30.2 / 6.5 | 9.8 / 7.2 |
| Act 3 second boss (F51, A20) | – | – | 49.4 / 7.4 | – |
| Act 3 cleared without three keys | – | – | – | 9.5 / 6.3 |
| Shield and Spear (F54) | – | – | – | 3.2 / 1.9 |
| The Heart (F55) | – | – | – | **49.9 / 28.7** |
| Win | 85.4 | 52.5 | 7.6 | 28.8 |

| Share of losses (%) | A0 | A10 | A20 | Heart |
|---|---|---|---|---|
| Ending in act 1 / 2 / 3 / 4 | 14 / 39 / 47 / – | 18 / 50 / 32 / – | 24 / 51 / 25 / – | 8 / 22 / 27 / 43 |
| On a boss floor (16, 33, 50, 51 at A20, 55) | 68 | 59 | 53 | 71 (Heart alone 40) |
| On the floor before a boss (15, 32, 49) | 0 | 0 | 0 | 0 |
| On floors 14 / 31 / 48 | 1.9 | 3.3 | 3.7 | 1.5 |
| Anywhere else | 30 | 37 | 44 | 19 (+ 8.8 key-less) |

Floors 15/32/49 are always rest sites [inferred from the zero counts and the map rule].

**Killer type, share of all games (%).** A20: boss 48.7, elite 23.2, hallway 18.5, event fight 2.0. A10: 28.2 / 10.1 / 7.9 / 1.3. A0: 9.9 / 2.6 / 1.6 / 0.4. Heart: boss 50.4 (the Heart 28.7), elite 8.3, hallway 5.1, event 1.1, key-less 6.3.

**A20 per boss** (deaths / a third of entrants; assumes equally likely bosses [inferred]): Hexaghost 18%, Guardian 15%, Slime Boss 14%; Collector 43%, Automaton 42%, Champ 33%; first act-3 boss Awakened One 40%, Donu and Deca 28%, Time Eater 23%; as second boss about 63 / 46 / 38%. Elites: Slavers end 4.8% of all games, Book of Stabbing 4.0%, Gremlin Leader 4.0%. Non-boss deaths spike on each act's first elite floor (6, 23, 40): floor 23 alone holds 560 elite deaths (4.0% of games).

**A20 Heart** (2,000 games): 3.65% clear act 3 (7.6% without keys); 23 of those 73 are key-less; 42 Heart fights, 6 won (14%).

## 3. What extra search buys and where

Death rate among entrants (%), 1x → 2x, pooled over all 2x pairs of a track, same seeds:

| Stage | A20 (n=5,000) | A10 (n=5,000) | A0 (n=4,000) | Heart (n=4,992) |
|---|---|---|---|---|
| Act 1 before boss | 7.3 → 6.6 | 3.6 → 3.0 | 0.7 → 0.6 | 1.1 → 1.0 |
| Act 1 boss | 16.2 → 14.4 | 5.4 → 4.3 | 1.2 → 1.1 | 5.0 → 4.4 |
| Act 2 before boss | 34.4 → 32.0 | 11.1 → 10.2 | 2.2 → 1.4 | 6.9 → 5.6 |
| Act 2 boss | 40.9 → 38.6 | 18.5 → 16.1 | 4.5 → 3.5 | 13.2 → 10.6 |
| Act 3 before boss | 30.9 → 29.2 | 9.3 → 7.8 | 2.5 → 2.0 | 7.3 → 6.4 |
| Act 3 boss | 31.2 → 27.0 | 16.1 → 14.4 | 6.2 → 4.8 | 10.4 → 9.0 |
| Act 3 second boss | 50.0 → 46.9 | – | – | – |
| The Heart (per Heart fight) | – | – | – | 49.2 → 47.1 (SE 1.3) |
| Win rate | 7.2 → 9.2 | 50.3 → 55.2 | 83.8 → 87.2 | 27.9 → 31.9 |

Current bests alone (F44/F48): A20 7.65 → 9.10, A10 50.4 → 56.6, A0 84.8 → 89.0, Heart 27.9 → 32.7. 1x → 2x → 4x (F39, older bests, `program.md:232`): A20 6.0 → 8.5 → 9.3; A10 48.2 → 52.8 → 57.5; Heart 26.8 → 30.7 → 32.5.

- **No floor stops being a death floor.** Every stage's death rate falls by 1–4 points. Relative reduction per doubling: about 20–35% at A0 (act 2 onward), 8–20% at A10, 5–13% at A20.
- **Where the wins come from** (replace one stage's rate by its 2x value): A0 +3.45 points in total, of which act-3 boss +1.2 and act-2 boss +0.9; A10 +4.9: act-2 boss +1.5, act-3 boss +1.0; A20 +2.0: the two act-3 bosses +0.9, act 2 +0.5; Heart +3.9: Heart fight +1.1, act-2 boss +0.8.
- **The Heart fight barely responds to search**: 49 → 47% lost at 2x (z = −1.6), 51 → 49% at 4x (SE 2.9). The Heart-track gain comes from reaching act 4 more often (56.8 → 61.7%).
- **A20 flattens**: the second doubling adds +0.8 points; at A10 each doubling adds about +4.6.
- **Outcomes are only half seed-determined.** A20: of 153 seeds won at 1x, 79 are won at 2x (52%; win correlation 0.43). A0: 43% of seeds lost at 1x are won by the same agent at 0.675x search (`f52_ctl675`).

## 4. Logged columns: losing runs vs winning runs

A20, by where the run ended:

| Group | n | Max HP | Deck | Battles | Decisions per battle | Search actions per decision | Holds a potion |
|---|---|---|---|---|---|---|---|
| Wins | 1,058 | 82.7 | 29.7 | 25.8 | 21.3 | 7,574 | 2% |
| Died at act-3 bosses | 1,940 | 81.1 | 29.1 | 24.8 | 19.6 | 6,500 | 3% |
| Died at act-2 boss | 2,841 | 79.0 | 23.4 | 16.3 | 17.6 | 5,479 | 3% |
| Died in act 2 before boss | 3,785 | 77.1 | 21.1 | 13.1 | 15.9 | 4,463 | 29% |
| Died at act-1 boss | 2,035 | 75.6 | 17.6 | 8.8 | 15.4 | 4,360 | 3% |

- End-of-run columns do not separate winners from late losers (deck 29.7 vs 29.1, max HP 82.7 vs 81.1). Heart fights are won 39% of the time with decks under 22 cards and 57% with 31 or more; 43% below 80 max HP, 57% at 100 or more — weak, correlational.
- Winners finish with little margin: mean final HP 54% of max at A20 (10th percentile 17%), 52% on the Heart track, 63% at A10, 72% at A0.
- Potions: boss deaths hold almost none, but 40% of A20 hallway deaths (7.4% of all A20 games) end with an unused potion; `hall_potions=0` keeps potions out of the hallway search until HP is below 25% (`FairAgent.cpp:135`). 19% of Heart-fight deaths hold one.
- Search effort: 2.7–3.8 ms of wall-clock and 5,900–7,900 simulator actions per combat decision; 286 (A20) to 480 (Heart) decisions and 0.8–1.8 s per whole game. Simulations per decision and early-stop frequency are not logged.
- From the 240 A20 traces (small sample), acts 1/2/3: death rate per fight — hallway 0.5 / 3.5 / 3.4%, elite 4 / 13 / 15% (Slavers 24%). HP lost per survived fight, as a share of max HP: hallway 11 / 16 / 12%, elite 26 / 25 / 20%, boss 36 / 38 / 25%. Fatal hallway fights start at 27–31% HP, fatal bosses at 68 / 73 / 59%. Entering the act-2 boss at 85% HP or more still loses 25% (18 of 71); below 60%, 68% (13 of 19). At boss deaths a median 29% of enemy HP is left (multi-phase bosses make this look closer than it is). The 11 winners vs losers: relics at the act-2 boss 10.2 vs 8.4; removals 3.7 vs 2.35 and shop gold spent 705 vs 325 per run.

## 5. What the existing notes claim, with their evidence

- **Deck-strength gap, not misplays** (`program.md:140`, `:143`, F50): 240 traced A20 games; act-2-boss deaths entered at 73% HP vs 88% for winners; no repeatable misplay; an elite veto below 50% HP was neutral (F51, 4,000 seeds). Same picture at A10 (`program.md:213`; 180 hand-picked traces, correlational).
- **"All three tracks lose mainly in combat search quality"** (`program.md:232`, F39; rule: 4x gain ≥ +0.05 score). Section 3 qualifies this: the gain is a uniform small shift, the Heart fight is unmoved, and A20 flattens after 2x.
- **No decision headroom outside combat except paths** (`program.md:339`, `:359`, F33–F34): choosing by 8 playouts per option beats the agent only at map nodes (+0.014 score per decision). This tests single decisions under the agent's own continuation, not a different overall strategy [inferred].
- **Boss-fight value shape** (`program.md:2601-2604`): random rollouts cannot win boss fights, so the only gradient is damage dealt plus turns survived; own HP is invisible. The direct test (rollout cut-off valued with HP) lost at A0 and A20 (`program.md:2648`).
- **Act-2 elites are not a gating problem** (`program.md:2683-2685`): a simulated-fight gate left Slavers/Leader/Book deaths at 38 vs 40.
- **Heart**: "the Heart fight is the bottleneck" (`program.md:1137`); 3x search in the Heart fight alone gave +2.8 points of Heart kills (`program.md:495`); keys cost only 1.6 points of act-3 clears (`program.md:500`); key-less clears mostly lacked the emerald (22) or ruby (17) key of 37 (`program.md:2632-2636`, an older agent); collecting more keys did not move the score (`program.md:2829`).
- **A20 Heart is about 50x away; chasing keys costs about 4 floors at A20** (`program.md:168`, F47).
- **No valid upper bound exists.** The only "cheating" combat run used the simulator's own RNG-aware search (`FairAgent.cpp:146`) and scored far below the fair agent (`results.tsv:27`, 9.6%).

## 6. What is NOT known (cheap to get)

1. **A perfect-information ceiling for combat**: the fair search on the true hidden state at equal simulations, to split "hidden information" from "search and value quality". Needs a small diagnostic switch; `determinise` is unconditional (`cpp/FairSearch.cpp:470`).
2. **The search curve beyond 4x** (16x, 64x on 500 seeds): where random-rollout search saturates.
3. **Heart-fight traces**: none exist — HP, deck, relics and potions on entry, turns survived, Heart HP left at death. `FAIR_TRACE` already prints them (`eval_main.cpp:54`).
4. **Per-fight logs at scale**: HP lost per fight by type and act, elites fought, relics, gold, removals — today only from 240 A20 traces.
5. **Deck vs piloting**: the same combat agent on strong reference decks at the act-2 boss and the Heart. Never measured.
6. **Seed-level win probability**: the agent seed is a function of the run seed (`eval_main.cpp:51`), so no replicate runs exist to separate luck from skill.
7. **Which potions are left at hallway deaths**; which key is missing on the current Heart best (the in-process mode prints keys, `eval_main.cpp:147`).
8. **A par reference**: HP lost per fight under strong human play at A20.

## 7. Hypotheses about the dominant loss mechanisms, ranked

**A20**

1. **[inferred] Too little deck and relic power by the act-2 boss.** 39.6% of entrants die there although mean entry HP is 82%, and 25% die even from 85% HP or more; the second act-3 boss kills 49%; 2x–4x search moves boss death rates by only 2–4 points; winners hold more relics and removals and spend twice the shop gold.
2. **[inferred] HP attrition in ordinary fights.** Hallway fights cost 11–16% of max HP each, elites 25%; act-2 non-boss floors kill 34.5% of entrants (27% of all games); fatal hallway fights start near 28% HP. Attrition also pushes rest sites toward healing (761 heals vs 596 other rest-site actions in losing traces; 40 vs 57 in winning ones), feeding hypothesis 1.
3. **[inferred] Act 1 is already lost too often**: 21.7% of games end there (boss 15.7% of entrants vs 1.2% at A0), with 17-card decks after 9 fights.
4. **[inferred] Elites cannot be afforded**: act-2 elites end 12.9% of games (Slavers 24% per fight); avoiding them was neutral, so the cost shows up as missing relics.
5. **[inferred] Long boss fights exceed what random rollouts and the hand-written value can judge**: search gains are real but flatten after 2x.
6. **[inferred] Unused potions in hallway deaths**: 7.4% of games; a small lever, since each saved run still faces the stages above.

**Heart (A0)**

1. **[inferred] The deck that reaches the Heart is not built to beat it.** Half of all Heart fights are lost (40% of losses); 2x and 4x search leave that rate at 47–49%.
2. **[inferred] The Heart policy pays about 20 points of act-3 survival**: 34% of Heart-track games die before clearing act 3 vs 14.6% on the A0 track; the notes attribute 1.6 points to keys and the rest to Heart-oriented picks.
3. **[inferred] Key-less clears**: 9.5% of act-3 clears; worth at most about +3 points at a 50% Heart kill rate.

**A20 Heart** [inferred]: both factors compound — act-3 clears with keys are 3.65% and the Heart is beaten in 14% of fights. Reaching 15% needs roughly a tenfold rise in A20 act-3 clears and a Heart kill rate near 50%.
