# HHY Language Support

Syntax highlighting, comments, bracket handling, folding, indentation, snippets,
diagnostics, formatting, definition navigation, Hover and Contract-aware
completion for `.hhy` files.

Version 0.2.0 starts the bundled HHY language server and invokes the `hhy` CLI.
Set `hhy.executablePath` when `hhy` is not available on `PATH`. Diagnostics use
the versioned `hhy check --format json` contract; formatting uses `hhy fmt`.

See the repository `editors/README.md` for installation and development instructions.

Licensed under Apache-2.0 with the HHY repository.
