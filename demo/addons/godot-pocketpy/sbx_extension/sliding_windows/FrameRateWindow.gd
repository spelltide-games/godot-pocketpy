extends SlidingWindow

class_name FrameRateWindow

var prev_time: float = 0.0

func tick():
	var now = Time.get_ticks_msec()
	if prev_time > 0.0:
		var delta = now - prev_time
		append(1000.0 / delta)
	prev_time = now
