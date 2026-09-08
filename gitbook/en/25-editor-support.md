# 25. Editor Language Support

Install HHY language packages for VS Code and Sublime Text, generated from one syntax source.

## 25.1 HHY 1.7.0 implementation and compatibility

1.7.0 passes local tests, four-platform CI and release validation. Language specification 1.0 remains frozen. Official sample, HTML and macOS/Linux Database 1.0.0 extensions are verified compatible. VS Code 0.2.0 and Sublime 0.1.0 retain independent versions; syntax and editor contract checks pass.


HIR, six optimization passes, integer MIR, type-feedback guards/deopt and local List scalar replacement are implemented. Explicitly enable HHY_COMPILER=ir, HHY_FEEDBACK_SPECIALIZATION=1 and HHY_SCALAR_REPLACEMENT=1; direct Bytecode remains default. Real workloads have not met default-enablement benefit gates. Scalar replacement retains original GC/quota reservations.


[Optimization switches, verification and supported scope](/docs/COMPILER_IR.md)

v1.7.0 implementation


[1.7.0 release notes](/docs/RELEASE_NOTES_1.7.0.md)

Released capabilities and limits


## 25.2 HHY Language Support 0.2.0

The editor packages recognize .hhy files and provide syntax highlighting, bracket auto-closing, indentation, and common snippets. The VS Code package also bundles the HHY Language Server and uses the hhy CLI for diagnostics, formatting, go-to-definition, hover, and completion; the Sublime Text package remains lightweight and process-free.


{% hint style="info" %}
The current VS Code version is 0.2.0; the Sublime Text package version is 0.1.0. Both derive syntax rules from the repository-owned editors/syntax/hhy-syntax.json source.
{% endhint %}


[Open the editor language-support source ↗](https://github.com/hh696-wq/hhy-vm/tree/main/editors)

Includes the shared syntax source, generator, VS Code and Sublime Text packages, and real .hhy regression fixtures.


## 25.3 Generate and verify the packages

```sh
git clone https://github.com/hh696-wq/hhy-vm.git
cd hhy-vm/editors
npm install
npm run generate
npm run check
npm run package
```


The package command creates dist/hhy-language-support-0.2.0.vsix and dist/HHY-0.1.0.sublime-package. The check command compares Lexer keywords and literal suffixes, validates plugin metadata and generated-file freshness, and checks every fixture with the real HHY binary.


## 25.4 Install in VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.2.0.vsix
```


You can also open Extensions in VS Code and choose Install from VSIX from the top-right menu. After installation, any .hhy file is automatically recognized as HHY.


## 25.5 Install in Sublime Text

Copy editors/dist/HHY-0.1.0.sublime-package into Sublime Text's Installed Packages directory. For development, copy editors/sublime into Packages/HHY. Opening a .hhy file then enables HHY syntax automatically.


{% hint style="info" %}
The HHY Lexer distinguishes Regex from division using the previous token. Editor grammars conservatively recognize Regex only in expression-start contexts, preferring a missed Regex highlight over mis-highlighting the remainder of a division expression.
{% endhint %}
