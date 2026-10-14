#include "search_engine/synonyms.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace search_engine {

std::vector<std::string> synonym_terms(std::string_view term) {
    static const std::unordered_map<std::string, std::vector<std::string>> dictionary{
        {"car", {"auto", "automobile", "vehicle"}},
        {"automobile", {"auto", "car", "vehicle"}},
        {"vehicle", {"auto", "automobile", "car"}},
        {"database", {"db", "storage", "datastore"}},
        {"bug", {"defect", "error", "issue"}},
        {"error", {"bug", "defect", "issue"}},
        {"find", {"search", "lookup"}},
        {"search", {"find", "lookup"}},
        {"lookup", {"find", "search"}},
        {"program", {"code", "software"}},
        {"software", {"code", "program"}},
    };

    const auto match = dictionary.find(std::string(term));
    return match == dictionary.end() ? std::vector<std::string>{} : match->second;
}

}  // namespace search_engine
