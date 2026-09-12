"""Drives a headless Godot game over the remote debugger protocol.

The point is to exercise the whole break path -- pocketpy's trace hook, the
`_debug_*` bridge in src/lang/PythonDebugger.cpp, and Godot's engine-side
protocol -- without an editor in the loop. This stands in for the editor: it
listens, launches the game pointed at it, and answers the same commands the
Debugger dock would.

Breakpoints are seeded with Godot's own `--breakpoints file:line` so nothing has
to be clicked, and lines are resolved from `BREAK:` marker comments in the
fixtures rather than written down here.
"""

import os
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import protocol

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROJECT_DIR = os.path.join(REPO_ROOT, "demo")

# Messages that arrive unbidden and are never what a suite is waiting for.
_NOISE = {
    "set_pid",
    "output",
    "error",
    "performance:profile_frame",
    "scene:scene_tree",
    "servers:function_signature",
    "window:title",
}


def godot_binary():
    path = os.environ.get("GODOT_BIN")
    if not path:
        raise SystemExit(
            "Set GODOT_BIN to a Godot 4.4+ editor binary, e.g.\n"
            "  GODOT_BIN=/path/to/godot.windows.editor.dev.x86_64.console.exe python tests/debugger/run.py"
        )
    if not os.path.exists(path):
        raise SystemExit(f"GODOT_BIN does not exist: {path}")
    return path


def fixture_line(res_path, marker):
    """Line number of the `# BREAK:<marker>` comment in a res:// fixture.

    Keeps the suites from hard-coding line numbers, so a fixture can be edited
    without silently retargeting a breakpoint at the wrong statement.
    """
    local = os.path.join(PROJECT_DIR, res_path.removeprefix("res://"))
    needle = f"BREAK:{marker}"
    with open(local, encoding="utf-8") as f:
        for number, text in enumerate(f, start=1):
            if needle in text:
                return number
    raise AssertionError(f"no '{needle}' marker in {res_path}")


class Break:
    """What the game reported when it stopped."""

    def __init__(self, can_continue, reason, has_stackdump):
        self.can_continue = can_continue
        self.reason = reason
        self.has_stackdump = has_stackdump

    def __repr__(self):
        return f"Break(reason={self.reason!r}, has_stackdump={self.has_stackdump})"


class DebugSession:
    """A running game, stopped and resumed synchronously.

    Used as a context manager; the game is killed and its output kept on the
    way out, so a failing suite can print what the game itself said.
    """

    def __init__(self, scene, breakpoints=(), port=0, timeout=30, verbose=False):
        self.scene = scene
        self.breakpoints = list(breakpoints)
        self.port = port
        self.timeout = timeout
        self.verbose = verbose
        self.game = None
        self.session = None
        self.game_output = ""
        self._log_path = None

    # -- lifecycle ---------------------------------------------------------

    def __enter__(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", self.port))
        server.listen(1)
        self.port = server.getsockname()[1]

        self._log_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), f".game-{self.port}.log"
        )
        log = open(self._log_path, "w", encoding="utf-8")
        args = [
            godot_binary(),
            "--headless",
            "--path",
            PROJECT_DIR,
            self.scene,
            "--remote-debug",
            f"tcp://127.0.0.1:{self.port}",
        ]
        if self.breakpoints:
            args += ["--breakpoints", ",".join(f"{f}:{n}" for f, n in self.breakpoints)]
        self._log = log
        self.game = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)

        server.settimeout(self.timeout)
        try:
            conn, _ = server.accept()
        except socket.timeout:
            self._teardown()
            raise AssertionError("the game never connected to the debug host")
        finally:
            server.close()
        conn.settimeout(self.timeout)
        self.session = protocol.Session(conn)
        return self

    def __exit__(self, *exc):
        self._teardown()
        return False

    def _teardown(self):
        if self.game is not None and self.game.poll() is None:
            self.game.kill()
            self.game.wait(timeout=10)
        if getattr(self, "_log", None) is not None:
            self._log.close()
            self._log = None
        if self._log_path and os.path.exists(self._log_path):
            with open(self._log_path, encoding="utf-8", errors="replace") as f:
                self.game_output = f.read()
            os.unlink(self._log_path)

    # -- driving -----------------------------------------------------------

    def _recv(self, wanted):
        """Next message named `wanted`, skipping the unsolicited traffic."""
        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() > deadline:
                raise AssertionError(f"timed out waiting for {wanted!r}")
            message = self.session.recv()
            if message is None:
                raise AssertionError(f"game disconnected while waiting for {wanted!r}")
            name, data = message[0], message[2]
            if name == wanted:
                if self.verbose:
                    print(f"    <- {name}", flush=True)
                return data
            if name not in _NOISE:
                raise AssertionError(f"expected {wanted!r}, got {name!r} with {data!r}")

    def _send(self, command, data=None):
        if self.verbose:
            print(f"    -> {command} {data or []}", flush=True)
        self.session.send(command, data)

    def wait_for_break(self):
        data = self._recv("debug_enter")
        return Break(bool(data[0]), data[1], bool(data[2]))

    def stack(self):
        """Frames as (file, line, func), innermost first."""
        self._send("get_stack_dump")
        data = self._recv("stack_dump")
        # DebuggerMarshalls::ScriptStackDump: [n*3, file, line, func, ...]
        flat = data[1:]
        return [tuple(flat[i : i + 3]) for i in range(0, len(flat), 3)]

    def frame_vars(self, level=0):
        """Variables of one frame as (scope, name, value).

        scope is "local", "member" or "global", matching the type flag
        RemoteDebugger::_send_stack_vars puts on each one.
        """
        self._send("get_stack_frame_vars", [level])
        expected = self._recv("stack_frame_vars")[0]
        scopes = {0: "local", 1: "member", 2: "global"}
        out = []
        for _ in range(expected):
            # DebuggerMarshalls::ScriptStackVariable:
            # [name, scope, value type, value, type hint]
            data = self._recv("stack_frame_var")
            out.append((scopes.get(data[1], data[1]), data[0], data[3]))
        return out

    def resume(self, command="continue"):
        """Answer the break with continue / next / step / out."""
        assert command in ("continue", "next", "step", "out"), command
        self._send(command)
        self._recv("debug_exit")

    def stop_breaking(self):
        """Let the game run to the end without stopping again."""
        self._send("set_skip_breakpoints", [True])


class Suite:
    """Collects checks so one failure does not hide the rest."""

    def __init__(self, name):
        self.name = name
        self.failures = []
        self.checks = 0
        print(f"\n=== {name} ===", flush=True)

    def check(self, label, condition, got=None):
        self.checks += 1
        if condition:
            print(f"  PASS  {label}", flush=True)
        else:
            print(f"  FAIL  {label}   (got {got!r})", flush=True)
            self.failures.append(label)

    def error(self, exc, game_output=""):
        self.checks += 1
        print(f"  FAIL  {self.name} did not run: {exc}", flush=True)
        if game_output:
            tail = "\n".join(game_output.splitlines()[-15:])
            print(f"  --- game output (tail) ---\n{tail}", flush=True)
        self.failures.append(str(exc))
