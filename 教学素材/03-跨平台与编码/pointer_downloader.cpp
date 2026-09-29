#include "pointer_downloader.h"
#include <logger.h>
#include <plugin_log_sink.h>
#include <filesystem>
#include <fstream>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QCryptographicHash>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <QCoreApplication>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace NeoBuild {

namespace fs = std::filesystem;

PointerDownloader::PointerDownloader() = default;

PointerDownloader::~PointerDownloader()
{
    for (auto& r : resolvers_) {
        r.instance.reset();
        if (r.dllHandle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(r.dllHandle));
#else
            dlclose(r.dllHandle);
#endif
            r.dllHandle = nullptr;
        }
    }
    resolvers_.clear();
}

void PointerDownloader::registerResolver(
    std::unique_ptr<NeoCore::IPluginPointer> resolver)
{
    if (!resolver) return;

    for (const auto& r : resolvers_) {
        if (r.instance && r.instance->name() == resolver->name()) {
            CLogger::Warn("PointerDownloader: resolver already registered: {}",
                resolver->name());
            return;
        }
    }

    CLogger::Info("PointerDownloader: registered resolver: {}", resolver->name());

    LoadedResolver lr;
    lr.instance = std::move(resolver);
    lr.dllHandle = nullptr;
    resolvers_.push_back(std::move(lr));
}

void PointerDownloader::scanResolvers(const std::string& pointersDir)
{
    if (!fs::exists(pointersDir) || !fs::is_directory(pointersDir)) {
        CLogger::Info("PointerDownloader: pointers dir not found: {}", pointersDir);
        return;
    }

    for (const auto& entry : fs::directory_iterator(pointersDir,
        fs::directory_options::skip_permission_denied)) {

        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
            loadResolverDLL(entry.path().string());
        }
    }

    CLogger::Info("PointerDownloader: loaded {} resolver(s) from {}",
        resolvers_.size(), pointersDir);
}

void PointerDownloader::loadResolverDLL(const std::string& dllPath)
{
    try {
        void* handle = nullptr;
#ifdef _WIN32
        std::wstring widePath(dllPath.begin(), dllPath.end());
        handle = LoadLibraryW(widePath.c_str());
        if (!handle) {
            DWORD err = GetLastError();
            CLogger::Warn("PointerDownloader: LoadLibrary failed for {}: error {}",
                dllPath, err);
            return;
        }
#else
        handle = dlopen(dllPath.c_str(), RTLD_NOW);
        if (!handle) {
            CLogger::Warn("PointerDownloader: dlopen failed for {}: {}",
                dllPath, dlerror());
            return;
        }
#endif

        NeoCore::CreatePointerFunc createFunc = nullptr;
#ifdef _WIN32
        createFunc = reinterpret_cast<NeoCore::CreatePointerFunc>(
            GetProcAddress(static_cast<HMODULE>(handle), "CreatePointer"));
#else
        createFunc = reinterpret_cast<NeoCore::CreatePointerFunc>(
            dlsym(handle, "CreatePointer"));
#endif

        if (!createFunc) {
            CLogger::Warn("PointerDownloader: CreatePointer not found in {}", dllPath);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return;
        }

        // 插件日志注入 (可选): 插件导出 SetPluginLogSink 则注入宿主 sink
        static LoggerLogSink pointerLogSink;
#ifdef _WIN32
        if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
                GetProcAddress(static_cast<HMODULE>(handle), "SetPluginLogSink"))) {
            setSink(&pointerLogSink);
        }
#else
        if (auto setSink = reinterpret_cast<void (*)(ILogSink*)>(
                dlsym(handle, "SetPluginLogSink"))) {
            setSink(&pointerLogSink);
        }
#endif

        NeoCore::IPluginPointer* rawInstance = createFunc();
        if (!rawInstance) {
            CLogger::Warn("PointerDownloader: CreatePointer returned null from {}",
                dllPath);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return;
        }

        for (const auto& r : resolvers_) {
            if (r.instance && r.instance->name() == rawInstance->name()) {
                CLogger::Warn("PointerDownloader: resolver already loaded: {}",
                    rawInstance->name());
                delete rawInstance;
#ifdef _WIN32
                FreeLibrary(static_cast<HMODULE>(handle));
#else
                dlclose(handle);
#endif
                return;
            }
        }

        LoadedResolver lr;
        lr.instance.reset(rawInstance);
        lr.dllHandle = handle;
        resolvers_.push_back(std::move(lr));

        CLogger::Info("PointerDownloader: loaded resolver from {}", dllPath);
    } catch (const std::exception& e) {
        CLogger::Error("PointerDownloader::loadResolverDLL exception: {}", e.what());
    }
}

NeoCore::IPluginPointer* PointerDownloader::findResolver(
    const NeoCore::PointerInfo& ptr) const
{
    for (const auto& r : resolvers_) {
        if (r.instance && r.instance->can_handle(ptr)) {
            return r.instance.get();
        }
    }
    return nullptr;
}

NeoBuild::ResolveResult PointerDownloader::resolveUrl(
    const NeoCore::PointerInfo& pointer) const
{
    ResolveResult out;
    out.resolver = pointer.resolver.empty() ? "<unknown>" : pointer.resolver;

    auto* resolver = findResolver(pointer);
    if (!resolver) {
        out.success = false;
        out.errorMessage = "No resolver found for pointer type: " + pointer.resolver;
        return out;
    }

    out.resolver = resolver->name();
    try {
        out.url = resolver->resolve_url(pointer);
    } catch (const std::exception& e) {
        out.success = false;
        out.errorMessage = "Resolver threw: " + std::string(e.what());
        return out;
    }

    out.success = !out.url.empty();
    if (!out.success) {
        out.errorMessage = "Resolver returned empty URL: " + out.resolver;
    }
    return out;
}

DownloadResult PointerDownloader::download(const NeoCore::PointerInfo& pointer,
    const std::string& cacheDir,
    std::function<void(const DownloadProgress&)> progressCallback,
    NeoCore::CancelToken* cancelToken)
{
    DownloadResult result;
    result.sha256 = pointer.sha256;

    if (cancelToken && cancelToken->is_cancelled()) {
        result.success = false;
        result.errorMessage = "Download cancelled";
        return result;
    }

    if (pointer.sha256.empty()) {
        result.success = false;
        result.errorMessage = "Empty SHA-256 in pointer";
        return result;
    }

    std::string cached = cachePath(cacheDir, pointer.sha256);
    if (fs::exists(cached)) {
        if (validateFile(cached, pointer.sha256)) {
            result.success = true;
            result.cachedPath = cached;
            result.sha256 = pointer.sha256;
        } else {
            CLogger::Warn("PointerDownloader: cached file hash mismatch for {}, "
                "re-downloading", pointer.sha256);
            std::error_code ec;
            fs::remove(cached, ec);
        }
    }

    if (result.success) {
        if (progressCallback) {
            DownloadProgress dp;
            dp.sha256 = pointer.sha256;
            dp.bytesDownloaded = static_cast<int64_t>(fs::file_size(cached));
            dp.totalBytes = dp.bytesDownloaded;
            dp.completed = true;
            progressCallback(dp);
        }
        return result;
    }

    auto* resolver = findResolver(pointer);
    if (!resolver) {
        result.success = false;
        result.errorMessage = "No resolver found for pointer type: " + pointer.resolver;
        CLogger::Error("PointerDownloader: {}", result.errorMessage);
        return result;
    }

    std::string url = resolver->resolve_url(pointer);
    if (url.empty()) {
        result.success = false;
        result.errorMessage = "Resolver returned empty URL: " + pointer.resolver;
        CLogger::Error("PointerDownloader: {}", result.errorMessage);
        return result;
    }

    CLogger::Info("PointerDownloader: downloading {} (SHA-256: {})", url,
        pointer.sha256);

    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    auto wrappedCallback = [&progressCallback](const DownloadProgress& dp) {
        if (progressCallback) {
            progressCallback(dp);
        }
    };

    if (!downloadToPath(url, cached, wrappedCallback, cancelToken)) {
        result.success = false;
        result.errorMessage = "Download failed";

        if (fs::exists(cached)) {
            fs::remove(cached, ec);
        }
        return result;
    }

    if (cancelToken && cancelToken->is_cancelled()) {
        result.success = false;
        result.errorMessage = "Download cancelled";
        if (fs::exists(cached)) {
            fs::remove(cached, ec);
        }
        return result;
    }

    if (!validateFile(cached, pointer.sha256)) {
        result.success = false;
        result.errorMessage = "SHA-256 hash mismatch after download";
        CLogger::Error("PointerDownloader: hash mismatch for {}", pointer.sha256);
        fs::remove(cached, ec);
        return result;
    }

    result.success = true;
    result.cachedPath = cached;
    CLogger::Info("PointerDownloader: download complete: {}", cached);

    return result;
}

std::vector<DownloadResult> PointerDownloader::downloadAll(
    const std::vector<NeoCore::PointerInfo>& pointers,
    const std::string& cacheDir,
    std::function<void(const DownloadProgress&)> progressCallback,
    NeoCore::CancelToken* cancelToken)
{
    std::vector<DownloadResult> results;
    results.reserve(pointers.size());

    for (const auto& ptr : pointers) {
        if (cancelToken && cancelToken->is_cancelled()) {
            DownloadResult cancelled;
            cancelled.success = false;
            cancelled.sha256 = ptr.sha256;
            cancelled.errorMessage = "Download cancelled";
            results.push_back(cancelled);
            continue;
        }

        results.push_back(download(ptr, cacheDir, progressCallback, cancelToken));
    }

    return results;
}

bool PointerDownloader::isCached(const std::string& cacheDir,
    const std::string& sha256) const
{
    if (sha256.empty()) return false;

    std::string path = cachePath(cacheDir, sha256);
    if (!fs::exists(path)) return false;

    return validateFile(path, sha256);
}

std::string PointerDownloader::cachePath(const std::string& cacheDir,
    const std::string& sha256) const
{
    return (fs::path(cacheDir) / sha256).string();
}

bool PointerDownloader::validateFile(const std::string& filepath,
    const std::string& expectedSha256) const
{
    try {
        QFile f(QString::fromStdString(filepath));
        if (!f.open(QIODevice::ReadOnly)) {
            return false;
        }

        QCryptographicHash hasher(QCryptographicHash::Sha256);

        constexpr qint64 bufferSize = 64 * 1024;

        while (!f.atEnd()) {
            QByteArray buffer = f.read(bufferSize);
            if (buffer.isEmpty() && f.error() != QFile::NoError) {
                f.close();
                return false;
            }
            hasher.addData(buffer);
        }

        f.close();

        QString computedHash = hasher.result().toHex();
        return computedHash.toStdString() == expectedSha256;
    } catch (const std::exception& e) {
        CLogger::Error("PointerDownloader::validateFile exception: {}", e.what());
        return false;
    }
}

bool PointerDownloader::downloadToPath(const std::string& url,
    const std::string& outputPath,
    std::function<void(const DownloadProgress&)> progressCallback,
    NeoCore::CancelToken* cancelToken)
{
    QNetworkAccessManager mgr;
    mgr.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    mgr.setTransferTimeout(300000);

    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    request.setRawHeader("User-Agent",
        "NeoServerUpdateModpack/1.0 (NeoServer)");
    request.setRawHeader("Accept", "*/*");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = mgr.get(request);
    if (!reply) {
        CLogger::Error("PointerDownloader: failed to create network request");
        return false;
    }

    QFile outputFile(QString::fromStdString(outputPath));
    if (!outputFile.open(QIODevice::WriteOnly)) {
        CLogger::Error("PointerDownloader: cannot open output file: {}", outputPath);
        reply->abort();
        reply->deleteLater();
        return false;
    }

    bool downloadComplete = false;
    bool downloadSuccess = false;
    QString lastError;

    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                outputFile.write(data);
            }
        }
    });

    QObject::connect(reply, &QNetworkReply::downloadProgress,
        [&progressCallback, &cancelToken, reply](qint64 received, qint64 total) {
            if (progressCallback) {
                DownloadProgress dp;
                dp.bytesDownloaded = received;
                dp.totalBytes = total;
                dp.completed = false;
                progressCallback(dp);
            }

            if (cancelToken && cancelToken->is_cancelled()) {
                reply->abort();
            }
        });

    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        if (reply->error() != QNetworkReply::NoError) {
            lastError = reply->errorString();
            CLogger::Error("PointerDownloader: network error: {}",
                lastError.toStdString());

            if (progressCallback) {
                DownloadProgress dp;
                dp.completed = false;
                dp.error = lastError.toStdString();
                progressCallback(dp);
            }

            outputFile.close();
            downloadSuccess = false;
            downloadComplete = true;
            return;
        }

        int statusCode = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode >= 200 && statusCode < 300) {
            QByteArray remaining = reply->readAll();
            if (!remaining.isEmpty()) {
                outputFile.write(remaining);
            }

            outputFile.flush();
            qint64 fileSize = outputFile.size();
            outputFile.close();

            DownloadProgress dp;
            dp.bytesDownloaded = fileSize;
            dp.totalBytes = fileSize;
            dp.completed = true;
            if (progressCallback) {
                progressCallback(dp);
            }

            downloadSuccess = true;
        } else {
            CLogger::Error("PointerDownloader: HTTP error {} for {}",
                statusCode, url);

            if (progressCallback) {
                DownloadProgress dp;
                dp.completed = false;
                dp.error = "HTTP " + std::to_string(statusCode);
                progressCallback(dp);
            }

            outputFile.close();
            downloadSuccess = false;
        }

        downloadComplete = true;
    });

    QTimer cancelTimer;
    if (cancelToken) {
        QObject::connect(&cancelTimer, &QTimer::timeout, [&]() {
            if (cancelToken->is_cancelled()) {
                reply->abort();
                downloadComplete = true;
                downloadSuccess = false;
            }
        });
        cancelTimer.start(100);
    }

    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::errorOccurred, [&](QNetworkReply::NetworkError) {
        if (!downloadComplete) {
            loop.quit();
        }
    });

    QTimer forceTimeout;
    QObject::connect(&forceTimeout, &QTimer::timeout, [&]() {
        if (!downloadComplete) {
            reply->abort();
            downloadComplete = true;
            downloadSuccess = false;
            CLogger::Error("PointerDownloader: download timed out for {}", url);
        }
        loop.quit();
    });
    forceTimeout.setSingleShot(true);
    forceTimeout.start(300000);

    loop.exec();

    cancelTimer.stop();
    forceTimeout.stop();

    reply->deleteLater();

    if (!downloadComplete) {
        outputFile.close();
        downloadSuccess = false;
    }

    return downloadSuccess;
}

} // namespace NeoBuild

