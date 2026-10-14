#include "search_engine/tokenizer.h"

#include <cctype>
#include <unordered_set>
#include <utility>

namespace search_engine {
namespace {

bool is_token_character(unsigned char character) {
    return std::isalnum(character) != 0;
}

char lowercase_ascii(unsigned char character) {
    return static_cast<char>(std::tolower(character));
}

}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;

    for (const unsigned char character : text) {
        if (is_token_character(character)) {
            current.push_back(lowercase_ascii(character));
            continue;
        }

        if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
        }
    }

    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }

    return tokens;
}

std::vector<std::string> unique_tokens(std::string_view text) {
    const auto tokens = tokenize(text);
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;

    for (auto token : tokens) {
        if (seen.insert(token).second) {
            unique.push_back(std::move(token));
        }
    }

    return unique;
}

}  // namespace search_engine
