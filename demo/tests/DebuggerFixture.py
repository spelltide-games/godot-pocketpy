# Fixture for the debugger tests in tests/debugger.
#
# The suites locate the lines they break on by searching for the `BREAK:` marker
# comments below, never by line number, so this file can be edited freely.
from godot import *
from godot.classes import Node

import dbgfixture

# A user global may share a name with a constant outside the godot root module.
KEY_SPACE = 123


class DebuggerFixture(Extends(Node)):
    tag = export(str, default='fixture')

    def _ready(self):
        total = self.add_up(2, 3)
        label = self.describe(total)
        print('FIXTURE ready', label)

    def add_up(self, a: int, b: int):
        return dbgfixture.add(a, b)  # BREAK:before_helper_call

    def describe(self, value: int):
        prefix = 'value'  # BREAK:first_of_three
        joined = f'{prefix}={value}'
        return joined
