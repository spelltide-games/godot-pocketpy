extends LockstepGoClient

class_name LockstepGoClientDebug

@export var enabled: bool = true
@export var host: String = '127.0.0.1'
@export var port: int = 5999
@export var udp_redundancy: int = 1

@export var is_host_player: bool

var server_latency_wnd = TimeDeltaWindow.new(20, 0)
var local_latency_wnd = TimeDeltaWindow.new(20, 0)
var stabilizer: FrameStabilizer

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
	else:
		while true:
			# pre-join room
			var res = await join_room(port+1, '1.0', true)
			if check_error(res['error']):
				var curr_players: Dictionary = res['retval']['players']
				var first_id = curr_players.values().get(0)['id']
				res = await export_game_state(first_id, '123321')
				print('game_state: ', res)
				await get_tree().create_timer(20.0).timeout
				# join room
				res = await join_room(port+1, '1.0', false)
				if check_error(res['error']):
					new_room = res['retval']
					break
			await get_tree().create_timer(1.0).timeout
	
	if new_room != null:
		print(self.name, ': ', new_room)
		await connect_room(new_room, udp_redundancy)
		
		if not is_host_player:
			stabilizer = FrameStabilizer.new(room['frame_rate'])
			print_latency()	
		
func _export_game_state(arg):
	return arg
	
func _on_room_event(event: Dictionary) -> void:
	print(event)

func _process(delta: float) -> void:
	if not enabled:
		return
		
	var server_frame = poll(delta)	
	if stabilizer != null:
		if server_frame != null:
			server_latency_wnd.tick()
			stabilizer.add_frame(server_frame['inputs'])
		
		var frame = stabilizer.get_frame(delta)
		if frame != null:
			local_latency_wnd.tick()
			send_input('test')
			
			
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
