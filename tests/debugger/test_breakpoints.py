"""A breakpoint stops the VM, and the editor can read the stop."""

from harness import DebugSession, Suite, fixture_line

FIXTURE = "res://tests/DebuggerFixture.py"
SCENE = "res://tests/DebuggerFixture.tscn"


def run(verbose=False):
    suite = Suite("breakpoints")
    line = fixture_line(FIXTURE, "first_of_three")
    try:
        with DebugSession(SCENE, [(FIXTURE, line)], verbose=verbose) as game:
            stop = game.wait_for_break()
            # Godot derives "is this an error break" from the reason, and the
            # editor's DAP bridge turns anything but this literal into a
            # stopped(exception). A plain breakpoint has to say "Breakpoint".
            suite.check("reason is 'Breakpoint'", stop.reason == "Breakpoint", stop.reason)
            # RemoteDebugger::debug() reports this from
            # debug_get_stack_level_count() > 0; at zero the editor never even
            # asks for a stack.
            suite.check("stack dump advertised", stop.has_stackdump)
            suite.check("break is resumable", stop.can_continue)

            frames = game.stack()
            suite.check("stack is not empty", len(frames) > 0, frames)
            if frames:
                suite.check(
                    "innermost frame is the breakpoint",
                    frames[0] == (FIXTURE, line, "describe"),
                    frames[0],
                )
                suite.check(
                    "caller frames are present",
                    any(f[2] == "_ready" for f in frames),
                    [f[2] for f in frames],
                )

            variables = game.frame_vars(0)
            names = {(scope, name) for scope, name, _ in variables}
            values = {name: value for _, name, value in variables}
            # `self` twice over: once as a genuine Python local, and once as the
            # member row RemoteDebugger prepends from
            # _debug_get_stack_level_instance -- which is null unless the
            # engine-side ScriptInstance was handed back correctly.
            suite.check("locals include 'self'", ("local", "self") in names, sorted(names))
            suite.check("members include 'self'", ("member", "self") in names, sorted(names))
            suite.check(
                "exported member converts to a Variant",
                values.get("tag") == "fixture",
                values.get("tag"),
            )
            # Root imports still include singletons and built-in types, which
            # the globals scope is supposed to filter back out.
            suite.check(
                "globals are not flooded by the star import",
                sum(1 for scope, _, _ in variables if scope == "global") < 20,
                sum(1 for scope, _, _ in variables if scope == "global"),
            )
            suite.check(
                "constant names outside godot root remain visible as user globals",
                ("global", "KEY_SPACE") in names and values.get("KEY_SPACE") == 123,
                values.get("KEY_SPACE"),
            )

            game.resume("continue")
            game.stop_breaking()
            suite.check("game survived the break", True)
    except AssertionError as exc:
        suite.error(exc, getattr(exc, "game_output", ""))
    return suite
