#include "search_engine/web_loader.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace search_engine {
namespace {

constexpr std::size_t kMaxResponseSize = 16ULL * 1024ULL * 1024ULL;

struct CurlGlobal {
    CurlGlobal() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw std::runtime_error("Unable to initialize libcurl");
        }
    }

    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

std::size_t write_response(char* data, std::size_t size, std::size_t count, void* user_data) {
    auto& response = *static_cast<std::string*>(user_data);
    const auto bytes = size * count;
    if (response.size() + bytes > kMaxResponseSize) {
        return 0;
    }
    response.append(data, bytes);
    return bytes;
}

bool starts_with_case_insensitive(std::string_view value, std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(prefix[index]))) {
            return false;
        }
    }
    return true;
}

std::string decode_entities(std::string text) {
    const std::pair<std::string_view, std::string_view> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "},
    };
    for (const auto& [entity, replacement] : entities) {
        std::size_t position = 0;
        while ((position = text.find(entity, position)) != std::string::npos) {
            text.replace(position, entity.size(), replacement);
            position += replacement.size();
        }
    }
    return text;
}

std::string html_to_text(std::string_view html) {
    std::string text;
    text.reserve(html.size());
    bool in_hidden_block = false;

    for (std::size_t index = 0; index < html.size(); ++index) {
        if (html[index] == '<') {
            const auto close = html.find('>', index + 1);
            if (close == std::string_view::npos) {
                break;
            }
            std::string tag(html.substr(index + 1, close - index - 1));
            std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });

            if (starts_with_case_insensitive(tag, "!--")) {
                const auto comment_end = html.find("-->", close + 1);
                index = comment_end == std::string_view::npos ? html.size() : comment_end + 2;
                continue;
            }

            const bool closing = !tag.empty() && tag.front() == '/';
            if (starts_with_case_insensitive(tag, "script") ||
                starts_with_case_insensitive(tag, "style")) {
                in_hidden_block = !closing;
            } else if (closing &&
                       (starts_with_case_insensitive(tag, "/script") ||
                        starts_with_case_insensitive(tag, "/style"))) {
                in_hidden_block = false;
            }
            text.push_back(' ');
            index = close;
            continue;
        }

        if (!in_hidden_block) {
            text.push_back(html[index]);
        }
    }

    return decode_entities(std::move(text));
}

}  // namespace

std::vector<Document> load_web_page(std::string_view url) {
    if (!starts_with_case_insensitive(url, "https://") &&
        !starts_with_case_insensitive(url, "http://")) {
        throw std::invalid_argument("Web loader accepts only http:// or https:// URLs");
    }

    static const CurlGlobal curl_global;
    static_cast<void>(curl_global);

    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        throw std::runtime_error("Unable to create libcurl handle");
    }

    std::string response;
    curl_easy_setopt(handle, CURLOPT_URL, std::string(url).c_str());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "cpp-search-engine/0.1");
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

    const auto result = curl_easy_perform(handle);
    long response_code = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(handle);

    if (result == CURLE_WRITE_ERROR && response.size() >= kMaxResponseSize) {
        throw std::runtime_error("Web response exceeds the 16 MiB limit");
    }
    if (result != CURLE_OK) {
        throw std::runtime_error("Unable to fetch URL: " + std::string(curl_easy_strerror(result)));
    }
    if (response_code < 200 || response_code >= 300) {
        throw std::runtime_error("Web server returned HTTP " + std::to_string(response_code));
    }

    return {Document{std::string(url), std::filesystem::path(std::string(url)), html_to_text(response)}};
}

}  // namespace search_engine
