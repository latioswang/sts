# Worker task: Phase F31 interventional decision data at A10 (branch playouts) — NAME = your worker name; W and SEED below

Pre-registered in research/program.md ("Phase F31", 2026-11-08). Uses the F26 tool `fair_eval --branch_eval K M` (offline
measurement mode, never used in play): each seed is played by the A10 best to its (seed mod 10)-th single-card-reward
screen, then each option (each card, and skip) is played out 8 times from re-seeded copies; one file `branch_<seed>.bin` per
seed (rows of 666 run features + 6 floats). The F26 shared-map fix (each playout copy owns its map) is in the code — a crash
rate above ~2% means a broken build: stop and report.

Assignments (SEED = first seed; 2,400 seeds each):
- f31w1 -> W=1, SEED=5440000     - f31w5 -> W=5, SEED=5449600
- f31w2 -> W=2, SEED=5442400     - f31w6 -> W=6, SEED=5452000
- f31w3 -> W=3, SEED=5444800     - f31w7 -> W=7, SEED=5454400
- f31w4 -> W=4, SEED=5447200     - f31w8 -> W=8, SEED=5456800

Before the queue: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical). `python3 research/tools/data_sync.py ls` must work.

Queue lines for `research/queue.txt` (replace W and SEED), then start `research/tools/runq.sh` **detached**:
`setsid nohup research/tools/runq.sh > research/workers/.<name>_runq.out 2>&1 < /dev/null & disown` (after a container restart
start it the same way again; a restart of the first line regenerates all its seeds from scratch):
```
mkdir -p research/vf/data/branch_f31_wW && VF_LOG_DIR=research/vf/data/branch_f31_wW /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start SEED --count 2400 --asc 10 --threads 4 $(sed 's/^/--param /' research/best_a10_params.txt | tr '\n' ' ') --branch_eval 10 8 > research/vf/data/branch_f31_wW/summary.tsv && echo "F31 wW: $(grep -c BRANCH research/vf/data/branch_f31_wW/summary.tsv) branch, $(awk -F'\t' '$10==2' research/vf/data/branch_f31_wW/summary.tsv | wc -l) crashed"
python3 research/vf/runstore.py pack research/vf/data/branch_f31_wW research/vf/data/branch_f31_wW_z/all --glob 'branch_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push branch_f31_wW_z
```
Expected run time ≈ 3.5 h for the first line (A10 games are ≈ 2× longer than A20). `runstore.py pack` byte-verifies before it
returns. If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Keep a 30-minute Monitor on your runner output and re-arm it at every expiry while the queue runs (an idle session can be reclaimed); at each wake check `pgrep -f runq.sh` and restart it detached if it is down.
Hand back: the `F31 wW:` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed to
worker/<name>. Do not delete anything from the store. Never commit research/vf/data/.
