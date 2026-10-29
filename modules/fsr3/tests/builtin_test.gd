extends Node3D

var frame: int = 0
var source: FSR3Upscaler
var binary: FidelityFXUpscaler
var small: SubViewport
var small_source: FSR3Upscaler
var basic: SimpleComputeUpscaler
var window: Window
var object: MeshInstance3D

func _ready() -> void:
	window = get_window()
	assert(not DirAccess.dir_exists_absolute("res://addons"))
	assert(ClassDB.class_get_api_type("FSR3Upscaler") == ClassDB.API_CORE)
	assert(window.scaling_3d_mode == Viewport.SCALING_3D_MODE_FSR3)
	source = window.get_fsr3_upscaler()
	assert(source != null)
	assert(source.get_source_version() == "3.1.6")
	var world: WorldEnvironment = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.13, 0.2)
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_energy = 1.0
	add_child(world)
	var camera: Camera3D = Camera3D.new()
	camera.position = Vector3(0, 1, 5)
	add_child(camera)
	camera.look_at(Vector3.ZERO)
	object = MeshInstance3D.new()
	object.mesh = SphereMesh.new()
	add_child(object)
	small = SubViewport.new()
	small.size = Vector2i(320, 180)
	small.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	small.scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR3
	small.scaling_3d_scale = 0.67
	add_child(small)
	var small_camera: Camera3D = Camera3D.new()
	small_camera.position = Vector3(0, 1, 5)
	small.add_child(small_camera)
	small_camera.look_at(Vector3.ZERO)
	small_source = small.get_fsr3_upscaler()
	assert(small_source != source)
	basic = SimpleComputeUpscaler.new()

func _process(_delta: float) -> void:
	frame += 1
	object.position.x = sin(frame * 0.03)
	match frame:
		50:
			assert(source.get_dispatch_count() > 30)
			window.scaling_3d_scale = 1.0
		85:
			window.scaling_3d_scale = 0.5
		110:
			window.size = Vector2i(1001, 563)
		130:
			window.scaling_3d_mode = Viewport.SCALING_3D_MODE_CUSTOM
			window.scaling_3d_custom_upscaler = basic
		150:
			window.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
		170:
			window.scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR3
			assert(window.get_fsr3_upscaler() == source)
			window.scaling_3d_scale = 1.5
			assert(is_equal_approx(window.scaling_3d_scale, 1.0))
		180:
			window.scaling_3d_scale = 0.67
			window.fsr_sharpness = 0.8
			source.reset_history()
		190:
			assert(small_source.get_dispatch_count() > 100)
			small.queue_free()
			small = null
			window.fsr3_library_path = "res://missing-runtime.so"
			window.fsr3_backend = 1
			binary = window.get_fsr3_upscaler()
		220:
			assert(binary.get_active_backend() == "Source FSR3")
			assert(not binary.get_binary_error().is_empty())
			assert(binary.get_dispatch_count() > 15)
			window.fsr3_backend = 0
		240:
			var active: FSR3Upscaler = window.get_fsr3_upscaler()
			var passed: bool = source.get_dispatch_count() > 120 and source.get_context_count() >= 4 and source.get_failure_count() == 0 and source.get_gpu_dispatch_count() >= source.get_dispatch_count() * 8 and active.get_dispatch_count() > 10 and active.get_failure_count() == 0 and basic.get_dispatch_count() > 10 and binary.get_failure_count() == 0
			print("BUILTIN_FSR3_TEST ", "PASS" if passed else "FAIL", " source_frames=", source.get_dispatch_count(), " gpu_passes=", source.get_gpu_dispatch_count(), " contexts=", source.get_context_count(), " subviewport_frames=", small_source.get_dispatch_count(), " binary_fallback_frames=", binary.get_dispatch_count(), " failures=", source.get_failure_count() + binary.get_failure_count())
			get_tree().quit(0 if passed else 1)
