# Worker task: Phase F35 path-decision data (map choices with per-option path features) — NAME = your worker name

Pre-registered in research/program.md ("Phase F35", 2026-11-08). Offline measurement mode (never used in play):
`fair_eval --branch_eval 12 8 --branch_kind 5` plays each seed by the track's best agent to its (seed mod 12)-th map choice
with >= 2 legal next nodes, then plays every option out 8 times from re-seeded copies and writes `path_<seed>.bin` (rows of
666 run features of the pre-move state + 20 path features + 8 tail floats). Many seeds end with no decision (tail n = -1): normal.

Assignments (all K=12, M=8, KIND=5):
- f35w1  -> A20: ASC=20 PF=research/best_a20_params.txt START=5600000 N=4000 SET=f35_a20_w1
- f35w2  -> A20: ASC=20 PF=research/best_a20_params.txt START=5604000 N=4000 SET=f35_a20_w2
- f35w3  -> A20: ASC=20 PF=research/best_a20_params.txt START=5608000 N=4000 SET=f35_a20_w3
- f35w4  -> A20: ASC=20 PF=research/best_a20_params.txt START=5612000 N=4000 SET=f35_a20_w4
- f35w5  -> A10: ASC=10 PF=research/best_a10_params.txt START=5620000 N=2700 SET=f35_a10_w5
- f35w6  -> A10: ASC=10 PF=research/best_a10_params.txt START=5622700 N=2700 SET=f35_a10_w6
- f35w7  -> A10: ASC=10 PF=research/best_a10_params.txt START=5625400 N=2700 SET=f35_a10_w7
- f35w8  -> A10: ASC=10 PF=research/best_a10_params.txt START=5628100 N=2700 SET=f35_a10_w8
- f35w9  -> A10: ASC=10 PF=research/best_a10_params.txt START=5630800 N=2700 SET=f35_a10_w9
- f35w10 -> A10: ASC=10 PF=research/best_a10_params.txt START=5633500 N=2700 SET=f35_a10_w10

Before the run: README steps 1–3 (branch `worker/<NAME>` from origin/autoresearch/ironclad, `bash research/setup.sh`, the three
fork repro_check gates — stop and report if any is not bit-identical).

Run (RESUMABLE — only seeds with no line in summary.tsv yet are run):
`research/tools/f33_gen.sh research/vf/data/SET START N ASC PF 12 8 5`
then: `python3 research/vf/runstore.py pack research/vf/data/SET research/vf/data/SET_z/all --glob 'path_*.bin' && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push SET_z`

**Keep the container alive:** run the generator as a harness-tracked background task (the Bash tool with run_in_background), NOT
with setsid/nohup. Keep a 30-minute Monitor (or a send_later check) chained while it runs. After a container restart or the
2-hour background limit, run the same f33_gen.sh command again (tracked) — it skips finished seeds. Do not delete or regenerate
finished files. Expected time: A20 ≈ 3–4 h, A10 ≈ 5–6 h. If a push fails on HTTP 429, wait 15 minutes and re-run that line.
Hand back: the last `F33 GEN ...` line and the PACK / PUSH ... DONE lines in research/workers/<name>.log.md, committed and pushed
to worker/<name>; message @parent with them (include any crashed seed: a summary row with column 10 = 2). Never commit
research/vf/data/.
