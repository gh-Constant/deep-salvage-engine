/**************************************************************************/
/*  fidelityfx_upscaler.cpp                                                 */
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

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "fidelityfx_library.h"
#include "fidelityfx_upscaler.h"
#include "thirdparty/amd-fidelityfx-api/ffx_upscale.h"
#include "thirdparty/amd-fidelityfx-api/vk/ffx_api_vk.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

#include <functional>

struct FidelityFXBinaryStats {
	std::mutex mutex;
	String backend = "Initializing";
	String version;
	String error;
	PackedStringArray versions;
	std::atomic<int64_t> frames{ 0 };
	std::atomic<int64_t> failures{ 0 };
	std::atomic<bool> dispatch_failed{ false };
};

namespace {
// Source-specific velocity sentinels and reactive alpha are normalized before
// entering the unmodified AMD binary. No vendor shader patches are required.
const char *PREPARE_INPUTS = R"GLSL(#version 450
#extension GL_EXT_samplerless_texture_functions : require
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(set=0,binding=0) uniform texture2D velocity_input;
layout(set=0,binding=1) uniform texture2D depth_input;
layout(set=0,binding=2) uniform texture2D color_input;
layout(rg16f,set=0,binding=3) uniform writeonly image2D velocity_output;
layout(r8,set=0,binding=4) uniform writeonly image2D reactive_output;
layout(push_constant,std430) uniform Params { mat4 reprojection; ivec2 size; } params;
void main() {
    ivec2 p=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(p,params.size))) return;
    vec2 velocity=texelFetch(velocity_input,p,0).xy;
    if(all(lessThanEqual(velocity,vec2(-1.0)))) {
        vec2 uv=(vec2(p)+0.5)/vec2(params.size);
        float depth=texelFetch(depth_input,p,0).r;
        vec4 previous=params.reprojection*vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
        velocity=abs(previous.w)>1e-6 ? 0.5+previous.xy/previous.w*0.5-uv : vec2(0.0);
    }
    imageStore(velocity_output,p,vec4(velocity,0.0,0.0));
    imageStore(reactive_output,p,vec4(clamp(texelFetch(color_input,p,0).a,0.0,0.9)));
}
)GLSL";

// Godot retains this callable in the frame until its GPU fence completes. Its
// captured shared_ptr keeps SDK contexts and loaded modules alive for that long.
class NativeJob : public CallableCustom {
	std::function<void(uint64_t)> function;
	static bool equal(const CallableCustom *a, const CallableCustom *b) { return a == b; }
	static bool less(const CallableCustom *a, const CallableCustom *b) { return std::less<const void *>{}(a, b); }

public:
	explicit NativeJob(std::function<void(uint64_t)> value) : function(std::move(value)) {}
	bool is_valid() const override { return true; }
	ObjectID get_object() const override { return ObjectID(); }
	uint32_t hash() const override { return uint32_t(reinterpret_cast<uintptr_t>(this)); }
	String get_as_text() const override { return "FidelityFX native compute job"; }
	CompareEqualFunc get_compare_equal_func() const override { return equal; }
	CompareLessFunc get_compare_less_func() const override { return less; }
	int get_argument_count(bool &valid) const override {
		valid = true;
		return 1;
	}
	void call(const Variant **arguments, int count, Variant &, Callable::CallError &error) const override {
		if (count != 1) {
			error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
			error.expected = 1;
			return;
		}
		error.error = Callable::CallError::CALL_OK;
		function(uint64_t(*arguments[0]));
	}
};

struct VulkanLoader {
	void *module = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
	VulkanLoader() {
#ifdef _WIN32
		HMODULE existing = nullptr;
		if (GetModuleHandleExW(0, L"vulkan-1.dll", &existing)) {
			module = existing;
		} else {
			module = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
		}
		if (module) {
			get_device_proc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(static_cast<HMODULE>(module), "vkGetDeviceProcAddr"));
		}
#else
		module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if (module) {
			get_device_proc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(module, "vkGetDeviceProcAddr"));
		}
#endif
	}
	~VulkanLoader() {
		if (!module) {
			return;
		}
#ifdef _WIN32
		FreeLibrary(static_cast<HMODULE>(module));
#else
		dlclose(module);
#endif
	}
};

struct BinaryContext {
	std::shared_ptr<fidelityfx::Library> library;
	std::shared_ptr<VulkanLoader> vulkan;
	ffxContext context = nullptr;
	// AMD's API requires creation descriptors to outlive the context.
	ffxCreateContextDescUpscale create{};
	ffxCreateBackendVKDesc backend{};
	ffxOverrideVersion override_version{};
	Vector2i internal, target;
	uint64_t reset_serial = 0;
	uint64_t last_frame = 0;
	~BinaryContext() {
		if (context) {
			library->destroy_context(&context, nullptr);
		}
	}
};
FfxApiResource wrap_texture(RID texture, bool writable = false) {
	FfxApiResource result{};
	if (!texture.is_valid()) {
		return result;
	}
	auto *rd = render_device();
	RD::TextureFormat format = rd->texture_get_format(texture);
	result.resource = reinterpret_cast<void *>(rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, texture, 0));
	result.description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
	result.description.format = ffxApiGetSurfaceFormatVK(VkFormat(rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_DATA_FORMAT, texture, 0)));
	result.description.width = format.width;
	result.description.height = format.height;
	result.description.depth = 1;
	result.description.mipCount = 1;
	if ((format.usage_bits & RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		result.description.usage |= FFX_API_RESOURCE_USAGE_DEPTHTARGET;
	}
	if (writable) {
		result.description.usage |= FFX_API_RESOURCE_USAGE_UAV;
	}
	result.state = writable ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_COMPUTE_READ;
	return result;
}
void message(uint32_t type, const wchar_t *text) {
	if (type == FFX_API_MESSAGE_TYPE_ERROR) {
		ERR_PRINT(String(text));
	} else {
		WARN_PRINT(String(text));
	}
}
} //namespace

struct FidelityFXBinaryState {
	std::shared_ptr<fidelityfx::Library> library;
	std::shared_ptr<VulkanLoader> vulkan;
	std::vector<fidelityfx::Provider> versions;
	std::map<std::pair<uint64_t, uint32_t>, std::shared_ptr<BinaryContext>> views;
	String preferred;
	RID prepare_shader, prepare_pipeline;
	~FidelityFXBinaryState() {
		views.clear();
		if (prepare_shader.is_valid() && render_device()) {
			render_device()->free_rid(prepare_shader);
		}
	}
};
namespace {
void destroy_state(uint64_t pointer) {
	delete reinterpret_cast<FidelityFXBinaryState *>(pointer);
}
} //namespace

void FidelityFXUpscaler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library_path", "path"), &FidelityFXUpscaler::set_library_path);
	ClassDB::bind_method(D_METHOD("get_library_path"), &FidelityFXUpscaler::get_library_path);
	ClassDB::bind_method(D_METHOD("set_preferred_version", "version"), &FidelityFXUpscaler::set_preferred_version);
	ClassDB::bind_method(D_METHOD("get_preferred_version"), &FidelityFXUpscaler::get_preferred_version);
	ClassDB::bind_method(D_METHOD("set_allow_source_fallback", "enabled"), &FidelityFXUpscaler::set_allow_source_fallback);
	ClassDB::bind_method(D_METHOD("get_allow_source_fallback"), &FidelityFXUpscaler::get_allow_source_fallback);
	ClassDB::bind_method(D_METHOD("get_active_backend"), &FidelityFXUpscaler::get_active_backend);
	ClassDB::bind_method(D_METHOD("get_active_version"), &FidelityFXUpscaler::get_active_version);
	ClassDB::bind_method(D_METHOD("get_binary_error"), &FidelityFXUpscaler::get_binary_error);
	ClassDB::bind_method(D_METHOD("get_available_versions"), &FidelityFXUpscaler::get_available_versions);
	ClassDB::bind_method(D_METHOD("get_binary_dispatch_count"), &FidelityFXUpscaler::get_binary_dispatch_count);
	ClassDB::bind_method(D_METHOD("get_binary_failure_count"), &FidelityFXUpscaler::get_binary_failure_count);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "library_path", PROPERTY_HINT_FILE, "*.dll,*.so"), "set_library_path", "get_library_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preferred_version"), "set_preferred_version", "get_preferred_version");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_source_fallback"), "set_allow_source_fallback", "get_allow_source_fallback");
}
FidelityFXUpscaler::FidelityFXUpscaler() : stats(std::make_shared<FidelityFXBinaryStats>()) {}
FidelityFXUpscaler::~FidelityFXUpscaler() {
	if (binary) {
		RenderingServer::get_singleton()->call_on_render_thread(callable_mp_static(destroy_state).bind(uint64_t(binary.release())));
	}
}
void FidelityFXUpscaler::set_library_path(const String &value) {
	std::lock_guard<std::mutex> lock(settings_mutex);
	library_path = value;
	settings_revision++;
}
String FidelityFXUpscaler::get_library_path() const {
	std::lock_guard<std::mutex> lock(settings_mutex);
	return library_path;
}
void FidelityFXUpscaler::set_preferred_version(const String &value) {
	std::lock_guard<std::mutex> lock(settings_mutex);
	preferred_version = value;
	settings_revision++;
}
String FidelityFXUpscaler::get_preferred_version() const {
	std::lock_guard<std::mutex> lock(settings_mutex);
	return preferred_version;
}
void FidelityFXUpscaler::set_allow_source_fallback(bool value) {
	std::lock_guard<std::mutex> lock(settings_mutex);
	allow_source_fallback = value;
	settings_revision++;
}
bool FidelityFXUpscaler::get_allow_source_fallback() const {
	std::lock_guard<std::mutex> lock(settings_mutex);
	return allow_source_fallback;
}
void FidelityFXUpscaler::reset_history() {
	FSR3Upscaler::reset_history();
	binary_reset_serial++;
}
int64_t FidelityFXUpscaler::get_dispatch_count() const {
	return FSR3Upscaler::get_dispatch_count() + stats->frames.load();
}
int64_t FidelityFXUpscaler::get_failure_count() const {
	return FSR3Upscaler::get_failure_count() + stats->failures.load();
}
int64_t FidelityFXUpscaler::get_binary_dispatch_count() const {
	return stats->frames.load();
}
int64_t FidelityFXUpscaler::get_binary_failure_count() const {
	return stats->failures.load();
}
String FidelityFXUpscaler::get_active_backend() const {
	std::lock_guard<std::mutex> lock(stats->mutex);
	return stats->backend;
}
String FidelityFXUpscaler::get_active_version() const {
	std::lock_guard<std::mutex> lock(stats->mutex);
	return stats->version;
}
String FidelityFXUpscaler::get_binary_error() const {
	std::lock_guard<std::mutex> lock(stats->mutex);
	return stats->error;
}
PackedStringArray FidelityFXUpscaler::get_available_versions() const {
	std::lock_guard<std::mutex> lock(stats->mutex);
	return stats->versions;
}

void FidelityFXUpscaler::render_callback(const RenderData *data) {
	if (!render_device()) {
		return;
	}
	bool fallback;
	String path, preferred;
	uint64_t revision;
	{
		std::lock_guard<std::mutex> lock(settings_mutex);
		path = library_path;
		preferred = preferred_version;
		fallback = allow_source_fallback;
		revision = settings_revision.load();
	}
	auto report = [&](const String &reason) {
		std::lock_guard<std::mutex> lock(stats->mutex);
		if (stats->error != reason) {
			if (fallback) {
				WARN_PRINT(reason + String("; using source FSR 3.1.4"));
			} else {
				ERR_PRINT(reason);
			}
		}
		stats->error = reason;
		stats->backend = fallback ? "Source FSR3" : "Unavailable";
		stats->version = fallback ? "3.1.4" : "";
	};
	if (revision != applied_revision) {
		binary.reset();
		initialized = false;
		applied_revision = revision;
	}
	if (stats->dispatch_failed.exchange(false)) {
		String reason;
		{
			std::lock_guard<std::mutex> lock(stats->mutex);
			reason = stats->error;
		}
		report(reason);
		binary.reset();
	}
	if (!initialized) {
		initialized = true;
		{
			std::lock_guard<std::mutex> lock(stats->mutex);
			stats->versions.clear();
		}
		binary = std::make_unique<FidelityFXBinaryState>();
		binary->preferred = preferred;
		String reason;
		if (!render_device()->has_method("native_compute_callback_add")) {
			reason = "Engine lacks native compute callback support";
		} else if (RenderingServer::get_singleton()->get_current_rendering_driver_name() != "vulkan") {
			reason = "AMD binary backend currently requires Vulkan";
		} else if (path.is_empty() && OS::get_singleton()->get_name() != "Windows") {
			reason = "AMD Windows DLLs are unavailable to native Linux";
		} else {
			if (path.is_empty()) {
				path = OS::get_singleton()->get_executable_path().get_base_dir().path_join("amd_fidelityfx_loader_vk.dll");
				if (!FileAccess::exists(path)) {
					path = OS::get_singleton()->get_executable_path().get_base_dir().path_join("amd_fidelityfx_vk.dll");
				}
			}
			path = ProjectSettings::get_singleton()->globalize_path(path);
			binary->library = std::make_shared<fidelityfx::Library>();
			if (!binary->library->open(std::filesystem::u8path(path.utf8().get_data()))) {
				reason = String(binary->library->error().c_str());
			} else if (!binary->library->versions(binary->versions)) {
				reason = String(binary->library->error().c_str());
			} else {
				binary->vulkan = std::make_shared<VulkanLoader>();
				if (!binary->vulkan->get_device_proc) {
					reason = "Cannot resolve vkGetDeviceProcAddr";
				} else {
					binary->prepare_shader = compile_compute(PREPARE_INPUTS);
					if (!binary->prepare_shader.is_valid()) {
						reason = "FidelityFX input preparation shader failed";
					} else {
						binary->prepare_pipeline = render_device()->compute_pipeline_create(binary->prepare_shader);
					}
				}
			}
		}
		if (!reason.is_empty()) {
			report(reason);
			binary.reset();
		} else {
			std::lock_guard<std::mutex> lock(stats->mutex);
			stats->error = "";
			stats->versions.clear();
			for (const auto &version : binary->versions) {
				stats->versions.push_back(String(version.name.c_str()));
			}
		}
	}
	if (!binary) {
		if (fallback) {
			FSR3Upscaler::render_callback(data);
		}
		return;
	}
	Ref<RenderSceneBuffersRD> buffers = data->get_render_scene_buffers();
	RenderSceneData *scene = data->get_render_scene_data();
	if (buffers.is_null() || !scene) {
		return;
	}
	// Native SDK resources are complete images, not Godot texture-array slices.
	if (buffers->get_view_count() != 1 || !scene->has_method("get_temporal_upscaler_data")) {
		report("Binary upscaling requires mono rendering and temporal temporal_data");
		if (fallback) {
			FSR3Upscaler::render_callback(data);
		}
		return;
	}
	Dictionary temporal_data = scene->call("get_temporal_upscaler_data");
	Vector2i internal = buffers->get_internal_size(), target = buffers->get_target_size();
	uint64_t frame = Engine::get_singleton()->get_frames_drawn();
	auto &context = binary->views[{ uint64_t(buffers->get_instance_id()), 0 }];
	if (!context || context->internal != internal || context->target != target) {
		auto candidate = std::make_shared<BinaryContext>();
		candidate->library = binary->library;
		candidate->vulkan = binary->vulkan;
		candidate->internal = internal;
		candidate->target = target;
		candidate->backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
		candidate->backend.vkDevice = reinterpret_cast<VkDevice>(render_device()->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0));
		candidate->backend.vkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(render_device()->get_driver_resource(RD::DRIVER_RESOURCE_PHYSICAL_DEVICE, RID(), 0));
		candidate->backend.vkDeviceProcAddr = candidate->vulkan->get_device_proc;
		candidate->create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
		candidate->create.header.pNext = &candidate->backend.header;
		candidate->create.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
		candidate->create.maxRenderSize = { uint32_t(internal.x), uint32_t(internal.y) };
		candidate->create.maxUpscaleSize = { uint32_t(target.x), uint32_t(target.y) };
		candidate->create.fpMessage = message;
		bool found = preferred.is_empty();
		for (const auto &provider : binary->versions) {
			if (String(provider.name.c_str()) == preferred) {
				candidate->override_version.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
				candidate->override_version.versionId = provider.id;
				candidate->backend.header.pNext = &candidate->override_version.header;
				found = true;
				break;
			}
		}
		if (!found) {
			report("Requested AMD provider is not available: " + preferred);
			binary.reset();
			if (fallback) {
				FSR3Upscaler::render_callback(data);
			}
			return;
		}
		auto result = binary->library->create_context(&candidate->context, &candidate->create.header, nullptr);
		if (result != FFX_API_RETURN_OK || !candidate->context) {
			report("AMD Vulkan context creation failed (code " + String::num_uint64(result) + ")");
			binary.reset();
			if (fallback) {
				FSR3Upscaler::render_callback(data);
			}
			return;
		}
		ffxQueryGetProviderVersion version{};
		version.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
		result = binary->library->query(&candidate->context, &version.header);
		{
			std::lock_guard<std::mutex> lock(stats->mutex);
			stats->backend = "AMD binary / Vulkan";
			stats->version = (result == FFX_API_RETURN_OK && version.versionName) ? String(version.versionName) : "Unknown AMD provider";
			stats->error = "";
		}
		context = std::move(candidate);
	}
	RID velocity = buffers->create_texture("fidelityfx_binary", "velocity", RD::DATA_FORMAT_R16G16_SFLOAT, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT, RD::TEXTURE_SAMPLES_1, internal, 1, 1, false, false);
	RID reactive = buffers->create_texture("fidelityfx_binary", "reactive", RD::DATA_FORMAT_R8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT, RD::TEXTURE_SAMPLES_1, internal, 1, 1, false, false);
	LocalVector<RD::Uniform> uniforms;
	uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_TEXTURE, 0, buffers->get_velocity_buffer(false, 0)));
	uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_TEXTURE, 1, buffers->get_depth_texture(0)));
	uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_TEXTURE, 2, buffers->get_internal_texture(0)));
	uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_IMAGE, 3, velocity));
	uniforms.push_back(make_uniform(RD::UNIFORM_TYPE_IMAGE, 4, reactive));
	struct Push {
		float reprojection[16];
		int32_t size[2];
	} push{};
	Projection reprojection = temporal_data["reprojection"];
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			push.reprojection[c * 4 + r] = reprojection[c][r];
		}
	}
	push.size[0] = internal.x;
	push.size[1] = internal.y;
	PackedByteArray constants;
	constants.resize(sizeof(push));
	memcpy(constants.ptrw(), &push, sizeof(push));
	auto *rd = render_device();
	auto list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(list, binary->prepare_pipeline);
	rd->compute_list_bind_uniform_set(list, UniformSetCacheRD::get_singleton()->get_cache_vec(binary->prepare_shader, 0, uniforms), 0);
	rd->compute_list_set_push_constant(list, constants.ptr(), constants.size());
	rd->compute_list_dispatch(list, (internal.x + 7) / 8, (internal.y + 7) / 8, 1);
	rd->compute_list_end();
	ffxDispatchDescUpscale dispatch{};
	dispatch.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
	dispatch.color = wrap_texture(buffers->get_internal_texture(0));
	dispatch.depth = wrap_texture(buffers->get_depth_texture(0));
	dispatch.motionVectors = wrap_texture(velocity);
	dispatch.reactive = wrap_texture(reactive);
	dispatch.output = wrap_texture(buffers->get_upscaled_layer(0), true);
	Vector2 jitter = temporal_data["jitter"];
	dispatch.jitterOffset = { float(jitter.x * internal.x * 0.5), float(jitter.y * internal.y * 0.5) };
	dispatch.motionVectorScale = { float(internal.x), float(internal.y) };
	dispatch.renderSize = { uint32_t(internal.x), uint32_t(internal.y) };
	dispatch.upscaleSize = { uint32_t(target.x), uint32_t(target.y) };
	dispatch.preExposure = 1;
	dispatch.viewSpaceToMetersFactor = 1;
	dispatch.cameraNear = temporal_data["z_near"];
	dispatch.cameraFar = temporal_data["z_far"];
	dispatch.cameraFovAngleVertical = temporal_data["fovy"];
	dispatch.frameTimeDelta = double(temporal_data["delta_seconds"]) * 1000.0;
	dispatch.sharpness = get_sharpness();
	dispatch.enableSharpening = dispatch.sharpness > 1e-6;
	dispatch.reset = context->reset_serial != binary_reset_serial.load() || frame > context->last_frame + 1;
	auto state = context;
	auto statistics = stats;
	auto work = [state, statistics, dispatch](uint64_t command) mutable {
		dispatch.commandList = reinterpret_cast<void *>(command);
		auto result = state->library->dispatch(&state->context, &dispatch.header);
		if (result == FFX_API_RETURN_OK) {
			statistics->frames++;
		} else {
			statistics->failures++;
			statistics->dispatch_failed = true;
			std::lock_guard<std::mutex> lock(statistics->mutex);
			statistics->error = "AMD dispatch failed (code " + String::num_uint64(result) + ")";
			ERR_PRINT(statistics->error);
		}
	};
	Callable job(memnew(NativeJob(std::move(work))));
	TypedArray<RID> reads, writes;
	reads.push_back(buffers->get_internal_texture(0));
	reads.push_back(buffers->get_depth_texture(0));
	reads.push_back(velocity);
	reads.push_back(reactive);
	writes.push_back(buffers->get_upscaled_layer(0));
	int64_t result = rd->native_compute_callback_add(job, reads, writes);
	if (result != OK) {
		report("Godot rejected the native FidelityFX compute callback");
	}
	context->last_frame = frame;
	context->reset_serial = binary_reset_serial.load();
	for (auto it = binary->views.begin(); it != binary->views.end();) {
		if (frame > it->second->last_frame + 120) {
			it = binary->views.erase(it);
		} else {
			++it;
		}
	}
}
