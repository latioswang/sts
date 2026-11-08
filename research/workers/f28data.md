# Worker task: Phase F28 interventional decision data at A20 (branch playouts) — NAME = your worker name; W and SEED below

Pre-registered in research/program.md ("Phase F28", 2026-11-08). Uses the F26 tool `fair_eval --branch_eval K M` (offline
measurement mode, never used in play): each seed is played by the A20 best to its (seed mod 10)-th single-card-reward
screen, then each option (each card, and skip) is played out 8 times from re-seeded copies; one file `branch_<seed>.bin` per
seed (rows of 666 run features + 6 floats). The F26 shared-map fix (each playout copy owns its map) is in the code — a crash
rate above ~2% means a broken build: stop and report.

Assignments (SEED = first seed; 3,200 seeds each):
- f28w1 -> W=1, SEED=5410000     - f28w4 -> W=4, SEED=5419600
- f28w2 -> W=2, SEED=5413200     - f28w5 -> W=5, SEED=5422800
- f28w3 -> W=3, SEED=5416400     - f28w6 -> W=6, SEED=5426000

Before the queue: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical). `python3 research/tools/data_sync.py ls` must work.

Queue lines for `research/queue.txt` (replace W and SEED), then start `research/tools/runq.sh` **detached**:
`setsid nohup research/tools/runq.sh > research/workers/.<name>_runq.out 2>&1 < /dev/null & disown` (after a container restart
start it the same way again; a restart of the first line regenerates all its seeds from scratch):
```
mkdir -p research/vf/data/branch_f28_wW && VF_LOG_DIR=research/vf/data/branch_f28_wW /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start SEED --count 3200 --asc 20 --threads 4 $(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ') --branch_eval 10 8 > research/vf/data/branch_f28_wW/summary.tsv && echo "F28 wW: $(grep -c BRANCH research/vf/data/branch_f28_wW/summary.tsv) branch, $(awk -F'\t' '$10==2' research/vf/data/branch_f28_wW/summary.tsv | wc -l) crashed"
python3 research/vf/runstore.py pack research/vf/data/branch_f28_wW research/vf/data/branch_f28_wW_z/all --glob 'branch_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push branch_f28_wW_z
```
Expected run time ≈ 3 h for the first line (≈ 18 decisions per minute on 4 vCPUs). `runstore.py pack` byte-verifies before it
returns. If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the `F28 wW:` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed to
worker/<name>. Do not delete anything from the store. Never commit research/vf/data/.
