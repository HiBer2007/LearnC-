#include "PluginLoader.h"
#include "logger.h"
#include "plugin_log_sink.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>

static std::string getLastWindowsError()
{
    DWORD err = GetLastError();
    if (err == 0) return "unknown error";
    LPSTR msgBuf = nullptr;
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msgBuf, 0, nullptr);
    std::string result;
    if (msgBuf && len > 0) {
        result = std::string(msgBuf, len);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        LocalFree(msgBuf);
    } else {
        result = "error code " + std::to_string(err);
    }
    return result + " (0x" + []{
        std::stringstream ss;
        ss << std::hex << GetLastError();
        return ss.str();
    }() + ")";
}

static void* loadNativeLibrary(const std::string& path)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len <= 0) return nullptr;
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);
    void* handle = (void*)LoadLibraryW(wpath.c_str());
    if (!handle) {
        CLogger::Error("LoadLibrary failed for '{}': {}", path,
            getLastWindowsError());
    }
    return handle;
}

static void* getNativeProc(void* handle, const char* name)
{
    void* proc = (void*)GetProcAddress((HMODULE)handle, name);
    if (!proc) {
        CLogger::Error("GetProcAddress '{}' failed: {}", name,
            getLastWindowsError());
    }
    return proc;
}

static void freeNativeLibrary(void* handle)
{
    if (handle) {
        FreeLibrary((HMODULE)handle);
    }
}

#define LOAD_LIBRARY(path)    loadNativeLibrary(path)
#define GET_PROC(handle, name) getNativeProc(handle, name)
#define FREE_LIBRARY(handle)  freeNativeLibrary(handle)

#else
#include <dlfcn.h>

static void* loadNativeLibrary(const std::string& path)
{
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        CLogger::Error("dlopen failed for '{}': {}", path, dlerror());
    }
    return handle;
}

static void* getNativeProc(void* handle, const char* name)
{
    void* proc = dlsym(handle, name);
    if (!proc) {
        CLogger::Error("dlsym '{}' failed: {}", name, dlerror());
    }
    return proc;
}

static void freeNativeLibrary(void* handle)
{
    if (handle) dlclose(handle);
}

#define LOAD_LIBRARY(path)    loadNativeLibrary(path)
#define GET_PROC(handle, name) getNativeProc(handle, name)
#define FREE_LIBRARY(handle)  freeNativeLibrary(handle)
#endif

#include <nlohmann/json.hpp>

namespace NeoCore {

namespace fs = std::filesystem;

PluginLoader::PluginLoader() {}
PluginLoader::~PluginLoader()
{
    std::vector<void*> handles;
    for (auto& p : owned_parsers_) {
        handles.push_back(p.handle);
    }
    owned_parsers_.clear();
    for (auto* h : handles) {
        FREE_LIBRARY(h);
    }
}

void PluginLoader::ScanDirectory(const std::string& parsersDir)
{
    if (!fs::exists(parsersDir) || !fs::is_directory(parsersDir)) {
        CLogger::Warn("Plugin directory not found: '{}'", parsersDir);
        return;
    }

    int loadedCount = 0;
    for (const auto& entry : fs::directory_iterator(parsersDir)) {
        if (!entry.is_regular_file()) continue;

        auto path = entry.path();
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".json") {
            // meta 命名: NeoParser_XXX.meta.json -> DLL: NeoParser_XXX.dll
            const std::string fname = path.filename().string();
            const std::string metaSuffix = ".meta.json";
            if (fname.size() < metaSuffix.size()
                || fname.compare(fname.size() - metaSuffix.size(),
                    metaSuffix.size(), metaSuffix) != 0) {
                continue;
            }
            auto dllPath = path.parent_path()
                / (fname.substr(0, fname.size() - metaSuffix.size()) + ".dll");
            if (fs::exists(dllPath)) {
                if (LoadPlugin(dllPath.string(), path.string())) {
                    ++loadedCount;
                }
            } else {
                auto dllName = dllPath.filename().string();
                CLogger::Error("Plugin meta exists but DLL missing: '{}' "
                    "(expected: '{}')", path.filename().string(), dllName);
            }
        }
    }

    CLogger::Info("Loaded {} parsers from '{}'", loadedCount, parsersDir);
}

bool PluginLoader::LoadPlugin(const std::string& dllPath,
    const std::string& metaPath)
{
    std::ifstream metaFile(metaPath);
    if (!metaFile.is_open()) {
        CLogger::Error("Cannot open meta file: '{}'", metaPath);
        return false;
    }

    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(metaFile);
    } catch (const std::exception& e) {
        CLogger::Error("Failed to parse meta file '{}': {}", metaPath,
            e.what());
        return false;
    }

    void* handle = LOAD_LIBRARY(dllPath.c_str());
    if (!handle) return false;

    auto createFunc = reinterpret_cast<CreateParserFunc>(
        GET_PROC(handle, "CreateParser"));
    if (!createFunc) {
        FREE_LIBRARY(handle);
        return false;
    }

    // 插件日志注入 (可选): 插件导出 SetPluginLogSink 则注入宿主 sink,
    // 否则插件内 PluginLog 回退 CLogger (default_logger 跨模块)
    static LoggerLogSink pluginLogSink;
    if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
            GET_PROC(handle, "SetPluginLogSink"))) {
        setSink(&pluginLogSink);
    }

    auto* parser = createFunc();
    if (!parser) {
        CLogger::Error("CreateParser() returned null for '{}'", dllPath);
        FREE_LIBRARY(handle);
        return false;
    }

    LoadedParser loaded;
    loaded.instance.reset(parser);
    loaded.capability = parser->capability();
    loaded.handle = handle;

    RegisterParser(std::move(loaded));

    CLogger::Info("Loaded parser: {} (handles {})",
        loaded.capability.name,
        [](const auto& exts){
            std::string s;
            for (auto& e : exts) { if (!s.empty()) s += ","; s += e; }
            return s;
        }(loaded.capability.extensions));

    return true;
}

void PluginLoader::RegisterParser(LoadedParser&& parser)
{
    for (const auto& ext : parser.capability.extensions) {
        auto it = registry_.find(ext);
        if (it != registry_.end() && it->second) {
            // 扩展名已被注册: 按 priority 仲裁, 保留更高者 (避免目录迭代顺序决定归属)
            int curPriority = -1;
            for (const auto& p : owned_parsers_) {
                if (p.instance.get() == it->second) {
                    curPriority = p.capability.priority;
                    break;
                }
            }
            if (curPriority > parser.capability.priority) {
                CLogger::Info("Keep parser {} for extension {} "
                    "(priority {} > {})", it->second->capability().name,
                    ext, curPriority, parser.capability.priority);
                continue;
            }
        }
        registry_[ext] = parser.instance.get();
    }
    owned_parsers_.push_back(std::move(parser));
}

IConfigParser* PluginLoader::FindParser(const std::string& filepath) const
{
    fs::path p(filepath);
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = registry_.find(ext);
    if (it != registry_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ParserCapability> PluginLoader::ListParsers() const
{
    std::vector<ParserCapability> caps;
    for (const auto& p : owned_parsers_) {
        caps.push_back(p.capability);
    }
    return caps;
}

} // namespace NeoCore
