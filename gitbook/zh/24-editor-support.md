# 24. 编辑器语言支持

为 VS Code 与 Sublime Text 安装由统一语法源生成的 HHY 语言包。

## 24.1 HHY Language Support 0.1.0

编辑器语言包识别 .hhy 文件，提供 HHY 语法高亮、# 注释、shebang、字符串与转义、Regex、数字与单位、关键字和运算符，并配置括号自动闭合、缩进与常用代码片段。VS Code 使用 TextMate Grammar，Sublime Text 使用 .sublime-syntax。


{% hint style="info" %}
0.1.0 是不启动 HHY 进程的轻量语言支持：当前不提供保存时格式化、诊断、跳转定义或 LSP。语法规则以仓库中的 editors/syntax/hhy-syntax.json 为唯一事实源。
{% endhint %}


[查看编辑器语言包源码 ↗](https://github.com/hh696-wq/hhy-vm/tree/main/editors)

包含统一语法源、生成脚本、VS Code 与 Sublime Text 包以及真实 .hhy 回归样例。


## 24.2 生成并验证语言包

```sh
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm/editors
npm install
npm run generate
npm run check
npm run package
```


package 生成 dist/hhy-language-support-0.1.0.vsix 与 dist/HHY-0.1.0.sublime-package。check 会核对 Lexer 关键字和字面量后缀、插件元数据、生成文件新鲜度，并用真实 HHY 二进制检查 fixtures。


## 24.3 安装到 VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.1.0.vsix
```


也可以在 VS Code 中打开“扩展”，从右上角菜单选择“从 VSIX 安装”。安装后打开任意 .hhy 文件，语言模式会自动识别为 HHY。


## 24.4 安装到 Sublime Text

把 editors/dist/HHY-0.1.0.sublime-package 复制到 Sublime Text 的 Installed Packages 目录。开发时也可以把 editors/sublime 复制到 Packages/HHY。之后打开 .hhy 文件即可自动启用 HHY 语法。


{% hint style="info" %}
HHY Lexer 会根据前一个 token 区分 Regex 与除法。编辑器语法采用保守的表达式起始上下文识别 Regex，宁可少高亮一个 Regex，也避免把除法表达式的后续内容误判为 Regex。
{% endhint %}
