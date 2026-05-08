@tool
extends CubePhysicsBody

@export var init_vel: Vector3
@export var is_input_enabled: bool

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
	print('body_enter:  ', self.name + ' -> ' + other.name, ' (', normal)

func _on_body_exited(other: CubePhysicsBody):
	print('body_exited: ', self.name + ' -> ' + other.name)

func process_user_input(delta: float):
	var h = Input.get_axis("ui_left", "ui_right")
	var v = Input.get_axis("ui_up", "ui_down")
	velocity = Vector3(h, 0, v) * 5

func _process(delta: float) -> void:
	if is_input_enabled:
		process_user_input(delta)

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
	
	# 绘制精细线框
	return
	ImmediateGizmos3D.set_color(Color.GRAY)
	var segments_y = 10 # 竖向切分层
	var corner_res = 8  # 圆角精细度
	
	# 计算物体上下边界
	var min_y = cube.vmin.y - cube.radius
	var max_y = cube.vmax.y + cube.radius

	for i in range(segments_y + 1):
		var t = float(i) / segments_y
		var curr_y = lerpf(min_y, max_y, t)

		# 计算当前高度相对于核心方块的垂直偏移
		var dy = 0.0
		if curr_y < cube.vmin.y:
			dy = cube.vmin.y - curr_y 
		elif curr_y > cube.vmax.y:
			dy = curr_y - cube.vmax.y 

		# 勾股定理算半径
		var r = sqrt(max(0.0, cube.radius**2 - dy**2))
		var loop_points = PackedVector3Array()

		# 按四个象限依次收集圆弧顶点
		# 第一象限 (+X, +Z)
		for j in range(corner_res + 1):
			var ang = float(j) / corner_res * (PI / 2.0)
			loop_points.append(Vector3(cube.vmax.x + r * sin(ang), curr_y, cube.vmax.z + r * cos(ang)))
		# 第二象限 (+X, -Z)
		for j in range(corner_res + 1):
			var ang = PI / 2.0 + float(j) / corner_res * (PI / 2.0)
			loop_points.append(Vector3(cube.vmax.x + r * sin(ang), curr_y, cube.vmin.z + r * cos(ang)))
		# 第三象限 (-X, -Z)
		for j in range(corner_res + 1):
			var ang = PI + float(j) / corner_res * (PI / 2.0)
			loop_points.append(Vector3(cube.vmin.x + r * sin(ang), curr_y, cube.vmin.z + r * cos(ang)))
		# 第四象限 (-X, +Z)
		for j in range(corner_res + 1):
			var ang = 3.0 * PI / 2.0 + float(j) / corner_res * (PI / 2.0)
			loop_points.append(Vector3(cube.vmin.x + r * sin(ang), curr_y, cube.vmax.z + r * cos(ang)))

		ImmediateGizmos3D.line_polygon(loop_points)
