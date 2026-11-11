# Worker task: Phase F52 leaf-value data (10× the S2 data) — NAME = your worker name (f52w1 … f52w10)

Pre-registered in research/program.md ("Phase F52", 2026-11-11 ≈ 08:58 UTC; owner chose option A). Play A0 games with the
current A0 best (research/best_params.txt = f45a0_sf) and the leaf logger on (`leaf_log=1`: about 1 in 500 new search-tree
leaves is written with the mean and sd of 8 extra rollouts; the logger saves and restores the search's own rng, so play is
unchanged). Output: `leaf_<seed>.bin`, rows of 206 float32 (204 features, rollout mean, rollout sd).

Assignments (train-only seed ranges, disjoint, never used for keep/discard):
- f52w1  -> START=7000000   f52w2  -> START=7007500   f52w3 -> START=7015000   f52w4 -> START=7022500   f52w5  -> START=7030000
- f52w6  -> START=7037500   f52w7  -> START=7045000   f52w8 -> START=7052500   f52w9 -> START=7060000   f52w10 -> START=7067500
Every worker: N=7500 games, SET=f52_<NAME> (e.g. f52_f52w3).

Before the run (README steps 1–2, then the CURRENT gates — the README's fidb gates are out of date):
1. `git checkout -B worker/<NAME> origin/autoresearch/ironclad`; repo at /home/user/sts; `bash research/setup.sh`
   (builds fair_eval from the fidelity fork at the pinned commit 1b3b779).
2. Gates — all four must print `OK bit-identical`, otherwise stop and report:
   `research/tools/repro_check.sh cb0_a0 16 fs1428500n2000`, `research/tools/repro_check.sh cb0_a10 16 fs1426500n2000`,
   `research/tools/repro_check.sh cb0_a20 16 fs1424500n2000`, `research/tools/repro_check.sh cb0_heart 16 fs1430500n2000`.
3. `grep -E '^(sims|es_frac|prior_mlp)=' research/best_params.txt` must print prior_mlp=1, es_frac=0.1, sims=1000.

Queue lines (research/queue.txt, run with research/tools/runq.sh as a harness-tracked background task, timeout 7200000;
chunks are resumable — after a restart or the 2-hour limit just restart runq.sh):
```
LOGP="--param leaf_log=1" research/tools/sv_gen.sh SET 7500 250 START
python3 -c "import glob,os; f=glob.glob('research/vf/data/SET/all/leaf_*.bin'); b=sum(os.path.getsize(x) for x in f); print(f'F52 ROWS SET files={len(f)} rows={b//824} rem={b%824}')"
python3 research/vf/runstore.py pack research/vf/data/SET/all research/vf/data/SET_z/all --glob 'leaf_*.bin' --per 250
STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z
```
(replace SET and START with your values). Expected: ≈ 0.4 s per game on 4 threads → ≈ 1 h of generation [estimate from the
coordinator's f45a0_sf timing; please paste the real per-chunk times]. If a push fails on HTTP 429, wait 15 minutes and re-run it.

Keep the container alive: generator as a tracked background task (not setsid/nohup) and a 20–30-minute send_later or
Monitor check chained while it runs.

Hand back: in research/workers/<NAME>.log.md paste the four gate lines, the first and last `sv chunk` lines, the
`SV GEN ... DONE`, `F52 ROWS ...`, `PACK ...` and push `DONE` lines; commit and push to worker/<NAME> (never commit
research/vf/data/); then message the coordinator session with those lines. Rows with column 10 = 2 in any games.tsv are crashes:
list their seeds. Do not edit research/program.md, results.tsv, REPORT.md or research/progress/.
