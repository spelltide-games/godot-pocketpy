"""next / step / out, which the engine drives through lines_left and depth.

Those two counters live on Godot's ScriptDebugger and are set by the editor's
commands; PythonDebugger only reads them and keeps depth in step with Python's
call depth. So these checks are really about that bookkeeping being right.
"""

from harness import DebugSession, Suite, fixture_line

FIXTURE = "res://tests/DebuggerFixture.py"
HELPER = "res://site-packages/dbgfixture.py"
SCENE = "res://tests/DebuggerFixture.tscn"


def run(verbose=False):
    suite = Suite("stepping")
    try:
        _step_over(suite, verbose)
        _step_in_and_out(suite, verbose)
        _breakpoint_in_imported_module(suite, verbose)
    except AssertionError as exc:
        suite.error(exc)
    return suite


def _step_over(suite, verbose):
    """`next` advances a line at a time without leaving the function."""
    line = fixture_line(FIXTURE, "first_of_three")
    with DebugSession(SCENE, [(FIXTURE, line)], verbose=verbose) as game:
        game.wait_for_break()
        first = game.stack()
        suite.check(
            "stopped in describe()", first[0] == (FIXTURE, line, "describe"), first[0]
        )

        game.resume("next")
        game.wait_for_break()
        second = game.stack()
        suite.check(
            "next lands on the following line, same function",
            second[0] == (FIXTURE, line + 1, "describe"),
            second[0],
        )
        suite.check("next did not change depth", len(second) == len(first), len(second))

        game.resume("next")
        game.wait_for_break()
        third = game.stack()
        suite.check(
            "next again advances one more line",
            third[0] == (FIXTURE, line + 2, "describe"),
            third[0],
        )

        game.stop_breaking()
        game.resume("continue")


def _step_in_and_out(suite, verbose):
    """`step` descends into a Python callee, `out` returns to the caller."""
    line = fixture_line(FIXTURE, "before_helper_call")
    with DebugSession(SCENE, [(FIXTURE, line)], verbose=verbose) as game:
        game.wait_for_break()
        outer = game.stack()
        suite.check(
            "stopped in add_up()", outer[0] == (FIXTURE, line, "add_up"), outer[0]
        )

        game.resume("step")
        game.wait_for_break()
        inner = game.stack()
        suite.check(
            "step enters the imported helper",
            inner[0][0] == HELPER and inner[0][2] == "add",
            inner[0],
        )
        suite.check(
            "step went one frame deeper", len(inner) == len(outer) + 1, len(inner)
        )

        game.resume("out")
        game.wait_for_break()
        back = game.stack()
        suite.check(
            "out returns to the caller at its own depth",
            back[0][0] == FIXTURE and back[0][2] == "add_up" and len(back) == len(outer),
            (back[0], len(back)),
        )

        game.stop_breaking()
        game.resume("continue")


def _breakpoint_in_imported_module(suite, verbose):
    """A breakpoint in res://site-packages/ has to match too.

    pocketpy names an imported module's frames by the relative path it probed
    for, not by a res:// path, while Godot keys breakpoints by res:// path --
    so this is the check that the two are still being reconciled.
    """
    line = fixture_line(HELPER, "inside_helper")
    with DebugSession(SCENE, [(HELPER, line)], verbose=verbose) as game:
        game.wait_for_break()
        frames = game.stack()
        suite.check(
            "breakpoint under site-packages fires",
            frames[0] == (HELPER, line, "add"),
            frames[0],
        )
        suite.check(
            "its caller is the Godot script",
            any(f[0] == FIXTURE for f in frames),
            [f[0] for f in frames],
        )
        game.stop_breaking()
        game.resume("continue")
