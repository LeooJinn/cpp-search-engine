#include "search_engine/sqlite_loader.h"

#include <sqlite3.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace search_engine {
namespace {

struct DatabaseCloser {
    void operator()(sqlite3* database) const noexcept {
        if (database != nullptr) {
            sqlite3_close(database);
        }
    }
};

struct StatementFinalizer {
    void operator()(sqlite3_stmt* statement) const noexcept {
        if (statement != nullptr) {
            sqlite3_finalize(statement);
        }
    }
};

}  // namespace

std::vector<Document> load_sqlite(const std::filesystem::path& database_path,
                                  std::string_view query) {
    sqlite3* raw_database = nullptr;
    const auto open_result = sqlite3_open_v2(
        database_path.string().c_str(), &raw_database, SQLITE_OPEN_READONLY, nullptr);
    std::unique_ptr<sqlite3, DatabaseCloser> database(raw_database);
    if (open_result != SQLITE_OK) {
        const std::string message = raw_database == nullptr
                                         ? "unknown SQLite error"
                                         : sqlite3_errmsg(raw_database);
        throw std::runtime_error("Unable to open SQLite database: " + message);
    }

    sqlite3_stmt* raw_statement = nullptr;
    const auto prepare_result = sqlite3_prepare_v2(
        database.get(), query.data(), static_cast<int>(query.size()), &raw_statement, nullptr);
    std::unique_ptr<sqlite3_stmt, StatementFinalizer> statement(raw_statement);
    if (prepare_result != SQLITE_OK) {
        throw std::runtime_error("Unable to prepare SQLite query: " +
                                 std::string(sqlite3_errmsg(database.get())));
    }
    if (sqlite3_column_count(statement.get()) < 2) {
        throw std::invalid_argument("SQLite query must return document ID and document text columns");
    }

    constexpr int max_rows = 100'000;
    constexpr int max_text_bytes = 64 * 1024 * 1024;
    std::vector<Document> documents;
    while (true) {
        const auto step_result = sqlite3_step(statement.get());
        if (step_result == SQLITE_DONE) {
            break;
        }
        if (step_result != SQLITE_ROW) {
            throw std::runtime_error("Unable to execute SQLite query: " +
                                     std::string(sqlite3_errmsg(database.get())));
        }
        if (documents.size() >= max_rows) {
            throw std::runtime_error("SQLite query returned more than 100000 rows");
        }

        const auto* raw_id = sqlite3_column_text(statement.get(), 0);
        const auto* raw_text = sqlite3_column_text(statement.get(), 1);
        const int text_bytes = sqlite3_column_bytes(statement.get(), 1);
        if (text_bytes > max_text_bytes) {
            throw std::runtime_error("SQLite document text exceeds the 64 MiB limit");
        }

        const std::string id = raw_id == nullptr
                                   ? "row-" + std::to_string(documents.size())
                                   : reinterpret_cast<const char*>(raw_id);
        const std::string text = raw_text == nullptr
                                     ? ""
                                     : std::string(reinterpret_cast<const char*>(raw_text), text_bytes);
        documents.push_back(Document{id, database_path, text});
    }

    return documents;
}

}  // namespace search_engine
