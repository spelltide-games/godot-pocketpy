extends RefCounted

class_name FrameStabilizer

var ideal_fps: int
var min_fps: int
var max_fps: int
var buf: Array = []
var curr_fps: float
var accum_time: float = 0.0
var integral: float = 0.0

var buf_size_wnd: SlidingWindow
var err_wnd: SlidingWindow
var fps_wnd: SlidingWindow

var a = 0.5
var kp = 1.0
var ki = 0.2
var ilim = 2

func _init(p_ideal_fps: int):
	ideal_fps = p_ideal_fps
	min_fps = ideal_fps / 2
	max_fps = ideal_fps + ideal_fps / 2
	curr_fps = min_fps
	buf_size_wnd = SlidingWindow.new(ideal_fps)
	err_wnd = SlidingWindow.new(ideal_fps)
	fps_wnd = SlidingWindow.new(ideal_fps)
	
func add_frame(inputs: Dictionary):
	buf.append(inputs)
		
func loss(x):
	if x < 1.0:
		return -log(x)
	if x > 1.5:
		return -log(x-0.5)
	return 0.0
		
func update_fps(delta: float) -> float:
	var err = loss(buf_size_wnd.avg())
	err = clampf(err, -100, 100)
	#print(err)
	integral += err * delta
	integral = clamp(integral, -ilim, ilim)
	var target_fps = ideal_fps - kp * err - ki * integral
	curr_fps = lerp(curr_fps, target_fps, a)
	curr_fps = clamp(curr_fps, min_fps, max_fps)
	return err
	
func get_frame(delta: float):
	var err = update_fps(delta)
	accum_time += delta
	var rest_time = 1 / curr_fps - accum_time
	if rest_time > 0:
		return null
	# return frame
	#print(err)
	
	buf_size_wnd.append(buf.size())
	err_wnd.append(err)
	fps_wnd.append(curr_fps)
	
	if buf.is_empty():
		return null
	accum_time = -rest_time
	return buf.pop_front()
