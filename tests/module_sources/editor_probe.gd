@tool
extends EditorPlugin

const HELPER = "res://site-packages/math_helpers.py"
var failures: Array[String] = []
var checks = 0

func _enter_tree() -> void:
	if "--module-source-tests" in OS.get_cmdline_user_args():
		_run.call_deferred()
	elif "--module-source-restore-tests" in OS.get_cmdline_user_args():
		_restore.call_deferred()

func check(label: String, condition: bool) -> void:
	checks += 1
	print("MODULE_CHECK ", "PASS " if condition else "FAIL ", label)
	if not condition:
		failures.append(label)

func wait_for_editor() -> void:
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	await get_tree().create_timer(1.0).timeout

func finish() -> void:
	print("MODULE_EDITOR_RESULT ", checks - failures.size(), "/", checks)
	get_tree().quit(0 if failures.is_empty() else 1)

func find_item(root: TreeItem, path: String) -> TreeItem:
	if root.get_metadata(0) == path:
		return root
	for child in root.get_children():
		var found = find_item(child, path)
		if found:
			return found
	return null

func _run() -> void:
	await wait_for_editor()
	var docks = EditorInterface.get_base_control().find_children("*", "PythonModulesDock", true, false)
	check("module dock registered in the actual editor", docks.size() == 1)
	if docks.size() != 1:
		finish()
		return
	var dock = docks[0]
	var tree: Tree = dock.get_node("Modules")
	check("gdignore retained", FileAccess.file_exists("res://site-packages/.gdignore"))
	check("ignored module appears in dock", find_item(tree.get_root(), HELPER) != null)
	check("nested package appears in dock", find_item(tree.get_root(), "res://site-packages/nested/math_helpers.py") != null)
	check("module absent from resource filesystem", EditorInterface.get_resource_filesystem().get_file_type(HELPER).is_empty())

	for path in [HELPER, "res://site-packages/empty/__init__.py", "res://site-packages/broken.py", "res://site-packages/raise_on_open.py"]:
		var source = ResourceLoader.load(path)
		check("source loads without executing: " + path, source != null and source.get_class() == "PythonModuleSource")
		if source:
			check("module cannot instantiate: " + path, not source.can_instantiate() and source.is_abstract())
			check("source reload does not execute: " + path, source.reload() == OK)
			check("source save does not execute: " + path, ResourceSaver.save(source) == OK)

	var source: Script = ResourceLoader.load(HELPER)
	if source == null:
		finish()
		return
	var node = Node.new()
	print("MODULE_EXPECTED_ATTACH_REJECTION_BEGIN")
	node.set_script(source)
	print("MODULE_EXPECTED_ATTACH_REJECTION_END")
	check("set_script cannot attach a module", node.get_script() == null)
	node.free()
	check("module has no global class name", source.get_global_name().is_empty())

	var item = find_item(tree.get_root(), HELPER)
	item.select(0)
	tree.item_activated.emit()
	await get_tree().create_timer(0.2).timeout
	var editor = EditorInterface.get_script_editor()
	check("dock opens module in native script editor", editor.get_current_script() == source)
	var code: CodeEdit = editor.get_current_editor().get_base_editor()
	check("Python highlighter installed", code.get_syntax_highlighter().get_class() == "PythonSyntaxHighlighter")
	code.set_line_as_breakpoint(1, true)
	check("native breakpoint collection includes module", HELPER + ":2" in editor.get_breakpoints())

	var edited = source.source_code + "\n# unsaved buffer\n"
	source.source_code = edited
	var cached = ResourceLoader.load(HELPER)
	check("debugger lookup preserves cached edits", cached == source and cached.source_code == edited)
	var disk = ResourceLoader.load(HELPER, "", ResourceLoader.CACHE_MODE_IGNORE)
	check("uncached disk read stays separate", disk != source and not "unsaved buffer" in disk.source_code)
	check("disk read does not replace cache", ResourceLoader.load(HELPER) == source)
	source.source_code = disk.source_code

	var filter: LineEdit = dock.find_child("Filter", true, false)
	filter.text = "nested"
	filter.text_changed.emit(filter.text)
	check("filter matches package path", find_item(tree.get_root(), "res://site-packages/nested/math_helpers.py") != null)
	check("filter hides unrelated module", find_item(tree.get_root(), HELPER) == null)
	filter.text = ""
	filter.text_changed.emit("")
	var added = FileAccess.open("res://site-packages/added.py", FileAccess.WRITE)
	added.store_string("answer = 42\n")
	added.close()
	dock.call("refresh")
	check("refresh discovers files below gdignore", find_item(tree.get_root(), "res://site-packages/added.py") != null)

	# Only the native editor's breakpoint list seeds the child process. This
	# exercises startup breakpoints and automatic source navigation together.
	EditorInterface.play_custom_scene("res://runner.tscn")
	var located = false
	for i in range(300):
		await get_tree().create_timer(0.05).timeout
		var current = editor.get_current_script()
		if current and current.resource_path == HELPER:
			var current_code: CodeEdit = editor.get_current_editor().get_base_editor()
			if current_code.is_line_executing(1):
				located = true
				break
	check("startup module breakpoint pauses and marks native source line", located)
	EditorInterface.stop_playing_scene()
	await get_tree().create_timer(1.0).timeout
	check("stop clears execution marker", not code.is_line_executing(1))
	# Let the native editor persist its tab/breakpoint cache before restarting.
	await get_tree().create_timer(2.0).timeout
	finish()

func _restore() -> void:
	await wait_for_editor()
	var editor = EditorInterface.get_script_editor()
	check("module breakpoint restored after editor restart", HELPER + ":2" in editor.get_breakpoints())
	var found = false
	for source in editor.get_open_scripts():
		if source.resource_path == HELPER:
			found = source.get_class() == "PythonModuleSource"
	check("module source tab restored after editor restart", found)
	finish()
