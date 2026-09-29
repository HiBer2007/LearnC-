#include "file_scanner.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <regex>
#include <chrono>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <nlohmann/json.hpp>
#include <logger.h>

namespace NeoWorkspace {

namespace fs = std::filesystem;

FileScanner::FileScanner()
{
}

std::vector<FileEntry> FileScanner::scanDirectory(const std::string& rootDir,
    const std::vector<std::string>& excludePatterns,
    bool computeHashes,
    NeoCore::CancelToken* cancelToken)
{
    std::vector<FileEntry> entries;

    try {
        if (cancelToken && cancelToken->is_cancelled()) {
            CLogger::Info("FileScanner: Scan cancelled before start");
            return entries;
        }

        fs::path root = fs::absolute(rootDir);
        if (!fs::exists(root) || !fs::is_directory(root)) {
            CLogger::Error("FileScanner: Root directory does not exist or is not a directory: {}",
                rootDir);
            return entries;
        }

        constexpr size_t cancelCheckInterval = 100;
        size_t fileCount = 0;

        for (const auto& entry : fs::recursive_directory_iterator(root,
            fs::directory_options::skip_permission_denied)) {

            if (cancelToken && (fileCount % cancelCheckInterval == 0)) {
                if (cancelToken->is_cancelled()) {
                    CLogger::Info("FileScanner: Scan cancelled after {} files", fileCount);
                    return entries;
                }
            }

            if (!entry.is_regular_file()) {
                continue;
            }

            try {
                auto relPath = fs::relative(entry.path(), root).generic_string();

                if (matchesExclude(relPath, excludePatterns)) {
                    continue;
                }

                FileEntry fe;
                fe.relativePath = relPath;
                fe.absolutePath = entry.path().generic_string();
                fe.fileSize = entry.file_size();
                fe.isPointer = entry.path().extension() == ".pointer";
                fe.lastModified = 0;

                QFileInfo qfi(QString::fromStdString(entry.path().string()));
                fe.lastModified = static_cast<std::time_t>(
                    qfi.lastModified().toSecsSinceEpoch());

                if (computeHashes) {
                    fe.sha256 = computeSha256(entry.path().string());
                }
                else {
                    fe.sha256 = "";
                }

                entries.push_back(std::move(fe));
                ++fileCount;
            }
            catch (const fs::filesystem_error& e) {
                CLogger::Warn("FileScanner: Error scanning file in {}: {}", rootDir, e.what());
            }
        }

        CLogger::Info("FileScanner: Scanned {} files in {}", fileCount, rootDir);
    }
    catch (const fs::filesystem_error& e) {
        CLogger::Error("FileScanner: Filesystem error scanning {}: {}", rootDir, e.what());
    }
    catch (const std::exception& e) {
        CLogger::Error("FileScanner: Error scanning {}: {}", rootDir, e.what());
    }

    return entries;
}

std::vector<FileEntry> FileScanner::scanPointers(const std::string& dir)
{
    std::vector<FileEntry> pointers;

    try {
        fs::path root = fs::absolute(dir);
        if (!fs::exists(root) || !fs::is_directory(root)) {
            CLogger::Error("FileScanner: Pointer directory does not exist: {}", dir);
            return pointers;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root,
            fs::directory_options::skip_permission_denied)) {

            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() != ".pointer") {
                continue;
            }

            try {
                FileEntry fe;
                fe.relativePath = fs::relative(entry.path(), root).generic_string();
                fe.absolutePath = entry.path().generic_string();
                fe.isPointer = true;
                fe.fileSize = entry.file_size();

                std::string stem = entry.path().stem().string();
                std::replace(stem.begin(), stem.end(), '\\', '/');
                fe.sha256 = stem;

                QFileInfo qfi(QString::fromStdString(entry.path().string()));
                fe.lastModified = static_cast<std::time_t>(
                    qfi.lastModified().toSecsSinceEpoch());

                pointers.push_back(std::move(fe));
            }
            catch (const fs::filesystem_error& e) {
                CLogger::Warn("FileScanner: Error scanning pointer file: {}", e.what());
            }
        }

        CLogger::Info("FileScanner: Found {} pointer files in {}", pointers.size(), dir);
    }
    catch (const fs::filesystem_error& e) {
        CLogger::Error("FileScanner: Filesystem error scanning pointers in {}: {}",
            dir, e.what());
    }
    catch (const std::exception& e) {
        CLogger::Error("FileScanner: Error scanning pointers in {}: {}", dir, e.what());
    }

    return pointers;
}

std::string FileScanner::computeSha256(const std::string& filepath)
{
    try {
        QFile file(QString::fromStdString(filepath));
        if (!file.open(QIODevice::ReadOnly)) {
            CLogger::Error("FileScanner: Cannot open file for hash: {}", filepath);
            return "";
        }

        QCryptographicHash hasher(QCryptographicHash::Sha256);

        constexpr qint64 chunkSize = 64 * 1024;
        QByteArray buffer;

        while (!file.atEnd()) {
            buffer = file.read(chunkSize);
            if (buffer.isEmpty()) {
                break;
            }
            hasher.addData(buffer);
        }

        file.close();

        if (hasher.result().isEmpty()) {
            CLogger::Error("FileScanner: Empty hash result for {}", filepath);
            return "";
        }

        return QString(hasher.result().toHex()).toStdString();
    }
    catch (const std::exception& e) {
        CLogger::Error("FileScanner: Exception computing hash for {}: {}",
            filepath, e.what());
        return "";
    }
}

NeoCore::PointerInfo FileScanner::parsePointerFile(const std::string& filepath)
{
    NeoCore::PointerInfo result;

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            CLogger::Error("FileScanner: Cannot open pointer file: {}", filepath);
            return result;
        }

        nlohmann::json j = nlohmann::json::parse(file);
        file.close();

        result.resolver = j.value("resolver", "");

        if (j.contains("metadata")) {
            result.metadata = j["metadata"];
        }

        std::string filename = fs::path(filepath).stem().string();
        std::replace(filename.begin(), filename.end(), '\\', '/');
        result.sha256 = filename;

        CLogger::Debug("FileScanner: Parsed pointer file for resolver '{}' (sha256: {})",
            result.resolver, result.sha256);
    }
    catch (const nlohmann::json::parse_error& e) {
        CLogger::Error("FileScanner: JSON parse error in pointer file {}: {}",
            filepath, e.what());
    }
    catch (const std::exception& e) {
        CLogger::Error("FileScanner: Error parsing pointer file {}: {}",
            filepath, e.what());
    }

    return result;
}

FileScanner::DiffResult FileScanner::diff(const std::vector<FileEntry>& oldFiles,
    const std::vector<FileEntry>& newFiles) const
{
    DiffResult result;

    try {
        std::unordered_map<std::string, const FileEntry*> newMap;
        newMap.reserve(newFiles.size());
        for (const auto& fe : newFiles) {
            newMap[fe.relativePath] = &fe;
        }

        std::unordered_map<std::string, const FileEntry*> oldMap;
        oldMap.reserve(oldFiles.size());
        for (const auto& fe : oldFiles) {
            oldMap[fe.relativePath] = &fe;

            auto it = newMap.find(fe.relativePath);
            if (it == newMap.end()) {
                result.removed.push_back(fe.relativePath);
            }
            else {
                if (!fe.sha256.empty() && !it->second->sha256.empty()) {
                    if (fe.sha256 == it->second->sha256) {
                        result.unchanged.push_back(fe.relativePath);
                    }
                    else {
                        result.modified.push_back(fe.relativePath);
                    }
                }
                else if (fe.fileSize == it->second->fileSize &&
                    fe.lastModified == it->second->lastModified) {
                    result.unchanged.push_back(fe.relativePath);
                }
                else {
                    result.modified.push_back(fe.relativePath);
                }
            }
        }

        for (const auto& fe : newFiles) {
            if (oldMap.find(fe.relativePath) == oldMap.end()) {
                result.added.push_back(fe.relativePath);
            }
        }

        std::sort(result.added.begin(), result.added.end());
        std::sort(result.removed.begin(), result.removed.end());
        std::sort(result.modified.begin(), result.modified.end());
        std::sort(result.unchanged.begin(), result.unchanged.end());

        CLogger::Info(
            "FileScanner: Diff complete: +{} -{} ~{} ={}",
            result.added.size(), result.removed.size(),
            result.modified.size(), result.unchanged.size());
    }
    catch (const std::exception& e) {
        CLogger::Error("FileScanner: Error computing diff: {}", e.what());
    }

    return result;
}

bool FileScanner::matchesExclude(const std::string& path,
    const std::vector<std::string>& patterns) const
{
    if (patterns.empty()) {
        return false;
    }

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    for (const auto& pattern : patterns) {
        try {
            std::regex re(pattern, std::regex::ECMAScript);
            if (std::regex_match(normalized, re)) {
                return true;
            }
        }
        catch (const std::regex_error&) {
            CLogger::Warn("FileScanner: Invalid exclude pattern '{}', ignoring", pattern);
        }
    }

    return false;
}

} // namespace NeoWorkspace

