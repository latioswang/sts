# Stage 0 report — where the gap is, and which route (2026-11-16)

All runs are measurement; none is an agent result except the fair 1× and 16× arms. Raw outputs: `research/s0_e1a.out`,
`research/s0_e1b.out`, `research/s0_e2.out`, `research/s0/e2_summary.txt`, `research/breakthrough/e3_card_table.md`,
`research/breakthrough/e5_results.txt`. Seeds: fresh slice fs1443700n1000 (A20) for E1 and E2.

## Results

**E1 — our A20 strategy under four pilots (same 1,000 seeds).**

| Pilot | Act-3 win | vs fair 1× |
|---|---|---|
| Our fair search, 1× (the current best) | 7.8% | — |
| Our fair search, 16× | 12.7% | +4.9 |
| Our search given the true future, 4× | 24.8% | +17.0 |
| Lightspeed's clairvoyant searcher, 40,000 simulations | 23.7% | +15.9 |

Pre-registered reading: our decks under a near-perfect pilot win between 20% and 40% → **both strategy and piloting bind.**
Two clairvoyant pilots of different design agree (23.7 / 24.8%). Hidden information is worth more to our search than
compute: 4× with the true future beats 16× without by 12 points.

**E2 — boss arrivals re-fought 16× (fair) and 2× (clairvoyant), 1,922 arrivals from 934 games.**

| Boss | Fair re-fight win | Clairvoyant | Top quarter by entry HP (fair) | Bottom quarter |
|---|---|---|---|---|
| Act 1 (floor 16) | 83.6% | 90.3% | 92.2% | 68.7% |
| Act 2 (floor 33) | 57.1% | 69.9% | **72.9%** | 31.6% |
| Act 3, first (50) | 70.9% | 79.6% | 80.2% | 45.9% |
| Act 3, second (51) | 50.1% | 59.9% | 85.2% | 3.6% |

Pre-registered reading: the top quarter at the act-2 boss wins 72.9% < 76% → **with this pilot, no drafting policy
reaches 25%.** This is weak evidence: 130 arrivals, standard error ≈ 3.9 points [computed], the bar is 0.8 SE above.
Entry HP separates outcomes more than relic count does. Outcomes are largely fixed on arrival (the in-sample top
quarter wins every re-fight, the bottom quarter none).

**E3 — card-value tables from 40,648 measured decisions:** do not beat the agent's own picks (A20 +0.6 SE, A10 −1.3 SE;
bar 2 SE). Measured card tables are closed until there is a new argument.

**E5a — the leader, `spire-engine` ironclad-v1, in its own engine (rented RTX 4090, $0.18):** 21.4% on its author's
seeds (claim 21.9%: reproduced) and **18.7% [16.4, 21.2] on 1,024 unseen seeds**, with no search. **E5b** (its training
speed) is not measured yet: the first runs failed on a Python-version mismatch (fixed), the third was stopped when the
vast.ai credit ran out.

**E4 — instruments:** replicate seeds (`FAIR_AGENT_SALT`), configurable hang guard (`FAIR_ALARM`), three sealed
2,000-seed claim sets (`research/eval/sealed.md`), boss re-fight and clairvoyant switches (measurement only).

## What it means [inferred]

1. Strategy alone cannot get us there: our decks are worth about 24% even with a near-perfect pilot, and with our pilot
   the best quarter of act-2 boss arrivals still lose more than T1 allows.
2. Piloting alone cannot either: a perfect pilot of today's decks reaches about 24%, just above the bar, and no fair
   pilot gets near a clairvoyant one.
3. The leader shows the two halves trained together reach about 19% on unseen seeds with no search at all. That is
   the recipe our measurements point to: one policy whose fights and choices are learned against each other.

## Recommendation

**Route L (learn it), on the leader's own engine first.**
- Its code, engine and released network run today on a rented GPU; the reproduction is done.
- First step: the training-speed and learning-curve measurement (E5c, about 2–3 GPU-hours, under $1.50). It tells us
  what a full training run costs. Then, if affordable: continue training from the released network with an A20 Heart
  objective for the Ironclad (no one has trained for that), and add a search on top (the author's own look-ahead gave
  +5.6 points).
- A claim made on their engine is like-for-like with the current leader. Our own simulator stays as the cross-check
  and, later, the place to port a winning policy.

Route M (rebuild our agent by measurement) is not recommended as the main line: E2 says its pilot caps it.

## Owner decisions

1. Route L on the leader's engine (recommended), or on our simulator (needs an environment and encoder port first,
   25–35 days before a meaningful run).
2. GPU budget beyond the first $20 once E5c gives the cost of a full run.
3. vast.ai credit: currently $0.00; nothing GPU-side can run until it is topped up in the console.
