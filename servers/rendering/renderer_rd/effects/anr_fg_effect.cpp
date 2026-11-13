/**************************************************************************/
/*  anr_fg_effect.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "anr_fg_effect.h"

#include "core/os/os.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"

using namespace RendererRD;

AnrFgEffect *AnrFgEffect::singleton = nullptr;

// DLSS-G's layout, as ANR's own NGX host (ANR/record/replay/ngx_generator.cpp) builds its camera:
// the row-vector matrix stored row by row (translation in the last row, w from element 11). That is
// the column-vector matrix stored column by column, Godot's own layout.
static void _store_matrix(float *r_out, const Projection &p_matrix) {
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			r_out[column * 4 + row] = p_matrix.columns[column][row];
		}
	}
}

// The first version's layout (the column-vector matrix row by row), for GODOT_ANR_FG_MATRICES=legacy.
static void _store_matrix_transposed(float *r_out, const Projection &p_matrix) {
	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++) {
			r_out[row * 4 + column] = p_matrix.columns[column][row];
		}
	}
}

static void _store_vector(float *r_out, const Vector3 &p_vector) {
	r_out[0] = p_vector.x;
	r_out[1] = p_vector.y;
	r_out[2] = p_vector.z;
}

AnrFgEffect *AnrFgEffect::get_or_create() {
	if (singleton == nullptr) {
		singleton = memnew(AnrFgEffect);
	}
	return singleton;
}

void AnrFgEffect::free_singleton() {
	// NGX itself is shut down by RenderingDevice::finalize, once the GPU is idle.
	if (singleton != nullptr) {
		memdelete(singleton);
		singleton = nullptr;
	}
}

AnrFgEffect::AnrFgEffect() {
	Vector<String> modes;
	modes.push_back("\n");
	decode_shader.initialize(modes, "");
	decode_version = decode_shader.version_create();
	decode_pipeline = RD::get_singleton()->compute_pipeline_create(decode_shader.version_get_shader(decode_version, 0));
	RD::get_singleton()->set_resource_name(decode_pipeline, "ANR FG Motion Vector Decode Pipeline");
}

AnrFgEffect::~AnrFgEffect() {
	_free_outputs();
	_free_screen_target();
	decode_shader.version_free(decode_version);
}

void AnrFgEffect::_free_outputs() {
	if (output_interpolated.is_valid()) {
		RD::get_singleton()->free_rid(output_interpolated);
		output_interpolated = RID();
	}
	if (output_real.is_valid()) {
		RD::get_singleton()->free_rid(output_real);
		output_real = RID();
	}
	output_size = Size2i();
	output_format = RD::DATA_FORMAT_MAX;
}

void AnrFgEffect::_free_screen_target() {
	if (screen_texture.is_valid()) {
		// Its framebuffer depends on it and goes with it.
		RD::get_singleton()->free_rid(screen_texture);
		screen_texture = RID();
	}
	screen_framebuffer = RID();
	screen_size = Size2i();
	screen_format = RD::DATA_FORMAT_MAX;
}

bool AnrFgEffect::_ensure_screen_target(const Size2i &p_size, RD::DataFormat p_format) {
	if (screen_texture.is_valid() && screen_size == p_size && screen_format == p_format && RD::get_singleton()->framebuffer_is_valid(screen_framebuffer)) {
		return true;
	}
	_free_screen_target();
	RD *rd = RD::get_singleton();
	RD::TextureFormat tf;
	tf.texture_type = RD::TEXTURE_TYPE_2D;
	tf.format = p_format;
	tf.width = p_size.width;
	tf.height = p_size.height;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	screen_texture = rd->texture_create(tf, RD::TextureView());
	ERR_FAIL_COND_V(screen_texture.is_null(), false);
	rd->set_resource_name(screen_texture, "ANR FG Generated Screen Image");
	Vector<RID> attachments;
	attachments.push_back(screen_texture);
	screen_framebuffer = rd->framebuffer_create(attachments);
	if (screen_framebuffer.is_null()) {
		_free_screen_target();
		ERR_FAIL_V_MSG(false, "ANR FG: couldn't create the generated screen image's framebuffer.");
	}
	screen_size = p_size;
	screen_format = p_format;
	return true;
}

AnrFrameGeneration::Image AnrFgEffect::_image(RID p_texture, bool p_depth) {
	RD *rd = RD::get_singleton();
	AnrFrameGeneration::Image image;
	const RD::TextureFormat format = rd->texture_get_format(p_texture);
	image.image = rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, p_texture, 0);
	image.view = rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_VIEW, p_texture, 0);
	image.format = (uint32_t)rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_DATA_FORMAT, p_texture, 0);
	image.width = format.width;
	image.height = format.height;
	image.depth = p_depth;
	return image;
}

void AnrFgEffect::capture_scene(const RenderDataRD *p_render_data) {
	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());
	if (!rb->has_velocity_buffer(false)) {
		return;
	}

	const RenderSceneDataRD *scene_data = p_render_data->scene_data;
	const Size2i internal_size = rb->get_internal_size();
	RID depth = rb->get_depth_texture(0);
	RID motion = rb->get_velocity_buffer(false, 0);
	ERR_FAIL_COND(depth.is_null() || motion.is_null());

	// Godot's camera reprojection (as the engine's FSR 2 and DLSS paths build it), for the decode shader.
	Projection correction;
	correction.set_depth_correction(true, true, false);
	const Projection &prev_proj = scene_data->prev_cam_projection;
	const Projection &cur_proj = scene_data->cam_projection;
	const Transform3D &prev_transform = scene_data->prev_cam_transform;
	const Transform3D &cur_transform = scene_data->cam_transform;
	const Projection reprojection = (correction * prev_proj) * prev_transform.affine_inverse() * cur_transform * (correction * cur_proj).inverse();

	// Godot leaves the motion of pixels without geometry (sky) invalid: derive it from depth and
	// the camera, in place. The upscaler has already used this frame's motion vectors.
	{
		RD::get_singleton()->draw_command_begin_label("ANR FG Decode Motion Vectors");
		UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
		ERR_FAIL_NULL(uniform_set_cache);
		RID shader = decode_shader.version_get_shader(decode_version, 0);
		ERR_FAIL_COND(shader.is_null());

		RD::Uniform u_velocity_image(RD::UNIFORM_TYPE_IMAGE, 0, motion);
		RD::Uniform u_depth_texture(RD::UNIFORM_TYPE_TEXTURE, 0, depth);

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, decode_pipeline);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_velocity_image), 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_depth_texture), 1);

		float push_constants[20];
		push_constants[0] = internal_size.width;
		push_constants[1] = internal_size.height;
		push_constants[2] = 0.0f;
		push_constants[3] = 0.0f;
		memcpy(push_constants + 4, &reprojection.columns[0].x, sizeof(float) * 16);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, push_constants, sizeof(push_constants));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, internal_size.width, internal_size.height, 1);
		RD::get_singleton()->compute_list_end();
		RD::get_singleton()->draw_command_end_label();
	}

	scene.render_target = rb->get_render_target();
	scene.depth = depth;
	scene.motion = motion;
	scene.internal_size = internal_size;
	scene.frame = RendererCompositorRD::get_singleton()->get_frame_number();

	AnrFrameGeneration::EvaluateParams &p = scene.params;
	p.internal_width = internal_size.width;
	p.internal_height = internal_size.height;
	// Godot's motion vectors are UV offsets from this frame to the previous one (DLSS's direction).
	// The provider reads stored motion times MvecScale as pixels of the motion texture (side by
	// side captures: unit scales double moving edges); GODOT_ANR_FG_MVEC_SCALE=normalized passes 1.
	static const bool mvec_pixels = OS::get_singleton()->get_environment("GODOT_ANR_FG_MVEC_SCALE") != "normalized";
	p.mvec_scale[0] = mvec_pixels ? float(internal_size.width) : 1.0f;
	p.mvec_scale[1] = mvec_pixels ? float(internal_size.height) : 1.0f;
	const Vector2 jitter = scene_data->taa_jitter * Vector2(internal_size) * 0.5f;
	p.jitter[0] = jitter.x;
	p.jitter[1] = jitter.y;
	p.motion_jittered = false;
	p.depth_inverted = true; // Godot renders reverse Z.
	p.z_near = scene_data->z_near;
	p.z_far = scene_data->z_far;
	const real_t fov = cur_proj.get_fov();
	const real_t aspect = cur_proj.get_aspect();
	p.fov = Math::deg_to_rad(Projection::get_fovy(fov, 1.0 / aspect));
	p.aspect = aspect;
	_store_vector(p.camera_pos, cur_transform.get_origin());
	_store_vector(p.camera_fwd, -cur_transform.get_basis().get_column(2));
	_store_vector(p.camera_up, cur_transform.get_basis().get_column(1));
	_store_vector(p.camera_right, cur_transform.get_basis().get_column(0));

	// GODOT_ANR_FG_MATRICES: "" (default) the clip space of the depth buffer the provider reads
	// (reverse Z in [0, 1] with near at 1, Vulkan's Y down), "d3d" the same with Y up (D3D's clip
	// space), "legacy" the first version (raw projection, reprojection in [-1, 1], transposed).
	static const String matrix_mode = OS::get_singleton()->get_environment("GODOT_ANR_FG_MATRICES");
	if (matrix_mode == "legacy") {
		_store_matrix_transposed(p.view_to_clip, cur_proj);
		_store_matrix_transposed(p.clip_to_view, cur_proj.inverse());
		_store_matrix_transposed(p.clip_to_prev_clip, reprojection);
		_store_matrix_transposed(p.prev_clip_to_clip, reprojection.inverse());
	} else {
		Projection clip_correction;
		clip_correction.set_depth_correction(matrix_mode != "d3d", true, true);
		const Projection view_to_clip = clip_correction * cur_proj;
		const Projection prev_view_to_clip = clip_correction * prev_proj;
		const Projection clip_to_prev_clip = prev_view_to_clip * prev_transform.affine_inverse() * cur_transform * view_to_clip.inverse();
		_store_matrix(p.view_to_clip, view_to_clip);
		_store_matrix(p.clip_to_view, view_to_clip.inverse());
		_store_matrix(p.clip_to_prev_clip, clip_to_prev_clip);
		_store_matrix(p.prev_clip_to_clip, clip_to_prev_clip.inverse());
	}
}

void AnrFgEffect::process_frame() {
	AnrFrameGeneration *anr = AnrFrameGeneration::get_singleton();
	if (anr != nullptr) {
		anr->process_frame(RendererCompositorRD::get_singleton()->get_frame_number());
	}
}

RID AnrFgEffect::process_screen_blit(DisplayServerEnums::WindowID p_screen, RID p_render_target, bool p_present) {
	const uint64_t frame = RendererCompositorRD::get_singleton()->get_frame_number();
	if (scene.frame != frame || scene.render_target != p_render_target || p_render_target.is_null()) {
		return RID(); // The 3D scene of this render target was not drawn with ANR FG this frame.
	}
	scene.frame = UINT64_MAX; // One generated frame per drawn frame.

	AnrFrameGeneration *anr = AnrFrameGeneration::get_singleton();
	if (anr == nullptr || !anr->is_available()) {
		if (!provider_unavailable_reported) {
			provider_unavailable_reported = true;
			WARN_PRINT("ANR FG: frame generation is on but the provider is unavailable: " + (anr ? anr->get_status() : String("no singleton")));
		}
		return RID();
	}

	RD *rd = RD::get_singleton();
	RDD::SwapChainID swap_chain;
	Size2i window_size;
	if (p_present) {
		swap_chain = rd->screen_get_swap_chain(p_screen);
		if (swap_chain.id == 0) {
			return RID();
		}
		// No work for frames that cannot be shown (the swap chain cannot take a second image).
		const String blocker = rd->screen_get_frame_generation_blocker(p_screen);
		if (!blocker.is_empty()) {
			anr->set_inactive_reason(blocker);
			last_generated_frame = UINT64_MAX;
			return RID();
		}
		window_size = Size2i(rd->screen_get_width(p_screen), rd->screen_get_height(p_screen));
		const RD::DataFormat window_format = rd->screen_get_data_format(p_screen);
		if (window_size.width <= 0 || window_size.height <= 0 || window_format == RD::DATA_FORMAT_MAX || !_ensure_screen_target(window_size, window_format)) {
			return RID();
		}
	}

	TextureStorage *texture_storage = TextureStorage::get_singleton();
	RID backbuffer = texture_storage->render_target_get_rd_texture(p_render_target);
	ERR_FAIL_COND_V(backbuffer.is_null(), RID());
	const RD::TextureFormat backbuffer_format = rd->texture_get_format(backbuffer);
	const Size2i size(backbuffer_format.width, backbuffer_format.height);

	// GODOT_ANR_FG_OUTPUT_REAL=1 also passes DLSSG.OutputReal (the provider then copies the
	// backbuffer into it each frame; the engine has no use for it).
	static const bool pass_output_real = OS::get_singleton()->get_environment("GODOT_ANR_FG_OUTPUT_REAL") == "1";
	if (output_size != size || output_format != backbuffer_format.format || output_interpolated.is_null()) {
		_free_outputs();
		RD::TextureFormat tf;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.format = backbuffer_format.format;
		tf.width = size.width;
		tf.height = size.height;
		tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
		output_interpolated = rd->texture_create(tf, RD::TextureView());
		ERR_FAIL_COND_V(output_interpolated.is_null(), RID());
		rd->set_resource_name(output_interpolated, "ANR FG Generated Frame");
		if (pass_output_real) {
			output_real = rd->texture_create(tf, RD::TextureView());
			ERR_FAIL_COND_V(output_real.is_null(), RID());
			rd->set_resource_name(output_real, "ANR FG Real Frame");
		}
		output_size = size;
		output_format = backbuffer_format.format;
	}

	pending.params = scene.params;
	pending.params.backbuffer = _image(backbuffer, false);
	pending.params.depth = _image(scene.depth, true);
	pending.params.motion = _image(scene.motion, false);
	pending.params.output_interpolated = _image(output_interpolated, false);
	pending.params.output_real = output_real.is_valid() ? _image(output_real, false) : AnrFrameGeneration::Image();
	pending.params.hdr = texture_storage->render_target_is_using_hdr(p_render_target);
	pending.params.frame = frame;
	pending.params.frames_in_flight = rd->get_frame_delay();
	// A cut in the sequence of generated frames (first frame, another target, a skipped frame).
	pending.params.reset = last_render_target != p_render_target || last_generated_frame == UINT64_MAX || frame != last_generated_frame + 1;
	pending.swap_chain = swap_chain;
	const AnrFrameGeneration::Image screen_image = p_present ? _image(screen_texture, false) : AnrFrameGeneration::Image();
	pending.output_image = screen_image.image;
	pending.output_format = screen_image.format;
	pending.output_size = window_size;
	pending.dst_rect = Rect2i(Point2i(), window_size);
	last_render_target = p_render_target;
	last_generated_frame = frame;

	// The provider records into the frame's command buffer; the graph puts the inputs in
	// SHADER_READ_ONLY_OPTIMAL and the outputs in GENERAL.
	RD::CallbackResource resources[5];
	uint32_t resource_count = 0;
	resources[resource_count].rid = backbuffer;
	resources[resource_count++].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[resource_count].rid = scene.depth;
	resources[resource_count++].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[resource_count].rid = scene.motion;
	resources[resource_count++].usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
	resources[resource_count].rid = output_interpolated;
	resources[resource_count++].usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	if (output_real.is_valid()) {
		resources[resource_count].rid = output_real;
		resources[resource_count++].usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	}
	rd->driver_callback_add(_evaluate_callback, this, VectorView<RD::CallbackResource>(resources, resource_count));

	// GODOT_ANR_FG_DEBUG=generated shows the generated frame in place of the real one (so a
	// viewport capture saves it).
	static const bool debug_generated = OS::get_singleton()->get_environment("GODOT_ANR_FG_DEBUG") == "generated";
	if (debug_generated && CopyEffects::get_singleton() != nullptr) {
		CopyEffects::get_singleton()->copy_to_rect(output_interpolated, backbuffer, Rect2i(Point2i(), size), false, false, false, !pending.params.hdr);
	}
	return output_interpolated;
}

void AnrFgEffect::end_generated_screen_blit() {
	ERR_FAIL_COND(screen_texture.is_null());
	// Leaves the image in GENERAL at the end of the frame, the layout the driver copies it from.
	RD::CallbackResource resource;
	resource.rid = screen_texture;
	resource.usage = RD::CALLBACK_RESOURCE_USAGE_GENERAL;
	RD::get_singleton()->driver_callback_add(_layout_callback, this, VectorView<RD::CallbackResource>(&resource, 1));
}

void AnrFgEffect::_layout_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	// Nothing to record: the render graph's transition into the declared usage is the point.
}

void AnrFgEffect::_evaluate_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata) {
	AnrFgEffect *self = (AnrFgEffect *)p_userdata;
	AnrFrameGeneration *anr = AnrFrameGeneration::get_singleton();
	if (anr == nullptr) {
		return;
	}
	void *command_buffer = p_driver->command_buffer_get_native_handle(p_command_buffer);
	if (anr->evaluate(command_buffer, self->pending.params) && self->pending.swap_chain.id != 0) {
		p_driver->swap_chain_set_generated_frame(self->pending.swap_chain, self->pending.output_image, self->pending.output_format, self->pending.output_size, self->pending.dst_rect);
	}
}
