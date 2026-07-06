/**
 * @file StructuredTextExtractor.hpp
 * @brief JSON/YAML 结构化文本抽取。
 *
 * 规则和审计材料常把关键事实放在配置键和值中。本模块把结构键和值展开为可审计文本，
 * 避免只按纯文本读取时漏掉层级语义。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class StructuredTextExtractor {
  public:
    /**
     * @brief 抽取 JSON/YAML 的结构键和值。
     *
     * @param asset JSON/YAML 资产。
     * @return 成功时返回展开后的可审计文本；解析失败时返回 NEED_REVIEW 状态。
     */
    [[nodiscard]] Result<TextDocument> extract(const ProjectAsset& asset) const;
};

/**
 * @brief 判断扩展名是否应走结构化抽取。
 */
[[nodiscard]] bool isStructuredTextExtension(const std::string& extension);

} // namespace cc
