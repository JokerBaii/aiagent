cmake_minimum_required(VERSION 3.24)

project(ContestProjectTrustCompiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SKIP_RPATH ON)

option(CONTEST_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(CONTEST_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan in Debug builds" OFF)
option(CONTEST_REQUIRE_SYSTEM_DEPENDENCIES
    "Reject dependencies resolved from Conda/Anaconda environments" ON)

enable_testing()

if(CONTEST_REQUIRE_SYSTEM_DEPENDENCIES AND DEFINED ENV{CONDA_PREFIX}
   AND NOT "$ENV{CONDA_PREFIX}" STREQUAL "")
    list(PREPEND CMAKE_IGNORE_PREFIX_PATH "$ENV{CONDA_PREFIX}")
    list(REMOVE_DUPLICATES CMAKE_IGNORE_PREFIX_PATH)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibArchive REQUIRED)
find_package(pugixml CONFIG REQUIRED)

function(contest_require_system_path dependency_name)
    if(NOT CONTEST_REQUIRE_SYSTEM_DEPENDENCIES OR NOT UNIX)
        return()
    endif()

    foreach(candidate IN LISTS ARGN)
        if(NOT candidate OR NOT IS_ABSOLUTE "${candidate}")
            continue()
        endif()
        string(TOLOWER "${candidate}" normalized_candidate)
        if(normalized_candidate MATCHES "/(mini)?conda[0-9]*/|/anaconda[0-9]*/|/mambaforge/")
            message(FATAL_ERROR
                "${dependency_name} resolved outside the system toolchain: ${candidate}. "
                "Install the system development package and configure with a project preset.")
        endif()
    endforeach()
endfunction()

contest_require_system_path("C++ compiler" "${CMAKE_CXX_COMPILER}")
contest_require_system_path("nlohmann_json" "${nlohmann_json_DIR}")
contest_require_system_path("Catch2" "${Catch2_DIR}")
contest_require_system_path("ZLIB" "${ZLIB_INCLUDE_DIRS}" "${ZLIB_LIBRARIES}"
    "${ZLIB_INCLUDE_DIR}" "${ZLIB_LIBRARY}")
contest_require_system_path("LibArchive" "${LibArchive_INCLUDE_DIRS}" "${LibArchive_LIBRARIES}"
    "${LibArchive_INCLUDE_DIR}" "${LibArchive_LIBRARY}")
contest_require_system_path("pugixml" "${pugixml_DIR}")

function(contest_apply_common_options target_name)
    if(CONTEST_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4 /WX /permissive-)
        else()
            target_compile_options(${target_name} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror
            )
        endif()
    endif()

    if(CONTEST_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${target_name} PUBLIC
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target_name} PUBLIC
            -fsanitize=address,undefined
        )
    endif()
endfunction()

file(GLOB_RECURSE CONTEST_CORE_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp
    src/util/*.cpp
    src/loader/*.cpp
    src/inventory/*.cpp
    src/text/*.cpp
    src/cpir/*.cpp
    src/claim/*.cpp
    src/evidence/*.cpp
    src/consistency/*.cpp
    src/rules/*.cpp
    src/audit/*.cpp
    src/repair/*.cpp
    src/report/*.cpp
)

add_library(contest_core STATIC
    ${CONTEST_CORE_SOURCES}
)
target_include_directories(contest_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(contest_core PUBLIC
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    LibArchive::LibArchive
    pugixml::pugixml
)
contest_apply_common_options(contest_core)

file(GLOB_RECURSE CONTEST_AGENT_SOURCES CONFIGURE_DEPENDS
    src/agent/*.cpp
)
add_library(contest_agent STATIC
    ${CONTEST_AGENT_SOURCES}
)
target_include_directories(contest_agent PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(contest_agent PUBLIC contest_core)
contest_apply_common_options(contest_agent)

find_package(OpenSSL REQUIRED)
file(GLOB_RECURSE CONTEST_LLM_SOURCES CONFIGURE_DEPENDS
    src/llm/*.cpp
)
add_library(contest_llm STATIC
    ${CONTEST_LLM_SOURCES}
)
target_include_directories(contest_llm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(contest_llm PUBLIC contest_agent contest_core OpenSSL::SSL OpenSSL::Crypto)
contest_apply_common_options(contest_llm)

file(GLOB_RECURSE CONTEST_TEST_SOURCES CONFIGURE_DEPENDS
    tests/*.cpp
)
list(FILTER CONTEST_TEST_SOURCES EXCLUDE REGEX ".*/tests/catch2/.*\\.cpp$")
add_executable(contest_tests
    ${CONTEST_TEST_SOURCES}
)
target_link_libraries(contest_tests PRIVATE contest_agent contest_llm)
target_compile_definitions(contest_tests PRIVATE
    CONTEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
contest_apply_common_options(contest_tests)
add_test(NAME contest_tests COMMAND contest_tests)

add_executable(contest_dependency_tests
    tests/catch2/DependencySmokeTests.cpp
)
target_link_libraries(contest_dependency_tests PRIVATE
    Catch2::Catch2
    nlohmann_json::nlohmann_json
    pugixml::pugixml
)
contest_apply_common_options(contest_dependency_tests)
add_test(NAME contest_dependency_tests COMMAND contest_dependency_tests)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2)
contest_require_system_path("Qt6" "${Qt6_DIR}")
contest_require_system_path("OpenSSL" "${OPENSSL_INCLUDE_DIR}" "${OPENSSL_SSL_LIBRARY}"
    "${OPENSSL_CRYPTO_LIBRARY}")
qt_standard_project_setup()
qt_add_resources(CONTEST_WORKBENCH_RESOURCES
    apps/contest-workbench/resources.qrc
)
add_executable(contest-workbench
    apps/contest-workbench/main.cpp
    apps/contest-workbench/CompileController.cpp
    apps/contest-workbench/CompileController.hpp
    apps/contest-workbench/AuditResultModels.cpp
    apps/contest-workbench/AuditResultModels.hpp
    apps/contest-workbench/WorkbenchSessionModels.cpp
    apps/contest-workbench/WorkbenchSessionModels.hpp
    ${CONTEST_WORKBENCH_RESOURCES}
)
target_link_libraries(contest-workbench PRIVATE
    contest_agent
    contest_llm
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
)
contest_apply_common_options(contest-workbench)

install(TARGETS contest-workbench RUNTIME DESTINATION bin)
install(DIRECTORY rules examples DESTINATION share/contest-compiler)
install(DIRECTORY tools DESTINATION share/contest-compiler
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
install(DIRECTORY docs/
    DESTINATION share/contest-compiler/docs
    FILES_MATCHING PATTERN "*.md"
)
install(FILES README.md DESTINATION share/contest-compiler)

set(CPACK_PACKAGE_NAME "contest-project-trust-compiler")
set(CPACK_PACKAGE_VENDOR "Contest Project Trust Compiler Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "大学生竞赛项目材料包可信审计与自动补证系统")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "TGZ")
include(CPack)
