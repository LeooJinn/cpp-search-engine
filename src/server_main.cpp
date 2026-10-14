#include "search_engine/indexed_search.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int kDefaultPort = 8080;
constexpr std::size_t kMaxRequestSize = 8192;

constexpr char kIndexHtml[] = R"html(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>C++ Search Engine</title>
  <style>
    :root { color-scheme: dark; font-family: ui-sans-serif, system-ui, sans-serif; }
    body { margin: 0; background: #111827; color: #e5e7eb; }
    main { max-width: 820px; margin: 0 auto; padding: 48px 20px; }
    h1 { margin: 0 0 8px; font-size: 2rem; }
    p { color: #9ca3af; }
    form { display: flex; gap: 10px; margin: 28px 0 14px; }
    input[type="search"] { flex: 1; min-width: 0; padding: 13px 14px; border: 1px solid #374151; border-radius: 8px; background: #1f2937; color: #f9fafb; font-size: 1rem; }
    button { padding: 0 18px; border: 0; border-radius: 8px; background: #60a5fa; color: #111827; font-weight: 700; cursor: pointer; }
    label { margin-right: 18px; color: #d1d5db; font-size: .92rem; }
    #status { min-height: 1.5em; color: #9ca3af; }
    .result { margin: 10px 0; padding: 14px 16px; border: 1px solid #374151; border-radius: 8px; background: #1f2937; }
    .result strong { color: #bfdbfe; }
    .empty { padding: 26px 0; color: #9ca3af; }
    @media (max-width: 560px) { form { flex-direction: column; } button { min-height: 44px; } }
  </style>
</head>
<body>
  <main>
    <h1>C++ Search Engine</h1>
    <p>Search a persistent inverted index from your browser.</p>
    <form id="search-form">
      <input id="query" type="search" placeholder="Try: systems programming" autocomplete="off" autofocus>
      <button type="submit">Search</button>
    </form>
    <label><input id="synonyms" type="checkbox"> expand synonyms</label>
    <label><input id="fuzzy" type="checkbox"> allow spelling tolerance</label>
    <div id="status"></div>
    <section id="results" aria-live="polite"></section>
  </main>
  <script>
    const form = document.querySelector('#search-form');
    const query = document.querySelector('#query');
    const status = document.querySelector('#status');
    const results = document.querySelector('#results');
    const synonyms = document.querySelector('#synonyms');
    const fuzzy = document.querySelector('#fuzzy');
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const value = query.value.trim();
      if (!value) return;
      status.textContent = 'Searching…';
      results.replaceChildren();
      const params = new URLSearchParams({q: value});
      if (synonyms.checked) params.set('synonyms', '1');
      if (fuzzy.checked) params.set('fuzzy', '1');
      try {
        const response = await fetch('/api/search?' + params.toString());
        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'Search failed');
        status.textContent = `${data.results.length} result${data.results.length === 1 ? '' : 's'}`;
        if (!data.results.length) {
          results.innerHTML = '<div class="empty">No matching documents.</div>';
          return;
        }
        for (const result of data.results) {
          const article = document.createElement('article');
          article.className = 'result';
          article.innerHTML = `<strong>${escapeHtml(result.document_id)}</strong><br><span>score ${result.score}</span>`;
          results.append(article);
        }
      } catch (error) {
        status.textContent = error.message;
      }
    });
    function escapeHtml(value) {
      return value.replace(/[&<>"']/g, character => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[character]));
    }
  </script>
</body>
</html>
)html";

std::string url_decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded.push_back(' ');
        } else if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = [](char character) -> int {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                if (character >= 'A' && character <= 'F') return character - 'A' + 10;
                return -1;
            };
            const int high = hex(value[index + 1]);
            const int low = hex(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
            } else {
                decoded.push_back(value[index]);
            }
        } else {
            decoded.push_back(value[index]);
        }
    }
    return decoded;
}

std::string parameter(std::string_view target, std::string_view name) {
    const auto question = target.find('?');
    if (question == std::string_view::npos) return {};
    std::string_view query = target.substr(question + 1);
    while (!query.empty()) {
        const auto ampersand = query.find('&');
        const auto pair = query.substr(0, ampersand);
        const auto equals = pair.find('=');
        const auto key = pair.substr(0, equals);
        if (key == name) {
            return equals == std::string_view::npos ? "" : url_decode(pair.substr(equals + 1));
        }
        if (ampersand == std::string_view::npos) break;
        query.remove_prefix(ampersand + 1);
    }
    return {};
}

std::string json_escape(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}

void send_response(int client, int status, std::string_view content_type, std::string_view body) {
    const std::string status_text = status == 200 ? "OK" : status == 400 ? "Bad Request" : "Not Found";
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n" << body;
    const auto serialized = response.str();
    static_cast<void>(send(client, serialized.data(), serialized.size(), 0));
}

std::string search_json(const search_engine::IndexedSearchEngine& engine, std::string_view target) {
    const auto query = parameter(target, "q");
    if (query.empty()) {
        return R"({"error":"query is required"})";
    }

    search_engine::SearchOptions options;
    options.expand_synonyms = parameter(target, "synonyms") == "1";
    options.allow_fuzzy = parameter(target, "fuzzy") == "1";
    const auto search_results = engine.search(query, options);

    std::ostringstream body;
    body << R"({"results":[)";
    for (std::size_t index = 0; index < search_results.size() && index < 100; ++index) {
        if (index != 0) body << ',';
        body << R"({"document_id":")" << json_escape(search_results[index].document_id)
             << R"(","score":)" << search_results[index].score << '}';
    }
    body << "]}";
    return body.str();
}

void serve_client(int client, const search_engine::IndexedSearchEngine& engine) {
    char buffer[kMaxRequestSize + 1] = {};
    const auto bytes_read = recv(client, buffer, kMaxRequestSize, 0);
    if (bytes_read <= 0) {
        close(client);
        return;
    }
    buffer[bytes_read] = '\0';
    const std::string_view request(buffer, static_cast<std::size_t>(bytes_read));
    const auto line_end = request.find("\r\n");
    const auto request_line = request.substr(0, line_end);
    if (request_line.rfind("GET ", 0) != 0) {
        send_response(client, 400, "application/json", R"({"error":"only GET is supported"})");
        close(client);
        return;
    }

    const auto target_start = 4;
    const auto target_end = request_line.find(' ', target_start);
    const auto target = request_line.substr(target_start, target_end - target_start);
    if (target == "/" || target == "/index.html") {
        send_response(client, 200, "text/html; charset=utf-8", kIndexHtml);
    } else if (target.rfind("/api/search", 0) == 0) {
        const auto body = search_json(engine, target);
        const int status = parameter(target, "q").empty() ? 400 : 200;
        send_response(client, status, "application/json", body);
    } else {
        send_response(client, 404, "application/json", R"({"error":"not found"})");
    }
    close(client);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: search_server <index-file> [port]\n";
        return 2;
    }

    try {
        const auto engine = search_engine::IndexedSearchEngine::load(argv[1]);
        const int port = argc == 3 ? std::stoi(argv[2]) : kDefaultPort;
        if (port < 1 || port > 65535) {
            throw std::invalid_argument("port must be between 1 and 65535");
        }

        const int server = socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) throw std::runtime_error("unable to create server socket");
        int reuse = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
            listen(server, 16) < 0) {
            close(server);
            throw std::runtime_error("unable to bind/listen on localhost:" + std::to_string(port) +
                                     ": " + std::strerror(errno));
        }

        std::cout << "Search UI: http://127.0.0.1:" << port << '\n';
        std::cout << "Press Ctrl-C to stop.\n";
        while (true) {
            const int client = accept(server, nullptr, nullptr);
            if (client >= 0) serve_client(client, engine);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
