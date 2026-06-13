extends LockstepGoClient

class_name LockstepGoClientDebug

@export var enabled: bool = true
@export var host: String = '127.0.0.1'
@export var port: int = 5999
@export var udp_redundancy: int = 1

@export var is_host_player: bool

static var p1_id: String

var server_latency_wnd = TimeDeltaWindow.new(20, 0)
var local_latency_wnd = TimeDeltaWindow.new(20, 0)
var stabilizer: FrameStabilizer
var simulated_frame_id: int

func check_error(err: String) -> bool:
	if err.is_empty():
		return true
	print(err)
	return false

func _ready():
	if not enabled:
		return
		
	await connect_ws(host, port)
	print('ws connected: %s' % id)
	
	var new_room = null
	if is_host_player:
		var res = await create_room('1.0', 4, 20)
		if check_error(res['error']):
			new_room = res['retval']
			p1_id = id
	else:
		while true:
			# pre-join room
			var res = await join_room(port+1, '1.0', true)
			if check_error(res['error']):
				var curr_players: Dictionary = res['retval']['players']
				var first_id = curr_players.values().get(0)['id']
				var sync_frame_id = res['retval']['frame_id']
				var state = await export_game_state(first_id, sync_frame_id)
				print('game_state: ', state)
				await get_tree().create_timer(1.0).timeout
				apply_game_state(state)
				# join room
				res = await join_room(port+1, '1.0', false)
				if check_error(res['error']):
					new_room = res['retval']
					break
			await get_tree().create_timer(1.0).timeout
	
	if new_room != null:
		await get_tree().create_timer(0.5).timeout
		print(self.name, ': ', new_room)
		await connect_room(new_room, udp_redundancy)
		stabilizer = FrameStabilizer.new(room['frame_rate'])
		#print_latency()	
		
	if is_host_player:
		await get_tree().create_timer(3.0).timeout
		print('==============')
		var node = duplicate()
		node.name = 'P2'
		node.is_host_player = false
		self.get_parent().add_child(node)
		for c: Control in node.get_children():
			c.position += Vector2(get_viewport().size.x / 2, 0)
		
func _export_game_state(frame_id: int):
	while simulated_frame_id < frame_id - 1:
		await get_tree().process_frame
	return {
		'P1': $P1/HSlider.value,
		'P2': $P2/HSlider.value,
	}
	
func apply_game_state(state: Dictionary):
	$P1/HSlider.value = state['P1']
	$P2/HSlider.value = state['P2']
	
func _on_room_event(event: Dictionary) -> void:
	var p = event['player']
	var players: Dictionary = room['players']
	if event['type'] == 'join':
		players[p['conv']] = p
	elif event['type'] == 'leave':
		players.erase(p['conv'])
		

func _process(delta: float) -> void:
	if not enabled:
		return
		
	poll(delta)
	while get_available_frame_count() > 0:
		var server_frame = get_frame()
		assert(stabilizer != null)
		server_latency_wnd.tick()
		stabilizer.add_frame(server_frame)
		var frame_id = server_frame['frame_id']
		assert(room['frame_id'] == frame_id)
		room['frame_id'] += 1
		
		if is_host_player:
			print(server_frame['frame_id'], server_frame['inputs'])
		#if !is_host_player:
			#print('P2: ', frame_id, ' | ', $P1/HSlider.value)
		#else:
			#print('P1: ', frame_id, ' | ', $P1/HSlider.value)
		
	if stabilizer != null:
		var frame = stabilizer.get_frame(delta)
		if frame != null:
			local_latency_wnd.tick()	
			var inputs: Dictionary = frame['inputs']
			for conv in inputs.keys():
				var d = inputs[conv][0]
				for k in d.keys():
					get_node(k).get_child(0).value += d[k]
			simulated_frame_id = frame['frame_id']
			
		if is_host_player:
			if Input.is_key_pressed(KEY_A):
				send_input({self.name: -1})
			if Input.is_key_pressed(KEY_D):
				send_input({self.name: 1})
		else:
			if Input.is_key_pressed(KEY_LEFT):
				send_input({self.name: -1})
			if Input.is_key_pressed(KEY_RIGHT):
				send_input({self.name: 1})

func print_latency():
	while true:
		await get_tree().create_timer(1.0).timeout
		print({
			#'fps': roundi(Engine.get_frames_per_second()),
			'S': server_latency_wnd.text_stats(),
			'L': local_latency_wnd.text_stats(),
			'buf_size': stabilizer.buf_size_wnd.text_stats(),
			'err': stabilizer.err_wnd.text_stats(),
			'curr_fps': stabilizer.fps_wnd.avg(),
		})
