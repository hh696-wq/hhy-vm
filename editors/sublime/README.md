# HHY for Sublime Text

Sublime Text 4200 uses a conservative safety grammar without Regex-literal,
function-call, or member-access lookaround highlighting. The reduced grammar
avoids a native incremental-lexer crash observed on macOS arm64.

Syntax highlighting, comments, indentation and a function snippet for HHY `.hhy` files.

For development, copy this directory to `Packages/HHY`. For packaged installation, place `HHY.sublime-package` in Sublime Text's `Installed Packages` directory.

Licensed under Apache-2.0 with the HHY repository.
