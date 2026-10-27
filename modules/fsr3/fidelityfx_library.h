/**************************************************************************/
/*  fidelityfx_library.h                                                 */
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
#include "thirdparty/amd-fidelityfx-api/ffx_api.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fidelityfx {
struct Provider {
	uint64_t id = 0;
	std::string name;
};
// A module must outlive every context created through its entry points.
// No global SDK linkage, hard-coded provider IDs or automatic downloads.
class Library {
	void *module = nullptr;
#ifdef _WIN32
	void *directory_cookie = nullptr;
#endif
	std::string error_message;
	std::filesystem::path loaded_path;

public:
	PfnFfxCreateContext create_context = nullptr;
	PfnFfxDestroyContext destroy_context = nullptr;
	PfnFfxConfigure configure = nullptr;
	PfnFfxQuery query = nullptr;
	PfnFfxDispatch dispatch = nullptr;
	Library() = default;
	Library(const Library &) = delete;
	Library &operator=(const Library &) = delete;
	~Library();
	bool open(const std::filesystem::path &absolute_file);
	void close();
	bool versions(std::vector<Provider> &out, void *device = nullptr);
	const std::string &error() const { return error_message; }
	const std::filesystem::path &path() const { return loaded_path; }
	bool is_open() const { return module != nullptr; }
};
} //namespace fidelityfx
