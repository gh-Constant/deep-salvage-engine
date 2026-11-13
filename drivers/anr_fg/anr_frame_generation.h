/**************************************************************************/
/*  anr_frame_generation.h                                                */
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

#pragma once

#include "core/object/class_db.h" // IWYU pragma: keep
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"

#include <atomic>

// ANR FG (Apex Neural Rendering frame generation) driven natively: the engine loads the provider's
// nvngx_dlssg.dll itself and calls its NGX Vulkan exports (Init_Ext2, CreateFeature1, EvaluateFeature)
// as a game's NGX host would, with no OptiScaler, NVIDIA driver or Streamline in between. It runs on
// any Vulkan GPU the provider supports (AMD RX 5000 and later, Intel Arc, NVIDIA).
//
// The renderer (servers/rendering/renderer_rd/effects/anr_fg_effect.*) gathers the inputs and calls
// evaluate() from a RenderingDevice driver callback; the Vulkan driver presents the generated frame
// before the real one (RenderingDeviceDriver::swap_chain_set_generated_frame).
class AnrFrameGeneration : public Object {
	GDCLASS(AnrFrameGeneration, Object);

public:
	// A Vulkan image the provider reads or writes (handles as integers: this header is API-neutral).
	struct Image {
		uint64_t image = 0; // VkImage
		uint64_t view = 0; // VkImageView
		uint32_t format = 0; // VkFormat
		uint32_t width = 0;
		uint32_t height = 0;
		bool depth = false; // Depth aspect.
		bool is_valid() const { return image != 0 && view != 0; }
	};

	struct EvaluateParams {
		Image backbuffer; // Final color with the UI, at output size. Required.
		Image hudless; // Final color without the UI (optional).
		Image depth; // Render-size depth. Required.
		Image motion; // Render-size motion vectors (RG16F). Required.
		Image output_interpolated; // Generated frame, written by the provider.
		Image output_real; // The real frame as the provider passes it on (optional, unused by the engine).

		uint32_t internal_width = 0; // The render size this frame (depth and motion are this size).
		uint32_t internal_height = 0;
		uint64_t frame = 0; // The compositor's frame number.
		uint32_t frames_in_flight = 3; // Frames the GPU may still be working on.
		bool reset = false;
		bool depth_inverted = true;
		bool hdr = false;
		bool motion_jittered = false;
		float jitter[2] = {}; // Render pixels.
		float mvec_scale[2] = { 1.0f, 1.0f }; // Stored motion times this: pixels of the motion texture.
		float z_near = 0.05f;
		float z_far = 4000.0f;
		float fov = 1.0f; // Vertical, radians.
		float aspect = 1.0f;
		float camera_pos[3] = {};
		float camera_up[3] = {};
		float camera_right[3] = {};
		float camera_fwd[3] = {};
		// 4x4, DLSS-G layout as ANR's own NGX host gives them: the row-vector matrix stored row by
		// row (translation in the last row, w taken from element 11), clip space matching the depth
		// buffer it is paired with.
		float view_to_clip[16] = {};
		float clip_to_view[16] = {};
		float clip_to_prev_clip[16] = {};
		float prev_clip_to_clip[16] = {};
	};

private:
	static AnrFrameGeneration *singleton;
	static bool provider_used;

	mutable Mutex mutex;

	// Provider DLL.
	bool load_attempted = false;
	void *module = nullptr; // HMODULE
	String provider_path;
	String provider_version;
	void *fn_init_ext2 = nullptr;
	void *fn_init_ext = nullptr;
	void *fn_populate = nullptr;
	void *fn_create = nullptr;
	void *fn_create1 = nullptr;
	void *fn_evaluate = nullptr;
	void *fn_release = nullptr;
	void *fn_shutdown1 = nullptr;

	// NGX state on the engine's device.
	bool init_attempted = false;
	bool initialized = false;
	uint64_t vk_device = 0;
	class ParameterMap *params = nullptr;
	void *feature = nullptr; // ANR_NVSDK_NGX_Handle *
	uint32_t feature_width = 0;
	uint32_t feature_height = 0;
	uint32_t feature_format = 0;
	uint32_t feature_internal_width = 0;
	uint32_t feature_internal_height = 0;
	uint32_t multi_frame_count_max = 0;
	bool feature_failed = false;
	bool feature_dynamic = false; // Created with DLSSG.DynamicResolution: any render size up to the internal one.

	// Features replaced while frames using them may still be in flight: released once the GPU is
	// past them (no device-wide wait, which would race the transfer queues' submissions).
	struct RetiredFeature {
		void *handle = nullptr;
		uint64_t release_frame = 0;
	};
	LocalVector<RetiredFeature> retired_features;
	uint64_t last_frame = 0;
	bool inactive = false; // Evaluation skipped (status says why).

	String status = "not loaded";
	std::atomic<int64_t> evaluated_frames = { 0 };
	std::atomic<int64_t> presented_frames = { 0 };
	std::atomic<int64_t> failed_frames = { 0 };
	std::atomic<int64_t> last_result = { 0 };

	bool _load_provider();
	bool _initialize_device();
	bool _create_feature(void *p_command_buffer, const EvaluateParams &p_params);
	void _retire_feature(uint64_t p_frame, uint32_t p_frames_in_flight);
	void _release_retired_features(bool p_all);
	void _set_status(const String &p_status);

protected:
	static void _bind_methods();

public:
	static AnrFrameGeneration *get_singleton();
	// The provider was initialized on a device in this process (it must not be detached at exit).
	static bool provider_was_used() { return provider_used; }
	static void register_singleton();

	// The folder or DLL path the provider is loaded from: GODOT_ANR_FG_PATH, else the project
	// setting rendering/anr_frame_generation/provider_path, else <executable folder>/anr_fg/.
	static String resolve_provider_path();

	// Script API.
	bool is_available();
	String get_status() const;
	String get_provider_path() const;
	String get_provider_version() const;
	int64_t get_evaluated_frames() const { return evaluated_frames.load(); }
	int64_t get_presented_frames() const { return presented_frames.load(); }
	int64_t get_failed_frames() const { return failed_frames.load(); }
	int64_t get_last_result() const { return last_result.load(); }
	int get_max_generated_frames() const;

	// Engine API (rendering thread).
	// Records the provider's work into p_command_buffer (a VkCommandBuffer being recorded). Inputs
	// must be in SHADER_READ_ONLY_OPTIMAL (or GENERAL), outputs in GENERAL. Returns true when the
	// interpolated frame was written.
	bool evaluate(void *p_command_buffer, const EvaluateParams &p_params);
	void note_generated_frame_presented();
	// Frame generation is on but cannot run (for example the window's swap chain cannot take a
	// generated frame): get_status() says why. An empty reason clears it.
	void set_inactive_reason(const String &p_reason);
	// Releases replaced features the GPU is done with (called once per frame).
	void process_frame(uint64_t p_frame);
	// Releases the features and shuts NGX down. The GPU must be idle and no queue submitting
	// (RenderingDevice::finalize calls it after stalling for every frame and transfer worker).
	void finish();

	AnrFrameGeneration();
	~AnrFrameGeneration();
};
