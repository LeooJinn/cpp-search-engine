#include "search_engine/indexed_search.h"
#include "search_engine/tokenizer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
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
constexpr std::size_t kMaxDocumentResponse = 512ULL * 1024ULL;

constexpr char kIndexHtml[] = R"html(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>C++ Search Engine</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0f172a;
      --panel: #111c31;
      --panel-raised: #17243b;
      --border: #2b3b56;
      --text: #e7edf7;
      --muted: #9aabc4;
      --accent: #8cc8ff;
      --accent-ink: #09213b;
      --focus: #b8ddff;
      --danger: #ffb4ab;
    }
    * { box-sizing: border-box; }
    body { margin: 0; min-width: 320px; background: var(--bg); color: var(--text); font: 15px/1.5 ui-sans-serif, system-ui, -apple-system, sans-serif; }
    main { max-width: 1120px; margin: 0 auto; padding: 42px 20px 64px; }
    header { display: flex; align-items: end; justify-content: space-between; gap: 24px; border-bottom: 1px solid var(--border); padding-bottom: 24px; }
    h1, h2, p { margin: 0; }
    h1 { font-size: 1.8rem; line-height: 1.15; letter-spacing: -0.025em; }
    h2 { font-size: 1.2rem; line-height: 1.25; overflow-wrap: anywhere; }
    header p, .muted { color: var(--muted); }
    .index-stat { color: var(--muted); font-size: .9rem; text-align: right; white-space: nowrap; }
    .search-shell { margin: 28px 0 24px; }
    form { display: flex; gap: 10px; }
    input[type="search"] { flex: 1; min-width: 0; padding: 13px 14px; border: 1px solid var(--border); border-radius: 8px; outline: 0; background: var(--panel); color: var(--text); font: inherit; }
    input[type="search"]:focus { border-color: var(--focus); box-shadow: 0 0 0 3px rgb(140 200 255 / 22%); }
    button { border: 1px solid transparent; border-radius: 8px; background: var(--accent); color: var(--accent-ink); font: inherit; font-weight: 750; cursor: pointer; }
    button:hover { background: #b5ddff; }
    button:focus-visible, a:focus-visible { outline: 3px solid var(--focus); outline-offset: 2px; }
    button:disabled { cursor: wait; opacity: .6; }
    .submit { min-width: 104px; padding: 0 18px; }
    .filters { display: flex; flex-wrap: wrap; gap: 16px; margin-top: 12px; color: var(--muted); font-size: .9rem; }
    label { display: inline-flex; align-items: center; gap: 7px; cursor: pointer; }
    input[type="checkbox"] { accent-color: var(--accent); }
    #status { min-height: 1.5em; margin: 18px 0 12px; color: var(--muted); }
    .workspace { display: grid; grid-template-columns: minmax(0, .92fr) minmax(0, 1.08fr); gap: 16px; align-items: start; }
    .pane { min-width: 0; border: 1px solid var(--border); border-radius: 10px; background: var(--panel); }
    .pane-heading { display: flex; align-items: center; justify-content: space-between; gap: 16px; min-height: 52px; padding: 14px 16px; border-bottom: 1px solid var(--border); }
    .pane-heading span { color: var(--muted); font-size: .85rem; }
    #results { display: grid; gap: 1px; overflow: hidden; border-radius: 0 0 10px 10px; }
    .result { display: block; width: 100%; padding: 15px 16px; border: 0; border-bottom: 1px solid var(--border); border-radius: 0; background: transparent; color: var(--text); text-align: left; }
    .result:last-child { border-bottom: 0; }
    .result:hover, .result[aria-current="true"] { background: var(--panel-raised); }
    .result-title { display: block; font-weight: 720; overflow-wrap: anywhere; }
    .result-meta { display: flex; flex-wrap: wrap; gap: 8px 14px; margin-top: 4px; color: var(--accent); font-size: .82rem; }
    .result-preview { display: block; margin-top: 8px; color: var(--muted); font-size: .88rem; }
    .empty { padding: 28px 16px; color: var(--muted); }
    .detail { min-height: 330px; padding-bottom: 4px; }
    .detail-body { padding: 18px 16px 20px; }
    .detail-title { margin-bottom: 10px; }
    .detail-source { margin-bottom: 16px; color: var(--muted); font-size: .86rem; overflow-wrap: anywhere; }
    .detail-source code { color: var(--accent); font: inherit; }
    .detail-source a { color: var(--accent); }
    .detail-facts { display: flex; flex-wrap: wrap; gap: 8px 18px; margin-bottom: 20px; color: var(--muted); font-size: .86rem; }
    .detail-facts strong { color: var(--text); font-weight: 650; }
    .query-callout { margin-bottom: 18px; padding: 10px 12px; border: 1px solid var(--border); border-radius: 7px; background: var(--panel-raised); color: var(--muted); }
    .query-callout strong { color: var(--text); font-weight: 650; }
    .document-text { max-height: 470px; overflow: auto; padding: 14px; border: 1px solid var(--border); border-radius: 7px; background: #0b1324; color: #d6e1f1; white-space: pre-wrap; overflow-wrap: anywhere; font: 13px/1.65 ui-monospace, SFMono-Regular, Menlo, monospace; }
    .loading { color: var(--muted); }
    .error { color: var(--danger); }
    @media (max-width: 760px) {
      main { padding-top: 26px; }
      header { align-items: start; flex-direction: column; }
      .index-stat { text-align: left; }
      .workspace { grid-template-columns: 1fr; }
      .detail { min-height: 0; }
    }
    @media (max-width: 520px) {
      form { flex-direction: column; }
      .submit { min-height: 44px; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <h1>C++ Search Engine</h1>
        <p>Search a persistent inverted index from your browser.</p>
      </div>
      <div class="index-stat" id="index-stat">Index ready</div>
    </header>

    <section class="search-shell" aria-label="Search controls">
      <form id="search-form">
        <input id="query" type="search" placeholder="Try: systems programming" autocomplete="off" autofocus>
        <button class="submit" id="submit" type="submit">Search</button>
      </form>
      <div class="filters">
        <label><input id="synonyms" type="checkbox"> expand synonyms</label>
        <label><input id="fuzzy" type="checkbox"> allow spelling tolerance</label>
      </div>
    </section>

    <div id="status" role="status" aria-live="polite">Enter a query to search.</div>
    <section class="workspace" aria-label="Search workspace">
      <section class="pane" aria-label="Search results">
        <div class="pane-heading"><strong>Results</strong><span id="result-count">—</span></div>
        <div id="results"><div class="empty">Matching documents will appear here.</div></div>
      </section>
      <section class="pane detail" aria-label="Document detail">
        <div class="pane-heading"><strong>Document</strong><span>select a result</span></div>
        <div class="detail-body" id="detail"><div class="empty">Select a result to inspect its source, matched query, and extracted text.</div></div>
      </section>
    </section>
  </main>

  <script>
    const form = document.querySelector('#search-form');
    const queryInput = document.querySelector('#query');
    const status = document.querySelector('#status');
    const results = document.querySelector('#results');
    const resultCount = document.querySelector('#result-count');
    const detail = document.querySelector('#detail');
    const submit = document.querySelector('#submit');
    const synonyms = document.querySelector('#synonyms');
    const fuzzy = document.querySelector('#fuzzy');
    const indexStat = document.querySelector('#index-stat');
    let latestResults = [];

    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const value = queryInput.value.trim();
      if (!value) {
        status.textContent = 'Enter a query first.';
        queryInput.focus();
        return;
      }
      setLoading(true);
      const params = new URLSearchParams({q: value});
      if (synonyms.checked) params.set('synonyms', '1');
      if (fuzzy.checked) params.set('fuzzy', '1');
      try {
        const response = await fetch('/api/search?' + params.toString());
        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'Search failed');
        latestResults = data.results;
        renderResults(data.results);
        status.textContent = `Showing ${data.results.length} result${data.results.length === 1 ? '' : 's'} for “${value}”.`;
        if (data.results.length) selectResult(data.results[0], value);
        else showEmptyDetail();
      } catch (error) {
        latestResults = [];
        renderResults([]);
        showError(error.message);
      } finally {
        setLoading(false);
      }
    });

    function setLoading(isLoading) {
      submit.disabled = isLoading;
      submit.textContent = isLoading ? 'Searching…' : 'Search';
      if (isLoading) status.textContent = 'Searching the index…';
    }

    function renderResults(items) {
      resultCount.textContent = items.length ? `${items.length} found` : 'No matches';
      results.replaceChildren();
      if (!items.length) {
        const empty = document.createElement('div');
        empty.className = 'empty';
        empty.textContent = 'No matching documents. Try another phrase or enable spelling tolerance.';
        results.append(empty);
        return;
      }
      for (const item of items) {
        const button = document.createElement('button');
        button.className = 'result';
        button.type = 'button';
        button.dataset.documentId = item.document_id;
        button.setAttribute('aria-label', `Open ${item.document_id}`);
        button.innerHTML = '<span class="result-title"></span><span class="result-meta"></span><span class="result-preview"></span>';
        button.querySelector('.result-title').textContent = item.document_id;
        button.querySelector('.result-meta').textContent = `score ${item.score} · ${item.characters.toLocaleString()} characters`;
        button.querySelector('.result-preview').textContent = item.preview || 'No preview available.';
        button.addEventListener('click', () => selectResult(item, queryInput.value.trim()));
        results.append(button);
      }
    }

    async function selectResult(item, matchedQuery) {
      document.querySelectorAll('.result').forEach((button) => {
        button.setAttribute('aria-current', button.dataset.documentId === item.document_id ? 'true' : 'false');
      });
      detail.innerHTML = '<div class="loading">Opening document…</div>';
      try {
        const params = new URLSearchParams({id: item.document_id, q: matchedQuery || ''});
        const response = await fetch('/api/document?' + params.toString());
        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'Unable to open document');
        renderDetail(data, item.score);
      } catch (error) {
        detail.innerHTML = '<div class="error"></div>';
        detail.querySelector('.error').textContent = error.message;
      }
    }

    function renderDetail(data, score) {
      detail.replaceChildren();
      const title = document.createElement('h2');
      title.className = 'detail-title';
      title.textContent = data.document_id;
      detail.append(title);

      const source = document.createElement('p');
      source.className = 'detail-source';
      source.append('Source: ');
      if (/^https?:\/\//i.test(data.source)) {
        const link = document.createElement('a');
        link.href = data.source;
        link.target = '_blank';
        link.rel = 'noreferrer';
        link.textContent = data.source;
        source.append(link);
      } else {
        const code = document.createElement('code');
        code.textContent = data.source || 'embedded document';
        source.append(code);
      }
      detail.append(source);

      const facts = document.createElement('div');
      facts.className = 'detail-facts';
      facts.innerHTML = '<span>score <strong></strong></span><span><strong></strong> characters</span><span><strong></strong> tokens</span>';
      facts.children[0].querySelector('strong').textContent = score;
      facts.children[1].querySelector('strong').textContent = data.characters.toLocaleString();
      facts.children[2].querySelector('strong').textContent = data.tokens.toLocaleString();
      detail.append(facts);

      const queryCallout = document.createElement('div');
      queryCallout.className = 'query-callout';
      queryCallout.append('Matched query: ');
      const queryValue = document.createElement('strong');
      queryValue.textContent = data.matched_query || '—';
      queryCallout.append(queryValue);
      detail.append(queryCallout);

      const text = document.createElement('pre');
      text.className = 'document-text';
      text.textContent = data.text || '[This document has no extracted text.]';
      detail.append(text);
      if (data.truncated) {
        const note = document.createElement('p');
        note.className = 'muted';
        note.style.marginTop = '10px';
        note.textContent = 'The document is larger than the browser preview limit; the displayed text is truncated.';
        detail.append(note);
      }
    }

    function showEmptyDetail() {
      detail.innerHTML = '<div class="empty">No document to open. Search again with a different query.</div>';
    }

    function showError(message) {
      status.textContent = message;
      detail.innerHTML = '<div class="error"></div>';
      detail.querySelector('.error').textContent = message;
    }

    fetch('/api/info').then(response => response.json()).then(data => {
      indexStat.textContent = `${data.documents.toLocaleString()} documents · ${data.terms.toLocaleString()} terms`;
    }).catch(() => { indexStat.textContent = 'Index ready'; });
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
    for (const unsigned char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20) {
                    escaped += ' ';
                } else {
                    escaped += static_cast<char>(character);
                }
                break;
        }
    }
    return escaped;
}

std::string preview(std::string_view text) {
    constexpr std::size_t max_preview = 220;
    std::string result;
    result.reserve(max_preview);
    bool previous_was_space = false;
    for (const unsigned char character : text) {
        const bool is_space = std::isspace(character) != 0;
        if (is_space) {
            if (!previous_was_space) result.push_back(' ');
        } else {
            result.push_back(static_cast<char>(character));
        }
        previous_was_space = is_space;
        if (result.size() >= max_preview) break;
    }
    if (result.size() == max_preview) result += "…";
    return result;
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

std::string info_json(const search_engine::IndexedSearchEngine& engine) {
    return "{\"documents\":" + std::to_string(engine.document_count()) +
           ",\"terms\":" + std::to_string(engine.indexed_term_count()) + "}";
}

std::string search_json(const search_engine::IndexedSearchEngine& engine, std::string_view target) {
    const auto query = parameter(target, "q");
    if (query.empty()) return R"({"error":"query is required"})";

    search_engine::SearchOptions options;
    options.expand_synonyms = parameter(target, "synonyms") == "1";
    options.allow_fuzzy = parameter(target, "fuzzy") == "1";
    const auto search_results = engine.search(query, options);

    std::ostringstream body;
    body << R"({"results":[)";
    for (std::size_t index = 0; index < search_results.size() && index < 100; ++index) {
        const auto* document = engine.find_document(search_results[index].document_id);
        if (index != 0) body << ',';
        body << R"({"document_id":")" << json_escape(search_results[index].document_id)
             << R"(","score":)" << search_results[index].score
             << R"(,"source":")" << json_escape(document == nullptr ? "" : document->source.string())
             << R"(","characters":)" << (document == nullptr ? 0 : document->text.size())
             << R"(,"preview":")" << json_escape(document == nullptr ? "" : preview(document->text)) << "\"}";
    }
    body << "]}";
    return body.str();
}

std::string document_json(const search_engine::IndexedSearchEngine& engine, std::string_view target) {
    const auto id = parameter(target, "id");
    if (id.empty()) return R"({"error":"document id is required"})";
    const auto* document = engine.find_document(id);
    if (document == nullptr) return R"({"error":"document not found"})";

    const auto query = parameter(target, "q");
    const auto text_size = document->text.size();
    const auto displayed_size = std::min(text_size, kMaxDocumentResponse);
    const auto tokens = search_engine::tokenize(document->text).size();
    std::ostringstream body;
    body << R"({"document_id":")" << json_escape(document->id)
         << R"(","source":")" << json_escape(document->source.string())
         << R"(","matched_query":")" << json_escape(query)
         << R"(","characters":)" << text_size
         << R"(,"tokens":)" << tokens
         << R"(,"truncated":)" << (displayed_size < text_size ? "true" : "false")
         << R"(,"text":")" << json_escape(std::string_view(document->text).substr(0, displayed_size)) << "\"}";
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
    try {
        if (target == "/" || target == "/index.html") {
            send_response(client, 200, "text/html; charset=utf-8", kIndexHtml);
        } else if (target == "/api/info") {
            send_response(client, 200, "application/json", info_json(engine));
        } else if (target.rfind("/api/search", 0) == 0) {
            const auto body = search_json(engine, target);
            send_response(client, parameter(target, "q").empty() ? 400 : 200, "application/json", body);
        } else if (target.rfind("/api/document", 0) == 0) {
            const auto body = document_json(engine, target);
            const auto id = parameter(target, "id");
            send_response(client, id.empty() || engine.find_document(id) == nullptr ? 400 : 200,
                          "application/json", body);
        } else {
            send_response(client, 404, "application/json", R"({"error":"not found"})");
        }
    } catch (const std::exception& error) {
        const std::string body = "{\"error\":\"" + json_escape(error.what()) + "\"}";
        send_response(client, 400, "application/json", body);
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
