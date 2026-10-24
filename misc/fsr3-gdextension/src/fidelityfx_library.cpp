#include "fidelityfx_library.h"
#include "thirdparty/amd-fidelityfx-api/ffx_upscale.h"
#include <array>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <fstream>
#endif

namespace fidelityfx {
Library::~Library() { close(); }
void Library::close() {
    create_context = nullptr; destroy_context = nullptr; configure = nullptr;
    query = nullptr; dispatch = nullptr;
    if (module) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(module));
#else
        dlclose(module);
#endif
    }
    module = nullptr;
#ifdef _WIN32
    if (directory_cookie) RemoveDllDirectory(directory_cookie);
    directory_cookie = nullptr;
#endif
    loaded_path.clear();
}
bool Library::open(const std::filesystem::path &absolute_file) {
    close(); error_message.clear();
    if (!absolute_file.is_absolute()) {
        error_message = "FidelityFX library path must be absolute"; return false;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(absolute_file, ec)) {
        error_message = "FidelityFX library not found: " + absolute_file.u8string(); return false;
    }
#ifdef _WIN32
    // Restrict module/dependency lookup to this directory and Windows' safe
    // defaults. Keep the directory registered for the loader's delayed providers.
    directory_cookie = AddDllDirectory(absolute_file.parent_path().c_str());
    module = LoadLibraryExW(absolute_file.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (!module) {
        error_message = "LoadLibraryExW failed (Windows error " + std::to_string(GetLastError()) + "): " + absolute_file.u8string();
        close(); return false;
    }
    auto symbol = [this](const char *name) { return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(module), name)); };
#else
    // Native Linux cannot execute PE/Windows DLLs. Diagnose this before dlopen.
    std::ifstream file(absolute_file, std::ios::binary);
    std::array<char, 4> magic{}; file.read(magic.data(), magic.size());
    if (magic[0] == 'M' && magic[1] == 'Z') {
        error_message = "Windows PE DLL cannot be loaded by native Linux; use the source FSR 3.1.4 backend"; return false;
    }
    module = dlopen(absolute_file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) { const char *detail = dlerror(); error_message = detail ? detail : "dlopen failed"; return false; }
    auto symbol = [this](const char *name) { return dlsym(module, name); };
#endif
    create_context = reinterpret_cast<PfnFfxCreateContext>(symbol("ffxCreateContext"));
    destroy_context = reinterpret_cast<PfnFfxDestroyContext>(symbol("ffxDestroyContext"));
    configure = reinterpret_cast<PfnFfxConfigure>(symbol("ffxConfigure"));
    query = reinterpret_cast<PfnFfxQuery>(symbol("ffxQuery"));
    dispatch = reinterpret_cast<PfnFfxDispatch>(symbol("ffxDispatch"));
    if (!create_context || !destroy_context || !configure || !query || !dispatch) {
        error_message = "Library does not export all five required FidelityFX API functions";
        close(); return false;
    }
    loaded_path = absolute_file;
    return true;
}
bool Library::versions(std::vector<Provider> &out, void *device) {
    out.clear();
    if (!query) { error_message = "FidelityFX library is not open"; return false; }
    uint64_t count = 0;
    ffxQueryDescGetVersions description{};
    description.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
    description.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    description.device = device;
    description.outputCount = &count;
    auto result = query(nullptr, &description.header);
    if (result != FFX_API_RETURN_OK || count > 256) {
        error_message = "FidelityFX version query failed or returned an unreasonable provider count"; return false;
    }
    if (!count) { error_message = "No upscaler provider is available for this library/device"; return false; }
    std::vector<uint64_t> ids(count);
    std::vector<const char *> names(count, nullptr);
    description.versionIds = ids.data(); description.versionNames = names.data();
    result = query(nullptr, &description.header);
    if (result != FFX_API_RETURN_OK || count > ids.size()) {
        error_message = "FidelityFX provider enumeration failed"; return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (names[i] && *names[i]) out.push_back({ids[i], names[i]});
    }
    if (out.empty()) { error_message = "FidelityFX returned no named providers"; return false; }
    return true;
}
}
