/**
 * @file DependencySmokeTests.cpp
 * @brief 固定第三方解析依赖自检。
 */

#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_NO_COUNTER
#include <catch2/catch.hpp>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include <string>

TEST_CASE("parser dependency stack is available") {
    pugi::xml_document document;
    const auto parsed = document.load_string("<project><name>可信编译器</name></project>");
    REQUIRE(parsed);
    REQUIRE(std::string{document.child("project").child_value("name")} == "可信编译器");

    const auto value = nlohmann::json::parse(R"({"project":"trust"})");
    REQUIRE(value.at("project").get<std::string>() == "trust");
}
