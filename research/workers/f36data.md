# Worker task: Phase F36 (b) path-decision data from the new A20 best f35a20 (its path choices are made by the learned model pm_1) — NAME = your worker name

Pre-registered in research/program.md ("Phase F36", 2026-11-09). Same tool and procedure as F35; the playing agent is research/best_a20_params.txt, which is now f35a20 (map_model=1, model file research/models/pm_1.txt in the repo). Offline measurement mode (never used in play):
`fair_eval --branch_eval 12 8 --branch_kind 5` plays each seed by the track's best agent to its (seed mod 12)-th map choice
with >= 2 legal next nodes, then plays every option out 8 times from re-seeded copies and writes `path_<seed>.bin` (rows of
666 run features of the pre-move state + 20 path features + 8 tail floats). Many seeds end with no decision (tail n = -1): normal.

Assignments:
- f36w1 -> A20 data: ASC=20 PF=research/best_a20_params.txt START=5660000 N=4000 M=8  SET=f36_a20_w1
- f36w2 -> A20 data: ASC=20 PF=research/best_a20_params.txt START=5664000 N=4000 M=8  SET=f36_a20_w2
- f36w3 -> A20 data: ASC=20 PF=research/best_a20_params.txt START=5668000 N=4000 M=8  SET=f36_a20_w3
- f36w4 -> A20 data: ASC=20 PF=research/best_a20_params.txt START=5672000 N=4000 M=8  SET=f36_a20_w4
- f36w5 -> A20 held-out: ASC=20 PF=research/best_a20_params.txt START=5680000 N=800 M=16 SET=f36_hold_a20

Before the run: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical).

Run (RESUMABLE — only seeds with no line in summary.tsv yet are run):
`research/tools/f33_gen.sh research/vf/data/SET START N ASC PF 12 M 5`   (check first: `grep map_model research/best_a20_params.txt` must print map_model=1)
then: `python3 research/vf/runstore.py pack research/vf/data/SET research/vf/data/SET_z/all --glob 'path_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z`

**Keep the container alive:** run the generator as a harness-tracked background task (the Bash tool with run_in_background), NOT
with setsid/nohup. Keep a 30-minute Monitor (or a send_later check) chained while it runs. After a container restart or the
2-hour background limit, run the same f33_gen.sh command again (tracked) — it skips finished seeds. Do not delete or regenerate
finished files. Expected time ≈ 3–4 h (held-out set ≈ 1.5 h). If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the last `F33 GEN ...` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed
to worker/<name>; message @parent with them (include any crashed seed: a summary row with column 10 = 2). Never commit
research/vf/data/.
