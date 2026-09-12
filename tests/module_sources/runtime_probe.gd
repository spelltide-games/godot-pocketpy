extends SceneTree

func _initialize() -> void:
	var path = "res://site-packages/math_helpers.py"
	var valid = FileAccess.file_exists(path) and not ResourceLoader.exists(path, "Script")
	valid = valid and not ClassDB.class_exists("PythonModuleSource")
	# A real PythonScript still imports ordinary modules through FileAccess.
	var script = load("res://ModuleRunner.py")
	var node = script.new() if script else null
	valid = valid and node != null and node.call("compute") == 42
	if node:
		node.free()
	if "--packed" in OS.get_cmdline_user_args():
		valid = valid and FileAccess.file_exists("res://exported.marker")
		valid = valid and not FileAccess.file_exists("res://site-packages/nested/data.json")
		valid = valid and not FileAccess.file_exists("res://site-packages/excluded.py")
		valid = valid and not FileAccess.file_exists("res://site-packages/.gdignore")
		valid = valid and not FileAccess.file_exists("res://site-packages/__pycache__/ignored.py")
	print("MODULE_RUNTIME_RESULT ", "PASS" if valid else "FAIL")
	quit(0 if valid else 1)
