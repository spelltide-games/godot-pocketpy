from godot import *
from godot.classes import Node2D

count = 0

class LeakNode(Extends(Node2D)):

    z = export(int)
    x = export(float)
    y = export(str)

    def _ready(self):
        global count
        count += 1
        #print(count)

        self.leak = [1] * 1000
