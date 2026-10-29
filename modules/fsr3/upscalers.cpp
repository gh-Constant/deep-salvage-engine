/**************************************************************************/
/*  upscalers.cpp                                                 */
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

#include "upscalers.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

static constexpr const char *SIMPLE_SHADER = R"GLSL(#version 450
layout(local_size_x=8, local_size_y=8, local_size_z=1) in;
layout(rgba16f,set=0,binding=0) uniform readonly image2D input_image;
layout(rgba16f,set=0,binding=1) uniform writeonly image2D output_image;
void main() {
    ivec2 dst = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(output_image);
    if (any(greaterThanEqual(dst, size))) return;
    ivec2 src = min(ivec2((vec2(dst)+0.5)*vec2(imageSize(input_image))/vec2(size)),imageSize(input_image)-1);
    imageStore(output_image, dst, imageLoad(input_image, src));
}
)GLSL";

// Destructors run after the viewport has dropped its resource reference. Only
// detached RIDs are queued; callbacks never retain a pointer to a dead resource.
static void free_shader_on_render_thread(RID shader) {
	if (shader.is_valid() && render_device()) {
		render_device()->free_rid(shader);
	}
}
void SimpleComputeUpscaler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_dispatch_count"), &SimpleComputeUpscaler::get_dispatch_count);
}
void SimpleComputeUpscaler::render_callback(const RenderData *data) {
	if (!render_device()) {
		return;
	}
	Ref<RenderSceneBuffersRD> buffers = data->get_render_scene_buffers();
	if (buffers.is_null()) {
		return;
	}
	if (!shader.is_valid()) {
		shader = compile_compute(SIMPLE_SHADER);
		ERR_FAIL_COND(!shader.is_valid());
		pipeline = render_device()->compute_pipeline_create(shader);
	}
	Vector2i size = buffers->get_target_size();
	for (uint32_t view = 0; view < buffers->get_view_count(); ++view) {
		LocalVector<RD::Uniform> uniforms;
		uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_IMAGE, 0, buffers->get_internal_texture(view)));
		uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_IMAGE, 1, buffers->get_upscaled_layer(view)));
		RID set = UniformSetCacheRD::get_singleton()->get_cache_vec(shader, 0, uniforms);
		auto list = render_device()->compute_list_begin();
		render_device()->compute_list_bind_compute_pipeline(list, pipeline);
		render_device()->compute_list_bind_uniform_set(list, set, 0);
		render_device()->compute_list_dispatch(list, (size.x + 7) / 8, (size.y + 7) / 8, 1);
		render_device()->compute_list_end();
		dispatch_count.fetch_add(1);
	}
}
SimpleComputeUpscaler::~SimpleComputeUpscaler() {
	if (shader.is_valid()) {
		RenderingServer::get_singleton()->call_on_render_thread(callable_mp_static(free_shader_on_render_thread).bind(shader));
	}
}

void FSR3Upscaler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sharpness", "value"), &FSR3Upscaler::set_sharpness);
	ClassDB::bind_method(D_METHOD("get_sharpness"), &FSR3Upscaler::get_sharpness);
	ClassDB::bind_method(D_METHOD("configure_scale", "scale"), &FSR3Upscaler::configure_scale);
	ClassDB::bind_method(D_METHOD("reset_history"), &FSR3Upscaler::reset_history);
	ClassDB::bind_method(D_METHOD("get_dispatch_count"), &FSR3Upscaler::get_dispatch_count);
	ClassDB::bind_method(D_METHOD("get_gpu_dispatch_count"), &FSR3Upscaler::get_gpu_dispatch_count);
	ClassDB::bind_method(D_METHOD("get_context_count"), &FSR3Upscaler::get_context_count);
	ClassDB::bind_method(D_METHOD("get_source_version"), &FSR3Upscaler::get_source_version);
	ClassDB::bind_method(D_METHOD("get_failure_count"), &FSR3Upscaler::get_failure_count);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sharpness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_sharpness", "get_sharpness");
}
FSR3Upscaler::FSR3Upscaler() {
	set_requires_motion_vectors(true);
	set_mipmap_bias(1.0f);
	configure_scale(0.67);
}
String FSR3Upscaler::get_source_version() const {
	return vformat("%d.%d.%d", FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
}
void FSR3Upscaler::configure_scale(double scale) {
	scale = CLAMP(scale, 0.1, 1.0);
	uint32_t phases = static_cast<uint32_t>(8.0 / (scale * scale));
	set_jitter_phase_count(phases);
	reset_history();
}
void FSR3Upscaler::render_callback(const RenderData *data) {
	if (!render_device()) {
		return;
	}
	Ref<RenderSceneBuffersRD> buffers = data->get_render_scene_buffers();
	RenderSceneData *scene = data->get_render_scene_data();
	if (buffers.is_null() || !scene) {
		return;
	}
	if (!scene->has_method("get_temporal_upscaler_data")) {
		if (!missing_api_reported) {
			ERR_PRINT("FSR3 requires temporal renderer temporal_data; no fallback was dispatched.");
			missing_api_reported = true;
		}
		failure_count.fetch_add(1);
		return;
	}
	if (!backend) {
		backend = std::make_unique<RendererRD::FSR3UpscalerEffect>();
	}
	Dictionary temporal_data = scene->call("get_temporal_upscaler_data");
	Vector2i internal = buffers->get_internal_size(), target = buffers->get_target_size();
	if (internal.x <= 0 || internal.y <= 0 || target.x <= 0 || target.y <= 0) {
		return;
	}
	uint64_t frame = Engine::get_singleton()->get_frames_drawn();
	for (uint32_t view = 0; view < buffers->get_view_count(); ++view) {
		auto key = std::make_pair(uint64_t(buffers->get_instance_id()), view);
		auto &state = views[key];
		bool reset = state.reset_serial != reset_serial.load() || frame > state.last_frame + 1;
		if (!state.context || state.internal_size != internal || state.target_size != target) {
			state.context.reset();
			state.context.reset(backend->create_context(internal, target, false));
			state.internal_size = internal;
			state.target_size = target;
			reset = true;
			context_count.fetch_add(1);
		}
		if (!state.context) {
			failure_count.fetch_add(1);
			continue;
		}
		RendererRD::FSR3UpscalerEffect::Parameters params{};
		params.context = state.context.get();
		params.internal_size = internal;
		params.target_size = target;
		params.color = buffers->get_internal_texture(view);
		params.depth = buffers->get_depth_texture(view);
		params.velocity = buffers->get_velocity_buffer(false, view);
		params.output = buffers->get_upscaled_layer(view);
		// Reproduce the engine's alpha-only reactive view through the public
		// named-texture API. The scope/name belong to this experimental branch.
		RD::TextureView reactive_view;
		reactive_view.swizzle_r = RD::TEXTURE_SWIZZLE_A;
		reactive_view.swizzle_g = RD::TEXTURE_SWIZZLE_A;
		reactive_view.swizzle_b = RD::TEXTURE_SWIZZLE_A;
		reactive_view.swizzle_a = RD::TEXTURE_SWIZZLE_A;
		params.reactive = buffers->get_texture_slice_view("render_buffers", "color", view, 0, 1, 1, reactive_view);
		Vector2 jitter = temporal_data["jitter"];
		params.jitter = jitter * Vector2(internal) * 0.5f;
		params.z_near = temporal_data["z_near"];
		params.z_far = temporal_data["z_far"];
		params.fovy = temporal_data["fovy"];
		params.delta_time = double(temporal_data["delta_seconds"]) * 1000.0;
		params.reprojection = temporal_data["reprojection"];
		params.sharpness = sharpness.load();
		params.reset_accumulation = reset;
		render_device()->draw_command_begin_label("FSR 3.1.6 built-in", Color(0.4, 0.8, 0.2));
		uint64_t before = state.context->scratch.compute_dispatch_count;
		if (backend->upscale(params)) {
			dispatch_count.fetch_add(1);
		} else {
			failure_count.fetch_add(1);
		}
		gpu_dispatch_count.fetch_add(state.context->scratch.compute_dispatch_count - before);
		render_device()->draw_command_end_label();
		state.reset_serial = reset_serial.load();
		state.last_frame = frame;
	}
	// Release histories for viewports no longer rendered; resizing the same
	// buffers replaces its context above. Do this only on the render thread.
	for (auto it = views.begin(); it != views.end();) {
		if (frame > it->second.last_frame + 120) {
			it = views.erase(it);
		} else {
			++it;
		}
	}
}

namespace {
struct DetachedBackend {
	std::unique_ptr<RendererRD::FSR3UpscalerEffect> backend;
	std::map<std::pair<uint64_t, uint32_t>, std::unique_ptr<RendererRD::FSR3UpscalerContext>> contexts;
};
void free_detached_backend(uint64_t address) {
	delete reinterpret_cast<DetachedBackend *>(address);
}
} //namespace
FSR3Upscaler::~FSR3Upscaler() {
	if (!backend) {
		return;
	}
	auto *detached = new DetachedBackend;
	detached->backend = std::move(backend);
	for (auto &entry : views) {
		detached->contexts[entry.first] = std::move(entry.second.context);
	}
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp_static(free_detached_backend).bind(uint64_t(detached)));
}
