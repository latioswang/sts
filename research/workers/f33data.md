# Worker task: Phase F33 split-half decision measurement — NAME = your worker name; settings below

Pre-registered in research/program.md ("Phase F33", 2026-11-08). Offline measurement mode (never used in play):
`fair_eval --branch_eval K 16 --branch_kind KIND` plays each seed by the track's best agent to its (seed mod K)-th decision
of the given kind, then plays every option out 16 times from re-seeded copies and writes `split_<seed>.bin` (rows of 666
features + 8 floats: the two 8-playout half means separately). KIND 1 = card reward (K=10), KIND 2 = boss relic (K=2).

Assignments (400 seeds each):
- f33w1 -> A10 card:  ASC=10 PF=research/best_a10_params.txt K=10 KIND=1 START=5500000 SET=f33_a10card_w1
- f33w2 -> A10 card:  ASC=10 PF=research/best_a10_params.txt K=10 KIND=1 START=5500400 SET=f33_a10card_w2
- f33w3 -> A10 boss:  ASC=10 PF=research/best_a10_params.txt K=2  KIND=2 START=5501000 SET=f33_a10boss_w3
- f33w4 -> A10 boss:  ASC=10 PF=research/best_a10_params.txt K=2  KIND=2 START=5501400 SET=f33_a10boss_w4
- f33w5 -> A20 boss:  ASC=20 PF=research/best_a20_params.txt K=2  KIND=2 START=5503000 SET=f33_a20boss_w5
- f33w6 -> A20 boss:  ASC=20 PF=research/best_a20_params.txt K=2  KIND=2 START=5503400 SET=f33_a20boss_w6

Before the queue: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical).

Run (replace the capitals; this script is RESUMABLE — it only runs seeds with no line in summary.tsv yet):
`research/tools/f33_gen.sh research/vf/data/SET START 400 ASC PF K 16 KIND`
then: `python3 research/vf/runstore.py pack research/vf/data/SET research/vf/data/SET_z/all --glob 'split_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z`

**Keep the container alive:** run the generator as a harness-tracked background task (the Bash tool with run_in_background), NOT
with setsid/nohup — a detached process alone does not keep a cloud container from being reclaimed when the session is idle. Keep a
30-minute Monitor (or a send_later check) chained while it runs. After a container restart simply run the same f33_gen.sh command
again (tracked) — it skips finished seeds. Do not delete or regenerate finished files.
Expected time ≈ 2–2.5 h (16 playouts per option). If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the last `F33 GEN ...` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed to
worker/<name>; message @parent with them (include any crashed seed: a summary row with column 10 = 2). Never commit research/vf/data/.
