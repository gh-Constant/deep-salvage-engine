/**************************************************************************/
/*  anr_fg_ngx.h                                                          */
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

// The minimal binary interface of NVIDIA's NGX (DLSS SDK nvsdk_ngx*.h) that a host needs to drive a
// DLSS frame generation provider DLL (nvngx_dlssg.dll) directly through its Vulkan exports, without
// NVIDIA's NGX core, driver or Streamline. Declared here because the engine does not vendor the DLSS
// SDK. The layouts below must match the SDK's exactly: the provider reads them.
//
// Only included by anr_frame_generation.cpp, after the Vulkan headers.

#include <cstdint>

#if defined(_WIN32)
#define ANR_NGX_CONV __cdecl
#else
#define ANR_NGX_CONV
#endif

typedef enum ANR_NVSDK_NGX_Result {
	ANR_NGX_Result_Success = 0x1,
	ANR_NGX_Result_Fail = 0xBAD00000,
} ANR_NVSDK_NGX_Result_Enum;

typedef uint32_t ANR_NGX_Result;

static inline bool anr_ngx_failed(ANR_NGX_Result p_result) {
	return (p_result & 0xFFF00000u) == (uint32_t)ANR_NGX_Result_Fail;
}

// NVSDK_NGX_Version_API of the DLSS 3.x/4.x SDKs.
static constexpr uint32_t ANR_NGX_VERSION_API = 0x0000015;
// NVSDK_NGX_Feature_FrameGeneration.
static constexpr uint32_t ANR_NGX_FEATURE_FRAME_GENERATION = 11;

struct ID3D11Resource;
struct ID3D12Resource;

// NVSDK_NGX_Parameter: an abstract class the host implements and the provider calls through its
// vtable. The order of the virtual functions is the SDK's (MSVC lays out overloads by declaration).
struct ANR_NVSDK_NGX_Parameter {
	virtual void Set(const char *p_name, unsigned long long p_value) = 0;
	virtual void Set(const char *p_name, float p_value) = 0;
	virtual void Set(const char *p_name, double p_value) = 0;
	virtual void Set(const char *p_name, unsigned int p_value) = 0;
	virtual void Set(const char *p_name, int p_value) = 0;
	virtual void Set(const char *p_name, ID3D11Resource *p_value) = 0;
	virtual void Set(const char *p_name, ID3D12Resource *p_value) = 0;
	virtual void Set(const char *p_name, void *p_value) = 0;

	virtual ANR_NGX_Result Get(const char *p_name, unsigned long long *r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, float *r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, double *r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, unsigned int *r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, int *r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, ID3D11Resource **r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, ID3D12Resource **r_value) const = 0;
	virtual ANR_NGX_Result Get(const char *p_name, void **r_value) const = 0;

	virtual void Reset() = 0;
};

struct ANR_NVSDK_NGX_Handle {
	unsigned int id;
};

#ifdef VK_VERSION_1_0
// NVSDK_NGX_Resource_VK (nvsdk_ngx_defs.h): an image view with its image, given to the provider by
// pointer through Set(name, void *).
struct ANR_NVSDK_NGX_ImageViewInfo_VK {
	VkImageView ImageView;
	VkImage Image;
	VkImageSubresourceRange SubresourceRange;
	VkFormat Format;
	unsigned int Width;
	unsigned int Height;
};

struct ANR_NVSDK_NGX_BufferInfo_VK {
	VkBuffer Buffer;
	unsigned int SizeInBytes;
};

enum ANR_NVSDK_NGX_Resource_VK_Type {
	ANR_NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
	ANR_NVSDK_NGX_RESOURCE_VK_TYPE_VK_BUFFER,
};

struct ANR_NVSDK_NGX_Resource_VK {
	union {
		ANR_NVSDK_NGX_ImageViewInfo_VK ImageViewInfo;
		ANR_NVSDK_NGX_BufferInfo_VK BufferInfo;
	} Resource;
	ANR_NVSDK_NGX_Resource_VK_Type Type;
	bool ReadWrite;
};

typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_Init_Ext2)(unsigned long long p_app_id, const wchar_t *p_app_data_path,
		VkInstance p_instance, VkPhysicalDevice p_physical_device, VkDevice p_device,
		PFN_vkGetInstanceProcAddr p_gipa, PFN_vkGetDeviceProcAddr p_gdpa, uint32_t p_sdk_version, const void *p_feature_info);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_Init_Ext)(unsigned long long p_app_id, const wchar_t *p_app_data_path,
		VkInstance p_instance, VkPhysicalDevice p_physical_device, VkDevice p_device, uint32_t p_sdk_version, const void *p_feature_info);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_PopulateParameters)(ANR_NVSDK_NGX_Parameter *p_params);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_CreateFeature)(VkCommandBuffer p_cmd, uint32_t p_feature,
		ANR_NVSDK_NGX_Parameter *p_params, ANR_NVSDK_NGX_Handle **r_handle);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_CreateFeature1)(VkDevice p_device, VkCommandBuffer p_cmd, uint32_t p_feature,
		ANR_NVSDK_NGX_Parameter *p_params, ANR_NVSDK_NGX_Handle **r_handle);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_EvaluateFeature)(VkCommandBuffer p_cmd, const ANR_NVSDK_NGX_Handle *p_handle,
		const ANR_NVSDK_NGX_Parameter *p_params, void *p_progress_callback);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_ReleaseFeature)(ANR_NVSDK_NGX_Handle *p_handle);
typedef ANR_NGX_Result(ANR_NGX_CONV *PFN_ANR_NGX_VULKAN_Shutdown1)(VkDevice p_device);
#endif

// NGX parameter names (nvsdk_ngx_defs.h, nvsdk_ngx_defs_dlssg.h), as the provider reads them.
#define ANR_NGX_CreationNodeMask "CreationNodeMask"
#define ANR_NGX_VisibilityNodeMask "VisibilityNodeMask"
#define ANR_NGX_Width "Width"
#define ANR_NGX_Height "Height"
#define ANR_DLSSG(m_name) "DLSSG." m_name
