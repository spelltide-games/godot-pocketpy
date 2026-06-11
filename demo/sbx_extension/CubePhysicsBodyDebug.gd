@tool
extends CubePhysicsBody

@export var init_vel: Vector3
@export var is_input_enabled: bool

class Cube:
	var vmin: Vector3
	var vmax: Vector3
	var radius: float


func _ready() -> void:
	if not Engine.is_editor_hint():
		velocity = init_vel

		body_entered.connect(_on_body_entered)
		body_exited.connect(_on_body_exited)

func _on_body_entered(other: CubePhysicsBody, normal: Vector3):
	print('body_enter:  ', self.name + ' -> ' + other.name, ' (', normal)

func _on_body_exited(other: CubePhysicsBody):
	print('body_exited: ', self.name + ' -> ' + other.name)

func process_user_input(_delta: float):
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

	# 绘制线框
	ImmediateGizmos3D.reset()
	var size = extent * 2
	for axis in [0, 1, 2]:
		var t = cube.radius / size[axis]
		ImmediateGizmos3D.line_polygon(rr(cube, t, axis))
		ImmediateGizmos3D.line_polygon(rr(cube, 1-t, axis))
	
	#var segments = 10
	#for axis in [0, 1, 2]:
		#for i in range(segments + 1):
			#var t = float(i) / segments
			#var points = rr(cube, t, axis)
			#ImmediateGizmos3D.line_polygon(points)

func rr(cube: Cube, t: float, axis: int) -> PackedVector3Array:
	var corner_res = 8
	var loop_points = PackedVector3Array()
	var u: int
	var v: int
	if axis == 0:   # 法线为X轴
		u = 1; v = 2
	elif axis == 1: # 法线为Y轴
		u = 0; v = 2
	else:           # 法线为Z轴
		u = 0; v = 1

	var min_val = cube.vmin[axis] - cube.radius
	var max_val = cube.vmax[axis] + cube.radius
	var curr_val = lerpf(min_val, max_val, t)

	# 圆角区域的深度 dy
	var dy = 0.0
	if curr_val < cube.vmin[axis]:
		dy = cube.vmin[axis] - curr_val
	elif curr_val > cube.vmax[axis]:
		dy = curr_val - cube.vmax[axis]

	var r = sqrt(max(0.0, cube.radius * cube.radius - dy * dy))

	var make_vec3 = func(val_axis, val_u, val_v):
		var vec = Vector3.ZERO
		vec[axis] = val_axis
		vec[u] = val_u
		vec[v] = val_v
		return vec

	# 第一象限 (+u, +v)
	for j in range(corner_res + 1):
		var ang = float(j) / corner_res * (PI / 2.0)
		loop_points.append(make_vec3.call(curr_val, cube.vmax[u] + r * sin(ang), cube.vmax[v] + r * cos(ang)))
	
	# 第二象限 (+u, -v)
	for j in range(corner_res + 1):
		var ang = PI / 2.0 + float(j) / corner_res * (PI / 2.0)
		loop_points.append(make_vec3.call(curr_val, cube.vmax[u] + r * sin(ang), cube.vmin[v] + r * cos(ang)))
	
	# 第三象限 (-u, -v)
	for j in range(corner_res + 1):
		var ang = PI + float(j) / corner_res * (PI / 2.0)
		loop_points.append(make_vec3.call(curr_val, cube.vmin[u] + r * sin(ang), cube.vmin[v] + r * cos(ang)))
	
	# 第四象限 (-u, +v)
	for j in range(corner_res + 1):
		var ang = 3.0 * PI / 2.0 + float(j) / corner_res * (PI / 2.0)
		loop_points.append(make_vec3.call(curr_val, cube.vmin[u] + r * sin(ang), cube.vmax[v] + r * cos(ang)))

	return loop_points
