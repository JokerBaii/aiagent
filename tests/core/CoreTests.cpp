/**
 * @file CoreTests.cpp
 * @brief core 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/core/Enums.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/util/TimeUtil.hpp"

#include <unordered_set>

void runCoreTests() {
    auto parsed = cc::parseJson("{\"name\":\"demo\",\"items\":[1,true]}");
    requireTrue(parsed.ok(), "JSON parser should parse object");
    requireTrue(parsed.value().at("name").asString() == "demo", "JSON string value mismatch");
    auto unicode = cc::parseJson("{\"word\":\"\\u4fe1\\u4efb\"}");
    requireTrue(unicode.ok(), "JSON parser should parse unicode escapes");
    requireTrue(unicode.value().at("word").asString() == "信任",
                "nlohmann/json should preserve unicode text");
    requireTrue(cc::assetRoleFromString("SOURCE_CODE") == cc::AssetRole::SourceCode,
                "AssetRole conversion failed");

    std::unordered_set<std::string> sessionIds;
    for (int index = 0; index < 128; ++index) {
        sessionIds.insert(cc::util::makeSessionId());
    }
    requireTrue(sessionIds.size() == 128U,
                "rapidly created sessions must not share a workspace id");
}
