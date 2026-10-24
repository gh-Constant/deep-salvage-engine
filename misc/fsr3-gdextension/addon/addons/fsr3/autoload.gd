extends "res://addons/fsr3/fsr3_settings.gd"

func _ready() -> void:
	enabled = ProjectSettings.get_setting("rendering/fsr3/enabled", true)
	render_scale = ProjectSettings.get_setting("rendering/fsr3/render_scale", 0.67)
	native_aa = ProjectSettings.get_setting("rendering/fsr3/native_aa", false)
	sharpness = ProjectSettings.get_setting("rendering/fsr3/sharpness", 0.2)
	prefer_amd_binaries = ProjectSettings.get_setting("rendering/fsr3/prefer_amd_binaries", true)
	amd_library_path = ProjectSettings.get_setting("rendering/fsr3/amd_library_path", "")
	amd_version = ProjectSettings.get_setting("rendering/fsr3/amd_version", "")
	super._ready()
