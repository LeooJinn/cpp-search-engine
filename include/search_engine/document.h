#pragma once

#include <filesystem>
#include <string>

namespace search_engine {

struct Document {
    std::string id;
    std::filesystem::path source;
    std::string text;
};

}  // namespace search_engine
