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
}
