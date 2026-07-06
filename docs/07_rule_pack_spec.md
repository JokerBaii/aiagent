# 多赛道 JSON 规则包设计规范

## 1. 设计目标

规则包用于将竞赛项目审计逻辑从 C++ 代码中抽离，使系统能够面向不同赛道配置不同规则。

规则引擎必须满足：

- 规则可配置；
- 规则可解释；
- 规则可测试；
- 规则输出包含规则 ID；
- 规则触发原因可追溯；
- 规则不依赖 LLM 裁决。

## 2. 规则包文件

```text
rules/
├── common_rules.json
├── business_innovation_rules.json
├── software_project_rules.json
├── research_project_rules.json
├── social_practice_rules.json
└── ecommerce_rules.json
```

## 3. 规则结构

```json
{
  "rule_id": "BIZ_MARKET_001",
  "name": "市场规模证据检查",
  "track": "business_innovation",
  "severity": "blocker",
  "target": "market_analysis",
  "description": "检查项目中的市场规模声明是否具有可追溯证据，防止空泛市场描述直接通过审计。",
  "condition": {
    "required_assets": ["MARKET_RESEARCH"],
    "required_claim_evidence": ["MarketScale"]
  },
  "fail_reason": "项目存在市场规模描述，但缺少可追溯数据来源。",
  "fix_task": "补充市场规模来源，并给出 TAM/SAM/SOM 拆解。"
}
```

## 4. 必需字段

| 字段 | 说明 |
|---|---|
| rule_id | 全局唯一规则 ID |
| name | 中文规则名称 |
| track | 适用赛道 |
| severity | blocker / warning / info |
| target | 规则目标 |
| description | 中文规则说明 |
| condition | 触发条件 |
| fail_reason | 失败原因 |
| fix_task | 补证或修复任务 |

## 5. 支持的条件类型

```text
required_asset
missing_asset
minimum_asset_count
required_claim_evidence
consistency_check
forbidden_sensitive_file
doc_code_support
business_model_completeness
technical_route_completeness
research_reproducibility
social_impact_evidence
vendor_generated_ratio
```

## 6. common_rules 示例

```json
{
  "rule_id": "COMMON_SECRET_001",
  "name": "敏感文件检查",
  "track": "common",
  "severity": "blocker",
  "target": "project_assets",
  "description": "检查项目包中是否包含 .env、密钥、token、证书等敏感文件。",
  "condition": {
    "forbidden_roles": ["SECRET_RISK"]
  },
  "fail_reason": "项目包中存在敏感文件，不能直接作为竞赛提交材料。",
  "fix_task": "移除真实密钥文件，改为提供 .env.example 或配置说明。"
}
```

## 7. 商业创新规则示例

```json
{
  "rule_id": "BIZ_MODEL_001",
  "name": "商业模式闭环检查",
  "track": "business_innovation",
  "severity": "blocker",
  "target": "business_model",
  "description": "检查项目是否说明目标用户、付费方、收入来源、成本结构和渠道路径。",
  "condition": {
    "required_cpir_fields": [
      "target_user",
      "business_model",
      "financial_projection"
    ],
    "required_assets": ["BUSINESS_PLAN", "FINANCIAL_PLAN"]
  },
  "fail_reason": "项目商业模式缺少收入、成本或付费方说明。",
  "fix_task": "补充商业模式画布、收入来源、成本结构和财务预测假设。"
}
```

## 8. 软件项目规则示例

```json
{
  "rule_id": "SOFT_BUILD_001",
  "name": "构建入口检查",
  "track": "software_project",
  "severity": "blocker",
  "target": "source_code",
  "description": "检查软件类项目是否存在可识别的构建或依赖入口。",
  "condition": {
    "required_any_assets": ["BUILD_SYSTEM", "DEPENDENCY_MANIFEST"]
  },
  "fail_reason": "项目存在源码但缺少 CMakeLists.txt、package.json、requirements.txt 等构建或依赖入口。",
  "fix_task": "根据技术栈补充构建文件或依赖清单。"
}
```

## 9. 科研项目规则示例

```json
{
  "rule_id": "RES_EVAL_001",
  "name": "实验可复核性检查",
  "track": "scientific_research",
  "severity": "blocker",
  "target": "research_result",
  "description": "检查科研类项目是否具有实验数据、评价指标和对比基线。",
  "condition": {
    "required_assets": ["EXPERIMENT_DATA"],
    "required_claim_evidence": ["ResearchResult"]
  },
  "fail_reason": "项目存在研究结论，但缺少实验数据、评价指标或 baseline。",
  "fix_task": "补充实验数据、对比基线、评价指标和结果表。"
}
```

## 10. 社会实践规则示例

```json
{
  "rule_id": "SOC_IMPACT_001",
  "name": "社会影响证据检查",
  "track": "social_practice",
  "severity": "warning",
  "target": "social_value",
  "description": "检查社会实践项目是否有服务对象、活动过程和影响数据支撑。",
  "condition": {
    "required_assets": ["SOCIAL_PRACTICE_PROOF"],
    "required_claim_evidence": ["SocialImpact"]
  },
  "fail_reason": "项目存在社会影响描述，但缺少实践记录或影响数据。",
  "fix_task": "补充服务对象证明、活动记录、影响数据或合作单位证明。"
}
```

## 11. RuleEngine 输出

```json
{
  "rule_id": "BIZ_MARKET_001",
  "severity": "BLOCKER",
  "title": "市场规模证据检查失败",
  "reason": "商业计划书存在市场规模声明，但未发现市场调研或来源引用。",
  "evidence": ["商业计划书.docx"],
  "missing_evidence": ["行业报告", "统计数据", "TAM/SAM/SOM 拆解"],
  "fix_suggestion": "补充市场规模来源，并给出 TAM/SAM/SOM 拆解。"
}
```

## 12. 禁止规则

禁止：

```text
规则没有 rule_id
规则没有中文说明
规则没有修复建议
规则只有“判断项目是否优秀”这种主观描述
规则触发后无法解释
规则依赖 LLM 一句话判断
```
