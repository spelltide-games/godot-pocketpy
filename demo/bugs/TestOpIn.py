from godot import *
from godot.classes import Node2D

class TestOpIn(Extends(Node2D)):
    def _ready(self):
        a = Array()
        a.append(1)
        a.append(2)
        print(1 in a, 3 in a)
