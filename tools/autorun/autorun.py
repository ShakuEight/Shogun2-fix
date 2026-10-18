"""Unattended replay run: start Shogun 2 minimized, play one battle replay, quit the game.

    python tools\\autorun\\autorun.py "4 kyoto police running"          (name in the replays folder)
    python tools\\autorun\\autorun.py C:\\path\\to\\some.replay --capture --collect
    python tools\\autorun\\autorun.py --install                          (copy build\\version.dll in)

How it works (tools/autorun/README.md):
  1. backs up %APPDATA%\\The Creative Assembly\\Shogun2\\scripts\\user.script.txt and appends
     `game_startup_mode replay "<file>";`, so the game skips the frontend and opens the replay;
  2. starts shogun2.exe minimized (AUTORUN_MINIMIZE=1 makes the proxy keep it minimized;
     PATHFIX_CAPTURE=1 with --capture);
  3. talks to the version.dll proxy over \\\\.\\pipe\\s2autorun_<pid>: waits for BATTLE, then for
     BATTLE_OVER (the engine's battle-over flag) or a 30 s tick stall, then sends `close`
     (WM_CLOSE, the game shuts down cleanly in about 3 s);
  4. always restores user.script.txt, saves a transcript and the proxy log under
     tools/autorun/data/runs/<stamp>/ and, with --capture, copies them into the new pathfix run folder.

Exit codes: 0 battle played to its end and the game closed; 1 setup error; 2 timeout; 3 the game
exited or the pipe broke before the battle ended. Standard library only.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import hashlib
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_GAME_DIR = r"E:\SteamLibrary\steamapps\common\Total War SHOGUN 2"
APPDATA_DIR = os.path.join(os.environ.get("APPDATA", ""), "The Creative Assembly", "Shogun2")
USER_SCRIPT = os.path.join(APPDATA_DIR, "scripts", "user.script.txt")
SCRIPT_BAK = USER_SCRIPT + ".autorun_bak"
REPLAYS_DIR = os.path.join(APPDATA_DIR, "replays")
RUNS_DIR = os.path.join(REPO, "tools", "autorun", "data", "runs")

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.CreateFileW.restype = wt.HANDLE
k32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID, wt.DWORD, wt.DWORD, wt.HANDLE]
k32.PeekNamedPipe.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD, wt.LPVOID, ctypes.POINTER(wt.DWORD), wt.LPVOID]
k32.ReadFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID]
k32.WriteFile.argtypes = [wt.HANDLE, wt.LPCVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID]
k32.CloseHandle.argtypes = [wt.HANDLE]
INVALID_HANDLE = wt.HANDLE(-1).value
GENERIC_RW = 0x80000000 | 0x40000000
OPEN_EXISTING = 3


class Transcript:
    """Timestamped lines to stdout and to a file."""

    def __init__(self):
        self.lines = []

    def __call__(self, text):
        line = "[%s] %s" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], text)
        print(line, flush=True)
        self.lines.append(line)

    def save(self, path):
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(self.lines) + "\n")


log = Transcript()


class PipeClient:
    """Byte-mode named pipe client polled from one thread (no blocking read)."""

    def __init__(self, name):
        self.name = name
        self.h = None
        self.buf = b""

    def connect(self):
        h = k32.CreateFileW(self.name, GENERIC_RW, 0, None, OPEN_EXISTING, 0, None)
        if h == INVALID_HANDLE or h is None:
            return False
        self.h = h
        return True

    def send(self, line):
        data = (line + "\n").encode("utf-8")
        n = wt.DWORD()
        ok = k32.WriteFile(self.h, data, len(data), ctypes.byref(n), None)
        log("> " + line)
        return bool(ok) and n.value == len(data)

    def poll(self):
        """Return the complete lines received so far; None when the pipe is broken."""
        avail = wt.DWORD()
        if not k32.PeekNamedPipe(self.h, None, 0, None, ctypes.byref(avail), None):
            return None
        if avail.value:
            buf = ctypes.create_string_buffer(avail.value)
            got = wt.DWORD()
            if not k32.ReadFile(self.h, buf, avail.value, ctypes.byref(got), None):
                return None
            self.buf += buf.raw[: got.value]
        lines = []
        while b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            lines.append(line.decode("utf-8", "replace").rstrip("\r"))
        return lines

    def close(self):
        if self.h:
            k32.CloseHandle(self.h)
            self.h = None


def processes(name):
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq %s" % name, "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    pids = []
    for row in out.splitlines():
        parts = [p.strip('"') for p in row.split('","')]
        if len(parts) > 1 and parts[0].lower() == name.lower():
            pids.append(int(parts[1]))
    return pids


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_script(path):
    raw = open(path, "rb").read()
    return raw[2:].decode("utf-16-le") if raw[:2] == b"\xff\xfe" else raw.decode("utf-16-le")


def write_script(path, text):
    tmp = path + "_tmp"
    with open(tmp, "wb") as f:
        f.write(b"\xff\xfe" + text.encode("utf-16-le"))
    os.replace(tmp, path)


def restore_script():
    if os.path.exists(SCRIPT_BAK):
        os.replace(SCRIPT_BAK, USER_SCRIPT)
        log("user.script.txt restored from %s" % os.path.basename(SCRIPT_BAK))


def resolve_replay(arg):
    candidates = [arg, os.path.join(REPLAYS_DIR, arg), os.path.join(REPLAYS_DIR, arg + ".replay")]
    for c in candidates:
        if os.path.isfile(c):
            return os.path.abspath(c)
    return None


def install(game_dir):
    src = os.path.join(REPO, "build", "version.dll")
    dst = os.path.join(game_dir, "version.dll")
    if not os.path.exists(src):
        sys.exit("build\\version.dll not found: run build.bat -DTOOL_AUTORUN=ON first")
    if processes("shogun2.exe"):
        sys.exit("the game is running; close it before installing")
    if os.path.exists(dst):
        if sha256(dst) == sha256(src):
            print("version.dll in the game folder is already the current build")
            return
        bak = dst + ".bak"
        n = 1
        while os.path.exists(bak):
            n += 1
            bak = "%s.bak%d" % (dst, n)
        os.replace(dst, bak)
        print("previous version.dll kept as %s" % bak)
    tmp = dst + "_tmp"
    shutil.copyfile(src, tmp)
    os.replace(tmp, dst)
    print("installed %s -> %s" % (src, dst))


def preflight(game_dir, replay):
    ok = True
    if not processes("steam.exe"):
        log("ERROR Steam is not running (the game needs it)")
        ok = False
    if processes("shogun2.exe"):
        log("ERROR shogun2.exe is already running")
        ok = False
    exe = os.path.join(game_dir, "shogun2.exe")
    if not os.path.isfile(exe):
        log("ERROR %s not found" % exe)
        ok = False
    proxy = os.path.join(game_dir, "version.dll")
    built = os.path.join(REPO, "build", "version.dll")
    if not os.path.isfile(proxy):
        log("ERROR %s missing: run autorun.py --install" % proxy)
        ok = False
    elif os.path.isfile(built) and sha256(proxy) != sha256(built):
        log("WARNING the game folder's version.dll differs from build\\version.dll (autorun.py --install)")
    if not os.path.isfile(os.path.join(game_dir, "dinput8.dll")):
        log("WARNING no dinput8.dll in the game folder: the run plays the unmodified game")
    if replay is None:
        log("ERROR replay not found (give a path or a name in %s)" % REPLAYS_DIR)
        ok = False
    return ok


def latest_pathfix_run(game_dir):
    latest = os.path.join(game_dir, "pathfix_runs", "latest.txt")
    if not os.path.isfile(latest):
        return None
    name = open(latest).read().strip()
    path = os.path.join(game_dir, "pathfix_runs", name)
    return path if os.path.isdir(path) else None


def run(args):
    replay = resolve_replay(args.replay)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log("autorun %s: replay %s" % (stamp, replay))
    if os.path.exists(SCRIPT_BAK):
        log("a previous run left %s behind; restoring it first" % os.path.basename(SCRIPT_BAK))
        restore_script()
    if not preflight(args.game_dir, replay):
        return 1

    lines = ['game_startup_mode replay "%s";' % replay]
    if args.speed and args.speed != 1:
        lines.append("replay_speed %g;" % args.speed)
    lines += args.script_line or []

    shutil.copyfile(USER_SCRIPT, SCRIPT_BAK)
    proc = None
    pipe = None
    result = 1
    reached = set()
    pathfix_run = None
    try:
        text = read_script(SCRIPT_BAK)
        if text and not text.endswith("\r\n"):
            text += "\r\n"
        write_script(USER_SCRIPT, text + "".join(l + "\r\n" for l in lines))
        for l in lines:
            log("user.script.txt += %s" % l)

        env = dict(os.environ)
        env.pop("PATHFIX_CAPTURE", None)
        if args.capture:
            env["PATHFIX_CAPTURE"] = "1"
        if not args.no_minimize:
            env["AUTORUN_MINIMIZE"] = "1"
        for kv in args.env or []:
            name, _, value = kv.partition("=")
            env[name] = value
            log("env %s=%s" % (name, value))
        si = subprocess.STARTUPINFO()
        si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        si.wShowWindow = 1 if args.no_minimize else 7   # SW_SHOWNORMAL / SW_SHOWMINNOACTIVE
        t0 = time.time()
        proc = subprocess.Popen([os.path.join(args.game_dir, "shogun2.exe")], cwd=args.game_dir, env=env,
                                startupinfo=si)
        log("shogun2.exe started, pid %d%s" % (proc.pid, ", PATHFIX_CAPTURE=1" if args.capture else ""))

        pipe = PipeClient(r"\\.\pipe\s2autorun_%d" % proc.pid)
        while not pipe.connect():
            if proc.poll() is not None:
                log("ERROR the game exited (code %s) before the proxy pipe appeared" % proc.returncode)
                return 3
            if time.time() - t0 > args.connect_timeout:
                log("ERROR no proxy pipe after %d s (is version.dll installed?)" % args.connect_timeout)
                return 2
            time.sleep(0.5)
        log("connected to %s" % pipe.name)

        phase = "load"
        deadline = time.time() + args.load_timeout
        battle_start = None
        end_reason = None
        last_tick = None
        while True:
            got = pipe.poll()
            if got is None:
                log("pipe closed by the game")
                end_reason = end_reason or "pipe"
                break
            for line in got:
                if not line.startswith("event heartbeat") or args.verbose:
                    log("< " + line)
                parts = line.split()
                if line.startswith("event heartbeat"):
                    for p in parts:
                        if p.startswith("tick="):
                            last_tick = int(p[5:])
                if line.startswith("event state") or line.startswith("event hello"):
                    st = parts[2] if line.startswith("event state") else \
                        next((p[6:] for p in parts if p.startswith("state=")), "")
                    reached.add(st)
                    if st == "BATTLE" and phase == "load":
                        phase = "battle"
                        battle_start = time.time()
                        deadline = time.time() + args.timeout
                        log("battle running (load took %.0f s)" % (battle_start - t0))
                    elif st == "BATTLE_OVER" and phase in ("load", "battle"):
                        end_reason = "battle_over"
                    elif st == "FRONTEND" and phase == "battle":
                        end_reason = "frontend"
                if line.startswith("event stalled") and phase == "battle" and not args.no_stall_end:
                    end_reason = "stalled"
            if end_reason:
                break
            if proc.poll() is not None:
                log("the game exited (code %s) during phase %s" % (proc.returncode, phase))
                return 3
            if time.time() > deadline:
                log("TIMEOUT in phase %s after %.0f s" % (phase, time.time() - t0))
                end_reason = "timeout"
                break
            time.sleep(0.1)

        if battle_start:
            log("battle ended: %s after %.0f s of battle%s" % (
                end_reason, time.time() - battle_start, "" if last_tick is None else ", last tick %d" % last_tick))
        if end_reason in ("battle_over", "stalled", "frontend"):
            result = 0
        elif end_reason == "timeout":
            result = 2
        else:
            result = 3
        if args.keep_open:
            log("--keep-open: leaving the game running")
            return result
        if args.linger:
            time.sleep(args.linger)
        if pipe.h and pipe.send("close"):
            t_close = time.time()
            while time.time() - t_close < 3:
                got = pipe.poll()
                if got is None:
                    break
                for line in got:
                    log("< " + line)
                time.sleep(0.1)
        try:
            proc.wait(timeout=args.close_timeout)
            log("game exited (code %s) %.0f s after launch" % (proc.returncode, time.time() - t0))
        except subprocess.TimeoutExpired:
            log("ERROR the game did not exit within %d s of close; terminating it" % args.close_timeout)
            proc.terminate()
            proc.wait(timeout=30)
            result = result or 2
        return result
    finally:
        if pipe:
            pipe.close()
        if proc and proc.poll() is None and not args.keep_open:
            log("terminating the game (run aborted)")
            proc.terminate()
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                pass
        if args.keep_open and proc and proc.poll() is None:
            log("the game keeps running; user.script.txt is restored now (it is read only at startup)")
        restore_script()
        out_dir = os.path.join(RUNS_DIR, stamp)
        os.makedirs(out_dir, exist_ok=True)
        proxy_log = os.path.join(args.game_dir, "autorun_log.txt")
        if os.path.isfile(proxy_log):
            shutil.copyfile(proxy_log, os.path.join(out_dir, "autorun_log.txt"))
        if args.capture:
            pathfix_run = latest_pathfix_run(args.game_dir)
        log("result %d, states seen: %s" % (result, ", ".join(sorted(reached)) or "none"))
        if pathfix_run:
            log("pathfix run folder: %s" % pathfix_run)
        log.save(os.path.join(out_dir, "driver.txt"))
        if pathfix_run:
            for f in ("driver.txt", "autorun_log.txt"):
                src = os.path.join(out_dir, f)
                dst = os.path.join(pathfix_run, "autorun_" + f if not f.startswith("autorun") else f)
                if os.path.isfile(src) and not os.path.exists(dst):
                    shutil.copyfile(src, dst)
        print("transcript: %s" % out_dir, flush=True)
        if args.collect and args.capture:
            subprocess.run([sys.executable, os.path.join(REPO, "tools", "runs", "collect_runs.py")])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("replay", nargs="?", help="replay file, or a name in the replays folder")
    ap.add_argument("--game-dir", default=DEFAULT_GAME_DIR)
    ap.add_argument("--capture", action="store_true", help="start with PATHFIX_CAPTURE=1 (pathfix run folder)")
    ap.add_argument("--collect", action="store_true", help="with --capture: run tools/runs/collect_runs.py after")
    ap.add_argument("--speed", type=float, default=1, help="adds replay_speed <n>; (game-speed multiplier, 4 measured as 40 ticks/s; default 1)")
    ap.add_argument("--script-line", action="append", help="extra user.script.txt line, e.g. \"battle_time_limit 0;\"")
    ap.add_argument("--env", action="append", metavar="NAME=VALUE",
                    help="extra environment variable for the game process (repeatable), e.g. ARTFIX_OFF=1")
    ap.add_argument("--no-minimize", action="store_true", help="show the game window normally")
    ap.add_argument("--keep-open", action="store_true", help="do not close the game at the end")
    ap.add_argument("--no-stall-end", action="store_true", help="a 30 s tick stall does not end the run")
    ap.add_argument("--timeout", type=int, default=3600, help="battle phase limit in seconds (default 3600)")
    ap.add_argument("--load-timeout", type=int, default=300, help="launch to battle limit (default 300)")
    ap.add_argument("--connect-timeout", type=int, default=60)
    ap.add_argument("--close-timeout", type=int, default=60)
    ap.add_argument("--linger", type=float, default=2.0, help="seconds to wait after the battle before closing")
    ap.add_argument("--install", action="store_true", help="copy build\\version.dll into the game folder and stop")
    ap.add_argument("--verbose", action="store_true", help="also print the 5 s heartbeats")
    args = ap.parse_args()
    if args.install:
        install(args.game_dir)
        return 0
    if not args.replay:
        ap.error("give a replay (or --install)")
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
