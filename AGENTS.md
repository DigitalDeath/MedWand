# Project coding conventions

- Use four spaces for each indentation level; do not use tabs for indentation.
- Separate logical code blocks with one blank line, including adjacent function and class definitions.
- Preserve these conventions when adding or editing code. The user's instructions take precedence over formatter defaults.
- Use `scripts/format-code.py` with clang-format to maintain both indentation and single-blank-line spacing; `--check` verifies without editing. Expand function and lambda bodies for readability.
