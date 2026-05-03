---
name: sts-status
description: Report the current Slay the Spire run state — character, floor, HP, gold, deck, relics, potions, active archetype, and next milestones. Use when the user asks for a "status report", "current status", "where are we", "run state", or any equivalent ("现在的状态", "当前情况", "状态报告", "进展", etc.).
---

# /sts-status — Run state report

Read `status.md` and render a compact, scannable summary for the current run.
Do NOT advise on a card pick or combat action — that's the per-screenshot
workflow. This skill is purely a state read-out so the user (or you) can
quickly orient.

## Steps

1. Read `/home/user/sts/status.md`.
2. Render the report in this exact shape — keep each section tight, omit any
   field the file doesn't have data for:

```
**Run** — <character>, Asc <n>, F<floor> / Act <n>
**HP / Gold / Potions** — <hp>/<max> · <gold>g · <potion list or "0/3">
**Deck (<count> cards, <upgrades> upgraded)** — group by role:
  Attacks:   <list with cost+ marker>
  Skills:    <list>
  Powers:    <list>
  Curses:    <list, or omit if none>
**Relics** — bullet list, name + tier + 1-line effect
**Archetype** — <named archetype + 1-sentence why>
**Plan (next 2-3)** — pull from "Next milestones" section
**Risks / open questions** — pull verbatim from that section
```

3. After the report, add a 1-sentence delta line if the most recent decision
   in `status.md` is from the current floor (e.g. "Just took Predator on F2;
   waiting for the next screenshot.").

## Style rules

- Use the markdown structure above. Bold headers, no extra prose between
  sections.
- Keep the deck list to one card per line; don't expand into full card text.
- If `status.md` is empty/stale or character is unset, say so plainly:
  "No active run — `status.md` is empty or pre-Neow. Send a screenshot to
  bootstrap." and stop.
- If the run-character is Defect or Watcher, append: "_Note: no curated
  archetype guide for this character yet — strategy advice is from general
  meta._"
