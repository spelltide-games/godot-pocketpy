"""An unhandled Python exception breaks; a caught one does not.

No breakpoints are set anywhere in this suite, so the only thing that can stop
the game is the exception itself.

The stack and its variables come from the frame dump pocketpy records as the
exception propagates (py_BaseException__stpush), which it only fills in with
that much detail while py_appcallbacks()->debugger_status answers 1 -- so these
checks cover that callback being claimed, too.
"""

from harness import DebugSession, Suite, fixture_line

FIXTURE = "res://tests/DebuggerExcFixture.py"
SCENE = "res://tests/DebuggerExcFixture.tscn"


def run(verbose=False):
    suite = Suite("exceptions")
    raise_line = fixture_line(FIXTURE, "uncaught_raise")
    call_line = fixture_line(FIXTURE, "uncaught_call")
    try:
        with DebugSession(SCENE, verbose=verbose) as game:
            stop = game.wait_for_break()
            # Anything other than "Breakpoint" is an error break to Godot, which
            # is what makes this a stopped(exception) in a DAP client and what
            # puts it under "Ignore Error Breaks" instead of "Skip Breakpoints".
            suite.check(
                "reason describes the exception",
                stop.reason == "ValueError: fixture blew up",
                stop.reason,
            )
            suite.check("stack dump advertised", stop.has_stackdump)

            frames = game.stack()
            suite.check(
                "innermost frame is the raise",
                frames[:1] == [(FIXTURE, raise_line, "raise_uncaught")],
                frames[:1],
            )
            suite.check(
                "the caller is there too",
                (FIXTURE, call_line, "_ready") in frames,
                frames,
            )

            variables = game.frame_vars(0)
            values = {name: value for _, name, value in variables}
            # The frame this came from was gone before anyone knew the exception
            # was unhandled; the value is here because pocketpy captured it on
            # the way out.
            suite.check(
                "a local from the unwound frame is readable",
                values.get("answer") == 42,
                values.get("answer"),
            )

            game.resume("continue")
            output = game.game_output

        # One break, and the KeyError that _ready() catches was not it.
        suite.check(
            "the caught exception did not break",
            "FIXTURE caught ok" in output,
            output.splitlines()[-5:],
        )
        suite.check(
            "the game kept running after the break",
            "FIXTURE ready" in output,
            output.splitlines()[-5:],
        )
    except AssertionError as exc:
        suite.error(exc)
    return suite
