# Worker task: Phase F34 split-half decision measurement (map path + rest site) — NAME = your worker name; settings below

Pre-registered in research/program.md ("Phase F34", 2026-11-08). Same tool and procedure as F33. Offline measurement mode (never used in play):
`fair_eval --branch_eval K 16 --branch_kind KIND` plays each seed by the track's best agent to its (seed mod K)-th decision
of the given kind, then plays every option out 16 times from re-seeded copies and writes `split_<seed>.bin` (rows of 666
features + 8 floats: the two 8-playout half means separately). KIND 3 = map path choice with >= 2 next nodes (K=12), KIND 4 = rest vs smith (K=4). (KIND 1/2 were F33.)

Assignments:
- f34w1 -> A10 map:  ASC=10 PF=research/best_a10_params.txt K=12 KIND=3 START=5520000 N=400 SET=f34_a10map_w1
- f34w2 -> A10 map:  ASC=10 PF=research/best_a10_params.txt K=12 KIND=3 START=5520400 N=400 SET=f34_a10map_w2
- f34w3 -> A10 rest: ASC=10 PF=research/best_a10_params.txt K=4  KIND=4 START=5521000 N=400 SET=f34_a10rest_w3
- f34w4 -> A10 rest: ASC=10 PF=research/best_a10_params.txt K=4  KIND=4 START=5521400 N=400 SET=f34_a10rest_w4
- f34w5 -> A20 map:  ASC=20 PF=research/best_a20_params.txt K=12 KIND=3 START=5522000 N=800 SET=f34_a20map_w5

Before the queue: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical).

Run (replace the capitals; this script is RESUMABLE — it only runs seeds with no line in summary.tsv yet):
`research/tools/f33_gen.sh research/vf/data/SET START N ASC PF K 16 KIND`
then: `python3 research/vf/runstore.py pack research/vf/data/SET research/vf/data/SET_z/all --glob 'split_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z`

**Keep the container alive:** run the generator as a harness-tracked background task (the Bash tool with run_in_background), NOT
with setsid/nohup — a detached process alone does not keep a cloud container from being reclaimed when the session is idle. Keep a
30-minute Monitor (or a send_later check) chained while it runs. After a container restart simply run the same f33_gen.sh command
again (tracked) — it skips finished seeds. Do not delete or regenerate finished files.
Expected time ≈ 1–3 h (16 playouts per option). If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the last `F33 GEN ...` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed to
worker/<name>; message @parent with them (include any crashed seed: a summary row with column 10 = 2). Never commit research/vf/data/.
