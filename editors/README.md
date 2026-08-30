# HHY Language Support 0.1.0

Editor language support for HHY, generated from one repository-owned syntax source and checked against the current HHY Lexer.

## Packages

- VS Code: TextMate grammar, language configuration and snippets.
- Sublime Text: `.sublime-syntax`, comment/indent preferences and function snippet.

Version 0.1.0 is intentionally process-free. It does not start `hhy`, format files, publish diagnostics or implement an LSP.

The editor packages use the HHY repository's Apache-2.0 license.

## Source of truth

`syntax/hhy-syntax.json` contains language metadata, Lexer keywords, units, Regex flags, operators and selected builtins. Run:

```sh
cd editors
npm run generate
npm run check
```

Generated files carry a warning and must not be edited directly:

- `vscode/syntaxes/hhy.tmLanguage.json`
- `sublime/HHY.sublime-syntax`

The verification script compares keywords and literal suffixes with `src/lexer.c`, validates plugin metadata, checks generated-file freshness and asks the real HHY binary to check every fixture.

## Regex versus division

HHY's Lexer classifies `/` using the previous token: after a token that can end an expression it is division; otherwise it starts a Regex. TextMate and Sublime grammars do not run that state machine.

The generated grammar therefore recognizes Regex only at conservative expression-start contexts, such as a line start, assignment, delimiter, `return`, `throw`, `attempt`, or logical operator. Expressions such as `total / count` remain operators. False negatives are preferred over highlighting the rest of a division expression as Regex.

## Build packages

```sh
cd editors
npm run package
```

Outputs:

- `dist/hhy-language-support-0.1.0.vsix`
- `dist/HHY-0.1.0.sublime-package`

`dist/` contains reproducible local build artifacts and is ignored by Git.

## Install VS Code

```sh
code --install-extension editors/dist/hhy-language-support-0.1.0.vsix
```

Or use **Extensions → … → Install from VSIX**.

## Install Sublime Text

For development, copy `editors/sublime` to the Sublime Text `Packages/HHY` directory. For packaged installation, copy `HHY-0.1.0.sublime-package` to Sublime Text's `Installed Packages` directory.
