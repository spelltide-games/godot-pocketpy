@tool
extends CubePhysicsBody

@export var init_vel: Vector3

class Cube:
	var vmin: Vector3
	var vmax: Vector3
	var radius: float

	func get_face_points(axis: int, sign: int) -> PackedVector3Array:
		var face_vmin = vmin
		var face_vmax = vmax
		if sign > 0:
			face_vmin[axis] = vmax[axis]
		else:
			face_vmax[axis] = vmin[axis]

		face_vmin[axis] += sign * radius
		face_vmax[axis] += sign * radius

		var lut := [
			[0b000, 0b100, 0b110, 0b010], # axis=0 sign=-1 (-X)
			[0b001, 0b011, 0b111, 0b101], # axis=0 sign=+1 (+X)
			[0b000, 0b001, 0b101, 0b100], # axis=1 sign=-1 (-Y)
			[0b010, 0b110, 0b111, 0b011], # axis=1 sign=+1 (+Y)
			[0b000, 0b010, 0b011, 0b001], # axis=2 sign=-1 (-Z)
			[0b100, 0b101, 0b111, 0b110], # axis=2 sign=+1 (+Z)
		]

		var points = PackedVector3Array()
		var row = axis * 2 + (1 if sign > 0 else 0)
		for i in range(4):
			var mask = lut[row][i]
			points.append(Vector3(
				face_vmax.x if ((mask >> 0) & 1) > 0 else face_vmin.x,
				face_vmax.y if ((mask >> 1) & 1) > 0 else face_vmin.y,
				face_vmax.z if ((mask >> 2) & 1) > 0 else face_vmin.z,
			))
		return points

func _ready() -> void:
	if not Engine.is_editor_hint():
		velocity = init_vel

		body_entered.connect(_on_body_entered)
		body_exited.connect(_on_body_exited)

func _on_body_entered(other: CubePhysicsBody, normal: Vector3):
	print('body_enter: ', self.name + ' -> ' + other.name, ' (', normal)

func _on_body_exited(other: CubePhysicsBody):
	print('body_exited:', self.name + ' -> ' + other.name)

func _process(_delta: float) -> void:
	var cube = Cube.new()
	cube.radius = radius01 * extent[extent.min_axis_index()]
	cube.vmin = global_position - (extent - Vector3.ONE * cube.radius)
	cube.vmax = global_position + (extent - Vector3.ONE * cube.radius)

	ImmediateGizmos3D.reset()

	# 绘制四边形
	for axis in [0, 1, 2]:
		for sign in [-1, 1]:
			var points = cube.get_face_points(axis, sign)
			ImmediateGizmos3D.line_polygon(points)

	# 绘制八个角
	for x_sign in [-1, 1]:
		for y_sign in [-1, 1]:
			for z_sign in [-1, 1]:
				var center = Vector3(
					cube.vmax.x if x_sign > 0 else cube.vmin.x,
					cube.vmax.y if y_sign > 0 else cube.vmin.y,
					cube.vmax.z if z_sign > 0 else cube.vmin.z,
				)
				var x_axis = Vector3(x_sign, 0, 0)
				var y_axis = Vector3(0, y_sign, 0)
				var z_axis = Vector3(0, 0, z_sign)
				var dx = x_axis * cube.radius
				var dy = y_axis * cube.radius
				var dz = z_axis * cube.radius
				ImmediateGizmos3D.line_arc(center, Vector3(0, 0, x_sign * y_sign), dx, PI / 2)
				ImmediateGizmos3D.line_arc(center, Vector3(y_sign * z_sign, 0, 0), dy, PI / 2)
				ImmediateGizmos3D.line_arc(center, Vector3(0, x_sign * z_sign, 0), dz, PI / 2)
