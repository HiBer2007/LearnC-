#include <IConfigParser.h>
#include <plugin_log_sink.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

static std::string read_file_content(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {};
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

static std::string strip_json_comments(const std::string& content)
{
    std::string result;
    result.reserve(content.size());
    bool in_string = false;
    char string_quote = 0;

    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];

        if (in_string) {
            result.push_back(c);
            if (c == '\\' && i + 1 < content.size()) {
                result.push_back(content[++i]);
            } else if (c == string_quote) {
                in_string = false;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            in_string = true;
            string_quote = c;
            result.push_back(c);
            continue;
        }

        if (c == '/') {
            if (i + 1 >= content.size()) {
                result.push_back(c);
                break;
            }
            if (content[i + 1] == '/') {
                ++i;
                while (i + 1 < content.size() && content[i + 1] != '\n') {
                    ++i;
                }
                result.push_back('\n');
                continue;
            }
            if (content[i + 1] == '*') {
                i += 2;
                while (i + 1 < content.size()) {
                    if (content[i] == '*' && content[i + 1] == '/') {
                        ++i;
                        break;
                    }
                    ++i;
                }
                ++i;
                result.push_back(' ');
                continue;
            }
        }

        result.push_back(c);
    }

    return result;
}

static json parse_json_content(const std::string& content, bool try_comments)
{
    if (try_comments) {
        try {
            return json::parse(content, nullptr, true, true);
        } catch (const std::exception&) {
        }
    }
    return json::parse(content, nullptr, true, false);
}

static std::vector<std::string> split_path(const std::string& path)
{
    std::vector<std::string> result;
    if (path.empty()) {
        return result;
    }

    size_t start = 0;
    size_t end = 0;
    while ((end = path.find('.', start)) != std::string::npos) {
        result.push_back(path.substr(start, end - start));
        start = end + 1;
    }
    result.push_back(path.substr(start));
    return result;
}

static void flatten_json(const json& node,
                         const std::string& prefix,
                         std::vector<NeoCore::ConfigEntry>& entries)
{
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string path = prefix.empty()
                                   ? it.key()
                                   : prefix + "." + it.key();
            if (it.value().is_object()) {
                flatten_json(it.value(), path, entries);
            } else {
                NeoCore::ConfigEntry entry;
                entry.key_path = path;
                if (it.value().is_string()) {
                    entry.remote_value = it.value().get<std::string>();
                } else {
                    entry.remote_value = it.value().dump();
                }
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            }
        }
    } else if (node.is_array()) {
        for (size_t i = 0; i < node.size(); ++i) {
            std::string path = prefix + "[" + std::to_string(i) + "]";
            if (node[i].is_object() || node[i].is_array()) {
                flatten_json(node[i], path, entries);
            } else {
                NeoCore::ConfigEntry entry;
                entry.key_path = path;
                if (node[i].is_string()) {
                    entry.remote_value = node[i].get<std::string>();
                } else {
                    entry.remote_value = node[i].dump();
                }
                entry.local_value.clear();
                entry.is_tracked = false;
                entries.push_back(entry);
            }
        }
    }
}

static json* navigate_json_path(json& root, const std::vector<std::string>& segments)
{
    json* current = &root;
    for (const auto& seg : segments) {
        if (!current->is_object()) {
            return nullptr;
        }
        auto it = current->find(seg);
        if (it == current->end()) {
            return nullptr;
        }
        current = &(*it);
    }
    return current;
}

static const json* navigate_json_path(const json& root,
                                       const std::vector<std::string>& segments)
{
    const json* current = &root;
    for (const auto& seg : segments) {
        if (!current->is_object()) {
            return nullptr;
        }
        auto it = current->find(seg);
        if (it == current->end()) {
            return nullptr;
        }
        current = &(*it);
    }
    return current;
}

class JsonConfigParser : public NeoCore::IConfigParser {
public:
    NeoCore::ParserCapability capability() const override
    {
        NeoCore::ParserCapability cap;
        cap.name = "JSON";
        cap.extensions = {".json", ".json5", ".jsonc"};
        cap.supported_modes = {
            NeoCore::TrackingMode::FullSync,
            NeoCore::TrackingMode::ConfigMerge,
            NeoCore::TrackingMode::NoSync
        };
        cap.supports_line_tracking = false;
        cap.priority = 100;
        return cap;
    }

    bool can_handle(const std::string& filepath) const override
    {
        auto pos = filepath.rfind('.');
        if (pos == std::string::npos) {
            return false;
        }
        std::string ext = filepath.substr(pos);
        return ext == ".json" || ext == ".json5" || ext == ".jsonc";
    }

    std::vector<NeoCore::ConfigEntry> parse_entries(
        const std::string& filepath) override
    {
        std::vector<NeoCore::ConfigEntry> entries;

        std::string content = read_file_content(filepath);
        if (content.empty()) {
            return entries;
        }

        std::string ext;
        auto pos = filepath.rfind('.');
        if (pos != std::string::npos) {
            ext = filepath.substr(pos);
        }

        json j;
        try {
            if (ext == ".json5" || ext == ".jsonc") {
                std::string stripped = strip_json_comments(content);
                j = json::parse(stripped, nullptr, true, false);
            } else {
                j = parse_json_content(content, true);
            }
        } catch (const std::exception&) {
            return entries;
        }

        if (j.is_discarded()) {
            return entries;
        }

        flatten_json(j, "", entries);
        return entries;
    }

    std::string merge_entries(
        const std::string& /*filepath*/,
        const std::vector<std::string>& tracked_keys,
        const std::string& remote_content,
        const std::string& local_content) override
    {
        if (local_content.empty() && remote_content.empty()) {
            return {};
        }
        if (local_content.empty()) {
            return remote_content;
        }
        if (remote_content.empty()) {
            return local_content;
        }

        json remote;
        json local;
        try {
            remote = json::parse(remote_content, nullptr, true, false);
        } catch (const std::exception&) {
            return local_content;
        }
        try {
            local = json::parse(local_content, nullptr, true, false);
        } catch (const std::exception&) {
            return remote_content;
        }

        if (remote.is_discarded() || local.is_discarded()) {
            return local_content;
        }

        json result = local;

        for (const auto& key : tracked_keys) {
            if (key.empty()) {
                continue;
            }

            auto segs = split_path(key);
            if (segs.empty()) {
                continue;
            }

            json* dst = navigate_json_path(result, segs);
            const json* src = navigate_json_path(remote, segs);

            if (dst && src) {
                *dst = *src;
            }
        }

        return result.dump(2);
    }

    std::vector<std::string> list_keys(
        const std::string& filepath) override
    {
        std::vector<std::string> keys;
        auto entries = parse_entries(filepath);
        keys.reserve(entries.size());
        for (auto& e : entries) {
            keys.push_back(std::move(e.key_path));
        }
        return keys;
    }
};

} // anonymous namespace

extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser()
{
    return new JsonConfigParser();
}

NEO_DECLARE_PLUGIN_LOG_SINK("NeoParser_JSON")

