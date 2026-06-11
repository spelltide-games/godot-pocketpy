extends LockstepGoClient

class_name LockstepGoClientDebug

@export var host: String = '127.0.0.1'
@export var port: int = 5999
@export var smooth_frames: int = 2

var _server_latency_recorder = LatencyRecorder.new(20)
var _local_latency_recorder = LatencyRecorder.new(20)
var _leaky_bucket: LeakyBucket

func _ready():
	Engine.max_fps = 60
	
	await connect_ws(host, port)
	print(id)
	var res = await create_room('test_room', '1.0', 4, 20)
	if res['error'] == 0:
		await connect_room(res['room'])
		print(room)
		_leaky_bucket = LeakyBucket.new(room['frame_rate'], smooth_frames)
		print_latency()
	else:
		print('res.error: ', res['error'])

		
func print_latency():
	while true:
		await get_tree().create_timer(1.0).timeout
		print({
			'fps': roundi(Engine.get_frames_per_second()),
			'S': _server_latency_recorder.get_stats(),
			'L': _local_latency_recorder.get_stats(),
			'buf': _leaky_bucket.buf.size(),
			'buf_fps': _leaky_bucket.curr_fps,
		})
		
func _export_game_state() -> Variant:
	return {}

func _on_client_frame(frame_id: int, inputs: Dictionary):
	if _leaky_bucket != null:
		_server_latency_recorder.tick()
		_leaky_bucket.add_frame(inputs)

func _process(delta: float) -> void:
	poll()
	if _leaky_bucket != null:
		var frame = _leaky_bucket.get_frame(delta)
		if frame != null:
			_local_latency_recorder.tick()
			send_input('test')

###############################	

class LatencyRecorder:
	var max_ticks: int
	var ticks_sec: Array[float]

	func _init(p_max_ticks: int = 20):
		max_ticks = p_max_ticks + 1
		ticks_sec = []

	func tick():
		var time_ms = Time.get_ticks_msec()
		ticks_sec.push_back(time_ms / 1000.0)
		if ticks_sec.size() > max_ticks:
			ticks_sec.pop_front()

	func get_stats():
		var array_size = ticks_sec.size()		
		var diffs: Array[float] = []
		for i in range(1, array_size):
			var diff = ticks_sec[i] - ticks_sec[i-1]
			diffs.append(diff)

		if diffs.size() == 0:
			return null
		
		var sum: float = 0
		var min_val: float = diffs[0]
		var max_val: float = diffs[0]
		
		for diff in diffs:
			sum += diff
			if diff < min_val:
				min_val = diff
			if diff > max_val:
				max_val = diff
		
		var avg = sum / diffs.size()
		#print(diffs)

		return "avg:%d/min:%d/max:%d" % [
			roundi(avg * 1000),
			roundi(min_val * 1000.0),
			roundi(max_val * 1000.0),
		]

class SlidingWindow:
	var buf: Array[float]
	var size: int
	
	func _init(p_size: int):
		buf = []
		size = p_size
		
	func push(val: float):
		buf.append(val)
		if buf.size() > size:
			buf.pop_front()
			
	func avg() -> float:
		if buf.is_empty():
			return 0.0
		var total = 0.0
		for val in buf:
			total += val
		return total / buf.size()

class LeakyBucket:
	var ideal_fps: int
	var min_fps: int
	var max_fps: int
	var ideal_buf_size: int
	var buf: Array
	var curr_fps: float
	var accum_time: float
	var history: SlidingWindow

	func _init(p_ideal_fps: int, p_ideal_buf_size: int):
		ideal_fps = p_ideal_fps
		min_fps = ideal_fps / 2
		max_fps = ideal_fps + ideal_fps / 2
		ideal_buf_size = p_ideal_buf_size
		buf = []
		curr_fps = 0.0
		accum_time = 0.0
		history = SlidingWindow.new(10)
	
	func add_frame(inputs: Dictionary):
		buf.append(inputs)
		
	func update_fps():
		var diff = ideal_buf_size - buf.size()
		var factor = 0.03
		if diff > 1:
			curr_fps = lerpf(curr_fps, min_fps, factor)
		elif diff < -1:
			curr_fps = lerpf(curr_fps, max_fps, factor)
		else:
			curr_fps = lerpf(curr_fps, ideal_fps, factor)
		curr_fps = clampf(curr_fps, min_fps, max_fps)
	
	func get_frame(delta: float):
		update_fps()
		accum_time += delta
		var rest_time = 1 / curr_fps - accum_time
		if rest_time > 0:
			return null
		if buf.is_empty():
			return null
		accum_time = -rest_time
		return buf.pop_front()
