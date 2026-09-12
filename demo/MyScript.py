from godot import *
from godot.classes import Node, TileSet, Vector2, Vector3, Color, PhysicsRayQueryParameters3D
from godot.constants import TYPE_BOOL, KEY_SPACE

import test

class MyScript(Extends(Node)):
	x = export(int, default=10)
	y = export(Node)
	z = export_range(1.2, 10.2, 0.1, default=5)
	w = export(str, default='ab')

	health_changed = signal('old_value', 'new_value')

	def __init__(self):
		print('__init__()', TYPE_BOOL)
		print(TileSet.TILE_LAYOUT_STACKED_OFFSET)
		print('==>', test.add('hello', ' world'))
		#print("==>", self.x, self.y, self.z, self.w)

	def _ready(self):
		print('_ready()')
		print('==>', self.owner, self.owner.script)
		print('==>', self.x, self.y, self.z, self.w)
		print('==> path:', self.owner.get_path())
		# test @staticmethod
		print('==> color:', Color.from_hsv(0.307, 0.4589, 0.8118, 0.5))
		print('==> ray3d:', PhysicsRayQueryParameters3D.create(Vector3(1, 2, 3), Vector3(1, 2, 3)))
		print('==> vector2:', Vector2(1.5, 2.5))
		print('==> vector2:', Vector2(1.5, 2.5).angle())

		self.health_changed.emit(100, 200)
		self.start_coroutine(self.coro(3))

	def coro_one_sec(self, i: int):
		print(f'  coro_one_sec({i}) start')
		yield self.owner.get_tree().create_timer(1).timeout
		#yield 3
		print(f'  coro_one_sec({i}) end')

	def coro(self, secs: int):
		print(f'coro({secs}) start')
		for i in range(secs):
			yield from self.coro_one_sec(i)
		print(f'coro({secs}) end')

	def _process(self, delta: float) -> None:
		if Input.is_key_pressed(KEY_SPACE):
			print('SPACE!!!', f'_process({delta:.4f})')
