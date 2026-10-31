# Ironclad autoresearch program

Modelled on karpathy/autoresearch (`program.md`): the human writes this file,
the agent iterates on ONE editable surface, a FROZEN harness scores every
attempt, keep only strict improvements, log everything, never pause to ask.

## Goal

Maximise **Ironclad win rate** in `sts_lightspeed` with an agent that uses
**no hidden information** (no peeking at RNG, draw order, future rewards).

- Ladder: A0 → raise ascension whenever test win rate ≥ 50% at current level.
- Final claim requires the `test` split (see Eval), reported with a 95% CI.

## Why the built-in agent does not count

`ScumSearchAgent2` copies the live `BattleContext` (including all RNG state and
the draw-pile order) and replays the best action sequence it found. That's
save-scumming against a known future. It's used here only as a **cheating
upper reference**, never as a result.

## Editable surface (agent may change)

- `research/agent/` — Python policy (map, rewards, shop, rest, events, boss
  relics) and combat-search *parameters/eval functions*.
- `research/cpp/` — the leak-free combat searcher (C++). Any change here must
  pass the leak test before an eval counts.

## Frozen (agent must NOT change)

- `research/eval/` — the harness, seed lists, scoring, leak test.
- `sts_lightspeed` core game logic (src/game, src/combat). A bug fix there
  requires a separate commit with a reason, and invalidates all prior results.
- Compute budget per eval (search simulations per decision, wall clock).

## Eval protocol (frozen)

- Seeds: `dev` = 100 seeds (screening), `val` = 300 seeds (keep/discard gate),
  `test` = 500 seeds (touched only at milestones, max ~5 times total).
- Paired comparison: candidate vs current best on the **same seeds**.
- Primary metric: win rate. Screening metric: mean floor reached
  (lower variance). Score = mean over seeds of `floor/55 + win`.
- Keep rule: candidate score > best score on `val` AND paired bootstrap
  P(improvement) ≥ 0.9. Otherwise discard (git reset).
- Leak test: the action returned for a state must be identical when hidden
  state (draw-pile order, all RNG streams) is re-randomised, with the agent's
  own search seed held fixed. Any failure → result invalid.
- Fixed compute: same simulation budget per combat decision for candidate and
  baseline unless the experiment is explicitly "budget scaling" (logged as such).

## Loop

1. Pick ONE idea (smallest change that tests a hypothesis). Commit.
2. Run `dev` eval. If clearly worse (score drop > 2 SE), discard.
3. Run `val` eval, paired against best. Apply keep rule.
4. Append a row to `research/results.tsv`:
   `commit  split  n  winrate  mean_floor  score  p_improve  status  description`
5. Keep → new best. Discard → `git reset --hard` to best.
6. Crash/timeout (> 2× expected wall clock) → simple fix attempt, else discard.
7. Never stop to ask. Out of ideas → re-read `results.tsv` failures, combine
   near-misses, try more radical changes.

## Stop condition

Stop when 5 consecutive idea *families* fail on `val`, or compute/time runs out.
Then run `test` once on the best agent and write `research/REPORT.md`.
