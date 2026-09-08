# 25. 编辑器语言支持

为 VS Code 与 Sublime Text 安装由统一语法源生成的 HHY 语言包。

## 25.1 HHY 1.7.0 当前实现与兼容性

1.7.0 已通过本地测试、四平台 CI 和发行验收。语言规范仍为冻结的 1.0；官方 sample、HTML 以及 macOS/Linux Database 1.0.0 扩展已验证兼容。VS Code 0.2.0 与 Sublime 0.1.0 保持独立版本，语法和编辑器契约检查通过。


HIR、六个优化 pass、整数 MIR、类型反馈 guard/deopt 与局部 List 标量替换已交付。使用 HHY_COMPILER=ir、HHY_FEEDBACK_SPECIALIZATION=1、HHY_SCALAR_REPLACEMENT=1 显式启用；默认直接 Bytecode 不变。真实负载收益尚未满足默认启用门槛，标量替换保留原始 GC/配额预约。


[优化开关、验证与支持范围](/docs/COMPILER_IR.md)

v1.7.0 实施说明


[1.7.0 发行说明](/docs/RELEASE_NOTES_1.7.0.md)

已发布能力与边界


## 25.2 HHY Language Support 0.2.0

编辑器语言包识别 .hhy 文件，提供语法高亮、括号自动闭合、缩进与常用代码片段。VS Code 版还内置 HHY Language Server，通过 hhy CLI 提供诊断、格式化、定义跳转、Hover 与补全；Sublime Text 版继续提供轻量、无进程的语法支持。


{% hint style="info" %}
VS Code 当前版本为 0.2.0，Sublime Text 包版本为 0.1.0。两者的语法规则均以仓库中的 editors/syntax/hhy-syntax.json 为唯一事实源。
{% endhint %}


[查看编辑器语言包源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/editors)

包含统一语法源、生成脚本、VS Code 与 Sublime Text 包以及真实 .hhy 回归样例。


## 25.3 生成并验证语言包

```sh
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm/editors
npm install
npm run generate
npm run check
npm run package
```


package 生成 dist/hhy-language-support-0.2.0.vsix 与 dist/HHY-0.1.0.sublime-package。check 会核对 Lexer 关键字和字面量后缀、插件元数据、生成文件新鲜度，并用真实 HHY 二进制检查 fixtures。


## 25.4 安装到 VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.2.0.vsix
```


也可以在 VS Code 中打开“扩展”，从右上角菜单选择“从 VSIX 安装”。安装后打开任意 .hhy 文件，语言模式会自动识别为 HHY。


## 25.5 安装到 Sublime Text

把 editors/dist/HHY-0.1.0.sublime-package 复制到 Sublime Text 的 Installed Packages 目录。开发时也可以把 editors/sublime 复制到 Packages/HHY。之后打开 .hhy 文件即可自动启用 HHY 语法。


{% hint style="info" %}
HHY Lexer 会根据前一个 token 区分 Regex 与除法。编辑器语法采用保守的表达式起始上下文识别 Regex，宁可少高亮一个 Regex，也避免把除法表达式的后续内容误判为 Regex。
{% endhint %}
