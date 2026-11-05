#!/usr/bin/env python3
"""Communication Mod <-> socket relay. This is the process the mod launches (config `command=`).

The mod owns one child process for the lifetime of the game, and the game hangs for up to
`maxInitializationTimeout` seconds if that child is slow to print `ready`. Keeping that child trivial and
letting the real client attach over a Unix socket means the bridge can be rebuilt, restarted or swapped
without restarting the game.

    game --stdin/stdout-- commrelay.py --unix socket-- sts_bridge | commctl.py

Protocol on the socket is the mod's own: one message per line in each direction. The most recent state
line is replayed to each client on connect, so a client always starts from the current state.

Every line in both directions goes to <log-dir>/transcript-<start time>.jsonl. Those transcripts are the
ground truth for the protocol fixtures and for debugging a run after the fact.
"""
import argparse
import json
import os
import socket
import sys
import threading
import time


class Relay:
    def __init__(self, sock_path, log_dir):
        self.sock_path = sock_path
        self.lock = threading.Lock()
        self.client = None
        self.last_state = None
        os.makedirs(log_dir, exist_ok=True)
        stamp = time.strftime("%Y%m%d-%H%M%S")
        self.log = open(os.path.join(log_dir, f"transcript-{stamp}.jsonl"), "a", buffering=1)

    def record(self, direction, line):
        self.log.write(json.dumps({"t": round(time.time(), 3), "dir": direction, "line": line}) + "\n")

    def send_to_client(self, line):
        """Caller holds self.lock. A dead client is dropped; the game side keeps running."""
        if self.client is None:
            return
        try:
            self.client.sendall(line.encode("utf-8") + b"\n")
        except OSError:
            self.drop_client()

    def drop_client(self):
        try:
            self.client.close()
        except OSError:
            pass
        self.client = None

    def pump_game(self):
        """Game -> client. Returns when the game closes our stdin, which ends the relay."""
        for raw in sys.stdin:
            line = raw.rstrip("\n")
            self.record("game", line)
            with self.lock:
                self.last_state = line
                self.send_to_client(line)
        os._exit(0)

    def serve_client(self, conn):
        """Client -> game, until the client disconnects."""
        buf = conn.makefile("r", encoding="utf-8", newline="\n")
        for raw in buf:
            line = raw.rstrip("\n")
            if not line:
                continue
            # A line starting with # is for the transcript only. No mod command starts that way.
            if line.startswith("#"):
                self.record("note", line[1:])
                continue
            self.record("client", line)
            sys.stdout.write(line + "\n")
            sys.stdout.flush()
        with self.lock:
            if self.client is conn:
                self.drop_client()

    def run(self):
        if os.path.exists(self.sock_path):
            os.unlink(self.sock_path)
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        listener.bind(self.sock_path)
        listener.listen(1)
        threading.Thread(target=self.pump_game, daemon=True).start()
        while True:
            conn, _ = listener.accept()
            with self.lock:
                if self.client is not None:  # newest client wins; a stale one must not steal commands
                    self.drop_client()
                self.client = conn
                if self.last_state is not None:
                    self.send_to_client(self.last_state)
            threading.Thread(target=self.serve_client, args=(conn,), daemon=True).start()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sock", default="/tmp/sts_commrelay.sock")
    ap.add_argument("--log-dir", default=os.path.join(here, "logs"))
    args = ap.parse_args()

    relay = Relay(args.sock, args.log_dir)
    # The mod blocks the game until this line arrives, so it goes out before anything else can fail.
    sys.stdout.write("ready\n")
    sys.stdout.flush()
    relay.run()


if __name__ == "__main__":
    main()
