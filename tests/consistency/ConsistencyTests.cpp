/**
 * @file ConsistencyTests.cpp
 * @brief consistency 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/consistency/ConsistencyChecker.hpp"

void runConsistencyTests() {
    cc::ProjectInventory inventory;
    inventory.roleCounts[cc::AssetRole::SourceCode] = 1;
    auto issues = cc::ConsistencyChecker{}.check({}, inventory, {});
    requireTrue(!issues.empty(), "source without build should raise consistency issue");
}
