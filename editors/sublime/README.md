# HHY for Sublime Text

Sublime Text 4200 uses a linear safety grammar. Regex-literal, function-call,
member-access, capture, and lookaround rules are excluded after a native lexer
crash and runaway catalogue crawlers were observed on macOS arm64. Keywords,
numbers and units, strings, comments, builtins, and operators remain.

Syntax highlighting, comments, indentation and a function snippet for HHY `.hhy` files.

For development, copy this directory to `Packages/HHY`. For packaged installation, place `HHY.sublime-package` in Sublime Text's `Installed Packages` directory.

For a native Build 4200 regression run, also copy
`../tests/syntax_test_hhy.hhy` into `Packages/HHY`, open it, and choose
**Tools → Build**. The test fixture is intentionally excluded from the release
package.

Licensed under Apache-2.0 with the HHY repository.
