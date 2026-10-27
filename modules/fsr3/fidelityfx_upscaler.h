/**************************************************************************/
/*  fidelityfx_upscaler.h                                                 */
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
#include "upscalers.h"

#include "core/variant/variant.h"

#include <mutex>

struct FidelityFXBinaryState;
struct FidelityFXBinaryStats;

// The public AMD ABI supplies the algorithm. FSR3Upscaler remains the native
// source fallback, and owns the common jitter/sharpness/history configuration.
class FidelityFXUpscaler : public FSR3Upscaler {
	GDCLASS(FidelityFXUpscaler, FSR3Upscaler)
	mutable std::mutex settings_mutex;
	String library_path;
	String preferred_version;
	bool allow_source_fallback = true;
	std::atomic<uint64_t> settings_revision{ 1 };
	std::atomic<uint64_t> binary_reset_serial{ 1 };
	uint64_t applied_revision = 0;
	bool initialized = false;
	std::unique_ptr<FidelityFXBinaryState> binary;
	std::shared_ptr<FidelityFXBinaryStats> stats;

protected:
	static void _bind_methods();

public:
	FidelityFXUpscaler();
	~FidelityFXUpscaler();
	void render_callback(const RenderData *data) override;
	void reset_history() override;
	int64_t get_dispatch_count() const override;
	int64_t get_failure_count() const override;
	void set_library_path(const String &value);
	String get_library_path() const;
	void set_preferred_version(const String &value);
	String get_preferred_version() const;
	void set_allow_source_fallback(bool value);
	bool get_allow_source_fallback() const;
	String get_active_backend() const;
	String get_active_version() const;
	String get_binary_error() const;
	PackedStringArray get_available_versions() const;
	int64_t get_binary_dispatch_count() const;
	int64_t get_binary_failure_count() const;
};
