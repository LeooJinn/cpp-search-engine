#include "search_engine/pdf_loader.h"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef __unix__
#include <sys/wait.h>
#endif

namespace search_engine {
namespace {

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }
    quoted += '\'';
    return quoted;
}

}  // namespace

std::vector<Document> load_pdf(const std::filesystem::path& pdf_path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(pdf_path, error) || error) {
        throw std::invalid_argument("PDF file does not exist: " + pdf_path.string());
    }

    const std::string command = "pdftotext -layout " + shell_quote(pdf_path.string()) + " - 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("Unable to start pdftotext; install Poppler and ensure pdftotext is on PATH");
    }

    constexpr std::size_t max_text_size = 64ULL * 1024ULL * 1024ULL;
    std::string text;
    char buffer[8192];
    while (const auto bytes_read = std::fread(buffer, 1, sizeof(buffer), pipe)) {
        if (text.size() + bytes_read > max_text_size) {
            pclose(pipe);
            throw std::runtime_error("PDF text exceeds the 64 MiB extraction limit: " + pdf_path.string());
        }
        text.append(buffer, bytes_read);
    }

    const int status = pclose(pipe);
#ifdef __unix__
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("pdftotext could not extract PDF text: " + pdf_path.string());
    }
#else
    if (status != 0) {
        throw std::runtime_error("pdftotext could not extract PDF text: " + pdf_path.string());
    }
#endif

    return {Document{pdf_path.filename().string(), pdf_path, std::move(text)}};
}

}  // namespace search_engine
