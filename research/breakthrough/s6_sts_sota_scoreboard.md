# S6 — Slay the Spire state-of-the-art scoreboard (verified 2026-11-16)

Tools: WebSearch and WebFetch worked. Every row in sections 1 and 2 cites a page opened in this session; anything not opened is in section 5. Two hosts could not be reached because they are outside this session's allowed network domains and must be allowed in the network settings before they can be read: `tomokinotes.pages.dev` and `sts1.heart-rate.net`. Searches used generic terms only.

**Source keys**
- [SE] https://github.com/jackybabes/spire-engine and its model cards `docs/models/{ironclad,silent,defect,watcher}-v1.md`, `docs/architecture.md`
- [DM] https://github.com/destire-mio/sts-ironclad-agent (README, `steam/README.md`, `student/README.md`, `docs/live-original-distill3-student-20261001.md`, `docs/live-original-search-entry-20260927.md`)
- [EP1] https://epoch.ai/benchmarks/spireclimb · [EP2] https://epoch.ai/publications/ai-systems-are-improving-rapidly-at-slay-the-spire-and-its-sequel
- [SB] https://arxiv.org/html/2610.12596 · [ZS] https://arxiv.org/html/2611.12088
- [SM1] https://github.com/tfriedel/spire-mentor · [SM2] https://tfriedel.github.io/spire-mentor/
- [RL] https://github.com/Jialeiv/sts-rl-agent · [LS] https://github.com/gamerpuppy/sts_lightspeed
- [BA] https://github.com/xaved88/bottled_ai/blob/main/docs/winrates.md · [AA] https://justinochan.github.io/AscensionAI/
- [DS] https://dev.to/doronsinger/part-6-what-not-to-consider-54g5 · [ED] https://project-archive.inf.ed.ac.uk/ug4/20233951/ug4_proj.pdf
- [AT] https://github.com/AttemorySystem/spire-agent · [AG] https://arxiv.org/abs/2607.02255
- [FA] https://forgottenarbiter.github.io/Is-Every-Seed-Winnable/ · [NL] https://megacrit.com/news/2026-5-22-neowsletter-issue-22/
- [RE] https://www.resetera.com/threads/slay-the-spire-streamer-lifecoach-going-for-50th-win-in-record-breaking-streak-as-watcher-a20-heart.771584/ · [ST] https://steamcommunity.com/app/646570/discussions/0/5595184948711282784

## 1. Scoreboard

"Hidden info: YES" means the agent's search uses the true random-number state or draw order. IC = Ironclad. All results are self-reported unless stated.

### 1a. Slay the Spire 1 (hardest setting first, then win rate)

| Agent (method) | Char | Win rate (runs) | Where | Hidden info | Compute per decision | Date | Src |
|---|---|---|---|---|---|---|---|
| **A20, Heart required** | | | | | | | |
| sts-ironclad-agent (hybrid: one distilled network out of combat, exact-state tree search in combat) | IC | 50.1% (501; interval 45.7–54.5); first pass 48.8% (231/473); simulator 50.1% (2,000) and 49.5% (1,000) | real game through CommunicationMod, and sts_lightspeed | **YES** in combat | 32,000 simulations per search round, 384,000 at bosses; about 1 minute per game on one core | Oct 2026 | [DM] |
| GPT-6 Astra, SpireClimb (language model) | 4 chars | 33.3% (60) after a harness fix; 16.7% before | real game, screenshots | no | not stated (maximum reasoning) | 5 Nov 2026 | [EP1] |
| GPT-6.1 Sol, SpireClimb (language model) | 4 chars | 18.3% (60) after the fix; 20% before, with IC 27% (4/15), Watcher 33% | real game, screenshots | no | not stated | Oct–Nov 2026 | [EP1][EP2] |
| GPT-5.6 Sol, SpireBench (language model) | IC | 10% (1/10); all 19 other models 0% | real game, text | no | $8.39 per run | Oct 2026 | [SB] |
| spire-engine Watcher v1 (PPO plus imitation, no search) | Watcher | 8.0% (1,536) | own Rust simulator | no | not stated; its engine doc says a few ms in WebAssembly | 29 Oct 2026 | [SE] |
| spire-engine Defect v1 (same) | Defect | 0.9% (1,024) | simulator | no | same | 26 Oct 2026 | [SE] |
| Claude Opus 5.5, Sonnet 5.5, Fable 5.1 (language models) | 4 chars | 0% (60 each, before the fix) | real game | no | — | Oct 2026 | [EP2] |
| spire-agent (language model for the map, tree search on sts_lightspeed in combat) | IC, Defect | "multiple" wins, no rate; 12-run IC sample: 2 reached Act 4 | real game | not stated | — | 2026 | [AT] |
| Bottled AI in SpireBench (hand-written rules) | IC | 0% (10); clears Act 1 in 20% | real game | no | — | Oct 2026 | [SB] |
| **A20, Act 3 boss counts as the win** | | | | | | | |
| spire-engine Ironclad v1 plus fight look-ahead (unreleased, not re-measured) | IC | 27.5% (runs not stated) | simulator | not stated ("redrawn copies") | about 20× the base cost (figure from the Watcher card) | Oct 2026 | [SE] |
| spire-engine Watcher v1 plus look-ahead | Watcher | 22.6% Heart-or-Act-3 (1,024), base 15.1% | simulator | not stated | about 20× | Oct 2026 | [SE] |
| **spire-engine Ironclad v1** (PPO, 1.07M-parameter transformer, greedy, no search) | IC | **21.9%** (1,024; about ±1.2) | simulator | no | a few ms | 21 Oct 2026 | [SE] |
| Claude Fable 5.1, SpireBench (Act 3 cleared) | IC | 20% (2/10); GPT-6 Astra and GPT-5.6 Sol 10% | real game | no | $62 per run | Oct 2026 | [SB] |
| spire-engine Silent v1 | Silent | 2.9% (1,536) | simulator | no | a few ms | 24 Oct 2026 | [SE] |
| sts-rl-agent (learned scorer plus search) | IC | 0% | sts_lightspeed | **YES** | 2,000 simulations | 2026 | [RL] |
| Edinburgh project (card-pick rules plus the scumthespire save-state battle search) | IC | 0% (20); A0 and A10 wins shown only in a figure | real game | **YES** (save states reveal card order) | 10 minutes to 3 hours per run | 2023 | [ED] |
| **A10** | | | | | | | |
| **spire-engine Ironclad v1** | IC | **75.2%** (1,024), Act 3 | simulator | no | a few ms | Oct 2026 | [SE] |
| spire-engine Silent v1 | Silent | 47.9% (1,024), Act 3 | simulator | no | — | Oct 2026 | [SE] |
| spire-engine Watcher v1 / Defect v1 | W / D | Heart 26.3% (1,536) / 10.4% (1,024) | simulator | no | — | Oct 2026 | [SE] |
| **A0** | | | | | | | |
| GPT-6 Astra / GPT-6.1 Sol / Claude Opus 5.5 / GPT-6 Sol (language models) | 4 chars | 98 / 97 / 93 / 92% (15 per character; win condition not stated) | real game | no | not stated | Oct 2026 | [EP2] |
| **spire-engine Ironclad v1** | IC | **93.1%** (1,024), Act 3 | simulator | no | a few ms | Oct 2026 | [SE] |
| spire-engine Silent v1 | Silent | 75.8% (1,024), Act 3 | simulator | no | — | Oct 2026 | [SE] |
| spire-engine Watcher v1 / Defect v1 | W / D | Heart 43.8% / 27.5% (512 each) | simulator | no | — | Oct 2026 | [SE] |
| sts-rl-agent | IC | 14% (50 seeds) | sts_lightspeed | **YES** ("uses the true future draw order") | 50,000 simulations | 2026 | [RL] |
| **Ascension not stated on the page** | | | | | | | |
| Bottled AI release-03 (hand-written rules, one-turn graph search) | all four | Watcher 52% (50), Silent 40% (48), Defect 33% (52), IC 20% (50) | real game | no | — | Oct 2024 | [BA] |
| D. Singer's agents (search with hand-made or learned evaluators) | Silent | Bottled AI port 26%; own best 6.7% (2/30) | headless environment | not stated | 5–20 s | year not shown | [DS] |
| AscensionAI (1-ply search plus rules) | IC | 6.0% (9/150); 0 wins in about 21,700 live-game PPO runs | sts_lightspeed | not stated | — | Jun 2026 | [AA] |

### 1b. Slay the Spire 2 (no Heart equivalent; Ascension 10 is the maximum)

| Agent (method) | Char | Win rate (runs) | Where | Hidden info | Compute per decision | Date | Src |
|---|---|---|---|---|---|---|---|
| **A10** | | | | | | | |
| GPT-6 Astra, SpireClimb (language model) | 5 chars | 35% (75); IC 40%, Regent 53% (15 each). GPT-6.1 Sol 29% | real game | no | not stated | Oct 2026 | [EP2] |
| Spire Mentor (chance-node tree search plus PPO run policy) | IC | 30.0% (400; 25.7–34.7) | own Rust simulator | no | 4,000 simulations, 16,000 at bosses; 1,000 ≈ 14 ms on one thread | 31 Oct 2026 | [SM1] |
| Spire Mentor, earlier champions, engine before a reward fix ("somewhat flattering") | IC / Defect / Silent / Regent / Necrobinder | 27.2 / 22.0 / 9.0 / 6.2 / 5.6% (500 each) | simulator | no | 8,000 simulations | Oct 2026 | [SM1][SM2] |
| Spire Mentor re-run by Chen and Xie | IC; five-character mean | 16.8% (400); 8.8% (2,000) | game's own code, headless | no | 4,000–16,000 simulations | Nov 2026 | [ZS] |
| Zero-search policy (PPO, 6.6M-parameter transformer) | IC; five-character mean | 15.59% (2,560; 14.23–17.04); 8.05% (12,800) | game's own code, headless | no random state; sees the enemy's next-move id | 21–27 ms on one core | Nov 2026 | [ZS] |
| GPT-6 Astra, text interface | — | 0/11 | game's own code | no | — | Nov 2026 | [ZS] |
| **A8, real game** | | | | | | | |
| Spire Mentor, August champions | IC / Silent | 32.5% (13/40) / 27.5% (11/40) | real game | no | 8 trees × 1,000 simulations | Aug 2026 | [SM2] |
| **A0** | | | | | | | |
| Spire Mentor | IC / Defect | 96.0% / 93.0% (200 each) | simulator | no | 8,000 simulations | Oct 2026 | [SM2] |
| Zero-search policy | IC; mean | 89.8%; 80.2% (512 seeds per character) | game's own code | as above | 21–27 ms | Nov 2026 | [ZS] |
| AgenticSTS (language model plus skill memory) | — | 6/10 against 3/10 without (p ≈ 0.37) | own harness | — | — | Jul 2026 | [AG] |

## 2. Human reference

| Who | Setting | Figure | Src |
|---|---|---|---|
| XecnaR, "widely considered the single best player" | StS1 A20 Heart | 80–95% depending on character (Epoch cites a stats site I did not open) | [EP2] |
| Epoch's estimate of top humans | StS1 A20 Heart / StS2 A10 | about 85% / about 95% | [EP1] |
| Baalorlord, 687 automatically logged runs, 2020–2026 | IC A20 | Heart 63%; Act 3 cleared 70% | [SB] |
| panacea108 (51 runs) / vmService (50) / cheg0 (46); player-selected samples | IC A20 Heart | 65% / 58% / 46% | [SB] |
| TerrenceM, last 50 runs per character (2020) | A20 Heart | IC 52%, Silent 40%, Defect 46%, Watcher 72% | [FA] |
| Lifecoach, last 50 games (2020) | Watcher A20 Heart | 94% | [FA] |
| Streak list, 5 Oct 2023 (forum post) | A20 Heart | IC 19 (Xecnar), Silent 27 (Xecnar), Defect 16 (Lifecoach), rotating 20 (Baalorlord), Watcher 52 (Lifecoach, ended 8 Oct 2023) | [RE] |
| Streak list, Oct 2022 (forum comment) | A20 Heart | IC 18, Silent 16, Defect 11, Watcher 38, rotating 16 | [ST] |
| ForgottenArbiter's estimate of optimal play | A20 Heart / A20 without Heart | IC at least 75%, Silent 60%, Defect 65%, Watcher 96% / about 83, 73, 75, 97% | [FA] |
| All players, Mega Crit | StS1 A0 / A20 | 9% / 3% | [NL] |
| All players, 206,082 A20 Ironclad runs (2020 telemetry) | IC A20 | Act 3 cleared 7%, Heart 3.5–4%; 1,000+ hours 7%; players with an earlier Heart kill 16% | [SB] |
| All players, StS2 | A0 / A10 | 16% / about 17% | [NL] |

## 3. What "state of the art" means today

**Ascension 0.** Saturated. Language models win 92–98% in the real game across four characters [EP2]; the best specialised agent, spire-engine Ironclad, wins 93.1% of 1,024 simulator seeds with no search [SE]. Nobody publishes an Ironclad A0 Heart rate; the only A0 Heart figures are Watcher 43.8% and Defect 27.5% [SE].

**Ascension 10.** spire-engine Ironclad, 75.2% of 1,024 (Act 3 win) [SE]. No language-model or real-game result exists for StS1 at this level.

**Ascension 20 without the Heart.** spire-engine Ironclad, 21.9% of 1,024, greedy [SE]. Its author reports 27.5% with a fight look-ahead that is not released and was not re-measured. Language models cleared Act 3 in at most 2 of 10 runs [SB].

**Ascension 20 Heart.** Three different leaders, depending on the rules:
- Any automated system: sts-ironclad-agent, 50.1% of 501 in the real game, Ironclad [DM]. Its combat search runs on the exact imported state, including the combat random streams and draw order (`steam/README.md`: "exact-state MCTS"; the upstream project says the search "uses the true future draw order" [RL]). It is the counterpart of this project's cheating baseline, not of its fair agent.
- No hidden information, any method: GPT-6 Astra, 33.3% of 60 across four characters in the real game [EP1]. The same model scored 16.7% before a harness fix and Sol moved from 20% to 18.3%, so run-to-run noise is about ±10 points. Ironclad alone: 27% (4/15) [EP2] and 10% (1/10) [SB].
- No hidden information, specialised agent: spire-engine Watcher, 8.0% of 1,536, simulator [SE]. No fair specialised Ironclad result is published.

**What is not comparable.** Simulator rates against real-game rates (Spire Mentor reports 27–30% at A10 in its own engine; a released version scored 16.8% in another host). Act 3 wins against Heart wins. Characters (Watcher is the easiest for experts; language models did best on Watcher and Ironclad). Compute: spire-engine uses one small network pass; Spire Mentor 4,000–16,000 simulations; sts-ironclad-agent tens to hundreds of thousands; language models cost $8–62 per run [SB].

## 4. Where this project stands, and what would be a defensible claim

Project today (Ironclad, simulator, no hidden information, about 3 ms per decision): A0 about 85%, A10 about 52%, A20 about 8%, A0 Heart about 29%, A20 Heart 0.3%.

| Setting | Project | Best fair specialised | Best fair of any kind | Expert human |
|---|---|---|---|---|
| A0 | 85% | 93.1% | 98% (real game) | near 100% (Epoch's chart line) |
| A10 | 52% | 75.2% | 75.2% | — |
| A20, Act 3 | 8% | 21.9% (27.5% with look-ahead) | same | 70% (Baalorlord) |
| A20 Heart | 0.3% | 8.0% (Watcher); none for Ironclad | 33.3% (four characters) | 63% logged; 80–95% for the best player |

The project is behind the best fair specialised agent at every level, and that agent uses no search and a comparable compute budget, so the 3 ms limit does not explain the gap. The project's A0 Heart figure has no published Ironclad comparison.

Defensible claims, each on at least 1,000 unseen seeds with an interval:
1. **Best specialised agent (no hidden information, simulator).** Ironclad A0 above 93%, A10 above 75%, A20 Act 3 above 22%, and above 27.5% to clear the unreleased look-ahead figure. For A20 Heart, above 8%; any Ironclad rate reliably above zero would be the first fair specialised Ironclad figure. The claim must state "no hidden information", because sts-ironclad-agent reports 50%.
2. **Best agent of any kind.** A20 Heart above 33% matches the best language model; the 95% interval on 20/60 is about 23–46% (my calculation), so a lower bound above 46% beats it beyond noise. The language-model figure is real-game and four-character, so the claim needs real-game replay or parity evidence and the label "Ironclad only". Passing 50% would also pass the clairvoyant hybrid.
3. **Expert human level.** Ironclad A20 Heart at 60% or more (Baalorlord 63% over 687 runs; other streamers 46–65%). Top-human level is 80% or more; the estimated ceiling for Ironclad is at least 75%.

## 5. Unverified or doubtful

**Verified, but weaker than they look**
- sts-ironclad-agent 50% [DM]: clairvoyant combat search; simulator parity marked "INCOMPLETE"; 34 of 507 real-game attempts faulted, and re-running them on a repaired importer added 28 results, of which 20 were wins by my arithmetic (71%, against 48.8% in the first pass); the locked 1,024-seed acceptance was not run; single author.
- spire-engine [SE]: simulator only, no real-game win rate; the Watcher checkpoint was chosen among six candidates by Heart wins, and the card does not say the reported seeds are separate from the selection seeds; the look-ahead figures are outside the release.
- Spire Mentor 30.0% [SM1][SM2]: the README names the champion `cont300_it1150` (31 October) and the site `cardmods_it850` (27 October) with the same number and interval; it was promoted after missing its own p < 0.01 gate; the independent re-run gave 16.8% [ZS].
- SpireClimb [EP1]: 60 runs; per-character figures and all other models' scores date from before the Neow fix; whether A0 was re-run is not stated.
- SpireBench [SB]: 10 runs per model.
- Bottled AI and AscensionAI: ascension not stated on the pages opened.

**Not opened, so not counted**
- XecnaR's stats page (cited by Epoch).
- "GPT-6 Astra won 2 of 10 livestreamed StS2 A10 runs": seen only on an aggregator (https://digg.com/ai/95ef7bfe).
- sts2-rl-research, 26.6% Ironclad at StS2 A2: cited inside [ZS] only.
- Lifecoach 64% over 50 Ironclad A20 games: found only on a scraped page.
- Self-uploaded A20 Heart rates (Watcher 29.9%, Ironclad 24.6%): search snippet of the blocked `sts1.heart-rate.net`.
- Spire Pilot (https://www.spirepilot.com/): a snippet mentioned one verified A20 Heart win and a 90% goal; the pages opened state no results.
- AlphaSpire (StS2): only its networks without search were measured, 0.55% [ZS].
- Orak (https://arxiv.org/abs/2506.03610): the abstract has no Slay the Spire numbers. Bateni and Whitehead 2024: not found.
- AlphaStS, SlayTheModel, the StS2 PPO showcase (https://cmcour.github.io/sts2Showcase/): opened; no run-level win rates.

**Leads from the earlier survey**
- spire-engine 93 / 75 / 22% and about 27%: confirmed (93.1 / 75.2 / 21.9%; 27.5%). The docs do not call the look-ahead shallow.
- Spire Mentor 27–30%, lower on re-run: confirmed.
- arXiv 2611.12088 and 2610.12596: confirmed.
- SpireClimb 18–33%: confirmed as 33.3% and 18.3% after the fix; these are averages over four characters, not Ironclad figures.
