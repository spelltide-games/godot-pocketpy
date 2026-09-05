from godot import *
from godot.classes import Node2D

class TestYieldSignalWithArgs(Extends(Node2D)):
    signal_with_args = signal('x', 'y')

    def _ready(self):
        self.start_coroutine(self.gen())

    def gen(self):
        print(1 )
        yield self.owner.get_tree().create_timer(1.0).timeout
        print(2)
        yield self.signal_with_args
        print(3)

    def _process(self, delta: float):
        if Input.is_key_pressed(KEY_SPACE):
            self.signal_with_args.emit(1, 2)
