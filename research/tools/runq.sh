#!/usr/bin/env bash
# Sequential experiment queue (replaces chains of background waiters, which deadlocked twice on
# self-matching `pgrep -f`). Lines of research/queue.txt are shell commands run one at a time from
# the repo root; '#' lines are comments. A line is removed from the queue when it starts and its
# start/end are appended to research/queue.log. Exits with "QUEUE EMPTY" when nothing is left.
# Append new lines to research/queue.txt at any time; only one runner may be active (flock).
# Build steps belong in the queue too, so a rebuild never races a running evaluation.
set -uo pipefail
cd "$(dirname "$0")/../.."
Q=research/queue.txt; LOG=research/queue.log; LOCK=research/.queue.lock
touch "$Q"
exec 9>"$LOCK"; flock -n 9 || { echo "a queue runner is already active" >&2; exit 1; }
CUR=research/queue.current   # the line being executed; re-run first if the runner was killed mid-line
if [ -s "$CUR" ]; then
  echo "== $(date -u +%H:%M:%S) RESUME interrupted line: $(cat "$CUR")" | tee -a "$LOG"
  { cat "$CUR"; echo; cat "$Q"; } > "$Q.tmp" && mv "$Q.tmp" "$Q"; : > "$CUR"
fi
while :; do
  line=$(grep -m1 -vE '^[[:space:]]*(#|$)' "$Q" || true)
  if [ -z "$line" ]; then echo "QUEUE EMPTY $(date -u +%H:%M:%S)"; exit 0; fi
  python3 - "$Q" "$line" <<'PY'
import sys
q, l = sys.argv[1], sys.argv[2]
lines = open(q).read().split('\n'); lines.remove(l); open(q, 'w').write('\n'.join(lines))
PY
  printf '%s\n' "$line" > "$CUR"
  echo "== $(date -u +%H:%M:%S) START: $line" | tee -a "$LOG"
  bash -c "$line"; rc=$?
  : > "$CUR"
  echo "== $(date -u +%H:%M:%S) END rc=$rc: $line" | tee -a "$LOG"
done
