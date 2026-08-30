# HHY for Sublime Text

Sublime Text 4200 uses a minimal, linear safety grammar. Rich Regex, number/unit,
function-call, member-access, capture, and lookaround rules are excluded after
a native lexer crash and runaway catalogue crawlers were observed on macOS arm64.
Keywords, simple integers, strings, comments, builtins, and operators remain.

Syntax highlighting, comments, indentation and a function snippet for HHY `.hhy` files.

For development, copy this directory to `Packages/HHY`. For packaged installation, place `HHY.sublime-package` in Sublime Text's `Installed Packages` directory.

Licensed under Apache-2.0 with the HHY repository.
