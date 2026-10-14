cmake_minimum_required(VERSION 3.20)

project(cpp_search_engine VERSION 0.1.0 LANGUAGES CXX)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(search_engine
    src/document_loader.cpp
    src/indexed_search.cpp
    src/linear_search.cpp
    src/pdf_loader.cpp
    src/sqlite_loader.cpp
    src/synonyms.cpp
    src/tokenizer.cpp
    src/web_loader.cpp
)

target_include_directories(search_engine
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(search_engine
    PRIVATE
        CURL::libcurl
        SQLite3::SQLite3
)

target_compile_options(search_engine PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_executable(search_cli src/main.cpp)
target_link_libraries(search_cli PRIVATE search_engine)

target_compile_options(search_cli PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_executable(search_ingest src/ingest_main.cpp)
target_link_libraries(search_ingest PRIVATE search_engine)

target_compile_options(search_ingest PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_executable(search_server src/server_main.cpp)
target_link_libraries(search_server PRIVATE search_engine)

target_compile_options(search_server PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

add_executable(search_benchmark benchmarks/search_benchmark.cpp)
target_link_libraries(search_benchmark PRIVATE search_engine)

target_compile_options(search_benchmark PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

include(CTest)
if(BUILD_TESTING)
    add_executable(search_engine_tests tests/search_engine_tests.cpp)
    target_link_libraries(search_engine_tests PRIVATE search_engine)
    target_compile_options(search_engine_tests PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
    )
    add_test(NAME search_engine_tests COMMAND search_engine_tests)
endif()
