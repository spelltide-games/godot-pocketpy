extends Node3D

class_name SpaceRenderer

var tiles: PackedVector3Array
var bodies: PackedVector3Array

func _ready() -> void:
	tiles = PythonScript.eval(
		'__import__("sbx").playground.instance.draw_chunk_tiles()')
	bodies = PythonScript.eval(
		'__import__("sbx").playground.instance.draw_chunk_bodies()')

func _process(_delta: float) -> void:
	ImmediateGizmos3D.line_polygon(tiles)
	ImmediateGizmos3D.line_polygon(bodies)
