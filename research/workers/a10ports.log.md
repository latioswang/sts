# a10ports log

## Bit-identity gate
```
repro_check research/runs/mapd085.val.a0.tsv: 16 games compared, 0 differ -> OK bit-identical
repro_check research/runs/heart_bfrelic10.val.a0.tsv: 16 games compared, 0 differ -> OK bit-identical
repro_check research/runs/a20_restboss1.val.a20.tsv: 16 games compared, 0 differ -> OK bit-identical
```

## Results
- `a10_mapd085` 26.7% win, floor 36.7, -0.039 vs base, P=0.011 → discard
- `a10_potsave` 29.3% win, floor 37.4, +0.001 vs base, P=0.517 → discard
- `a10_wea1` 28.7% win, floor 37.2, -0.010 vs base, P=0.331 → discard
- No A10 keep, so no rep_dev replication was needed.
- `a20_mapd085` 2.0% win, floor 25.5, +0.009 vs base, P=0.704 → info
  - pooled val,dev2,dev3 [act3]: n=500  a20_mapd085 win=0.032 floor=27.53  a20_restboss1 win=0.028 floor=27.41  dscore=+0.0062 dwin=+0.004 P(improve)=0.833
- `a20_potv8` 0.0% win, floor 24.1, -0.037 vs base, P=0.002 → info
  - pooled val,dev2,dev3 [act3]: n=500  a20_potv8 win=0.016 floor=26.54  a20_restboss1 win=0.028 floor=27.41  dscore=-0.0287 dwin=-0.012 P(improve)=0.002

Queue empty; no keeps on A10 or A20; best_a10_* / best_a20_* unchanged. (note.py shows the last stage row for A20 names; the pooled line is the verdict.)
