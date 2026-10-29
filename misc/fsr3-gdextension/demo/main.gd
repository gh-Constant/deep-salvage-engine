extends Node3D

var fsr: FSR3Upscaler
var simple: SimpleComputeUpscaler
var camera: Camera3D
var moving: MeshInstance3D
var frame := 0
var automated := "--verify" in OS.get_cmdline_user_args()
var basic_only := "--simple" in OS.get_cmdline_user_args()
var output_directory := OS.get_environment("FSR3_TEST_OUTPUT")
var label: Label

func _ready() -> void:
	fsr = get_viewport().get_fsr3_upscaler() as FSR3Upscaler
	simple = SimpleComputeUpscaler.new()
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.025, 0.04, 0.07)
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color.WHITE
	environment.environment.ambient_light_energy = 0.55
	add_child(environment)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, -25, 0)
	light.shadow_enabled = true
	add_child(light)
	camera = Camera3D.new()
	camera.position = Vector3(0, 3.5, 9)
	add_child(camera)
	camera.look_at(Vector3(0, 0.6, 0))
	for z in range(-4, 5):
		for x in range(-5, 6):
			var box := MeshInstance3D.new()
			box.mesh = BoxMesh.new()
			box.position = Vector3(x * 1.1, 0.25, z * 1.1)
			box.scale = Vector3(0.8, 0.5 + (x + z + 20) % 4 * 0.4, 0.8)
			var material := StandardMaterial3D.new()
			material.albedo_color = Color.from_hsv(float((x + z + 20) % 8) / 8.0, 0.65, 0.7)
			box.material_override = material
			add_child(box)
	moving = MeshInstance3D.new()
	moving.mesh = SphereMesh.new()
	moving.position = Vector3(0, 2.2, 1)
	add_child(moving)
	var canvas := CanvasLayer.new()
	add_child(canvas)
	label = Label.new()
	label.position = Vector2(16, 16)
	canvas.add_child(label)
	select_upscaler(simple if basic_only else fsr, 0.67)
	print("FSR3_TEST_START device=", RenderingServer.get_video_adapter_name(), " basic_only=", basic_only)

func select_upscaler(upscaler: ViewportUpscaler, scale: float) -> void:
	var vp := get_viewport()
	vp.use_taa = false
	if upscaler is FSR3Upscaler and not upscaler is FidelityFXUpscaler:
		vp.fsr3_backend = 0
		vp.scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR3
		vp.scaling_3d_scale = scale
		fsr = vp.get_fsr3_upscaler() as FSR3Upscaler
	else:
		if upscaler is FSR3Upscaler:
			upscaler.configure_scale(scale)
		vp.scaling_3d_mode = Viewport.SCALING_3D_MODE_CUSTOM
		vp.scaling_3d_scale = scale
		vp.scaling_3d_custom_upscaler = upscaler

func _process(_delta: float) -> void:
	frame += 1
	var t := float(frame) / 60.0
	moving.position.x = sin(t * 2.0) * 3.0
	moving.rotation.y = t
	camera.position.x = sin(t * 0.35) * 1.5
	camera.look_at(Vector3(0, 0.6, 0))
	label.text = "FSR %s built-in | 1: Quality  2: Native AA  3: Basic compute  4: Bilinear\nFSR dispatches: %d | contexts: %d | failures: %d" % [fsr.get_source_version(), fsr.get_dispatch_count(), fsr.get_context_count(), fsr.get_failure_count()]
	if not automated:
		return
	if frame in [45, 90, 135, 180, 225, 270, 315]:
		capture("frame_%03d" % frame)
		print("FSR3_CHECKPOINT frame=", frame, " fsr=", fsr.get_dispatch_count(), " simple=", simple.get_dispatch_count(), " contexts=", fsr.get_context_count(), " gpu_passes=", fsr.get_gpu_dispatch_count(), " failures=", fsr.get_failure_count())
	if not basic_only:
		match frame:
			60: select_upscaler(fsr, 1.0)
			105: select_upscaler(fsr, 0.5)
			150: get_window().size = Vector2i(1001, 563)
			195: select_upscaler(simple, 0.67)
			240:
				get_viewport().scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
			255: select_upscaler(fsr, 0.67)
			285:
				fsr.sharpness = 0.8
				fsr.reset_history()
	if frame == 330:
		var success: bool = simple.get_dispatch_count() > 0 if basic_only else fsr.get_dispatch_count() > 0 and fsr.get_gpu_dispatch_count() >= fsr.get_dispatch_count() * 8 and fsr.get_failure_count() == 0 and fsr.get_context_count() >= 4 and simple.get_dispatch_count() > 0
		print("FSR3_TEST_RESULT ", "PASS" if success else "FAIL", " fsr_dispatches=", fsr.get_dispatch_count(), " basic_dispatches=", simple.get_dispatch_count(), " contexts=", fsr.get_context_count(), " gpu_passes=", fsr.get_gpu_dispatch_count(), " failures=", fsr.get_failure_count())
		get_tree().quit(0 if success else 1)

func capture(name: String) -> void:
	if output_directory.is_empty():
		return
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		push_error("Empty GPU output at " + name)
		get_tree().quit(2)
		return
	image.save_png(output_directory.path_join(name + ".png"))

func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_1: select_upscaler(fsr, 0.67)
			KEY_2: select_upscaler(fsr, 1.0)
			KEY_3: select_upscaler(simple, 0.67)
			KEY_4: get_viewport().scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
