# mcpp-index 文档

本目录为面向贡献者的参考文档,供人工与 agent 共同使用。新增一个包的端到端流程定义于 agent skill
[`add-mcpp-index-package`](../.agents/skills/add-mcpp-index-package/SKILL.md);下列文档提供各环节的细节参考。

| 文档 | 内容 |
|------|------|
| [package-types.md](package-types.md) | 四种库形态(C 源码 compat、header-only、C++23 module wrapper、外部 Form-A 模块仓)的描述符模板与样例 |
| [cn-mirror.md](cn-mirror.md) | GitCode `mcpp-res` CN 镜像闭环(`gtc` 工具、闭环校验、注意事项),含无 `mcpp-res` 写权限时的回退方案(plain-string 上游 url) |
| [repository-and-schema.md](repository-and-schema.md) | 仓库布局、描述符 schema 速查、`validate.yml` CI 行为、本地 lint 复现、案例索引 |

> 包的字段规范(`mcpp = { … }` 扩展)以上游为准,见
> `mcpp xpkg parse`(CI 使用的同一个解析器),语义与约束见 mcpp 仓的 [`docs/spec/`](https://github.com/mcpp-community/mcpp/tree/main/docs/spec)。
