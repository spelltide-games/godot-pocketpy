# Exception fixture for the debugger tests in tests/debugger.
#
# Raises twice on purpose: the first is caught, and must NOT produce a break,
# because the debugger is only meant to stop on exceptions that escape Python.
from godot import *
from godot.classes import Node


class DebuggerExcFixture(Extends(Node)):
    def _ready(self):
        try:
            self.raise_caught()
        except KeyError:
            print('FIXTURE caught ok')
        self.raise_uncaught()  # BREAK:uncaught_call
        print('FIXTURE ready')

    def raise_caught(self):
        raise KeyError('handled')

    def raise_uncaught(self):
        answer = 42
        raise ValueError('fixture blew up')  # BREAK:uncaught_raise
