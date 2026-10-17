"""Collect v16+ capture run folders from the game folder into fixes/pathfix/data/runs/.

The capture DLL (v16 and later) writes every launch into its own folder
<game>\\pathfix_runs\\<YYYYMMDD_HHMMSS>\\. This script copies each run folder that
is not yet in fixes/pathfix/data/runs/ (never overwrites, never deletes, never modifies the
source) and prints a one-line summary per run.

    python tools/runs/collect_runs.py [--game <game folder>] [--dest fixes/pathfix/data/runs] [--include-open] [--list]

A run whose log has neither "SHUTDOWN" nor "CRASH:" is still open (the game is
running, or it was killed without a crash record); it is skipped unless
--include-open is given. --list only prints the summaries.
"""
import argparse
import os
import re
import shutil
import sys

GAME = r"E:\SteamLibrary\steamapps\common\Total War SHOGUN 2"
HERE = os.path.dirname(os.path.abspath(__file__))
DEST = os.path.join(os.path.dirname(os.path.dirname(HERE)), "fixes", "pathfix", "data", "runs")   # capture runs are pathfix runs so far


def summary(run_dir):
    log_path = os.path.join(run_dir, "pathfix_log.txt")
    info = {"build": "?", "state": "open", "crash": "", "presets": [], "markers": 0, "window_s": None,
            "orders": None, "rows": None, "refused": False, "process": ""}
    if not os.path.exists(log_path):
        info["state"] = "no log"
        return info
    with open(log_path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    m = re.search(r"(?:CAPTURE|FIX PROTOTYPE) BUILD (v\d+)", text)
    if m:
        info["build"] = m.group(1)
    m = re.search(r"=== process (.+?) \(pid", text)
    if m:
        info["process"] = os.path.basename(m.group(1))
    if "REFUSED:" in text:
        info["refused"] = True
    if "SHUTDOWN" in text:
        info["state"] = "closed"
    m = re.search(r"CRASH: unhandled exception (0x[0-9A-F]+) at EIP 0x[0-9A-F]+ \(([^)]*)\)", text)
    if m:
        info["state"] = "crashed"
        info["crash"] = "%s at %s" % (m.group(1), m.group(2))
    info["presets"] = re.findall(r"F6: (?:override preset|corner fix mode) (\d+)|(?:F[67]|I|Lane): (?:corner fix (?:ON|OFF)|lane mode \d selected, fix ON), mode (\d+)", text)
    info["presets"] = [a or b for a, b in info["presets"]]
    info["fix_disabled"] = "FIX DISABLED:" in text
    info["markers"] = len(re.findall(r"(?:F8|O): marker", text))
    secs = re.findall(r"Capturing\.\.\. .*?(\d+)/\d+ s", text)
    if secs:
        info["window_s"] = int(secs[-1])
    m = re.search(r"Soldier trace: (\d+) order records, \d+ go-to records, \d+ samples, (\d+) soldier rows", text)
    if m:
        info["orders"], info["rows"] = int(m.group(1)), int(m.group(2))
    if os.path.exists(os.path.join(run_dir, "crash.dmp")) and not info["crash"]:
        info["crash"] = "crash.dmp present"
    return info


def line(name, info, action):
    parts = ["%-17s" % name, "%-4s" % info["build"], "%-8s" % info["state"]]
    if info["process"]:
        parts.append(info["process"])
    if info["refused"]:
        parts.append("hooks REFUSED (not the game process or bytes differ)")
    if info["window_s"] is not None:
        parts.append("%d s" % info["window_s"])
    if info["orders"] is not None:
        parts.append("%d orders, %d soldier rows" % (info["orders"], info["rows"]))
    if info["presets"]:
        parts.append("fix keys -> " + ",".join(info["presets"]))
    if info["markers"]:
        parts.append("%d markers" % info["markers"])
    if info.get("fix_disabled"):
        parts.append("FIX DISABLED (self-check)")
    if info["crash"]:
        parts.append("CRASH " + info["crash"])
    return "%-9s %s" % (action, "  ".join(parts))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default=GAME)
    ap.add_argument("--dest", default=DEST)
    ap.add_argument("--include-open", action="store_true")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    src_root = os.path.join(args.game, "pathfix_runs")
    if not os.path.isdir(src_root):
        print("no run folders yet: %s does not exist" % src_root)
        return 0
    names = sorted(n for n in os.listdir(src_root) if os.path.isdir(os.path.join(src_root, n)))
    if not names:
        print("no run folders in %s" % src_root)
        return 0
    os.makedirs(args.dest, exist_ok=True)
    copied = 0
    for name in names:
        src = os.path.join(src_root, name)
        dst = os.path.join(args.dest, name)
        info = summary(src)
        if args.list:
            print(line(name, info, "list"))
            continue
        if os.path.exists(dst):
            print(line(name, info, "present"))
            continue
        if info["state"] == "open" and not args.include_open:
            print(line(name, info, "skipped"))
            continue
        shutil.copytree(src, dst)   # fails rather than overwrite (dst does not exist)
        copied += 1
        print(line(name, info, "copied"))
    if not args.list:
        print("%d run folder(s) copied to %s" % (copied, args.dest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
