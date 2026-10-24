@tool
extends Node
## Add this node or use the included autoload to configure the root viewport.
@export var enabled: bool = true
@export_range(0.1, 1.0, 0.01) var render_scale: float = 0.67
@export_range(0.0, 1.0, 0.01) var sharpness: float = 0.2
@export var native_aa: bool = false
@export var prefer_amd_binaries: bool = true
@export_file("*.dll", "*.so") var amd_library_path: String = ""
@export var amd_version: String = ""
@export var upscaler: ViewportUpscaler

func _ready() -> void:
	if Engine.is_editor_hint():
		return
	apply()

func apply() -> void:
	var viewport := get_viewport()
	if not enabled:
		if viewport.scaling_3d_mode == Viewport.SCALING_3D_MODE_CUSTOM:
			viewport.scaling_3d_custom_upscaler = null
		viewport.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
		return
	if upscaler == null:
		upscaler = FidelityFXUpscaler.new() if prefer_amd_binaries and OS.has_feature("windows") else FSR3Upscaler.new()
	if upscaler is FidelityFXUpscaler:
		upscaler.library_path = amd_library_path
		upscaler.preferred_version = amd_version
	var scale := 1.0 if native_aa else render_scale
	if upscaler is FSR3Upscaler:
		upscaler.sharpness = sharpness
		upscaler.configure_scale(scale)
	viewport.use_taa = false
	viewport.scaling_3d_mode = Viewport.SCALING_3D_MODE_CUSTOM
	viewport.scaling_3d_scale = scale
	viewport.scaling_3d_custom_upscaler = upscaler
