# 24. Editor Language Support

Install HHY language packages for VS Code and Sublime Text, generated from one syntax source.

## 24.1 HHY Language Support 0.1.0

The editor packages recognize .hhy files and provide HHY syntax highlighting for # comments, shebangs, strings and escapes, Regex, numbers and units, keywords, and operators, plus bracket auto-closing, indentation, and common snippets. VS Code uses a TextMate grammar; Sublime Text uses .sublime-syntax.


{% hint style="info" %}
Version 0.1.0 is lightweight, process-free language support. It does not yet provide format-on-save, diagnostics, go-to-definition, or an LSP. The repository-owned editors/syntax/hhy-syntax.json file is the single source of truth.
{% endhint %}


[Open the editor language-support source ↗](https://github.com/hh696-wq/hhy-vm/tree/main/editors)

Includes the shared syntax source, generator, VS Code and Sublime Text packages, and real .hhy regression fixtures.


## 24.2 Generate and verify the packages

```sh
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm/editors
npm install
npm run generate
npm run check
npm run package
```


The package command creates dist/hhy-language-support-0.1.0.vsix and dist/HHY-0.1.0.sublime-package. The check command compares Lexer keywords and literal suffixes, validates plugin metadata and generated-file freshness, and checks every fixture with the real HHY binary.


## 24.3 Install in VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.1.0.vsix
```


You can also open Extensions in VS Code and choose Install from VSIX from the top-right menu. After installation, any .hhy file is automatically recognized as HHY.


## 24.4 Install in Sublime Text

Copy editors/dist/HHY-0.1.0.sublime-package into Sublime Text's Installed Packages directory. For development, copy editors/sublime into Packages/HHY. Opening a .hhy file then enables HHY syntax automatically.


{% hint style="info" %}
The HHY Lexer distinguishes Regex from division using the previous token. Editor grammars conservatively recognize Regex only in expression-start contexts, preferring a missed Regex highlight over mis-highlighting the remainder of a division expression.
{% endhint %}
