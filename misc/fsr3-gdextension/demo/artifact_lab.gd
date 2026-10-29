extends "res://main.gd"
## A deliberately demanding temporal reconstruction scene, not an image-quality fix.

const BAY_X := [-6.3, -2.1, 2.1, 6.3]
const BAY_NAMES := ["01 / DISOCCLUSION", "02 / SUBPIXEL DETAIL", "03 / TRANSPARENCY", "04 / SPECULAR + LIGHT"]
const GUIDES := [
	"OVERVIEW — Pick a close-up with F1–F4. Compare moving edges with the native reference (6).",
	"DISOCCLUSION — Look behind the moving dark slats: newly revealed checker detail can smear or retain old edges.",
	"SUBPIXEL DETAIL — Watch the thin fence and rotating spokes for shimmer, disappearing wires and broken edges.",
	"TRANSPARENCY — Follow the glass, translucent ribbon and GPU sparks for trails and unstable background detail.",
	"SPECULAR + LIGHT — Follow the small highlights and moving emissive bars for sparkle, trailing and ringing."
]

var clock_time := 0.0
var motion_speed := 1.0
var paused := false
var camera_motion := true
var camera_zone := 1
var guide: Label
var status: Label
var slats: Node3D
var wheel: Node3D
var glass: Node3D
var sparks: GPUParticles3D
var ribbon: MeshInstance3D
var highlights: Node3D
var emitter_bar: MeshInstance3D
var moving_light: OmniLight3D
var spin_shapes: Array[Node3D] = []
var light_material: StandardMaterial3D

func _ready() -> void:
	fsr = get_viewport().get_fsr3_upscaler() as FSR3Upscaler
	simple = SimpleComputeUpscaler.new()
	_build_environment()
	_build_bays()
	_build_disocclusion()
	_build_thin_detail()
	_build_transparency()
	_build_highlights()
	_build_hud()
	select_upscaler(simple if basic_only else fsr, 0.67)
	_update_camera(0.0)
	print("FSR3_ARTIFACT_LAB_START device=", RenderingServer.get_video_adapter_name(), " basic_only=", basic_only)

func _material(color: Color, unshaded: bool = false) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	if unshaded:
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	return material

func _box(parent: Node3D, at: Vector3, size: Vector3, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	instance.mesh = mesh
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance

func _shader_material(code: String) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	var shader := Shader.new()
	shader.code = code
	material.shader = shader
	return material

func _build_environment() -> void:
	var world := WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.006, 0.009, 0.018)
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color(0.55, 0.68, 0.92)
	world.environment.ambient_light_energy = 0.55
	# Keep post-processing glow off so bloom does not hide temporal trails.
	add_child(world)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-40, -30, 0)
	key.light_energy = 2.2
	key.shadow_enabled = true
	add_child(key)
	var fill := OmniLight3D.new()
	fill.position = Vector3(0, 5, 5)
	fill.omni_range = 20.0
	fill.light_color = Color(0.4, 0.65, 1.0)
	fill.light_energy = 3.0
	add_child(fill)
	camera = Camera3D.new()
	camera.fov = 52.0
	camera.near = 0.05
	camera.far = 100.0
	add_child(camera)
	camera.make_current()
	var floor_material := _shader_material("""shader_type spatial;
void fragment() {
    vec2 grid = abs(fract(UV * vec2(40.0, 16.0) - 0.5) - 0.5);
    vec2 pixel_width = max(fwidth(UV * vec2(40.0, 16.0)), vec2(0.001));
    float line = 1.0 - clamp(min(grid.x / pixel_width.x, grid.y / pixel_width.y), 0.0, 1.0);
    ALBEDO = mix(vec3(0.018, 0.028, 0.045), vec3(0.07, 0.16, 0.21), line);
    ROUGHNESS = 0.7;
}
""")
	_box(self, Vector3(0, -0.12, 0), Vector3(19, 0.2, 9), floor_material)

func _build_bays() -> void:
	var frame_material := _material(Color(0.055, 0.08, 0.12))
	var backdrop := _shader_material("""shader_type spatial;
render_mode unshaded;
uniform vec4 ink: source_color = vec4(0.1, 0.13, 0.17, 1.0);
void fragment() {
    // Deliberately fine detail. Native raster comparison also shows its own aliasing.
    float check = mod(floor(UV.x * 88.0) + floor(UV.y * 64.0), 2.0);
    ALBEDO = mix(ink.rgb, vec3(0.72), check);
}
""")
	for i in range(4):
		var center: float = BAY_X[i]
		_box(self, Vector3(center, 1.7, -1.65), Vector3(3.95, 3.4, 0.16), frame_material)
		_box(self, Vector3(center, 1.7, -1.54), Vector3(3.65, 3.05, 0.04), backdrop)
		_box(self, Vector3(center, 3.05, -1.42), Vector3(3.5, 0.25, 0.03), _material(Color(0.008, 0.015, 0.025), true))
		var title := Label3D.new()
		title.text = BAY_NAMES[i]
		title.font_size = 36
		title.pixel_size = 0.0045
		title.position = Vector3(center, 3.05, -1.38)
		title.modulate = Color(0.45, 0.85, 0.95)
		add_child(title)

func _build_disocclusion() -> void:
	slats = Node3D.new()
	slats.name = "MovingOccluder"
	slats.position = Vector3(BAY_X[0], 1.7, -0.8)
	add_child(slats)
	var dark := _material(Color(0.004, 0.006, 0.009), true)
	for i in range(5):
		_box(slats, Vector3((i - 2) * 0.32, 0, 0), Vector3(0.17, 2.7, 0.18), dark)
	# This object is also used by the inherited deterministic verification harness.
	moving = MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 0.19
	sphere.height = 0.38
	moving.mesh = sphere
	moving.material_override = _material(Color.WHITE, true)
	moving.position = Vector3(BAY_X[0], 1.3, 0.3)
	add_child(moving)

func _build_thin_detail() -> void:
	var bright := _material(Color(0.85, 0.95, 1.0), true)
	var dark := _material(Color(0.003, 0.006, 0.01), true)
	for i in range(65):
		var x: float = BAY_X[1] - 1.7 + i * 0.053
		_box(self, Vector3(x, 1.65, -0.5), Vector3(0.009, 2.65, 0.012), bright if i % 2 == 0 else dark)
	wheel = Node3D.new()
	wheel.position = Vector3(BAY_X[1], 1.7, 0.1)
	add_child(wheel)
	for i in range(22):
		var spoke := _box(wheel, Vector3.ZERO, Vector3(0.018, 2.6, 0.025), bright)
		spoke.rotation.z = float(i) * PI / 22.0
	var diagonal := _box(self, Vector3(BAY_X[1], 1.7, 0.2), Vector3(0.015, 3.0, 0.02), _material(Color(1, 0.3, 0.08), true))
	diagonal.rotation.z = 0.6

func _build_transparency() -> void:
	glass = Node3D.new()
	glass.position = Vector3(BAY_X[2], 1.7, -0.3)
	add_child(glass)
	var material := _material(Color(0.05, 0.7, 1.0, 0.24))
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.roughness = 0.12
	_box(glass, Vector3.ZERO, Vector3(1.4, 2.5, 0.035), material)
	ribbon = _box(self, Vector3(BAY_X[2], 1.7, 0.2), Vector3(2.9, 0.8, 0.02), _shader_material("""shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix;
uniform float phase = 0.0;
void fragment() {
    float waves = sin(UV.x * 25.0 + phase) * 0.2 + 0.5;
    float band = 1.0 - smoothstep(0.06, 0.16, abs(UV.y - waves));
    ALBEDO = vec3(1.0, 0.2, 0.035);
    EMISSION = vec3(2.5, 0.25, 0.015) * band;
    ALPHA = band * 0.65;
}
"""))
	sparks = GPUParticles3D.new()
	sparks.position = Vector3(BAY_X[2], 0.4, 0.5)
	sparks.amount = 256
	sparks.lifetime = 1.8
	sparks.preprocess = 1.8
	sparks.seed = 31415
	sparks.use_fixed_seed = true
	sparks.visibility_aabb = AABB(Vector3(-3, -1, -2), Vector3(6, 6, 4))
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	process.emission_box_extents = Vector3(0.15, 0.1, 0.15)
	process.direction = Vector3(0, 1, 0)
	process.spread = 30.0
	process.initial_velocity_min = 1.2
	process.initial_velocity_max = 2.0
	process.gravity = Vector3(0.0, -0.15, 0.0)
	sparks.process_material = process
	var quad := QuadMesh.new()
	quad.size = Vector2(0.06, 0.06)
	quad.material = _shader_material("""shader_type spatial;
render_mode unshaded, cull_disabled, blend_add;
void vertex() {
    MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
}
void fragment() {
    float radial = max(0.0, 1.0 - length(UV * 2.0 - 1.0));
    ALBEDO = vec3(1.0, 0.5, 0.06) * 3.0;
    ALPHA = radial * radial;
}
""")
	sparks.draw_pass_1 = quad
	add_child(sparks)

func _build_highlights() -> void:
	highlights = Node3D.new()
	highlights.position = Vector3(BAY_X[3], 1.5, 0)
	add_child(highlights)
	var metal := _material(Color(0.75, 0.8, 0.9))
	metal.metallic = 0.9
	metal.roughness = 0.065
	for i in range(7):
		var ball := MeshInstance3D.new()
		var mesh := SphereMesh.new()
		mesh.radius = 0.24
		mesh.height = 0.48
		mesh.radial_segments = 48
		mesh.rings = 24
		ball.mesh = mesh
		ball.material_override = metal
		ball.position = Vector3((i % 3 - 1) * 0.75, (i / 3 - 1) * 0.65, 0)
		highlights.add_child(ball)
	var ring := MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = 0.65
	torus.outer_radius = 0.71
	ring.mesh = torus
	ring.material_override = metal
	ring.position.y = 0.7
	highlights.add_child(ring)
	spin_shapes.append(ring)
	light_material = _material(Color(1.0, 0.9, 0.65), true)
	light_material.emission_enabled = true
	light_material.emission = Color(1.0, 0.45, 0.07)
	light_material.emission_energy_multiplier = 6.0
	emitter_bar = _box(self, Vector3(BAY_X[3], 2.5, 0.5), Vector3(0.055, 1.7, 0.055), light_material)
	moving_light = OmniLight3D.new()
	moving_light.omni_range = 5.0
	moving_light.light_energy = 10.0
	moving_light.light_color = Color(1.0, 0.7, 0.35)
	add_child(moving_light)

func _build_hud() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var panel := PanelContainer.new()
	panel.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.015, 0.025, 0.045, 0.94)
	style.content_margin_left = 18
	style.content_margin_right = 18
	style.content_margin_top = 12
	style.content_margin_bottom = 12
	panel.add_theme_stylebox_override("panel", style)
	canvas.add_child(panel)
	var rows := VBoxContainer.new()
	panel.add_child(rows)
	label = Label.new()
	label.add_theme_font_size_override("font_size", 19)
	rows.add_child(label)
	status = Label.new()
	status.add_theme_font_size_override("font_size", 13)
	status.modulate = Color(0.5, 0.8, 0.95)
	rows.add_child(status)
	guide = Label.new()
	guide.add_theme_font_size_override("font_size", 13)
	guide.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rows.add_child(guide)
	var help := Label.new()
	help.text = "1 Quality  2 Native AA  3 Basic  4 Bilinear  5 Performance  6 Native raster\nF1–F4 Close-up  F5 Overview  C Camera  Space Pause  [ / ] Speed  R Reset history  X Camera cut"
	help.add_theme_font_size_override("font_size", 13)
	rows.add_child(help)

func _process(delta: float) -> void:
	if automated:
		# Retain the existing scale/resize/switch verification sequence and counters.
		super._process(delta)
		clock_time = float(frame) / 60.0
		match frame:
			1: _set_zone(1)
			61: _set_zone(2)
			106: _set_zone(3)
			151: _set_zone(4)
			196: _set_zone(0)
			256: _set_zone(3)
			286: _set_zone(2)
	else:
		frame += 1
		if not paused:
			clock_time += delta * motion_speed
	var t := clock_time
	slats.position.x = BAY_X[0] + sin(t * 2.4) * 1.05
	moving.position = Vector3(BAY_X[0] + sin(t * 3.4) * 1.6, 1.3, 0.3)
	wheel.rotation.z = t * 1.4
	glass.rotation.y = sin(t * 1.3) * 1.1
	glass.position.x = BAY_X[2] + sin(t * 1.7) * 0.65
	ribbon.position.y = 1.7 + sin(t * 2.2) * 0.7
	(ribbon.material_override as ShaderMaterial).set_shader_parameter("phase", t * 5.0)
	sparks.position.x = BAY_X[2] + sin(t * 2.3) * 1.15
	sparks.speed_scale = 0.0 if paused else motion_speed
	highlights.rotation.y = sin(t) * 0.45
	for shape in spin_shapes:
		shape.rotation = Vector3(t * 0.8, t * 0.7, t * 0.4)
	emitter_bar.position.x = BAY_X[3] + sin(t * 2.7) * 1.4
	emitter_bar.rotation.z = sin(t) * 0.7
	moving_light.position = Vector3(BAY_X[3] + cos(t * 1.4) * 1.4, 2.6 + sin(t) * 0.5, 1.2)
	light_material.emission_energy_multiplier = 3.5 + sin(t * 2.0) * 2.5
	_update_camera(t)
	var vp := get_viewport()
	var active_name := "Native raster / 1.0" if is_equal_approx(vp.scaling_3d_scale, 1.0) else "Bilinear / %.2f scale" % vp.scaling_3d_scale
	if vp.scaling_3d_mode == Viewport.SCALING_3D_MODE_FSR3 or vp.scaling_3d_custom_upscaler == fsr:
		active_name = "FSR3 / %.2f scale" % vp.scaling_3d_scale
	elif vp.scaling_3d_custom_upscaler == simple:
		active_name = "Basic compute / %.2f scale" % vp.scaling_3d_scale
	label.text = ("FSR %s  /  TEMPORAL ARTIFACT LAB  /  " % fsr.get_source_version()) + active_name
	status.text = "%.0f FPS  |  %dx%d output  |  %d FSR frames / %d GPU passes  |  %d failures  |  %.2fx speed%s" % [Engine.get_frames_per_second(), vp.size.x, vp.size.y, fsr.get_dispatch_count(), fsr.get_gpu_dispatch_count(), fsr.get_failure_count(), motion_speed, " / PAUSED" if paused else ""]
	if fsr is FidelityFXUpscaler:
		label.text = "FIDELITYFX  /  TEMPORAL ARTIFACT LAB  /  " + active_name
		status.text += "\n%s / %s | %d AMD API dispatches | %d binary failures" % [fsr.get_active_backend(), fsr.get_active_version(), fsr.get_binary_dispatch_count(), fsr.get_binary_failure_count()]
	guide.text = GUIDES[camera_zone]

func _update_camera(t: float) -> void:
	var pan := sin(t * 0.6) * (0.45 if camera_zone > 0 else 1.1) if camera_motion else 0.0
	if camera_zone == 0:
		camera.position = Vector3(pan, 4.3, 11.5)
		camera.look_at(Vector3(0, 1.65, -0.6))
	else:
		var x: float = BAY_X[camera_zone - 1]
		camera.position = Vector3(x + pan, 2.65, 3.6)
		camera.look_at(Vector3(x, 2.25, -0.6))

func _set_zone(zone: int, reset: bool = true) -> void:
	camera_zone = zone
	if reset:
		fsr.reset_history()
	_update_camera(clock_time)

func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo:
		return
	super._unhandled_key_input(event)
	match event.keycode:
		KEY_4, KEY_6:
			var vp := get_viewport()
			vp.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
			vp.scaling_3d_scale = 1.0 if event.keycode == KEY_6 else 0.67
		KEY_5:
			select_upscaler(fsr, 0.5)
		KEY_F1: _set_zone(1)
		KEY_F2: _set_zone(2)
		KEY_F3: _set_zone(3)
		KEY_F4: _set_zone(4)
		KEY_F5: _set_zone(0)
		KEY_C: camera_motion = not camera_motion
		KEY_SPACE: paused = not paused
		KEY_BRACKETLEFT: motion_speed = maxf(0.125, motion_speed * 0.5)
		KEY_BRACKETRIGHT: motion_speed = minf(4.0, motion_speed * 2.0)
		KEY_R: fsr.reset_history()
		KEY_X:
			# Intentionally cut WITHOUT resetting, to make stale history observable.
			_set_zone((camera_zone + 1) % 5, false)
