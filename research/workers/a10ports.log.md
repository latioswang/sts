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
