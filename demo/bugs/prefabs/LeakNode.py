from godot import *
from godot.classes import Node2D

from gc import setup_debug_callback


setup_debug_callback(lambda phase, info: print(f"GC phase: {phase}\n{info}"))

count = 0

class LeakNode(Extends(Node2D)):

    z = export(int)
    x = export(float)
    y = export(str)

    def _ready(self):
        global count
        count += 1
        #print(count)

        if 1:
            # this is okay
            # pkpy will run gc to collect them
            self.leak = [object() for i in range(1000)]
        else:
            # this is not okay
            # pkpy's gc is based on object counts
            # allocating large (but few) objects may not trigger gc
            # so you may think there is leakage
            self.leak = [1] * 1000
