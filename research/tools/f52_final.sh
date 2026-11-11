#!/usr/bin/env bash
# F52 final (pre-registered): pull+verify the last sets, train sv_55 (h=42) and sv_56 (h=96) on all 13 sets,
# play-test each on 300 fresh seeds at equal wall-clock (f52_eval.sh).
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 research/tools/data_sync.py pull f52_f52w10_z f52_f52w3b_z f52_f52w4b_z f52_f52w6b_z 2>&1 | grep PULL
python3 - <<'PY'
import sys; sys.path.insert(0,'research/vf'); import runstore
tot=0
for s in ['f52_f52w%d_z'%k for k in range(1,11)]+['f52_f52w3b_z','f52_f52w4b_z','f52_f52w6b_z']:
    fs=runstore.list_files(f'research/vf/data/{s}/all'); b=sum(runstore.size(f) for f in fs); tot+=b//824
    print(f'CHECK {s} files={len(fs)} rows={b//824} rem={b%824}')
print(f'CHECK TOTAL rows={tot}')
PY
while pgrep -x fair_eval >/dev/null; do sleep 20; done
SRC=$(for s in 1 2 3 4 5 6 7 8 9 10 3b 4b 6b; do echo research/vf/data/f52_f52w${s}_z/all; done)
python3 research/vf/train_sv_stream.py $SRC --id 55 --hidden 42 --epochs 8 --buf 6 | grep -E "^(groups|SV)"
python3 research/vf/train_sv_stream.py $SRC --id 56 --hidden 96 --epochs 8 --buf 6 | grep -E "^(groups|SV)"
research/tools/f52_eval.sh 55 f52_sv55 300
research/tools/f52_eval.sh 56 f52_sv56 300
echo "== F52 final done $(date -u '+%F %T UTC')"
