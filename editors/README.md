# HHY Language Support 0.2.0

Editor language support for HHY, generated from one repository-owned syntax source and checked against the current HHY Lexer.

## Packages

- VS Code: TextMate grammar, language configuration and snippets.
- Sublime Text: `.sublime-syntax`, comment/indent preferences and function snippet.

Sublime Text 4200 uses a linear safety grammar after a native incremental lexer
crash and runaway catalogue crawlers were observed on macOS arm64. It retains
comments, strings, keywords, numbers and units, builtins, operators, and
snippets. Regex literals, call/member lookarounds, and capture-based declaration
highlighting remain exclusive to VS Code.

VS Code version 0.2.0 includes the HHY language server. It publishes Core
diagnostics, formats through the canonical formatter, navigates local
definitions, shows callable Hover information and completes names sourced from
the Contract Registry. Configure `hhy.executablePath` if the CLI is not on
`PATH`. Sublime retains the hardened syntax-only package in this release; the
standalone `editors/lsp/server.mjs` can be connected through the third-party
Sublime LSP package.

The editor packages use the HHY repository's Apache-2.0 license.

## Source of truth

`syntax/hhy-syntax.json` contains language metadata, Lexer keywords, units, Regex flags, operators and selected builtins. Run:

```sh
cd editors
npm run generate
npm run check
npm run test:sublime-package
```

Generated files carry a warning and must not be edited directly:

- `vscode/syntaxes/hhy.tmLanguage.json`
- `sublime/HHY.sublime-syntax`

The verification script compares keywords and literal suffixes with `src/lexer.c`, validates plugin metadata, checks generated-file freshness and asks the real HHY binary to check every fixture.

`tests/syntax_test_hhy.hhy` is a native Sublime syntax-test fixture for Build
4200. Package verification also checks the final archive allowlist and compares
every packaged file byte-for-byte with its source.

To run the native fixture, copy `sublime/` to `Packages/HHY`, copy
`tests/syntax_test_hhy.hhy` into that directory, open the fixture in Sublime
Text 4200, and choose **Tools → Build**. Sublime's built-in `run_syntax_tests`
runner reports every scope assertion and exercises the real syntax engine.

## Regex versus division

HHY's Lexer classifies `/` using the previous token: after a token that can end an expression it is division; otherwise it starts a Regex. TextMate and Sublime grammars do not run that state machine.

The generated grammar therefore recognizes Regex only at conservative expression-start contexts, such as a line start, assignment, delimiter, `return`, `throw`, `attempt`, or logical operator. Expressions such as `total / count` remain operators. False negatives are preferred over highlighting the rest of a division expression as Regex.

## Build packages

```sh
cd editors
npm run package
```

Outputs:

- `dist/hhy-language-support-0.2.0.vsix`
- `dist/HHY-0.1.0.sublime-package`

`dist/` contains reproducible local build artifacts and is ignored by Git.

## Install VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.2.0.vsix
```

Or use **Extensions → … → Install from VSIX**.

## Install Sublime Text

For development, copy `editors/sublime` to the Sublime Text `Packages/HHY` directory. For packaged installation, copy `HHY-0.1.0.sublime-package` to Sublime Text's `Installed Packages` directory.
