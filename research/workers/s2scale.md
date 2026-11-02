# Worker task: S2 scale-up (NAME = s2scale)

Pre-registered in research/program.md ("S2 scale-up", 2026-11-02 13:15 UTC). Rebuild the 2,500-game leaf dataset
(identical, deterministic) plus 7,500 new games, train two small leaf models on all ≈ 3.3×10⁶ leaf states, and run the
two equal-wall-clock play tests. Queue lines, in order:

```
LOGP="--param leaf_log=1" research/tools/sv_gen.sh leafdata 2500 100 320000
LOGP="--param leaf_log=1" research/tools/sv_gen.sh leafdata2 7500 100 340000
( mkdir -p research/vf/data/leafall && ln -sf $(pwd)/research/vf/data/leafdata/all/leaf_*.bin research/vf/data/leafall/ && ln -sf $(pwd)/research/vf/data/leafdata2/all/leaf_*.bin research/vf/data/leafall/ && ls research/vf/data/leafall | wc -l )
python3 research/vf/train_sv.py research/vf/data/leafall --leaf --id 16 --rows 100000000 --hidden 42 2>&1 | tail -1
python3 research/vf/train_sv.py research/vf/data/leafall --leaf --id 17 --rows 100000000 --hidden 96 2>&1 | tail -1
/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --param end_turn_w=0.05 --param prior_mlp=1 --param leaf_sv=16 --param rollout_turns=0 | tail -1
research/tools/sv_eval.sh 16 "S2 scale-up: 4x leaf data x 1e4 params"
research/tools/sv_eval.sh 17 "S2 scale-up: 4x leaf data x 3e4 params"
```
Sanity check after the first line: `research/models/sv_11` was trained on that same 2,500-game set; its fit
(holdout RMSE 12.060 on 765,531 training rows) should be reproducible by retraining `--id 11` if anything looks off.
Hand back: `research/models/sv_16.*`, `sv_17.*`, the run files of `sv16`/`sv17`, `research/results.d/s2scale.tsv`.
