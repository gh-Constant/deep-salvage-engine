/**************************************************************************/
/*  anr_frame_generation.cpp                                              */
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

#include "anr_frame_generation.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"
#include "servers/rendering/rendering_device.h"

#if defined(WINDOWS_ENABLED) && defined(VULKAN_ENABLED)
#define ANR_FG_SUPPORTED 1
#include "drivers/vulkan/godot_vulkan.h"

#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "version.lib")
#endif
#endif

#include "anr_fg_ngx.h"

AnrFrameGeneration *AnrFrameGeneration::singleton = nullptr;
bool AnrFrameGeneration::provider_used = false;

// NGX's application ID for a host without an NVIDIA project ID (as ANR's own replay host uses).
static constexpr unsigned long long ANR_FG_APP_ID = 1;

static String _ngx_result_text(uint32_t p_result) {
	return vformat("0x%08x", (int64_t)p_result);
}

// The parameter map a host keeps for a provider: values by name, numbers readable as any number
// type, pointers as their own type or as void *.
class ParameterMap final : public ANR_NVSDK_NGX_Parameter {
	enum Kind {
		KIND_NUMBER,
		KIND_POINTER,
	};
	struct Value {
		Kind kind = KIND_NUMBER;
		double number = 0.0; // Every NGX number type converts exactly to and from double.
		void *pointer = nullptr;
	};
	HashMap<String, Value> values;

	void _number(const char *p_name, double p_value) {
		Value v;
		v.number = p_value;
		values[String(p_name)] = v;
	}
	void _pointer(const char *p_name, void *p_value) {
		Value v;
		v.kind = KIND_POINTER;
		v.pointer = p_value;
		values[String(p_name)] = v;
	}
	template <typename T>
	ANR_NGX_Result _read(const char *p_name, T *r_value) const {
		if (r_value == nullptr || p_name == nullptr) {
			return ANR_NGX_Result_Fail;
		}
		HashMap<String, Value>::ConstIterator it = values.find(String(p_name));
		if (it == values.end() || it->value.kind != KIND_NUMBER) {
			return ANR_NGX_Result_Fail;
		}
		*r_value = (T)it->value.number;
		return ANR_NGX_Result_Success;
	}
	template <typename T>
	ANR_NGX_Result _read_pointer(const char *p_name, T **r_value) const {
		if (r_value == nullptr || p_name == nullptr) {
			return ANR_NGX_Result_Fail;
		}
		HashMap<String, Value>::ConstIterator it = values.find(String(p_name));
		if (it == values.end() || it->value.kind != KIND_POINTER) {
			return ANR_NGX_Result_Fail;
		}
		*r_value = (T *)it->value.pointer;
		return ANR_NGX_Result_Success;
	}

public:
	virtual void Set(const char *p_name, unsigned long long p_value) override { _number(p_name, (double)p_value); }
	virtual void Set(const char *p_name, float p_value) override { _number(p_name, p_value); }
	virtual void Set(const char *p_name, double p_value) override { _number(p_name, p_value); }
	virtual void Set(const char *p_name, unsigned int p_value) override { _number(p_name, p_value); }
	virtual void Set(const char *p_name, int p_value) override { _number(p_name, p_value); }
	virtual void Set(const char *p_name, ID3D11Resource *p_value) override { _pointer(p_name, (void *)p_value); }
	virtual void Set(const char *p_name, ID3D12Resource *p_value) override { _pointer(p_name, (void *)p_value); }
	virtual void Set(const char *p_name, void *p_value) override { _pointer(p_name, p_value); }

	virtual ANR_NGX_Result Get(const char *p_name, unsigned long long *r_value) const override { return _read(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, float *r_value) const override { return _read(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, double *r_value) const override { return _read(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, unsigned int *r_value) const override { return _read(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, int *r_value) const override { return _read(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, ID3D11Resource **r_value) const override { return _read_pointer(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, ID3D12Resource **r_value) const override { return _read_pointer(p_name, r_value); }
	virtual ANR_NGX_Result Get(const char *p_name, void **r_value) const override { return _read_pointer(p_name, r_value); }

	virtual void Reset() override { values.clear(); }

	void erase(const char *p_name) { values.erase(String(p_name)); }
};

void AnrFrameGeneration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &AnrFrameGeneration::is_available);
	ClassDB::bind_method(D_METHOD("get_status"), &AnrFrameGeneration::get_status);
	ClassDB::bind_method(D_METHOD("get_provider_path"), &AnrFrameGeneration::get_provider_path);
	ClassDB::bind_method(D_METHOD("get_provider_version"), &AnrFrameGeneration::get_provider_version);
	ClassDB::bind_method(D_METHOD("get_evaluated_frames"), &AnrFrameGeneration::get_evaluated_frames);
	ClassDB::bind_method(D_METHOD("get_presented_frames"), &AnrFrameGeneration::get_presented_frames);
	ClassDB::bind_method(D_METHOD("get_failed_frames"), &AnrFrameGeneration::get_failed_frames);
	ClassDB::bind_method(D_METHOD("get_last_result"), &AnrFrameGeneration::get_last_result);
	ClassDB::bind_method(D_METHOD("get_max_generated_frames"), &AnrFrameGeneration::get_max_generated_frames);
}

AnrFrameGeneration *AnrFrameGeneration::get_singleton() {
	return singleton;
}

void AnrFrameGeneration::register_singleton() {
	GDREGISTER_CLASS(AnrFrameGeneration);
	Engine::get_singleton()->add_singleton(Engine::Singleton("AnrFrameGeneration", AnrFrameGeneration::get_singleton()));
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "rendering/anr_frame_generation/provider_path", PROPERTY_HINT_GLOBAL_DIR), "");
}

AnrFrameGeneration::AnrFrameGeneration() {
	singleton = this;
}

AnrFrameGeneration::~AnrFrameGeneration() {
	finish();
#ifdef ANR_FG_SUPPORTED
	// The provider DLL stays loaded until exit: it may own threads and Vulkan objects.
#endif
	if (singleton == this) {
		singleton = nullptr;
	}
	if (provider_used) {
		print_line("ANR FG: engine side cleaned up.");
		fflush(stdout);
	}
}

void AnrFrameGeneration::_set_status(const String &p_status) {
	status = p_status;
}

String AnrFrameGeneration::resolve_provider_path() {
	String path = OS::get_singleton()->get_environment("GODOT_ANR_FG_PATH");
	if (path.is_empty() && ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("rendering/anr_frame_generation/provider_path")) {
		path = GLOBAL_GET("rendering/anr_frame_generation/provider_path");
		if (!path.is_empty() && (path.begins_with("res://") || path.begins_with("user://"))) {
			path = ProjectSettings::get_singleton()->globalize_path(path);
		}
	}
	if (path.is_empty()) {
		path = OS::get_singleton()->get_executable_path().get_base_dir().path_join("anr_fg");
	}
	if (!path.to_lower().ends_with(".dll")) {
		path = path.path_join("nvngx_dlssg.dll");
	}
	return path.replace("\\", "/");
}

bool AnrFrameGeneration::_load_provider() {
	if (load_attempted) {
		return module != nullptr;
	}
	load_attempted = true;

#ifdef ANR_FG_SUPPORTED
	provider_path = resolve_provider_path();
	const String native_path = provider_path.replace("/", "\\");
	if (GetFileAttributesW((LPCWSTR)native_path.utf16().get_data()) == INVALID_FILE_ATTRIBUTES) {
		_set_status("provider not found: " + provider_path);
		print_verbose("ANR FG: provider not found at " + provider_path + " (set GODOT_ANR_FG_PATH or rendering/anr_frame_generation/provider_path).");
		return false;
	}

	// The provider loads its adapters, kernels and programs from its own folder.
	HMODULE dll = LoadLibraryExW((LPCWSTR)native_path.utf16().get_data(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (dll == nullptr) {
		const DWORD error = GetLastError();
		_set_status(vformat("cannot load %s (Windows error %d)", provider_path, (int64_t)error));
		ERR_PRINT("ANR FG: " + status);
		return false;
	}
	module = dll;

#define ANR_GETPROC(m_name) (void *)GetProcAddress(dll, m_name)
	fn_init_ext2 = ANR_GETPROC("NVSDK_NGX_VULKAN_Init_Ext2");
	fn_init_ext = ANR_GETPROC("NVSDK_NGX_VULKAN_Init_Ext");
	fn_populate = ANR_GETPROC("NVSDK_NGX_VULKAN_PopulateParameters_Impl");
	fn_create = ANR_GETPROC("NVSDK_NGX_VULKAN_CreateFeature");
	fn_create1 = ANR_GETPROC("NVSDK_NGX_VULKAN_CreateFeature1");
	fn_evaluate = ANR_GETPROC("NVSDK_NGX_VULKAN_EvaluateFeature");
	fn_release = ANR_GETPROC("NVSDK_NGX_VULKAN_ReleaseFeature");
	fn_shutdown1 = ANR_GETPROC("NVSDK_NGX_VULKAN_Shutdown1");
#undef ANR_GETPROC

	if ((fn_init_ext2 == nullptr && fn_init_ext == nullptr) || (fn_create == nullptr && fn_create1 == nullptr) || fn_evaluate == nullptr || fn_release == nullptr) {
		_set_status(provider_path + " lacks NGX's Vulkan exports (Init_Ext2, CreateFeature, EvaluateFeature, ReleaseFeature)");
		ERR_PRINT("ANR FG: " + status);
		module = nullptr;
		return false;
	}

	// The DLL's version resource.
	const Char16String wide_path = native_path.utf16();
	DWORD handle = 0;
	const DWORD info_size = GetFileVersionInfoSizeW((LPCWSTR)wide_path.get_data(), &handle);
	if (info_size > 0) {
		Vector<uint8_t> info;
		info.resize(info_size);
		if (GetFileVersionInfoW((LPCWSTR)wide_path.get_data(), 0, info_size, info.ptrw())) {
			VS_FIXEDFILEINFO *fixed = nullptr;
			UINT bytes = 0;
			if (VerQueryValueW(info.ptr(), L"\\", (void **)&fixed, &bytes) && fixed != nullptr) {
				provider_version = vformat("%d.%d.%d.%d", (int64_t)HIWORD(fixed->dwFileVersionMS), (int64_t)LOWORD(fixed->dwFileVersionMS),
						(int64_t)HIWORD(fixed->dwFileVersionLS), (int64_t)LOWORD(fixed->dwFileVersionLS));
			}
		}
	}
	if (provider_version.is_empty()) {
		provider_version = "unknown";
	}

	_set_status("loaded");
	print_line(vformat("ANR FG: loaded %s version %s", provider_path, provider_version));
	return true;
#else
	_set_status("not supported on this platform or rendering driver");
	return false;
#endif
}

bool AnrFrameGeneration::_initialize_device() {
	if (init_attempted) {
		return initialized;
	}
	if (!_load_provider()) {
		return false;
	}

#ifdef ANR_FG_SUPPORTED
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (rd == nullptr) {
		return false; // Not yet: tried again once the device exists.
	}
	init_attempted = true;
	if (rd->get_device_api_name() != "Vulkan") {
		_set_status("needs the Vulkan rendering driver (running " + rd->get_device_api_name() + ")");
		WARN_PRINT("ANR FG: " + status);
		return false;
	}

	VkInstance instance = (VkInstance)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_INSTANCE, RID(), 0);
	VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE, RID(), 0);
	VkDevice device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_DEVICE, RID(), 0);
	if (instance == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
		_set_status("no Vulkan device handles");
		ERR_PRINT("ANR FG: " + status);
		return false;
	}
	vk_device = (uint64_t)device;

	const String temp = OS::get_singleton()->get_temp_path().replace("/", "\\");
	const Char16String temp_wide = temp.utf16();
	ANR_NGX_Result result;
	if (fn_init_ext2 != nullptr) {
		result = ((PFN_ANR_NGX_VULKAN_Init_Ext2)fn_init_ext2)(ANR_FG_APP_ID, (const wchar_t *)temp_wide.get_data(), instance, physical_device, device,
				vkGetInstanceProcAddr, vkGetDeviceProcAddr, ANR_NGX_VERSION_API, nullptr);
	} else {
		result = ((PFN_ANR_NGX_VULKAN_Init_Ext)fn_init_ext)(ANR_FG_APP_ID, (const wchar_t *)temp_wide.get_data(), instance, physical_device, device,
				ANR_NGX_VERSION_API, nullptr);
	}
	last_result.store(result);
	if (anr_ngx_failed(result)) {
		_set_status("NGX Init_Ext2 failed: " + _ngx_result_text(result));
		ERR_PRINT("ANR FG: " + status);
		return false;
	}

	params = memnew(ParameterMap);
	if (fn_populate != nullptr) {
		((PFN_ANR_NGX_VULKAN_PopulateParameters)fn_populate)(params);
	}
	unsigned int max_count = 0;
	params->Get(ANR_DLSSG("MultiFrameCountMax"), &max_count);
	multi_frame_count_max = max_count;

	initialized = true;
	provider_used = true;
	_set_status("ready");
	print_line(vformat("ANR FG: NGX Vulkan initialized (result %s, MultiFrameCountMax %d).", _ngx_result_text(result), (int64_t)max_count));
	return true;
#else
	init_attempted = true;
	return false;
#endif
}

bool AnrFrameGeneration::_create_feature(void *p_command_buffer, const EvaluateParams &p_params) {
#ifdef ANR_FG_SUPPORTED
	ParameterMap *p = params;
	p->Set(ANR_NGX_CreationNodeMask, 1u);
	p->Set(ANR_NGX_VisibilityNodeMask, 1u);
	p->Set(ANR_NGX_Width, p_params.backbuffer.width);
	p->Set(ANR_NGX_Height, p_params.backbuffer.height);
	p->Set(ANR_DLSSG("Width"), p_params.backbuffer.width);
	p->Set(ANR_DLSSG("Height"), p_params.backbuffer.height);
	p->Set(ANR_DLSSG("BackbufferFormat"), p_params.backbuffer.format);
	p->Set(ANR_DLSSG("InternalWidth"), p_params.internal_width);
	p->Set(ANR_DLSSG("InternalHeight"), p_params.internal_height);
	p->Set(ANR_DLSSG("DynamicResolution"), 0u);

	ANR_NVSDK_NGX_Handle *handle = nullptr;
	ANR_NGX_Result result;
	if (fn_create1 != nullptr) {
		result = ((PFN_ANR_NGX_VULKAN_CreateFeature1)fn_create1)((VkDevice)vk_device, (VkCommandBuffer)p_command_buffer, ANR_NGX_FEATURE_FRAME_GENERATION, p, &handle);
	} else {
		result = ((PFN_ANR_NGX_VULKAN_CreateFeature)fn_create)((VkCommandBuffer)p_command_buffer, ANR_NGX_FEATURE_FRAME_GENERATION, p, &handle);
	}
	last_result.store(result);
	if (anr_ngx_failed(result) || handle == nullptr) {
		_set_status("NGX CreateFeature (frame generation) failed: " + _ngx_result_text(result));
		ERR_PRINT("ANR FG: " + status);
		feature_failed = true;
		return false;
	}

	feature = handle;
	feature_width = p_params.backbuffer.width;
	feature_height = p_params.backbuffer.height;
	feature_format = p_params.backbuffer.format;
	feature_internal_width = p_params.internal_width;
	feature_internal_height = p_params.internal_height;
	_set_status("running");
	print_line(vformat("ANR FG: feature created %dx%d (render %dx%d, backbuffer VkFormat %d, result %s).", (int64_t)feature_width, (int64_t)feature_height,
			(int64_t)feature_internal_width, (int64_t)feature_internal_height, (int64_t)feature_format, _ngx_result_text(result)));
	return true;
#else
	return false;
#endif
}

void AnrFrameGeneration::_release_feature() {
#ifdef ANR_FG_SUPPORTED
	if (feature == nullptr) {
		return;
	}
	// The provider's resources may still be in use by frames in flight.
	if (vk_device != 0) {
		PFN_vkDeviceWaitIdle wait_idle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr((VkDevice)vk_device, "vkDeviceWaitIdle");
		if (wait_idle != nullptr) {
			wait_idle((VkDevice)vk_device);
		}
	}
	const ANR_NGX_Result result = ((PFN_ANR_NGX_VULKAN_ReleaseFeature)fn_release)((ANR_NVSDK_NGX_Handle *)feature);
	print_verbose("ANR FG: feature released (" + _ngx_result_text(result) + ").");
	feature = nullptr;
#endif
}

#ifdef ANR_FG_SUPPORTED
static void _fill_resource(ANR_NVSDK_NGX_Resource_VK &r_resource, const AnrFrameGeneration::Image &p_image, bool p_read_write) {
	memset(&r_resource, 0, sizeof(r_resource));
	r_resource.Type = ANR_NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
	r_resource.ReadWrite = p_read_write;
	ANR_NVSDK_NGX_ImageViewInfo_VK &info = r_resource.Resource.ImageViewInfo;
	info.ImageView = (VkImageView)p_image.view;
	info.Image = (VkImage)p_image.image;
	info.SubresourceRange.aspectMask = p_image.depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	info.SubresourceRange.baseMipLevel = 0;
	info.SubresourceRange.levelCount = 1;
	info.SubresourceRange.baseArrayLayer = 0;
	info.SubresourceRange.layerCount = 1;
	info.Format = (VkFormat)p_image.format;
	info.Width = p_image.width;
	info.Height = p_image.height;
}
#endif

bool AnrFrameGeneration::evaluate(void *p_command_buffer, const EvaluateParams &p_params) {
	MutexLock lock(mutex);
	if (!_initialize_device()) {
		return false;
	}
#ifdef ANR_FG_SUPPORTED
	ERR_FAIL_NULL_V(p_command_buffer, false);
	ERR_FAIL_COND_V(!p_params.backbuffer.is_valid() || !p_params.depth.is_valid() || !p_params.motion.is_valid() || !p_params.output_interpolated.is_valid(), false);

	bool reset = p_params.reset;
	if (feature != nullptr && (feature_width != p_params.backbuffer.width || feature_height != p_params.backbuffer.height || feature_format != p_params.backbuffer.format ||
										feature_internal_width != p_params.internal_width || feature_internal_height != p_params.internal_height)) {
		_release_feature();
		feature_failed = false;
	}
	if (feature == nullptr) {
		if (feature_failed) {
			return false; // Not retried every frame; a size change retries.
		}
		if (!_create_feature(p_command_buffer, p_params)) {
			return false;
		}
		reset = true;
	}

	ANR_NVSDK_NGX_Resource_VK backbuffer, hudless, depth, motion, interpolated, real;
	_fill_resource(backbuffer, p_params.backbuffer, false);
	_fill_resource(depth, p_params.depth, false);
	_fill_resource(motion, p_params.motion, false);
	_fill_resource(interpolated, p_params.output_interpolated, true);

	ParameterMap *p = params;
	p->Set(ANR_NGX_Width, p_params.backbuffer.width);
	p->Set(ANR_NGX_Height, p_params.backbuffer.height);
	p->Set(ANR_DLSSG("Width"), p_params.backbuffer.width);
	p->Set(ANR_DLSSG("Height"), p_params.backbuffer.height);
	p->Set(ANR_DLSSG("BackbufferFormat"), p_params.backbuffer.format);
	p->Set(ANR_DLSSG("InternalWidth"), p_params.internal_width);
	p->Set(ANR_DLSSG("InternalHeight"), p_params.internal_height);

	p->Set(ANR_DLSSG("Backbuffer"), (void *)&backbuffer);
	if (p_params.hudless.is_valid()) {
		_fill_resource(hudless, p_params.hudless, false);
		p->Set(ANR_DLSSG("HUDLess"), (void *)&hudless);
	} else {
		p->erase(ANR_DLSSG("HUDLess"));
	}
	p->Set(ANR_DLSSG("Depth"), (void *)&depth);
	p->Set(ANR_DLSSG("MVecs"), (void *)&motion);
	p->Set(ANR_DLSSG("OutputInterpolated"), (void *)&interpolated);
	if (p_params.output_real.is_valid()) {
		_fill_resource(real, p_params.output_real, true);
		p->Set(ANR_DLSSG("OutputReal"), (void *)&real);
	} else {
		p->erase(ANR_DLSSG("OutputReal"));
	}

	// Whole textures: the subrects are their sizes.
	p->Set(ANR_DLSSG("BackbufferSubrectBaseX"), 0u);
	p->Set(ANR_DLSSG("BackbufferSubrectBaseY"), 0u);
	p->Set(ANR_DLSSG("BackbufferSubrectWidth"), p_params.backbuffer.width);
	p->Set(ANR_DLSSG("BackbufferSubrectHeight"), p_params.backbuffer.height);
	p->Set(ANR_DLSSG("DepthSubrectBaseX"), 0u);
	p->Set(ANR_DLSSG("DepthSubrectBaseY"), 0u);
	p->Set(ANR_DLSSG("DepthSubrectWidth"), p_params.internal_width);
	p->Set(ANR_DLSSG("DepthSubrectHeight"), p_params.internal_height);
	p->Set(ANR_DLSSG("MVecsSubrectBaseX"), 0u);
	p->Set(ANR_DLSSG("MVecsSubrectBaseY"), 0u);
	p->Set(ANR_DLSSG("MVecsSubrectWidth"), p_params.internal_width);
	p->Set(ANR_DLSSG("MVecsSubrectHeight"), p_params.internal_height);

	p->Set(ANR_DLSSG("Reset"), reset ? 1u : 0u);
	p->Set(ANR_DLSSG("MultiFrameCount"), 1u);
	p->Set(ANR_DLSSG("MultiFrameIndex"), 1u);
	p->Set(ANR_DLSSG("NotRenderingGameFrames"), 0u);
	p->Set(ANR_DLSSG("ColorBuffersHDR"), p_params.hdr ? 1u : 0u);
	p->Set(ANR_DLSSG("DepthInverted"), p_params.depth_inverted ? 1u : 0u);
	p->Set(ANR_DLSSG("MvecJittered"), p_params.motion_jittered ? 1u : 0u);
	p->Set(ANR_DLSSG("MvecDilated"), 0u);
	p->Set(ANR_DLSSG("CameraMotionIncluded"), 1u);
	p->Set(ANR_DLSSG("OrthoProjection"), 0u);
	p->Set(ANR_DLSSG("JitterOffsetX"), p_params.jitter[0]);
	p->Set(ANR_DLSSG("JitterOffsetY"), p_params.jitter[1]);
	p->Set(ANR_DLSSG("MvecScaleX"), p_params.mvec_scale[0]);
	p->Set(ANR_DLSSG("MvecScaleY"), p_params.mvec_scale[1]);
	p->Set(ANR_DLSSG("CameraNear"), p_params.z_near);
	p->Set(ANR_DLSSG("CameraFar"), p_params.z_far);
	p->Set(ANR_DLSSG("CameraFOV"), p_params.fov);
	p->Set(ANR_DLSSG("CameraAspectRatio"), p_params.aspect);
	p->Set(ANR_DLSSG("CameraPinholeOffsetX"), 0.0f);
	p->Set(ANR_DLSSG("CameraPinholeOffsetY"), 0.0f);
	static const char *axes[3] = { "X", "Y", "Z" };
	for (int i = 0; i < 3; i++) {
		p->Set((String(ANR_DLSSG("CameraPos")) + axes[i]).utf8().get_data(), p_params.camera_pos[i]);
		p->Set((String(ANR_DLSSG("CameraUp")) + axes[i]).utf8().get_data(), p_params.camera_up[i]);
		p->Set((String(ANR_DLSSG("CameraRight")) + axes[i]).utf8().get_data(), p_params.camera_right[i]);
		p->Set((String(ANR_DLSSG("CameraFwd")) + axes[i]).utf8().get_data(), p_params.camera_fwd[i]);
	}

	// The provider reads the matrices during the call: this storage outlives it.
	static float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	p->Set(ANR_DLSSG("CameraViewToClip"), (void *)p_params.view_to_clip);
	p->Set(ANR_DLSSG("ClipToCameraView"), (void *)p_params.clip_to_view);
	p->Set(ANR_DLSSG("ClipToLensClip"), (void *)identity);
	p->Set(ANR_DLSSG("ClipToPrevClip"), (void *)p_params.clip_to_prev_clip);
	p->Set(ANR_DLSSG("PrevClipToClip"), (void *)p_params.prev_clip_to_clip);

	const ANR_NGX_Result result = ((PFN_ANR_NGX_VULKAN_EvaluateFeature)fn_evaluate)((VkCommandBuffer)p_command_buffer, (const ANR_NVSDK_NGX_Handle *)feature, p, nullptr);
	last_result.store(result);

	// Pointers into this stack frame must not outlive the call.
	p->erase(ANR_DLSSG("Backbuffer"));
	p->erase(ANR_DLSSG("HUDLess"));
	p->erase(ANR_DLSSG("Depth"));
	p->erase(ANR_DLSSG("MVecs"));
	p->erase(ANR_DLSSG("OutputInterpolated"));
	p->erase(ANR_DLSSG("OutputReal"));

	if (anr_ngx_failed(result)) {
		const int64_t failures = failed_frames.fetch_add(1) + 1;
		_set_status("NGX EvaluateFeature failed: " + _ngx_result_text(result));
		if (failures <= 5 || (failures % 600) == 0) {
			ERR_PRINT(vformat("ANR FG: EvaluateFeature failed with %s (%d failures so far).", _ngx_result_text(result), failures));
		}
		return false;
	}

	const int64_t count = evaluated_frames.fetch_add(1) + 1;
	if (count == 1) {
		print_line(vformat("ANR FG: first frame generated (EvaluateFeature %s).", _ngx_result_text(result)));
	}
	return true;
#else
	return false;
#endif
}

void AnrFrameGeneration::note_generated_frame_presented() {
	const int64_t count = presented_frames.fetch_add(1) + 1;
	if (count == 1 || (count % 1000) == 0) {
		print_line(vformat("ANR FG: %d generated frames presented.", count));
	}
}

void AnrFrameGeneration::finish() {
	MutexLock lock(mutex);
#ifdef ANR_FG_SUPPORTED
	if (initialized) {
		// GODOT_ANR_FG_EXIT: "full" releases the feature and shuts NGX down, "release" only
		// releases the feature, "none" leaves both to the process exit.
		static const String exit_mode = OS::get_singleton()->get_environment("GODOT_ANR_FG_EXIT");
		const bool release = exit_mode != "none";
		const bool shutdown = exit_mode.is_empty() || exit_mode == "full";
		print_line(vformat("ANR FG: shutting down (release %s, NGX shutdown %s).", release ? "yes" : "no", shutdown ? "yes" : "no"));
		fflush(stdout);
		if (release) {
			_release_feature();
		}
		print_line("ANR FG: feature released.");
		fflush(stdout);
		if (shutdown && fn_shutdown1 != nullptr && vk_device != 0) {
			const ANR_NGX_Result result = ((PFN_ANR_NGX_VULKAN_Shutdown1)fn_shutdown1)((VkDevice)vk_device);
			print_line("ANR FG: NGX shut down (" + _ngx_result_text(result) + ").");
			fflush(stdout);
		}
		initialized = false;
	}
#endif
	if (params != nullptr) {
		memdelete(params);
		params = nullptr;
	}
	vk_device = 0;
	init_attempted = false;
	feature_failed = false;
}

bool AnrFrameGeneration::is_available() {
	MutexLock lock(mutex);
	if (!_load_provider()) {
		return false;
	}
	return _initialize_device();
}

String AnrFrameGeneration::get_status() const {
	MutexLock lock(mutex);
	return status;
}

String AnrFrameGeneration::get_provider_path() const {
	MutexLock lock(mutex);
	return provider_path.is_empty() ? resolve_provider_path() : provider_path;
}

String AnrFrameGeneration::get_provider_version() const {
	MutexLock lock(mutex);
	return provider_version;
}

int AnrFrameGeneration::get_max_generated_frames() const {
	MutexLock lock(mutex);
	return (int)multi_frame_count_max;
}
