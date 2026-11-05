#!/usr/bin/env python3
"""Drive the running game by hand through commrelay.py. Used to explore screens and capture protocol samples.

    commctl.py state                 print a summary of the current state
    commctl.py send "START IRONCLAD 0 ABC"   send a command, wait for the next stable state, print a summary
    commctl.py --raw send "STATE"    same, printing the full JSON message
"""
import argparse
import json
import socket
import sys
import time


def summarize(msg):
    if "error" in msg:
        return f"ERROR: {msg['error']}"
    gs = msg.get("game_state")
    head = f"ready={msg.get('ready_for_command')} cmds={msg.get('available_commands')}"
    if gs is None:
        return head + "\n(not in game)"
    lines = [
        head,
        f"floor={gs['floor']} act={gs['act']} hp={gs['current_hp']}/{gs['max_hp']} gold={gs['gold']} "
        f"screen={gs['screen_type']}/{gs['screen_name']} room={gs['room_type']}/{gs['room_phase']}",
    ]
    if gs.get("choice_list"):
        lines.append(f"choices={gs['choice_list']}")
    if gs.get("screen_state"):
        lines.append("screen_state=" + json.dumps(gs["screen_state"])[:1500])
    cs = gs.get("combat_state")
    if cs:
        p = cs["player"]
        lines.append(f"turn={cs['turn']} energy={p['energy']} block={p['block']} powers={[(x['id'], x['amount']) for x in p['powers']]}")
        lines.append("hand=" + ", ".join(f"{c['id']}+{c['upgrades']}({c['cost']})" for c in cs["hand"]))
        for i, m in enumerate(cs["monsters"]):
            lines.append(
                f"  [{i}] {m['id']} hp={m['current_hp']}/{m['max_hp']} block={m['block']} intent={m.get('intent')} "
                f"move={m.get('move_id')} dmg={m.get('move_adjusted_damage')}x{m.get('move_hits')} "
                f"powers={[(x['id'], x['amount']) for x in m['powers']]}"
            )
    return "\n".join(lines)


def read_message(f):
    line = f.readline()
    if not line:
        sys.exit("relay closed the connection")
    return json.loads(line)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sock", default="/tmp/sts_commrelay.sock")
    ap.add_argument("--raw", action="store_true", help="print the full JSON message")
    ap.add_argument("--timeout", type=float, default=60.0)
    ap.add_argument("verb", choices=["state", "send"])
    ap.add_argument("command", nargs="?")
    args = ap.parse_args()

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(args.sock)
    s.settimeout(args.timeout)
    f = s.makefile("r", encoding="utf-8", newline="\n")

    if args.verb == "state":
        s.sendall(b"STATE\n")
        # The replayed last state arrives first; the answer to STATE is the freshest one.
        msg = read_message(f)
        deadline = time.time() + 1.0
        s.settimeout(1.0)
        try:
            while time.time() < deadline:
                msg = read_message(f)
        except (socket.timeout, TimeoutError):
            pass
    else:
        # Consume the replayed state so the summary below reflects the result of this command.
        s.settimeout(0.3)
        try:
            read_message(f)
        except (socket.timeout, TimeoutError):
            pass
        s.settimeout(args.timeout)
        s.sendall(args.command.encode() + b"\n")
        msg = read_message(f)
        while "error" not in msg and not msg.get("ready_for_command"):
            msg = read_message(f)

    print(json.dumps(msg) if args.raw else summarize(msg))


if __name__ == "__main__":
    main()
