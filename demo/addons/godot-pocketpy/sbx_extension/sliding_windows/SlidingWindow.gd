extends RefCounted

class_name SlidingWindow

var buf: Array[float] = []
var sum: float = 0.0
var size: int
var decimals: int
	
func _init(p_size: int, p_decimals: int = 2):
	size = p_size
	decimals = p_decimals

func append(val: float):
	assert(!is_nan(val) && !is_inf(val))
	buf.append(val)
	sum += val
	if buf.size() > size:
		sum -= buf.pop_front()

func stats() -> Dictionary:
	var min_ = INF
	var max_ = -INF
	for val in buf:
		min_ = minf(min_, val)
		max_ = maxf(max_, val)
	return {
		'avg': avg(),
		'min': min_,
		'max': max_,
	}
	
func text_stats():
	var res = stats()
	for k in res.keys():
		res[k] = '%.*f' % [decimals, res[k]]
	return 'avg:{avg}/min:{min}/max:{max}'.format(res)
	
func avg():
	return 0.0 if buf.is_empty() else sum / buf.size()
	
