# Worker task: A10 / A20 ports of A0 keeps (NAME = a10ports)

`research/tools/track_diff.sh` shows A10 and A20 lack three A0 keeps: `map_discount=0.85`, `potion_v=8 potion_v_boss=4`
(A20 already has `potion_v_boss=2`, so only `potion_v=8`), and `w_elite_act1=1` (A20 deliberately uses -2; not ported).
On the Heart track none of these transferred (research/program.md, batch 51) — so the prediction is "most fail"; each
is still a cheap, pre-registered test. A10 uses the val-300 rule (`research/exp.sh` with TRACK=a10); A20 uses the
pooled-500 rule (`research/tools/exp_pooled.sh` with TRACK=a20). Queue lines, in order:

```
TRACK=a10 research/exp.sh a10_mapd085 "port A0 keep: map discount 0.85" map_discount=0.85
TRACK=a10 research/exp.sh a10_potsave "port A0 keep: potions worth 8 in hallway fights, 4 in boss fights" potion_v=8 potion_v_boss=4
TRACK=a10 research/exp.sh a10_wea1 "port A0 keep: act-1 elites +1 on the map" w_elite_act1=1
TRACK=a20 research/tools/exp_pooled.sh a20_mapd085 "port A0 keep: map discount 0.85" map_discount=0.85
TRACK=a20 research/tools/exp_pooled.sh a20_potv8 "port A0 keep: potions worth 8 in hallway fights" potion_v=8
```
If an A10 line keeps (exp.sh promotes it), the next A10 lines run on top of the new best automatically. Before claiming
an A10 keep, replicate it once on dev: `TRACK=a10 research/tools/rep_dev.sh <name> <previous best>` (the previous best's
`_dev` run must exist; if it does not, note that and skip).
Hand back: run files, params, `research/results.d/a10ports.tsv`, any changed `research/best_a10_*` / `research/best_a20_*`.
